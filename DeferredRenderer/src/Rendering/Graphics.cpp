#include "Graphics.h"
#include "Core/Exception.h"
#include "Utils.h"

Graphics::Graphics(Window& window)
	:WinHandle(window.Handle), SwapChainSize(window.Width, window.Height)
{
	Init();
}

Graphics::~Graphics()
{
	Shutdown();
}

void Graphics::Tick()
{
	uint32_t frameIndex = SwapChain->GetCurrentBackBufferIndex();

	const float clearColor[4] = { 0.4f, 0.6f, 0.2f, 1.0f };
	D3D::ResourceBarrier(CmdList,
						 FrameObjects[frameIndex].SwapChainBuffer,
						 D3D12_RESOURCE_STATE_PRESENT,
						 D3D12_RESOURCE_STATE_RENDER_TARGET);
	CmdList->ClearRenderTargetView(FrameObjects[frameIndex].RTVHandle, clearColor, 0, nullptr);
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
	}

	GRAPHICS_ASSERT(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
											  FrameObjects[0].CmdAllocator, nullptr, IID_PPV_ARGS(&CmdList)));
	GRAPHICS_ASSERT(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
	FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void Graphics::Shutdown()
{
	CmdQueue->Signal(Fence, ++FenceValue);
	Fence->SetEventOnCompletion(FenceValue, FenceEvent);
	WaitForSingleObject(FenceEvent, INFINITE);
}

void Graphics::EndFrame(UINT index)
{
	D3D::ResourceBarrier(CmdList, 
						 FrameObjects[index].SwapChainBuffer,
						 D3D12_RESOURCE_STATE_RENDER_TARGET, 
						 D3D12_RESOURCE_STATE_PRESENT);

	FenceValue = D3D::SubmitCommandList(CmdList, CmdQueue, Fence, FenceValue);
	SwapChain->Present(1, 0);

	uint32_t bufferIndex = SwapChain->GetCurrentBackBufferIndex();

	if (FenceValue > DefaultSwapChainBuffers)
	{
		Fence->SetEventOnCompletion(FenceValue - DefaultSwapChainBuffers + 1, FenceEvent);
		WaitForSingleObject(FenceEvent, INFINITE);
	}

	FrameObjects[bufferIndex].CmdAllocator->Reset();
	CmdList->Reset(FrameObjects[bufferIndex].CmdAllocator, nullptr);
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
