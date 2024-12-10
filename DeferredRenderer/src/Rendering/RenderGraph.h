#pragma once
#include "Core/Core.h"
#include "Resources.h"
#include "RenderPass.h"

class RenderGraph
{
public:
	RenderGraph() = default;
	void Init(ID3D12Device5Ptr device);
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
		Passes.emplace_back(std::move(renderPass));
	}

	void Tick();
	std::vector<UniquePtr<RenderPass>> Passes;
private:
	void SetInputTarget(const std::string& name, const std::string& target);
	void LinkInputs(RenderPass& renderPass);
	void LinkGlobalInputs();
	void Validate();

	void AddGlobalInputs(UniquePtr<PassInputBase> in);
	void AddGlobalOutputs(UniquePtr<PassOutputBase> out);

private:
	using Transitions2DArray = std::vector<std::vector<UniquePtr<TransitionBase>>>;
	std::vector<UniquePtr<PassInputBase>> GraphOutputs;
	std::vector<UniquePtr<PassOutputBase>> GraphInputs;
	Transitions2DArray Transitions;

	SharedPtr<ID3D12ResourcePtr> RTVBuffer{};
	SharedPtr<ID3D12ResourcePtr> DSVBuffer{};

	bool IsValidated = false;
};