#include "Blur.h"
#include "Rendering/Shader.h"


BlurPass::BlurPass(std::string&& name)
	:RenderPass(std::move(name))
{}

void BlurPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
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
	Controls.Bind(cmdList, 2);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0);
}

void BlurPass::InitResources(ID3D12Device5Ptr device)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	SRVHeap = D3D::CreateDescriptorHeap(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = SRVHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(*SRVToBlur, &srvDesc, srvHandle);

	CBVHeap = D3D::CreateDescriptorHeap(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = CBVHeap->GetCPUDescriptorHandleForHeapStart();
	Controls.Resource.Init(device, cbvHandle);

	// Create Render Target
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	clearValue.Color[0] = 1.0f;
	clearValue.Color[1] = 1.0f;
	clearValue.Color[2] = 1.0f;
	clearValue.Color[3] = 1.0f;

	auto resDesc = CD3DX12_RESOURCE_DESC(
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

	Heaps.PushBack(Globals.SamplerHeap);
	Heaps.PushBack(SRVHeap);
	//Heaps.PushBack(CBVHeap);
}

void BlurPass::InitRootSignature()
{
	std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges = {
	DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0)
	};

	std::vector<D3D12_DESCRIPTOR_RANGE> samplerRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0)
	};

	std::vector<D3D12_DESCRIPTOR_RANGE> cbvRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0)
	};

	RootSignatureData.AddDescriptorTable(samplerRanges, D3D12_SHADER_VISIBILITY_PIXEL);
	RootSignatureData.AddDescriptorTable(srvRanges, D3D12_SHADER_VISIBILITY_PIXEL);
	//RootSignatureData.AddDescriptorTable(cbvRanges, D3D12_SHADER_VISIBILITY_PIXEL);
	RootSignatureData.AddDescriptor(D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_PIXEL, 0);

	RootSignatureData.Build(Device);
}

void BlurPass::InitPipelineState()
{
	Shader<Vertex> vertexShader("FullScreenTriangle");
	Shader<Pixel> pixelShader("BlurPass");

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


HorizontalBlurPass::HorizontalBlurPass(std::string&& name)
	:BlurPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("processedResource", SRVToBlur, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("processedResource", SRVToBlur, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void HorizontalBlurPass::InitResources(ID3D12Device5Ptr device)
{
	BlurPass::InitResources(device);
	Controls.Resource.CPUData.IsHorizontal = true;
	Controls.Resource.Tick();
}

VerticalBlurPass::VerticalBlurPass(std::string&& name)
	:BlurPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("processedResource", SRVToBlur, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("processedResource", SRVToBlur, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void VerticalBlurPass::InitResources(ID3D12Device5Ptr device)
{
	BlurPass::InitResources(device);

	Controls.Resource.CPUData.IsHorizontal = false;
	Controls.Resource.Tick();
}