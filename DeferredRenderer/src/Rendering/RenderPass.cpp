#include "RenderPass.h"
#include "Core/Exception.h"
#include "Scene.h"
#include "Shader.h"
#include "Utils.h"

#include <algorithm>

static auto isValidName = [](const std::string& name)
	{
		return !name.empty() &&
			std::ranges::all_of(name, [](char c) { return std::isalnum(c) || c == '_'; }) &&
			!std::isdigit(name.front());
	};

PassInputBase::PassInputBase(std::string&& name, D3D12_RESOURCE_STATES state)
	: Name(std::move(name)), State(state)
{
	if (Name.empty())
		throw std::invalid_argument("Invalid name - Empty pass name");

	if (!isValidName(Name))
		throw std::invalid_argument("Invalid name - unsupported characters");
}

PassOutputBase::PassOutputBase(std::string&& name, D3D12_RESOURCE_STATES state)
	:Name(std::move(name)), State(state)
{}


void PassInputBase::SetTarget(std::string passName, std::string outputName)
{
	if (passName.empty())
		throw std::invalid_argument("Empty pass name");
	if (passName != "$" && !isValidName(passName))
		throw std::invalid_argument("Invalid pass name - unsupported characters");
	if (!isValidName(outputName))
		throw std::invalid_argument("Invalid output name - unsupported characters");

	PassName = std::move(passName);
	OutputName = std::move(outputName);
}

RenderPass::RenderPass(std::string&& name)
	:Name(name), RTVBuffer(MakeShared<ID3D12ResourcePtr>()), DSVBuffer(MakeShared<ID3D12ResourcePtr>())
{}

void RenderPass::Init(ID3D12Device5Ptr device)
{
	Device = device;
	InitResources(device);
	InitRootSignature();
	InitPipelineState();
}

void RenderPass::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	//RootSignatureData
	cmdList->SetGraphicsRootSignature(RootSignatureData.RootSignaturePtr.GetInterfacePtr());

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)Globals.WindowDimensions.x, (FLOAT)Globals.WindowDimensions.y, 0.0f, 1.0f };
	cmdList->RSSetViewports(1, &viewport);

	// Set scissor rect
	D3D12_RECT scissorRect = { 0, 0, Globals.WindowDimensions.x, Globals.WindowDimensions.y };
	cmdList->RSSetScissorRects(1, &scissorRect);
	cmdList->OMSetRenderTargets(1, &Globals.RTVHandle, FALSE, &Globals.DSVHandle);

	Heaps.Bind(cmdList);
}

void RenderPass::InitResources(ID3D12Device5Ptr device) 
{}

PassInputBase& RenderPass::GetInput(const std::string& name) const
{
	for (auto& in : Inputs)
		if (in->GetName() == name)
			return *in;

	throw std::range_error("Element not found among possible Inputs");
}

PassOutputBase& RenderPass::GetOutput(const std::string& name) const
{
	for (auto& out : Outputs)
		if (out->GetName() == name)
			return *out;

	throw std::range_error("Element not found among possible Outputs");
}

void RenderPass::SetInput(const std::string& inputName, const std::string& targetName)
{
	auto& input = GetInput(inputName);
	auto targetNames = SplitString(targetName, ".");

	if (targetNames.size() != 2)
		throw std::invalid_argument("Invalid target name");

	input.SetTarget(std::move(targetNames[0]), std::move(targetNames[1]));
}

void RenderPass::Validate()
{
	for (auto& in : Inputs)
		in->Validate();
	for (auto& out : Outputs)
		out->Validate();
}

std::vector<UniquePtr<TransitionBase>> RenderPass::GetInverseTransitions(const TransitionsArray& transitions)
{
	std::vector<UniquePtr<TransitionBase>> extraTransitions;

	for (const auto& input : Inputs)
	{
		auto it = std::find_if(Outputs.begin(), Outputs.end(), [&input](UniquePtr<PassOutputBase>& output)
				  {
					  return input->GetRscPtr() == output->GetRscPtr();
				  }
		);

		if (it == Outputs.end()) continue;

		if (input->GetResourceState() == (*it)->GetResourceState()) continue;

		auto& ptr = input->BuildTransition(input->GetResourceState(), (*it)->GetResourceState());
		if (!ptr.has_value()) continue;

		extraTransitions.emplace_back( std::move(ptr.value()));
	}

	return extraTransitions;
}

void RenderPass::Register(UniquePtr<PassInputBase> input)
{
	auto it = std::find_if(Inputs.begin(), Inputs.end(),
						   [&input](const UniquePtr<PassInputBase>& in)
						   {
							   return in->GetName() == input->GetName();
						   });

	if (it != Inputs.end()) throw std::invalid_argument("Registered input in conflict with existing registered input");
	Inputs.emplace_back(std::move(input));
}

void RenderPass::Register(UniquePtr<PassOutputBase> output)
{
	auto it = std::find_if(Outputs.begin(), Outputs.end(),
						   [&output](const UniquePtr<PassOutputBase>& in)
						   {
							   return in->GetName() == output->GetName();
						   });

	if (it != Outputs.end()) throw std::invalid_argument("Registered output in conflict with existing registered input");
	Outputs.emplace_back(std::move(output));
}

ForwardRenderPass::ForwardRenderPass(std::string&& name)
	: RenderPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassInput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	Register<PassOutput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void ForwardRenderPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	Bind(cmdList);
	scene.Bind<ForwardRenderPass>(cmdList);
}

