#include "Lighting.h"
#include "Rendering/Shader.h"

LightingPass::LightingPass(std::string&& name)
	:RenderPass(std::move(name))
{
	//Register<PassInput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer);
	Register<PassInput<DX::Texture>>("positions", Positions);
	Register<PassInput<DX::Texture>>("normals", Normals);
	Register<PassInput<DX::Texture>>("diffuse", Diffuse);
	Register<PassInput<DX::Texture>>("specular", Specular);
	Register<PassInput<DX::Texture>>("ambientOcclusion", AmbientOcclusion);
	Register<PassInput<DX::DescriptorHeap>>("srvHeap", GBufferHeap);

	Register<PassOutput<DX::Texture>>("renderTarget", RTVBuffer);
	Register<PassOutput<DX::Texture>>("positions", Positions);
	Register<PassOutput<DX::Texture>>("normals", Normals);
	Register<PassOutput<DX::Texture>>("diffuse", Diffuse);
	Register<PassOutput<DX::Texture>>("specular", Specular);
	Register<PassOutput<DX::Texture>>("ambientOcclusion", AmbientOcclusion);
	Register<PassOutput<DX::DescriptorHeap>>("srvHeap", GBufferHeap);
}

void LightingPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	Bind(cmdList);
}

void LightingPass::PreExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList)
{
	Positions->TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Normals->TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Diffuse->TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Specular->TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	AmbientOcclusion->TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void LightingPass::PostExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList)
{
	RTVBuffer->TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void LightingPass::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	//RootSignatureData
	cmdList->SetGraphicsRootSignature(RootSignatureData.RootSignaturePtr.Get());

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)Globals.WindowDimensions.x, (FLOAT)Globals.WindowDimensions.y, 0.0f, 1.0f };
	cmdList->RSSetViewports(1, &viewport);

	// Set scissor rect
	D3D12_RECT scissorRect = { 0, 0, Globals.WindowDimensions.x, Globals.WindowDimensions.y };
	cmdList->RSSetScissorRects(1, &scissorRect);

	const float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	cmdList->ClearRenderTargetView(RTVHeap.begin()->CPUHandle, clearColor, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &RTVHeap.begin()->CPUHandle, FALSE, nullptr);

	// Bind Heaps
	for (size_t i = 0; i < Heaps.size(); i++)
		Heaps[i].Bind(cmdList, i);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0);
}

void LightingPass::InitResources(ID3D12Device5Ptr device)
{
	AOHeap.Create(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	auto srvDesc = AmbientOcclusion->GetSRVDesc();
	device->CreateShaderResourceView(AmbientOcclusion->Resource.Get(), &srvDesc, AOHeap.begin()->CPUHandle);

	DX::TextureCreateInfo rtvTextureInfo;
	rtvTextureInfo.device = device;
	rtvTextureInfo.width = Globals.WindowDimensions.x;
	rtvTextureInfo.height = Globals.WindowDimensions.y;
	rtvTextureInfo.depth = 1;
	rtvTextureInfo.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvTextureInfo.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	rtvTextureInfo.resDim = RESOURCE_DIMENSION_TEXTURE2D;
	rtvTextureInfo.viewDim = VIEW_DIMENSION_TEXTURE2D;
	rtvTextureInfo.initialResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	rtvTextureInfo.clearValue = { .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, .Color = {1.0f, 1.0f, 1.0f, 1.0f} };

	// Create Render Target Texture
	RTVBuffer = MakeShared<DX::Texture>(&rtvTextureInfo);


	// RTV Heap
	RTVHeap.Create(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
	device->CreateRenderTargetView(RTVBuffer->Resource.Get(), nullptr, RTVHeap.begin()->CPUHandle);

	Heaps.push_back(*GBufferHeap);
	Heaps.push_back(AOHeap);
	Heaps.push_back(Globals.SamplerHeap);
	Heaps.push_back(Globals.LightsHeap);
	Heaps.push_back(Globals.CBVHeap);
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
	psoDesc.pRootSignature = RootSignatureData.RootSignaturePtr.Get();
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