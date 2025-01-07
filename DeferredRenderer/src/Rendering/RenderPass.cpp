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

GUIPass::GUIPass(std::string&& name, ImGuiLayer& layer)
	:RenderPass(std::move(name)), Layer(layer)
{}

void GUIPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene & scene)
{
	Layer.Begin();
	Bind(cmdList);
	scene.Bind<GUIPass>(cmdList);
	Layer.End(cmdList);
}

GeometryPass::GeometryPass(std::string&& name) : 
	RenderPass(std::move(name)) 
{
	Positions = MakeShared<ID3D12ResourcePtr>();
	Normals = MakeShared<ID3D12ResourcePtr>();
	Diffuse = MakeShared<ID3D12ResourcePtr>();
	
	Register<PassInput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	Register<PassOutput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("diffuse", Diffuse, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("specular", Specular, D3D12_RESOURCE_STATE_RENDER_TARGET);
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
		nullptr,
		IID_PPV_ARGS(&diffuse)));
	Diffuse = MakeShared<ID3D12ResourcePtr>(diffuse);

	ID3D12ResourcePtr specular;
	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		nullptr,
		IID_PPV_ARGS(&specular)));
	Specular = MakeShared<ID3D12ResourcePtr>(specular);

	auto resHeap = D3D::CreateDescriptorHeap(device, 4, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
	GBuffers = MakeShared<ID3D12DescriptorHeapPtr>(resHeap);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GBuffers->GetInterfacePtr()->GetCPUDescriptorHandleForHeapStart();
	UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

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
