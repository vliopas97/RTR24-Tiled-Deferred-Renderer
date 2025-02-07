#pragma once
#include "Core/Core.h"
#include "Resources.h"
#include "RenderPasses/RenderPass.h"
#include "RenderPasses/Blur.h"
#include "RenderPasses/AmbientOcclusion.h"
#include "RenderPasses/Geometry.h"
#include "RenderPasses/Lighting.h"
#include "RenderPasses/GUI.h"
#include "RenderPasses/Clear.h"
#include "RenderPasses/Forward.h"
#include "RenderPasses/ReflectionPass.h"
#include "RenderPasses/Blend.h"

class RenderGraph
{
public:
	RenderGraph(ID3D12Device5Ptr device);
	~RenderGraph() = default;

	void Execute(ID3D12GraphicsCommandList4Ptr cmdList, const class Scene& scene);

	template <typename PassType>
	requires std::is_base_of_v<RenderPass, PassType>
	void Add(UniquePtr<PassType>& renderPass)
	{
		ASSERT(!IsValidated, "Cannot add more renderPasses after validation has occured");

		auto it = std::find_if(Passes.begin(), Passes.end(),
							   [&renderPass](const auto& ptr)
							   {
								   return renderPass->GetName() == ptr->GetName();
							   });

		if (it != Passes.end()) throw std::invalid_argument("Pass name already exists");

		LinkInputs(*renderPass);
		renderPass->Init(Device);
		Passes.emplace_back(std::move(renderPass));
	}

	void Tick();
	std::vector<UniquePtr<RenderPass>> Passes;
private:
	void SetInputTarget(const std::string& name, const std::string& target);
	void LinkInputs(RenderPass& renderPass);
	void LinkGlobalInputs();
	void Validate();
	void TransitionUnpropagatedResources();

	void AddGlobalInputs(UniquePtr<PassInputBase> in);
	void AddGlobalOutputs(UniquePtr<PassOutputBase> out);

private:
	using Transitions2DArray = std::vector<std::vector<UniquePtr<TransitionBase>>>;

	ID3D12Device5Ptr Device;
	std::vector<UniquePtr<PassInputBase>> GraphOutputs;
	std::vector<UniquePtr<PassOutputBase>> GraphInputs;
	Transitions2DArray Transitions;

	SharedPtr<ID3D12ResourcePtr> RTVBuffer{};
	SharedPtr<ID3D12ResourcePtr> DSVBuffer{};

	bool IsValidated = false;
};