#include "ReflectionPass.h"
#include "Rendering/Shader.h"

ReflectionPass::ReflectionPass(std::string&& name)
	:RenderPass(std::move(name))
{
	//Register<PassInput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer);
	Register<PassInput<DX::Texture>>("positions", Positions);
	Register<PassInput<DX::Texture>>("normals", Normals);
	Register<PassInput<DX::Texture>>("pixelsColor", PixelsColor);

	Register<PassOutput<DX::Texture>>("renderTarget", RTVBuffer);
	Register<PassOutput<DX::Texture>>("positions", Positions);
	Register<PassOutput<DX::Texture>>("normals", Normals);
	Register<PassOutput<DX::Texture>>("pixelsColor", PixelsColor);
}

void ReflectionPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene & scene)
{
	Bind(cmdList);
}

void ReflectionPass::PreExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList)
{
	Positions->TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Normals->TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	PixelsColor->TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void ReflectionPass::PostExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList)
{
	RTVBuffer->TransitionTo(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void ReflectionPass::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	//RootSignatureData
	cmdList->SetGraphicsRootSignature(RootSignatureData.RootSignaturePtr.Get());

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)Globals.WindowDimensions.x, (FLOAT)Globals.WindowDimensions.y, 0.0f, 1.0f };
	cmdList->RSSetViewports(1, &viewport);

	// Set scissor rect
	D3D12_RECT scissorRect = { 0, 0, Globals.WindowDimensions.x, Globals.WindowDimensions.y };
	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	cmdList->ClearRenderTargetView(RTVHeap.begin()->CPUHandle, clearColor, 0, nullptr);
	cmdList->OMSetRenderTargets(1,&RTVHeap.begin()->CPUHandle, FALSE, nullptr);
	cmdList->RSSetScissorRects(1, &scissorRect);

	for (size_t i = 0; i < Heaps.size(); i++)
		Heaps[i].Bind(cmdList, i);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0);
}

void ReflectionPass::InitResources(ID3D12Device5Ptr device)
{
	SRVHeap.Create(device, 3, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	auto srvDesc = Positions->GetSRVDesc();
	device->CreateShaderResourceView(Positions->Resource.Get(), &srvDesc, SRVHeap[0].CPUHandle);
	srvDesc = Normals->GetSRVDesc();
	device->CreateShaderResourceView(Normals->Resource.Get(), &srvDesc, SRVHeap[1].CPUHandle);
	srvDesc = PixelsColor->GetSRVDesc();
	device->CreateShaderResourceView(PixelsColor->Resource.Get(), &srvDesc, SRVHeap[2].CPUHandle);

	// Render target creation
	RTVHeap.Create(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

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
	rtvTextureInfo.clearValue = { .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, .Color = {0.0f, 0.0f, 0.0f, 0.0f} }; // Black Clear Color

	// Create the Render Target Texture
	RTVBuffer = MakeShared<DX::Texture>(&rtvTextureInfo);
	device->CreateRenderTargetView(RTVBuffer->Resource.Get(), nullptr, RTVHeap.begin()->CPUHandle);

	Heaps.push_back(SRVHeap);
	Heaps.push_back(Globals.SamplerHeap);
	Heaps.push_back(Globals.CBVHeap);
}

void ReflectionPass::InitRootSignature()
{
	std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges = {
	DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0)
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

void ReflectionPass::InitPipelineState()
{
	Shader<Vertex> vertexShader("FullScreenTriangle");
	Shader<Pixel> pixelShader("ReflectionPass");

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