void ForwardRenderPass::InitResources(ID3D12Device5Ptr device)
{
	Heaps.PushBack(Globals.SRVHeap);
	Heaps.PushBack(Globals.UAVHeap);
	Heaps.PushBack(Globals.CBVHeap);
	Heaps.PushBack(Globals.SamplerHeap);
	Heaps.PushBack(Globals.LightsHeap);
}

void ForwardRenderPass::InitRootSignature()
{
	std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges;
	std::vector<D3D12_DESCRIPTOR_RANGE> cbvRanges;

	std::vector<D3D12_DESCRIPTOR_RANGE> uavRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0)
	};

	uint32_t userStart = NumGlobalSRVDescriptorRanges - NumUserDescriptorRanges;
	for (uint32_t i = 0; i < NumGlobalSRVDescriptorRanges; ++i)
	{
		UINT registerSpace = (i >= userStart) ? (i - userStart) + 100 : i;
		srvRanges.emplace_back(DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, registerSpace));
	}

	for (uint32_t i = 0; i < NumGlobalCBVDescriptorRanges; ++i)
		cbvRanges.emplace_back(DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, UINT_MAX, 0, i));

	std::vector<D3D12_DESCRIPTOR_RANGE> samplerRanges = {
	DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0)
	};

	std::vector<D3D12_DESCRIPTOR_RANGE> lightRanges = {
	DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 100)
	};

	RootSignatureData.AddDescriptorTable(srvRanges);
	RootSignatureData.AddDescriptorTable(uavRanges);
	RootSignatureData.AddDescriptorTable(cbvRanges);
	RootSignatureData.AddDescriptorTable(samplerRanges);
	RootSignatureData.AddDescriptorTable(lightRanges);
	RootSignatureData.AddDescriptor(D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL, 0, 200);

	RootSignatureData.Build(Device);
}

void ForwardRenderPass::InitPipelineState()
{
	Shader<Vertex> vertexShader("Shader");
	Shader<Pixel> pixelShader("Shader");

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
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;

	GRAPHICS_ASSERT(Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PipelineState)));
}

ClearPass::ClearPass(std::string&& name)
	:RenderPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassInput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	Register<PassOutput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void ClearPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	Bind(cmdList);
	scene.Bind<ClearPass>(cmdList);
}

void ClearPass::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	RenderPass::Bind(cmdList);

	const float clearColor[4] = { 0.4f, 0.6f, 0.2f, 1.0f };
	cmdList->ClearRenderTargetView(Globals.RTVHandle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(Globals.DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0.0f, 0, nullptr);
}

GUIPass::GUIPass(std::string&& name)
	:RenderPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("diffuse", Diffuse, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("specular", Specular, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12DescriptorHeapPtr>>("srvHeap", SRVHeap);

	Register<PassOutput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("diffuse", Diffuse, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("specular", Specular, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

GUIPass::~GUIPass() 
{
	Layer->OnDetach(); 
}

void GUIPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene & scene)
{
	Layer->Begin();
	Bind(cmdList);
	scene.Bind<GUIPass>(cmdList);

	GBuffersViewerWindow();

	Layer->End(cmdList);
}

void GUIPass::InitResources(ID3D12Device5Ptr device) 
{
	Layer = MakeUnique<ImGuiLayer>(*SRVHeap);
	Layer->OnAttach(Device);

	auto handle = Layer->DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < GPUHandlesGBuffers.size(); i++)
	{
		handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		GPUHandlesGBuffers[i] = handle;
	}
}

void GUIPass::GBuffersViewerWindow() const
{
	ImGui::Begin("G-Buffer Viewer");

	float aspectRatio = 9.0f / 16.0f;
	// Define the min and max sizes for the images
	ImVec2 minSize(360.0f, aspectRatio * 360.0f);
	ImVec2 maxSize(480.0f, aspectRatio * 480.0f);

	ImVec2 availableSize = ImGui::GetContentRegionAvail();

	float width = std::max(minSize.x, std::min(maxSize.x, availableSize.x));
	float height = width * 9.0f / 16.0f;

	// If height exceeds available height, adjust width and height
	if (height > availableSize.y)
	{
		height = std::max(minSize.y, std::min(maxSize.y, availableSize.y));
		width = height * 16.0f / 9.0f;
	}

	ImVec2 imageSize(width, height);

	// Dropdown items
	const char* items[] = {
		"Positions (View Space)",
		"Normals (View Space)",
		"Albedo",
		"Specular"
	};

	static int selectedItem = 0;

	// Dropdown
	if (ImGui::Combo("Select Texture", &selectedItem, items, IM_ARRAYSIZE(items)))
		std::cout << "Selected: " << items[selectedItem] << std::endl;

	ImGui::Image((ImTextureID)GPUHandlesGBuffers[selectedItem].ptr, imageSize);

	ImGui::End();
}

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
	for(auto handle : RTVHandles)
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
	Register<PassOutput<ID3D12ResourcePtr>>("ambientOcclusion", AmbientOcclusion, D3D12_RESOURCE_STATE_RENDER_TARGET);
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
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
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

AmbientOcclusionPass::AmbientOcclusionPass(std::string&& name)
	:RenderPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassOutput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void AmbientOcclusionPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene & scene)
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

	cmdList->CopyBufferRegion(gpuBuffer, 0, structuredBuffer, 0, sizeof(glm::float3)* ssaoKernelVals.size());

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
	Controls.Init(device, cbvHandle);

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
	Heaps.PushBack(CBVHeap);
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
	RootSignatureData.AddDescriptorTable(cbvRanges, D3D12_SHADER_VISIBILITY_PIXEL);

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
	Controls.CPUData.IsHorizontal = true;
	Controls.Tick();
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

	Controls.CPUData.IsHorizontal = false;
	Controls.Tick();
}