#include "Blur.h"
#include "Rendering/Shader.h"
#include "Scene.h"

ID3D12DescriptorHeapPtr CombinedBlurPassGlobal::FiltersHeap = nullptr;
ID3D12ResourcePtr CombinedBlurPassGlobal::Filters = nullptr;

// Gaussian filter generation code
// Source https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html
namespace GaussFilter
{
	double erf(double x)
	{
		const double a1 = 0.254829592;
		const double a2 = -0.284496736;
		const double a3 = 1.421413741;
		const double a4 = -1.453152027;
		const double a5 = 1.061405429;
		const double p = 0.3275911;

		int sign = (x < 0) ? -1 : 1;
		x = std::abs(x);

		double t = 1.0 / (1.0 + p * x);
		double y = 1.0 - (((((a5 * t + a4) * t + a3) * t + a2) * t + a1) * t * std::exp(-x * x));

		return sign * y;
	}

	std::vector<double> generateGaussianBlur(int radius, double sigma, bool linear, bool correction)
	{
		ASSERT(sigma > 0, "Sigma value invalid");

		std::vector<double> weights;
		double sumWeights = 0.0;

		for (int i = -radius; i <= radius; i++)
		{
			double w = 0;
			if (correction)
			{
				w = (erf((i + 0.5) / (sigma * std::sqrt(2))) - erf((i - 0.5) / (sigma * std::sqrt(2)))) / 2;
			}
			else
			{
				w = std::exp(-i * i / (sigma * sigma));
			}
			sumWeights += w;
			weights.push_back(w);
		}

		for (double& w : weights)
		{
			w /= sumWeights;
		}

		//std::vector<double> offsets;
		std::vector<double> newWeights;
		bool hasZeros = false;

		if (linear)
		{
			for (int i = -radius; i <= radius; i += 2)
			{
				if (i == radius)
				{
					//offsets.push_back(i);
					newWeights.push_back(weights[i + radius]);
				}
				else
				{
					double w0 = weights[i + radius];
					double w1 = weights[i + radius + 1];
					double w = w0 + w1;
					//if (w > 0)
					//{
					//	offsets.push_back(i + w1 / w);
					//}
					//else
					//{
					//	hasZeros = true;
					//	offsets.push_back(i);
					//}
					if (w <= 0) hasZeros = true;
					newWeights.push_back(w);
				}
			}
		}
		else
		{
			//for (int i = -radius; i <= radius; i++)
			//{
			//	offsets.push_back(i);
			//}
			for (double w : weights)
			{
				if (w == 0.0)
				{
					hasZeros = true;
				}
			}
			newWeights = weights;
		}

		ASSERT(!hasZeros, "Some weights are equal to zero; try using a smaller radius or a bigger sigma");

		return newWeights;
	}
}

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
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
	clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	clearValue.Color[0] = 1.0f;
	clearValue.Color[1] = 1.0f;
	clearValue.Color[2] = 1.0f;
	clearValue.Color[3] = 1.0f;

	auto resDesc = CD3DX12_RESOURCE_DESC(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		Globals.WindowDimensions.x, Globals.WindowDimensions.y, 1, 1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
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
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
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

CombinedBlurPass::CombinedBlurPass(std::string&& name, bool flag)
	:RenderPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("processedResource", SRVToBlur, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("processedResource", SRVToBlur, flag ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("renderTarget", BlurOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void CombinedBlurPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	cmdList->SetComputeRootSignature(RootSignatureData.RootSignaturePtr.GetInterfacePtr());
	Heaps.BindCompute(cmdList);
	scene.Bind<CombinedBlurPass>(cmdList);
}

void CombinedBlurPass::InitResources(ID3D12Device5Ptr device)
{
	// SRV setup for input image
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	SRVHeap = D3D::CreateDescriptorHeap(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = SRVHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(*SRVToBlur, &srvDesc, srvHandle);

	// UAV setup for output image
	auto resDesc = CD3DX12_RESOURCE_DESC(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		Globals.WindowDimensions.x, Globals.WindowDimensions.y, 1, 1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		1, 0,
		D3D12_TEXTURE_LAYOUT_UNKNOWN, 
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	ID3D12ResourcePtr uavResource;
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		NULL,
		IID_PPV_ARGS(&uavResource)));
	BlurOutput = MakeShared<ID3D12ResourcePtr>(uavResource);

	UAVHeap = D3D::CreateDescriptorHeap(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	auto uavHandle = UAVHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(*BlurOutput, &srvDesc, uavHandle);

	InitStaticResources(device);

	//----------------------------------------------
	Heaps.PushBack(SRVHeap);
	Heaps.PushBack(UAVHeap);
	Heaps.PushBack(FiltersHeap);

}

void CombinedBlurPass::InitRootSignature()
{
	std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0)
	};

	std::vector<D3D12_DESCRIPTOR_RANGE> uavRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0)
	};

	std::vector<D3D12_DESCRIPTOR_RANGE> filtersRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0)
	};

	RootSignatureData.AddDescriptorTable(srvRanges, D3D12_SHADER_VISIBILITY_ALL);
	RootSignatureData.AddDescriptorTable(uavRanges, D3D12_SHADER_VISIBILITY_ALL);
	RootSignatureData.AddDescriptorTable(filtersRanges, D3D12_SHADER_VISIBILITY_ALL);
	RootSignatureData.AddDescriptor(D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL, 0);

	RootSignatureData.Build(Device);
}

