#include "Geometry.h"
#include "Rendering/Shader.h"
#include "Scene.h"

GeometryPass::GeometryPass(std::string&& name) :
	RenderPass(std::move(name))
{
	Positions = MakeShared<DX::Texture>();
	Normals = MakeShared<DX::Texture>();
	Diffuse = MakeShared<DX::Texture>();

	Register<PassInput<DX::Texture>>("depthBuffer", DSVBuffer);
	Register<PassOutput<DX::Texture>>("depthBuffer", DSVBuffer);
	Register<PassOutput<DX::Texture>>("positions", Positions);
	Register<PassOutput<DX::Texture>>("normals", Normals);
	Register<PassOutput<DX::Texture>>("diffuse", Diffuse);
	Register<PassOutput<DX::Texture>>("specular", Specular);
	Register<PassOutput<DX::DescriptorHeap>>("srvHeap", SRVHeap);
}

void GeometryPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	//RootSignatureData
	cmdList->SetGraphicsRootSignature(RootSignatureData.RootSignaturePtr.Get());

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)Globals.WindowDimensions.x, (FLOAT)Globals.WindowDimensions.y, 0.0f, 1.0f };
	cmdList->RSSetViewports(1, &viewport);

	// Set scissor rect
	D3D12_RECT scissorRect = { 0, 0, Globals.WindowDimensions.x, Globals.WindowDimensions.y };
	cmdList->RSSetScissorRects(1, &scissorRect);

	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 4> rtvHandles;
	for (size_t i = 0; i < RTVHeap.GetNumDescriptors(); i++)
		rtvHandles[i] = RTVHeap[i].CPUHandle;
	cmdList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), FALSE, &Globals.DSVHandle);

	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	for (auto handle : RTVHeap)
		cmdList->ClearRenderTargetView(handle.CPUHandle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(Globals.DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0.0f, 0, nullptr);

	for (size_t i = 0; i < Heaps.size(); i++)
		Heaps[i].Bind(cmdList, i);

	scene.Bind<GeometryPass>(cmdList);
}

void GeometryPass::PreExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList)
{
	DSVBuffer->TransitionTo(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void GeometryPass::PostExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList)
{
	Positions->TransitionTo(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Normals->TransitionTo(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Diffuse->TransitionTo(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Specular->TransitionTo(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	DSVBuffer->TransitionTo(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void GeometryPass::InitResources(ID3D12Device5Ptr device)
{
	RTVHeap.Create(device, 4, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

	SRVHeap = MakeShared<DX::DescriptorHeap>();
	SRVHeap->Create(device, 4, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	DX::TextureCreateInfo texDesc{};
	texDesc.device = device;
	texDesc.clearValue = {.Format = DXGI_FORMAT_R32G32B32A32_FLOAT, .Color = {0.0f, 0.0f, 0.0f, 1.0f} };
	texDesc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texDesc.width = Globals.WindowDimensions.x;
	texDesc.height = Globals.WindowDimensions.y;
	texDesc.initialResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	texDesc.resDim = RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.viewDim = VIEW_DIMENSION_TEXTURE2D;
	texDesc.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	Positions = MakeShared<DX::Texture>(&texDesc);
	Normals = MakeShared<DX::Texture>(&texDesc);

	texDesc.format = texDesc.clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	Diffuse = MakeShared<DX::Texture>(&texDesc);
	Specular = MakeShared<DX::Texture>(&texDesc);

	// Creating Render Target Views
	device->CreateRenderTargetView(Positions->Resource.Get(), nullptr, RTVHeap[0].CPUHandle);
	device->CreateRenderTargetView(Normals->Resource.Get(), nullptr, RTVHeap[1].CPUHandle);
	device->CreateRenderTargetView(Diffuse->Resource.Get(), nullptr, RTVHeap[2].CPUHandle);
	device->CreateRenderTargetView(Specular->Resource.Get(), nullptr, RTVHeap[3].CPUHandle);

	auto& srvHeap = *SRVHeap;

	auto srvDesc = Positions->GetSRVDesc();
	device->CreateShaderResourceView(Positions->Resource.Get(), &srvDesc, srvHeap[0].CPUHandle);
	device->CreateShaderResourceView(Normals->Resource.Get(), &srvDesc, srvHeap[1].CPUHandle);

	srvDesc = Diffuse->GetSRVDesc();
	device->CreateShaderResourceView(Diffuse->Resource.Get(), &srvDesc, srvHeap[2].CPUHandle);
	device->CreateShaderResourceView(Specular->Resource.Get(), &srvDesc, srvHeap[3].CPUHandle);

	Heaps.push_back(Globals.SRVHeap);
	Heaps.push_back(Globals.CBVHeap);
	Heaps.push_back(Globals.SamplerHeap);
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
	psoDesc.pRootSignature = RootSignatureData.RootSignaturePtr.Get();
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
