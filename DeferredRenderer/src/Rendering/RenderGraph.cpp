#include "RenderGraph.h"
#include "Core/Exception.h"
#include "Rendering/Utils.h"
#include "Scene.h"

#include <optional>

auto createResourceTransition = [](const PassInputBase& input, const PassOutputBase& output)
-> std::optional<UniquePtr<TransitionBase>>
	{
		if (input.GetResourceState() != output.GetResourceState())
		{
			return input.GetResourceTransition(output.GetResourceState());
		}
		return std::nullopt;
	};

void RenderGraph::Init(ID3D12Device5Ptr device)
{
	RTVBuffer = MakeShared<ID3D12ResourcePtr>(Globals.RTVBuffer);
	DSVBuffer = MakeShared<ID3D12ResourcePtr>(Globals.DSVBuffer);

	GraphInputs.emplace_back(MakeUnique<PassOutput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_PRESENT));
	GraphInputs.emplace_back(MakeUnique<PassOutput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	GraphOutputs.emplace_back(MakeUnique<PassInput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_PRESENT));

	auto forwardPass = MakeUnique<ForwardRenderPass>("forwardPass");
	forwardPass->Init(device);
	forwardPass->SetInput("renderTarget", "$.renderTarget");
	forwardPass->SetInput("depthBuffer", "$.depthBuffer");
	Add(forwardPass);

	SetInputTarget("renderTarget", "forwardPass.renderTarget");
	Validate();
}

void RenderGraph::Tick()
{
	*RTVBuffer = Globals.RTVBuffer;
	*DSVBuffer = Globals.DSVBuffer;
}

void RenderGraph::SetInputTarget(const std::string & name, const std::string & target)
{
	auto it = std::find_if(GraphOutputs.begin(), GraphOutputs.end(),
						   [&name](const UniquePtr<PassInputBase>& ptr)
						   {
							   return ptr->GetName() == name;
						   });

	if (it == GraphOutputs.end()) throw std::invalid_argument("Name invalid - No such input exists");

	auto split = SplitString(target, ".");
	if (split.size() != 2) throw std::invalid_argument("Name invalid - Incorrect name form for global buffers");

	(*it)->SetTarget(split[0], split[1]);
}

void RenderGraph::Execute(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	ASSERT(IsValidated, "Validation hasn't happened");

	auto i = 0;
	for (const auto& pass : Passes)
	{
		Globals.CmdAllocator->Reset();
		cmdList->Reset(Globals.CmdAllocator, pass->GetPSO());// will only work for one pass FIX IT
		for (auto& t : Transitions[i]) t->Apply(cmdList);// Barriers
		pass->Bind(cmdList);
		scene.Bind(cmdList);

		i++;
		if (i == Passes.size())
			for (auto& t : Transitions[i]) t->Apply(cmdList); // Final layer of transitions (Transitions.size() = Passes.size() + 1)

		Globals.FenceValue = D3D::SubmitCommandList(cmdList, Globals.CmdQueue, Globals.Fence, Globals.FenceValue);
	}
}

void RenderGraph::LinkInputs(RenderPass& renderPass)
{
	std::vector<UniquePtr<TransitionBase>> passTransitions;

	for (auto& in : renderPass.GetInputs())
	{
		const std::string& name = in->GetPassName();

		if (name == "$")
		{
			bool bound = false;
			for (auto& out : GraphInputs)
			{
				if (out->GetName() == in->GetOutputName())
				{
					in->Bind(*out);
					bound = true;

					// TRANSITION
					if (auto transition = createResourceTransition(*in, *out))
						passTransitions.emplace_back(std::move(*transition));

					break;
				}
			}

			if (!bound)
				throw std::runtime_error("Output " + in->GetOutputName() + " not found in globals");
		}
		else
		{
			auto it = std::find_if(Passes.begin(), Passes.end(),
								   [&name](const auto& ptr)
								   {
									   return name == ptr->GetName();
								   });

			if (it == Passes.end()) throw std::runtime_error("Pass input not found among existing Passes");

			auto& out = (*it)->GetOutput(in->GetOutputName());
			in->Bind(out);

			// TRANSITION
			if (auto transition = createResourceTransition(*in, out))
				passTransitions.emplace_back(std::move(*transition));
		}
	}

	Transitions.emplace_back(std::move(passTransitions));
}

void RenderGraph::LinkGlobalInputs()
{
	std::vector<UniquePtr<TransitionBase>> passTransitions;

	for (auto& in : GraphOutputs)
	{
		const std::string& name = in->GetPassName();
		auto it = std::find_if(Passes.begin(), Passes.end(),
							   [&name](const auto& ptr)
							   {
								   return name == ptr->GetName();
							   });

		if (it == Passes.end()) throw std::runtime_error("Pass input not found among existing Passes");

		auto& out = (*it)->GetOutput(in->GetOutputName());
		in->Bind(out);

		// TRANSITIONS
		if (auto transition = createResourceTransition(*in, out))
			passTransitions.emplace_back(std::move(*transition));
	}

	Transitions.emplace_back(std::move(passTransitions));
}

void RenderGraph::Validate()
{
	ASSERT(!IsValidated, "Validation has already occured. Cannot validate again.");
	for (const auto& pass : Passes)
		pass->Validate();

	LinkGlobalInputs();
	IsValidated = true;
}

void RenderGraph::AddGlobalInputs(UniquePtr<PassInputBase> in)
{
	GraphOutputs.emplace_back(std::move(in));
}

void RenderGraph::AddGlobalOutputs(UniquePtr<PassOutputBase> out)
{
	GraphInputs.emplace_back(std::move(out));
}