void CombinedBlurPass::InitPipelineState()
{
	Shader<Compute> computeShader("BlurPass");

    // Describe and create the compute pipeline state object (PSO)
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = RootSignatureData.RootSignaturePtr.GetInterfacePtr();
    psoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.GetBlob());

    // Create the compute pipeline state
    GRAPHICS_ASSERT(Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&PipelineState)));
}

void CombinedBlurPass::InitStaticResources(ID3D12Device5Ptr device)
{
	static bool initialized = false;
	if (initialized) return;

	initialized = true;
	//-----------------------------------------
	// Descriptor Table with acceptable filters
	auto& cmdList = Globals.CmdList;
	auto& cmdAllocator = Globals.CmdAllocator;

	// Descriptor Heap for SRV
	FiltersHeap = D3D::CreateDescriptorHeap(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	constexpr uint32_t NumKernels = MaxRadius;

	// Create the structured buffer
	CD3DX12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Alignment = 0;
	resDesc.Width = sizeof(Kernel) * NumKernels; // Holds all 15 Kernels
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// Create the buffer resource
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&Filters)));

	// Map and write data
	Kernel kernelData[NumKernels];
	for (int i = 0; i < NumKernels; i++)
	{
		float radius = i + 1;
		auto weights = GaussFilter::generateGaussianBlur(radius, radius / 2.0f, false, true);
		memset(kernelData[i].Coeffs, 0.0f, (2 * MaxRadius + 1) * sizeof(float));
		std::copy(weights.begin(), weights.end(), kernelData[i].Coeffs);
	}

	void* mappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 };
	Filters->Map(0, &readRange, &mappedData);
	memcpy(mappedData, kernelData, sizeof(Kernel) * NumKernels);
	Filters->Unmap(0, nullptr);

	// Create SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = NumKernels;  // One element per kernel
	srvDesc.Buffer.StructureByteStride = sizeof(Kernel);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	auto filtersHandle = FiltersHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(Filters, &srvDesc, filtersHandle);

	// Synchronization Point - Important because I copy data to GPU above
	Globals.FenceValue = D3D::SubmitCommandList(cmdList, Globals.CmdQueue, Globals.Fence, Globals.FenceValue);
	Globals.Fence->SetEventOnCompletion(Globals.FenceValue, Globals.FenceEvent);
	WaitForSingleObject(Globals.FenceEvent, INFINITE);
	cmdList->Reset(cmdAllocator, nullptr);
}

CombinedBlurPassGlobal::CombinedBlurPassGlobal(std::string&& name, bool flag, uint radius)
	:CombinedBlurPass(std::move(name), flag)
{
	SetRadius(radius);
}

void CombinedBlurPassGlobal::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene & scene)
{
	cmdList->SetComputeRootSignature(RootSignatureData.RootSignaturePtr.GetInterfacePtr());
	Heaps.BindCompute(cmdList);
	FilterRadius.BindCompute(cmdList, 3);

	UINT threadGroupX = (Globals.WindowDimensions.x + 127) / (GroupSize);  // Assuming 16x16 thread group size
	UINT threadGroupY = (Globals.WindowDimensions.y + 127) / (GroupSize);
	cmdList->Dispatch(threadGroupX, threadGroupY, 1);
}

void CombinedBlurPassGlobal::SetRadius(uint radius) 
{ 
	ASSERT(radius >=1 && radius <= 16, "Kernel radius value not within the supported range of [1, 16]")
	FilterRadius.Resource.CPUData = radius; 
}

void CombinedBlurPassGlobal::InitResources(ID3D12Device5Ptr device)
{
	CombinedBlurPass::InitResources(device);

	CBVHeap = D3D::CreateDescriptorHeap(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	auto cbvHandle = CBVHeap->GetCPUDescriptorHandleForHeapStart();
	FilterRadius.Resource.Init(device, cbvHandle);
	FilterRadius.Resource.Tick();
}
