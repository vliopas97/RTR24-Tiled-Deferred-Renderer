#include "RenderPass.h"
#include "Core/Exception.h"
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

	Resources.Bind(cmdList);
}

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

void ClearPass::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	RenderPass::Bind(cmdList);

	const float clearColor[4] = { 0.4f, 0.6f, 0.2f, 1.0f };
	cmdList->ClearRenderTargetView(Globals.RTVHandle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(Globals.DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0.0f, 0, nullptr);

}
