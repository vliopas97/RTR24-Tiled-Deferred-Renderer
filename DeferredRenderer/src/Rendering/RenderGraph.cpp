#include "RenderGraph.h"
#include "Core/Exception.h"
#include "Rendering/Utils.h"
#include "Scene.h"

#include <optional>

auto createResourceTransition = [](const PassInputBase& input, const PassOutputBase& output)
-> std::optional<UniquePtr<TransitionBase>>
	{
		if (input.GetResourceState() != output.GetResourceState())
			return input.GetResourceTransition(output.GetResourceState());

		return std::nullopt;
	};

RenderGraph::RenderGraph(ID3D12Device5Ptr device)
	:Device(device)
{
	RTVBuffer = MakeShared<ID3D12ResourcePtr>(Globals.RTVBuffer);
	DSVBuffer = MakeShared<ID3D12ResourcePtr>(Globals.DSVBuffer);
	Globals.CBGlobalConstants.CPUData.SSAOEnabled = true;
	Globals.CBGlobalConstants.CPUData.SSREnabled = false;
	Globals.CBGlobalConstants.CPUData.RadiusSSAO = 0.5f;
	Globals.CBGlobalConstants.CPUData.IntensitySSAO = 2.0f;

	GraphInputs.emplace_back(MakeUnique<PassOutput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_PRESENT));
	GraphInputs.emplace_back(MakeUnique<PassOutput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	GraphOutputs.emplace_back(MakeUnique<PassInput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_PRESENT));

	// Clear Pass
	{
		auto pass = MakeUnique<ClearPass>("clear");
		//pass->Init(device);
		pass->SetInput("renderTarget", "$.renderTarget");
		pass->SetInput("depthBuffer", "$.depthBuffer");
		Add(pass);
	}
	// Geometry Pass
	{
		auto pass = MakeUnique<GeometryPass>("geometryPass");
		pass->SetInput("depthBuffer", "clear.depthBuffer");
		Add(pass);
	}
	// Ambient Occlusion Pass
	{
		auto pass = MakeUnique<AmbientOcclusionPass>("ambientOcclusion");
		pass->SetInput("normals", "geometryPass.normals");
		pass->SetInput("positions", "geometryPass.positions");
		Add(pass);
	}
	//// Horizontal Blur Pass
	//{
	//	auto pass = MakeUnique<HorizontalBlurPass>("horizontalBlur");
	//	pass->SetInput("processedResource", "ambientOcclusion.renderTarget");
	//	Add(pass);
	//}
	//// Vertical Blur Pass
	//{
	//	auto pass = MakeUnique<VerticalBlurPass>("verticalBlur");
	//	pass->SetInput("processedResource", "horizontalBlur.renderTarget");
	//	Add(pass);
	//}
	// Combined Blur Pass
	{
		auto pass = MakeUnique<CombinedBlurPassGlobal>("blur");
		pass->SetInput("processedResource", "ambientOcclusion.renderTarget");
		Add(pass);
	}
	// Lighting Pass
	{
		auto pass = MakeUnique<LightingPass>("lightingPass");
		//pass->SetInput("renderTarget", "clear.renderTarget");
		pass->SetInput("positions", "ambientOcclusion.positions");
		pass->SetInput("normals", "ambientOcclusion.normals");
		pass->SetInput("diffuse", "geometryPass.diffuse");
		pass->SetInput("specular", "geometryPass.specular");
		pass->SetInput("ambientOcclusion", "blur.renderTarget");
		pass->SetInput("srvHeap", "geometryPass.srvHeap");
		Add(pass);
	}
	// Reflections Pass
	{
		auto pass = MakeUnique<ReflectionPass>("reflectionPass");
		//pass->SetInput("renderTarget", "clear.renderTarget");
		pass->SetInput("positions", "lightingPass.positions");
		pass->SetInput("normals", "lightingPass.normals");
		pass->SetInput("pixelsColor", "lightingPass.renderTarget");
		Add(pass);
	}
	// Blur reflections pass
	{
		auto pass = MakeUnique<CombinedBlurPassGlobal>("reflectionBlur", true, 7);
		pass->SetInput("processedResource", "reflectionPass.renderTarget");
		Add(pass);
	}
	// Blend Pass
	{
		auto pass = MakeUnique<BlendPass>("blend");
		pass->SetInput("renderTarget", "clear.renderTarget");
		pass->SetInput("pixelsColor", "reflectionPass.pixelsColor");
		pass->SetInput("reflectionColor", "reflectionBlur.renderTarget");
		Add(pass);
	}
	// GUI layer
	{
		auto pass = MakeUnique<GUIPass>("GUI");
		pass->SetInput("positions", "reflectionPass.positions");
		pass->SetInput("normals", "reflectionPass.normals");
		pass->SetInput("diffuse", "lightingPass.diffuse");
		pass->SetInput("specular", "lightingPass.specular");
		pass->SetInput("ambientOcclusion", "lightingPass.ambientOcclusion");
		Add(pass);
	}

	SetInputTarget("renderTarget", "blend.renderTarget");
	Validate();
	TransitionUnpropagatedResources();
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
		cmdList->Reset(Globals.CmdAllocator, pass->GetPSO());

		for (auto& t : Transitions[i]) t->Apply(cmdList);// Barriers
		
		pass->Submit(cmdList, scene);
		
		if (++i == Passes.size())
			for (auto& t : Transitions[i]) t->Apply(cmdList); // Final layer of transitions (Transitions.size() = Passes.size() + 1)

		// Submit Render Pass for execution and synchronize before calling Reset() for next iteration
		Globals.FenceValue = D3D::SubmitCommandList(cmdList, Globals.CmdQueue, Globals.Fence, Globals.FenceValue);
		Globals.Fence->SetEventOnCompletion(Globals.FenceValue, Globals.FenceEvent);
		WaitForSingleObject(Globals.FenceEvent, INFINITE);
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

void RenderGraph::TransitionUnpropagatedResources()
{
	UINT i = 0;
	for (const auto& pass : Passes)
	{
		auto t = pass->GetInverseTransitions(Transitions[i]);
		Transitions[++i].insert(Transitions[i].end(), std::make_move_iterator(t.begin()), std::make_move_iterator(t.end()));
	}
}

void RenderGraph::AddGlobalInputs(UniquePtr<PassInputBase> in)
{
	GraphOutputs.emplace_back(std::move(in));
}

void RenderGraph::AddGlobalOutputs(UniquePtr<PassOutputBase> out)
{
	GraphInputs.emplace_back(std::move(out));
}
