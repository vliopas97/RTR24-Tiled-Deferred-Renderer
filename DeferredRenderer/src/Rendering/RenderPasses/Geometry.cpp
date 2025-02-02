#include "Geometry.h"
#include "Rendering/Shader.h"
#include "Scene.h"

GeometryPass::GeometryPass(std::string&& name) :
	RenderPass(std::move(name))
{
	Positions = MakeShared<ID3D12ResourcePtr>();
	Normals = MakeShared<ID3D12ResourcePtr>();
	Diffuse = MakeShared<ID3D12ResourcePtr>();

	Register<PassInput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	Register<PassOutput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	Register<PassOutput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("diffuse", Diffuse, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("specular", Specular, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12DescriptorHeapPtr>>("srvHeap", SRVHeap);
	Register<PassOutput<ID3D12DescriptorHeapPtr>>("srvHeapRO", SRVHeapRO);
}

void GeometryPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	//RootSignatureData
	cmdList->SetGraphicsRootSignature(RootSignatureData.RootSignaturePtr.GetInterfacePtr());

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)Globals.WindowDimensions.x, (FLOAT)Globals.WindowDimensions.y, 0.0f, 1.0f };
	cmdList->RSSetViewports(1, &viewport);

	// Set scissor rect
	D3D12_RECT scissorRect = { 0, 0, Globals.WindowDimensions.x, Globals.WindowDimensions.y };
	cmdList->RSSetScissorRects(1, &scissorRect);
	cmdList->OMSetRenderTargets(4, RTVHandles.data(), FALSE, &Globals.DSVHandle);

	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	for (auto handle : RTVHandles)
		cmdList->ClearRenderTargetView(handle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(Globals.DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0.0f, 0, nullptr);

	Heaps.Bind(cmdList);
	scene.Bind<GeometryPass>(cmdList);
}

void GeometryPass::InitResources(ID3D12Device5Ptr device)
{
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	clearValue.Color[0] = 0.0f; // Default clear color
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 1.0f;

	auto resDesc = CD3DX12_RESOURCE_DESC(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		Globals.WindowDimensions.x, Globals.WindowDimensions.y, 1, 1,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		1, 0,
		D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	ID3D12ResourcePtr positions;
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&positions)));
	Positions = MakeShared<ID3D12ResourcePtr>(positions);

	ID3D12ResourcePtr normals;
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&normals)));
	Normals = MakeShared<ID3D12ResourcePtr>(normals);

	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	ID3D12ResourcePtr diffuse;
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&diffuse)));
	Diffuse = MakeShared<ID3D12ResourcePtr>(diffuse);

	ID3D12ResourcePtr specular;
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&specular)));
	Specular = MakeShared<ID3D12ResourcePtr>(specular);

	auto rtvHeap = D3D::CreateDescriptorHeap(device, 4, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
	RTVHeap = MakeShared<ID3D12DescriptorHeapPtr>(rtvHeap);

	auto srvHeap = D3D::CreateDescriptorHeap(device, 4, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	SRVHeap = MakeShared<ID3D12DescriptorHeapPtr>(srvHeap);

	auto srvHeapRO = D3D::CreateDescriptorHeap(device, 4, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);
	SRVHeapRO = MakeShared<ID3D12DescriptorHeapPtr>(srvHeapRO);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = RTVHeap->GetInterfacePtr()->GetCPUDescriptorHandleForHeapStart();
	UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Creating Render Target Views
	RTVHandles[0] = rtvHandle;
	device->CreateRenderTargetView(*Positions, nullptr, rtvHandle);
	rtvHandle.ptr += rtvDescriptorSize;

	RTVHandles[1] = rtvHandle;
	device->CreateRenderTargetView(*Normals, nullptr, rtvHandle);
	rtvHandle.ptr += rtvDescriptorSize;

	RTVHandles[2] = rtvHandle;
	device->CreateRenderTargetView(*Diffuse, nullptr, rtvHandle);
	rtvHandle.ptr += rtvDescriptorSize;

	RTVHandles[3] = rtvHandle;
	device->CreateRenderTargetView(*Specular, nullptr, rtvHandle);

	// Creating Shader Visible Views to be used in subsequent Lighting Pass
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = SRVHeapRO->GetInterfacePtr()->GetCPUDescriptorHandleForHeapStart();
	UINT srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	device->CreateShaderResourceView(*Positions, &srvDesc, srvHandle);
	srvHandle.ptr += srvDescriptorSize;

	device->CreateShaderResourceView(*Normals, &srvDesc, srvHandle);
	srvHandle.ptr += srvDescriptorSize;

	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	device->CreateShaderResourceView(*Diffuse, &srvDesc, srvHandle);
	srvHandle.ptr += srvDescriptorSize;

	device->CreateShaderResourceView(*Specular, &srvDesc, srvHandle);
	srvHandle.ptr += srvDescriptorSize;

	device->CopyDescriptorsSimple(4, (*SRVHeap)->GetCPUDescriptorHandleForHeapStart(),
								  (*SRVHeapRO)->GetCPUDescriptorHandleForHeapStart(),
								  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	Heaps.PushBack(Globals.SRVHeap);
	Heaps.PushBack(Globals.CBVHeap);
	Heaps.PushBack(Globals.SamplerHeap);
}

void GeometryPass::InitRootSignature()
{
	std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges;
	uint32_t userStart = NumGlobalSRVDescriptorRanges - NumUserDescriptorRanges;
	for (uint32_t i = 0; i < NumGlobalSRVDescriptorRanges; ++i)
	{
		UINT registerSpace = (i >= userStart) ? (i - userStart) + 100 : i;
		srvRanges.emplace_back(DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, registerSpace));
	}

	std::vector<D3D12_DESCRIPTOR_RANGE> cbvRanges;
	for (uint32_t i = 0; i < NumGlobalCBVDescriptorRanges; ++i)
		cbvRanges.emplace_back(DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, UINT_MAX, 0, i));

	std::vector<D3D12_DESCRIPTOR_RANGE> samplerRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0)
	};

	RootSignatureData.AddDescriptorTable(srvRanges, D3D12_SHADER_VISIBILITY_ALL);
	RootSignatureData.AddDescriptorTable(cbvRanges, D3D12_SHADER_VISIBILITY_ALL);
	RootSignatureData.AddDescriptorTable(samplerRanges, D3D12_SHADER_VISIBILITY_ALL);
	RootSignatureData.AddDescriptor(D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL, 0, 200);

	RootSignatureData.Build(Device);
}

void GeometryPass::InitPipelineState()
{
	Shader<Vertex> vertexShader("GeometryPass");
	Shader<Pixel> pixelShader("GeometryPass");

	BufferLayout layout{ {"POSITION", DataType::float3},
						{"NORMAL", DataType::float3},
						{"TANGENT", DataType::float3},
						{"BITANGENT", DataType::float3},
						{"TEXCOORD", DataType::float2}, };

	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;  // No culling

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = layout;
	psoDesc.pRootSignature = RootSignatureData.RootSignaturePtr.GetInterfacePtr();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.GetBlob());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.GetBlob());
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 4;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
	psoDesc.RTVFormats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;
	psoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;

	GRAPHICS_ASSERT(Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PipelineState)));
}
