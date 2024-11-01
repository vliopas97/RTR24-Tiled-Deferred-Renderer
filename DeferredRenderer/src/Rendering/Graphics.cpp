#include "Graphics.h"
#include "Core/Exception.h"
#include "Utils.h"
#include "Shader.h"

Graphics::Graphics(Window& window)
	:WinHandle(window.Handle), SwapChainSize(window.Width, window.Height)
{
	Init();
	InitScene();
}

Graphics::~Graphics()
{
	Shutdown();
}

void Graphics::Tick()
{
	uint32_t frameIndex = SwapChain->GetCurrentBackBufferIndex();

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
											  FrameObjects[0].CmdAllocator, PipelineState.GetInterfacePtr(), IID_PPV_ARGS(&CmdList)));
	GRAPHICS_ASSERT(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
	FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void Graphics::InitScene()
{
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	RootSignature = D3D::CreateRootSignature(Device, rootSignatureDesc);

	Shader<Vertex> vertexShader("Shader");
	Shader<Pixel> pixelShader("Shader");

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = RootSignature.GetInterfacePtr();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.GetBlob());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.GetBlob());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	GRAPHICS_ASSERT(Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PipelineState)));

	// Create the vertex buffer.
	{
		// Define the geometry for a triangle.
		D3D::Vertex triangleVertices[] =
		{
			{ { 0.0f, 0.25f * aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		// Note: using upload heaps to transfer static data like vert buffers is not 
		// recommended. Every time the GPU needs it, the upload heap will be marshalled 
		// over. Please read up on Default Heap usage. An upload heap is used here for 
		// code simplicity and because there are very few verts to actually transfer.
		GRAPHICS_ASSERT(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&VertexBuffer)));

		// Copy the triangle data to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		GRAPHICS_ASSERT(VertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
		VertexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
		VertexBufferView.StrideInBytes = sizeof(D3D::Vertex);
		VertexBufferView.SizeInBytes = vertexBufferSize;
	}

	CmdList->Close();

	CmdQueue->Signal(Fence, ++FenceValue);
	Fence->SetEventOnCompletion(FenceValue, FenceEvent);
	WaitForSingleObject(FenceEvent, INFINITE);
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

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)SwapChainSize.x, (FLOAT)SwapChainSize.y, 0.0f, 1.0f };
	CmdList->RSSetViewports(1, &viewport);

	// Set scissor rect
	D3D12_RECT scissorRect = { 0, 0, SwapChainSize.x, SwapChainSize.y };
	CmdList->RSSetScissorRects(1, &scissorRect);

	CmdList->SetGraphicsRootSignature(RootSignature.GetInterfacePtr());
	
	const float clearColor[4] = { 0.4f, 0.6f, 0.2f, 1.0f };
	CmdList->OMSetRenderTargets(1, &FrameObjects[frameIndex].RTVHandle, FALSE, nullptr);
	CmdList->ClearRenderTargetView(FrameObjects[frameIndex].RTVHandle, clearColor, 0, nullptr);

	CmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CmdList->IASetVertexBuffers(0, 1, &VertexBufferView);
	CmdList->DrawInstanced(3, 1, 0, 0);


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
