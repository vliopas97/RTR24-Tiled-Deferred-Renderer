#include "AmbientOcclusion.h"
#include "Rendering/Shader.h"

AmbientOcclusionPass::AmbientOcclusionPass(std::string&& name)
	:RenderPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void AmbientOcclusionPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	//RootSignatureData
	cmdList->SetGraphicsRootSignature(RootSignatureData.RootSignaturePtr.GetInterfacePtr());

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)Globals.WindowDimensions.x, (FLOAT)Globals.WindowDimensions.y, 0.0f, 1.0f };
	cmdList->RSSetViewports(1, &viewport);

	// Set scissor rect
	D3D12_RECT scissorRect = { 0, 0, Globals.WindowDimensions.x, Globals.WindowDimensions.y };
	cmdList->RSSetScissorRects(1, &scissorRect);

	const float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	cmdList->ClearRenderTargetView(RTVHandle, clearColor, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &RTVHandle, FALSE, nullptr); // NEED CUSTOM RTV!

	Heaps.Bind(cmdList);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0);
}

void AmbientOcclusionPass::InitResources(ID3D12Device5Ptr device)
{
	auto& cmdList = Globals.CmdList;
	auto& cmdAllocator = Globals.CmdAllocator;

	auto randRange = [](float min, float max)
		{
			return min + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) / (max - min));
		};

	std::srand(static_cast<unsigned int>(std::time(nullptr)));
	std::array<glm::float3, 64> noiseTextureFloats;

	for (int i = 0; i < 64; i++)
	{
		noiseTextureFloats[i].x = randRange(-1.0f, 1.0f);
		noiseTextureFloats[i].y = randRange(-1.0f, 1.0f);
		noiseTextureFloats[i].z = 0.0f;
	}

	// Create Random Texture
	auto resDesc = CD3DX12_RESOURCE_DESC(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		8, 8, 1, 1,
		DXGI_FORMAT_R32G32B32_FLOAT,
		1, 0,
		D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE);

	ID3D12ResourcePtr randomTexture;
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&randomTexture)));
	RandomTexture = MakeShared<ID3D12ResourcePtr>(randomTexture);

	ID3D12ResourcePtr stagingBufferRandTex;
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(GetRequiredIntermediateSize(*RandomTexture, 0, 1)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&stagingBufferRandTex)));

	D3D12_SUBRESOURCE_DATA textureData{};
	textureData.pData = noiseTextureFloats.data();
	textureData.RowPitch = resDesc.Width * sizeof(glm::float3);
	textureData.SlicePitch = textureData.RowPitch * resDesc.Height;

	UpdateSubresources(cmdList, *RandomTexture, stagingBufferRandTex, 0, 0, 1, &textureData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(*RandomTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
	// Done Creating Random Texture

	// Create SSAO Kernel
	std::array<glm::float3, 16> ssaoKernelVals;

	for (int i = 0; i < 16; i++)
	{
		ssaoKernelVals[i].x = randRange(-1.0f, 1.0f);
		ssaoKernelVals[i].y = randRange(-1.0f, 1.0f);
		ssaoKernelVals[i].z = randRange(0.0f, 1.0f);

		float scale = (float)i / 16.0f;
		float scaleMul = glm::lerp(0.1f, 1.0f, scale * scale);

		ssaoKernelVals[i].x *= scaleMul;
		ssaoKernelVals[i].y *= scaleMul;
		ssaoKernelVals[i].z *= scaleMul;
	}

	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Alignment = 0;
	resDesc.Width = sizeof(glm::float3) * ssaoKernelVals.size(); // Total buffer size
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN; // No specific format for structured buffers
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12ResourcePtr structuredBuffer;
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&structuredBuffer));

	void* mappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 }; // We don't intend to read from this resource on the CPU
	structuredBuffer->Map(0, &readRange, &mappedData);
	memcpy(mappedData, ssaoKernelVals.data(), sizeof(glm::float3) * ssaoKernelVals.size());
	structuredBuffer->Unmap(0, nullptr);

	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeof(glm::float3) * ssaoKernelVals.size(); // Total buffer size
	resDesc.Format = DXGI_FORMAT_UNKNOWN;  // No specific format for structured buffers
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12ResourcePtr gpuBuffer;
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&gpuBuffer)));

	cmdList->CopyBufferRegion(gpuBuffer, 0, structuredBuffer, 0, sizeof(glm::float3) * ssaoKernelVals.size());

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(gpuBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	SSAOKernel = MakeShared<ID3D12ResourcePtr>(gpuBuffer);
	// Done Creating SSAO Kernel

	// Create Render Target
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	clearValue.Color[0] = 1.0f;
	clearValue.Color[1] = 1.0f;
	clearValue.Color[2] = 1.0f;
	clearValue.Color[3] = 1.0f;

	resDesc = CD3DX12_RESOURCE_DESC(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		Globals.WindowDimensions.x, Globals.WindowDimensions.y, 1, 1,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		1, 0,
		D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	ID3D12ResourcePtr renderTarget;
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&renderTarget)));
	RTVBuffer = MakeShared<ID3D12ResourcePtr>(renderTarget);

	// RTV Heap
	auto rtvHeap = D3D::CreateDescriptorHeap(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
	RTVHeap = MakeShared<ID3D12DescriptorHeapPtr>(rtvHeap);

	RTVHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateRenderTargetView(*RTVBuffer, nullptr, RTVHandle);

	// SRV Heap
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	auto srvHeap = D3D::CreateDescriptorHeap(device, 4, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	SRVHeap = MakeShared<ID3D12DescriptorHeapPtr>(srvHeap);

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = SRVHeap->GetInterfacePtr()->GetCPUDescriptorHandleForHeapStart();
	UINT srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	device->CreateShaderResourceView(*Positions, &srvDesc, srvHandle);
	srvHandle.ptr += srvDescriptorSize;

	srvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	device->CreateShaderResourceView(*RandomTexture, &srvDesc, srvHandle);
	srvHandle.ptr += srvDescriptorSize;

	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	device->CreateShaderResourceView(*Normals, &srvDesc, srvHandle);
	srvHandle.ptr += srvDescriptorSize;

	srvDesc.Format = DXGI_FORMAT_UNKNOWN; // Structured buffers have no specific format
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = ssaoKernelVals.size(); // Number of elements in the buffer
	srvDesc.Buffer.StructureByteStride = sizeof(glm::float3); // Size of each element
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	device->CreateShaderResourceView(*SSAOKernel, &srvDesc, srvHandle);

	// Set up heaps to bind
	Heaps.PushBack(*SRVHeap);
	Heaps.PushBack(Globals.SamplerHeap);
	Heaps.PushBack(Globals.CBVHeap);

	// Synchronization Point - Important because I copy data to GPU above
	Globals.FenceValue = D3D::SubmitCommandList(cmdList, Globals.CmdQueue, Globals.Fence, Globals.FenceValue);
	Globals.Fence->SetEventOnCompletion(Globals.FenceValue, Globals.FenceEvent);
	WaitForSingleObject(Globals.FenceEvent, INFINITE);
	cmdList->Reset(cmdAllocator, nullptr);
}

void AmbientOcclusionPass::InitRootSignature()
{
	std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0)
	};

	std::vector<D3D12_DESCRIPTOR_RANGE> samplerRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0)
	};

	std::vector<D3D12_DESCRIPTOR_RANGE> cbvRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0)
	};

	RootSignatureData.AddDescriptorTable(srvRanges, D3D12_SHADER_VISIBILITY_PIXEL);
	RootSignatureData.AddDescriptorTable(samplerRanges, D3D12_SHADER_VISIBILITY_PIXEL);
	RootSignatureData.AddDescriptorTable(cbvRanges, D3D12_SHADER_VISIBILITY_PIXEL);

	RootSignatureData.Build(Device);
}

void AmbientOcclusionPass::InitPipelineState()
{
	Shader<Vertex> vertexShader("FullScreenTriangle");
	Shader<Pixel> pixelShader("AmbientOcclusion");

	D3D12_INPUT_LAYOUT_DESC layoutDesc{};
	layoutDesc.pInputElementDescs = nullptr;
	layoutDesc.NumElements = 0;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = layoutDesc;
	psoDesc.pRootSignature = RootSignatureData.RootSignaturePtr.GetInterfacePtr();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.GetBlob());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.GetBlob());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;

	GRAPHICS_ASSERT(Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PipelineState)));
}