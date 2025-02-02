#include "RenderPass.h"
#include "Core/Exception.h"
#include "Scene.h"
#include "Rendering/Shader.h"
#include "Rendering/Utils.h"

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