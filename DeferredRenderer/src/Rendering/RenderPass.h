#pragma once

#include "Core/Core.h"
#include "Rendering/RootSignature.h"
#include "Rendering/Resources.h"

class RenderPass
{
public:
	RenderPass(std::string&& name);
	virtual ~RenderPass() = default;
	
	void Init(ID3D12Device5Ptr device);
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList);

	inline ID3D12PipelineStatePtr GetPSO() { return PipelineState; }
	inline const std::string& GetName() const noexcept { return Name; }
	inline const std::vector<UniquePtr<PassInputBase>>& GetInputs() const { return Inputs; }
	inline const std::vector<UniquePtr<PassOutputBase>>& GetOutputs() const { return Outputs; }

	PassInputBase& GetInput(const std::string& name) const;
	PassOutputBase& GetOutput(const std::string& name) const;

	void SetInput(const std::string& inputName, const std::string& targetName);
	virtual void Validate();

protected:
	virtual void InitRootSignature() = 0;
	virtual void InitPipelineState() = 0;

	void Register(UniquePtr<PassInputBase> input);
	void Register(UniquePtr<PassOutputBase> output);

	template<typename T, typename... Args>
		requires std::is_constructible_v<T, Args...>
	void Register(Args&&... args)
	{
		auto ptr = MakeUnique<T>(std::forward<Args>(args)...);
		Register(std::move(ptr));
	}

protected:
	std::string Name;
	ID3D12Device5Ptr Device;

	ID3D12PipelineStatePtr PipelineState;
	RootSignature RootSignatureData;
	RenderPassResources Resources;

	SharedPtr<ID3D12ResourcePtr> RTVBuffer{};
	SharedPtr<ID3D12ResourcePtr> DSVBuffer{};

	std::vector<UniquePtr<PassInputBase>> Inputs;
	std::vector<UniquePtr<PassOutputBase>> Outputs;
};

class ForwardRenderPass final : public RenderPass
{
public:
	ForwardRenderPass(std::string&& name);
protected:
	void InitRootSignature() override;
	void InitPipelineState() override;
};

