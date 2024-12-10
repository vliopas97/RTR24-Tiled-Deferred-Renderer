#include "Graphics.h"
#include "Core/Exception.h"
#include "Utils.h"
#include "Core/Layer.h"

#include "Shader.h"

namespace
{
	auto& SRVHeap = Globals.SRVHeap;
	auto& UAVHeap = Globals.UAVHeap;
	auto& CBVHeap = Globals.CBVHeap;
	auto& SamplerHeap = Globals.SamplerHeap;
	auto& LightsHeap = Globals.LightsHeap;

	auto& CBGlobalConstants = Globals.CBGlobalConstants;

	auto& Fence = Globals.Fence;
	auto& FenceEvent = Globals.FenceEvent;
	auto& FenceValue = Globals.FenceValue;

	auto& CmdQueue = Globals.CmdQueue;
}

Graphics::Graphics(Window& window)
	:WinHandle(window.GetHandle()), SwapChainSize(window.GetWidth(), window.GetHeight()), 
	SceneCamera(), ImGui(MakeUnique<ImGuiLayer>())
{
	Init();
	InitGlobals();
	//FRPass.Init(Device);

	ImGui->OnAttach(Device);
	InitScene();
	Graph.Init(Device);
}

Graphics::~Graphics()
{
	Shutdown();
}

void Graphics::Tick(float delta)
{
	uint32_t frameIndex = SwapChain->GetCurrentBackBufferIndex();

	//ImGui->Begin();

	// Update Pipeline Constants
	CmdQueue->Signal(Fence, ++FenceValue);
	Fence->SetEventOnCompletion(FenceValue, FenceEvent);
	WaitForSingleObject(FenceEvent, INFINITE);

	SceneCamera.Tick(delta);
	CBGlobalConstants.CPUData.CameraPosition = SceneCamera.GetPosition();
	CBGlobalConstants.CPUData.View = SceneCamera.GetView();
	CBGlobalConstants.CPUData.ViewProjection = SceneCamera.GetViewProjection();

	GlobalResManager::SetRTV(FrameObjects[frameIndex].SwapChainBuffer, FrameObjects[frameIndex].RTVHandle);
	GlobalResManager::SetDSV(FrameObjects[frameIndex].DepthStencilBuffer, FrameObjects[frameIndex].DSVHandle);
	GlobalResManager::SetCmdAllocator(FrameObjects[frameIndex].CmdAllocator);
	
	CBGlobalConstants.Tick();
	Graph.Tick();
	MainScene->Tick();

	Graph.Execute(CmdList, *MainScene);

	EndFrame(frameIndex);
}

void Graphics::Init()
{
#ifndef NDEBUG
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&Debug))))
		Debug->EnableDebugLayer();
