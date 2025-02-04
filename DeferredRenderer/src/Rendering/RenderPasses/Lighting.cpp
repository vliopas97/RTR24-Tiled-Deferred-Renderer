#include "Lighting.h"
#include "Rendering/Shader.h"

LightingPass::LightingPass(std::string&& name)
	:RenderPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassInput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("diffuse", Diffuse, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("specular", Specular, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("ambientOcclusion", AmbientOcclusion, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12DescriptorHeapPtr>>("srvHeap", GBufferHeap);

	Register<PassOutput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("diffuse", Diffuse, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("specular", Specular, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("ambientOcclusion", AmbientOcclusion, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12DescriptorHeapPtr>>("srvHeap", GBufferHeap);
}

void LightingPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	Bind(cmdList);
}

void LightingPass::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	//RootSignatureData
	cmdList->SetGraphicsRootSignature(RootSignatureData.RootSignaturePtr.GetInterfacePtr());

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)Globals.WindowDimensions.x, (FLOAT)Globals.WindowDimensions.y, 0.0f, 1.0f };
	cmdList->RSSetViewports(1, &viewport);

	// Set scissor rect
	D3D12_RECT scissorRect = { 0, 0, Globals.WindowDimensions.x, Globals.WindowDimensions.y };
	cmdList->RSSetScissorRects(1, &scissorRect);
	cmdList->OMSetRenderTargets(1, &Globals.RTVHandle, FALSE, nullptr);

	Heaps.Bind(cmdList);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0);
}

void LightingPass::InitResources(ID3D12Device5Ptr device)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	AOHeap = D3D::CreateDescriptorHeap(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = AOHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(*AmbientOcclusion, &srvDesc, srvHandle);

	Heaps.PushBack(*GBufferHeap);
	Heaps.PushBack(AOHeap);
	Heaps.PushBack(Globals.SamplerHeap);
	Heaps.PushBack(Globals.LightsHeap);
	Heaps.PushBack(Globals.CBVHeap);
}

void LightingPass::InitRootSignature()
{
	std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0)
	};
	RootSignatureData.AddDescriptorTable(srvRanges, D3D12_SHADER_VISIBILITY_PIXEL);

	srvRanges.clear();
	srvRanges.emplace_back(DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0));
	RootSignatureData.AddDescriptorTable(srvRanges, D3D12_SHADER_VISIBILITY_PIXEL);

	std::vector<D3D12_DESCRIPTOR_RANGE> samplerRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0)
	};

	std::vector<D3D12_DESCRIPTOR_RANGE> lightRanges = {
	DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 100)
	};

	std::vector<D3D12_DESCRIPTOR_RANGE> cbvRanges;
	for (uint32_t i = 0; i < NumGlobalCBVDescriptorRanges; ++i)
		cbvRanges.emplace_back(DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, UINT_MAX, 0, i));

	RootSignatureData.AddDescriptorTable(samplerRanges, D3D12_SHADER_VISIBILITY_PIXEL);
	RootSignatureData.AddDescriptorTable(lightRanges, D3D12_SHADER_VISIBILITY_PIXEL);
	RootSignatureData.AddDescriptorTable(cbvRanges, D3D12_SHADER_VISIBILITY_PIXEL);

	RootSignatureData.Build(Device);
}

void LightingPass::InitPipelineState()
{
	Shader<Vertex> vertexShader("FullScreenTriangle");
	Shader<Pixel> pixelShader("LightingPass");

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
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;

	GRAPHICS_ASSERT(Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PipelineState)));
}