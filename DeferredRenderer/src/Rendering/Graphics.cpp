#include "Graphics.h"
#include "Core/Exception.h"
#include "Utils.h"
#include "Shader.h"

Graphics::Graphics(Window& window)
	:WinHandle(window.Handle), SwapChainSize(window.Width, window.Height), SceneCamera()
{
	Init();

	RootSignature rootSignature(Device, D3D::InitializeGlobalRootSignature(Device));
	PipelineBindings.Initialize(Device, rootSignature);

	InitScene();
}

Graphics::~Graphics()
{
	Shutdown();
}

void Graphics::Tick(float delta)
{
	uint32_t frameIndex = SwapChain->GetCurrentBackBufferIndex();

	// Update Pipeline Constants
	SceneCamera.Tick(delta);
	PipelineBindings.CBGlobalConstants.CPUData.CameraPosition = SceneCamera.GetPosition();
	PipelineBindings.CBGlobalConstants.CPUData.ViewProjection = SceneCamera.GetViewProjection();

	PipelineBindings.Tick();
	MainScene->Tick();

	PopulateCommandList(frameIndex);
	SwapChain->Present(1, 0);

	if (FenceValue > DefaultSwapChainBuffers)
	{
		Fence->SetEventOnCompletion(FenceValue - DefaultSwapChainBuffers + 1, FenceEvent);
		WaitForSingleObject(FenceEvent, INFINITE);
	}

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

		// Create Depth Stencil Buffers
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc{};
		depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

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
											  FrameObjects[0].CmdAllocator, PipelineState.GetInterfacePtr(), IID_PPV_ARGS(&CmdList)));
	GRAPHICS_ASSERT(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
	FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void Graphics::InitScene()
{
	MainScene = MakeUnique<Scene>(Device);

	Shader<Vertex> vertexShader("Shader");
	Shader<Pixel> pixelShader("Shader");

	BufferLayout layout{ {"POSITION", DataType::float3},
						{"COLOR", DataType::float4},
						{"TEXCOORDS", DataType::float2} };


	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = layout;
	psoDesc.pRootSignature = PipelineBindings.GetRootSignaturePtr();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.GetBlob());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.GetBlob());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	GRAPHICS_ASSERT(Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PipelineState)));

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
}

void Graphics::PopulateCommandList(UINT frameIndex)
{
	FrameObjects[frameIndex].CmdAllocator->Reset();
	CmdList->Reset(FrameObjects[frameIndex].CmdAllocator, PipelineState.GetInterfacePtr());
	D3D::ResourceBarrier(CmdList,
						 FrameObjects[frameIndex].SwapChainBuffer,
						 D3D12_RESOURCE_STATE_PRESENT,
						 D3D12_RESOURCE_STATE_RENDER_TARGET);

	PipelineBindings.Bind(CmdList);

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)SwapChainSize.x, (FLOAT)SwapChainSize.y, 0.0f, 1.0f };
	CmdList->RSSetViewports(1, &viewport);

	// Set scissor rect
	D3D12_RECT scissorRect = { 0, 0, SwapChainSize.x, SwapChainSize.y };
	CmdList->RSSetScissorRects(1, &scissorRect);

	const float clearColor[4] = { 0.4f, 0.6f, 0.2f, 1.0f };
	CmdList->OMSetRenderTargets(1, &FrameObjects[frameIndex].RTVHandle, FALSE, &FrameObjects[frameIndex].DSVHandle);
	CmdList->ClearRenderTargetView(FrameObjects[frameIndex].RTVHandle, clearColor, 0, nullptr);
	CmdList->ClearDepthStencilView(FrameObjects[frameIndex].DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0.0f, 0, nullptr);

	MainScene->Bind(CmdList);

	D3D::ResourceBarrier(CmdList,
						 FrameObjects[frameIndex].SwapChainBuffer,
						 D3D12_RESOURCE_STATE_RENDER_TARGET,
						 D3D12_RESOURCE_STATE_PRESENT);

	FenceValue = D3D::SubmitCommandList(CmdList, CmdQueue, Fence, FenceValue);
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
	MainScene->CreateShaderResources(Device, CmdQueue, PipelineBindings);
}