#endif 

	GRAPHICS_ASSERT(CreateDXGIFactory1(IID_PPV_ARGS(&Factory)));

	CreateDevice();
	CmdQueue = D3D::CreateCommandQueue(Device);
	CreateSwapChain();
	RTVHeap.Heap = D3D::CreateDescriptorHeap(Device, RTVHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
	DSVHeap.Heap = D3D::CreateDescriptorHeap(Device, DSVHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);

#ifndef NDEBUG
	ID3D12InfoQueue* InfoQueue = nullptr;
	Device->QueryInterface(IID_PPV_ARGS(&InfoQueue));

	InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
	InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
	InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
#endif

	for (uint32_t i = 0; i < DefaultSwapChainBuffers; i++)
	{
		GRAPHICS_ASSERT(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
													   IID_PPV_ARGS(&FrameObjects[i].CmdAllocator)));
		GRAPHICS_ASSERT(SwapChain->GetBuffer(i, IID_PPV_ARGS(&FrameObjects[i].SwapChainBuffer)));
		FrameObjects[i].RTVHandle = D3D::CreateRTV(Device, 
												   FrameObjects[i].SwapChainBuffer,
												   RTVHeap.Heap, 
												   RTVHeap.UsedEntries,
												   DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

		D3D12_CLEAR_VALUE depthOptimizedClearValue{};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		GRAPHICS_ASSERT(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, SwapChainSize.x, SwapChainSize.y, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&FrameObjects[i].DepthStencilBuffer)
		));

		// DSV Handles
		FrameObjects[i].DSVHandle = D3D::CreateDSV(Device,
												   FrameObjects[i].DepthStencilBuffer,
												   DSVHeap.Heap,
												   DSVHeap.UsedEntries,
												   DXGI_FORMAT_D32_FLOAT);
	}

	GRAPHICS_ASSERT(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
											  FrameObjects[0].CmdAllocator, nullptr, IID_PPV_ARGS(&CmdList)));
	GRAPHICS_ASSERT(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
	FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void Graphics::InitGlobals()
{
	SRVHeap = D3D::CreateDescriptorHeap(Device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	UAVHeap = D3D::CreateDescriptorHeap(Device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	CBVHeap = D3D::CreateDescriptorHeap(Device, 400, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	SamplerHeap = D3D::CreateDescriptorHeap(Device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, true);
	LightsHeap = D3D::CreateDescriptorHeap(Device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	GlobalResManager::SetRTV(FrameObjects[0].SwapChainBuffer, FrameObjects[0].RTVHandle);
	GlobalResManager::SetDSV(FrameObjects[0].DepthStencilBuffer, FrameObjects[0].DSVHandle);
	GlobalResManager::SetCmdAllocator(FrameObjects[0].CmdAllocator);

	CBGlobalConstants.Init(Device, CBVHeap->GetCPUDescriptorHandleForHeapStart());
}

void Graphics::InitScene()
{
	MainScene = MakeUnique<Scene>(Device, SceneCamera);

	CmdList->Close();

	CmdQueue->Signal(Fence, ++FenceValue);
	Fence->SetEventOnCompletion(FenceValue, FenceEvent);
	WaitForSingleObject(FenceEvent, INFINITE);

	CreateShaderResources();
}

void Graphics::Shutdown()
{
	CmdQueue->Signal(Fence, ++FenceValue);
	Fence->SetEventOnCompletion(FenceValue, FenceEvent);
	WaitForSingleObject(FenceEvent, INFINITE);

	ImGui.reset();
}

void Graphics::CreateDevice()
{
	IDXGIAdapter1Ptr adapter;
	
	for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != Factory->EnumAdapters1(i, &adapter); i++)
	{
		DXGI_ADAPTER_DESC1 desc{};
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

		GRAPHICS_ASSERT(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&Device)));
	}
}

void Graphics::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC1 desc{};
	desc.BufferCount = DefaultSwapChainBuffers;
	desc.Width = SwapChainSize.x;
	desc.Height = SwapChainSize.y;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.SampleDesc.Count = 1;

	MAKE_SMART_COM_PTR(IDXGISwapChain1);
	IDXGISwapChain1Ptr swapChain;

	GRAPHICS_ASSERT(Factory->CreateSwapChainForHwnd(CmdQueue, WinHandle, &desc, nullptr, nullptr, &swapChain));

	GRAPHICS_ASSERT(swapChain->QueryInterface(IID_PPV_ARGS(&SwapChain)));
}

void Graphics::CreateShaderResources()
{
	MainScene->CreateShaderResources(Device, CmdQueue);
}

void Graphics::ResetCommandList()
{
	GetCommandAllocator()->Reset();
	CmdList->Reset(GetCommandAllocator(), Graph.Passes[0]->GetPSO());
}

void Graphics::EndFrame(UINT frameIndex)
{
	SwapChain->Present(1, 0);

	if (FenceValue > DefaultSwapChainBuffers)
	{
		Fence->SetEventOnCompletion(FenceValue - DefaultSwapChainBuffers + 1, FenceEvent);
		WaitForSingleObject(FenceEvent, INFINITE);
	}
}
