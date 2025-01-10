#pragma once

#include "Core/Core.h"
#include "Core/Layer.h"
#include "Rendering/RootSignature.h"
#include "Rendering/Resources.h"

#include <optional>

class PassOutputBase;
class Scene;

class PassInputBase
{
public:
	virtual ~PassInputBase() = default;

	inline const std::string& GetName() const noexcept { return Name; }
	inline const std::string& GetPassName() const noexcept { return PassName; }
	inline const std::string& GetOutputName() const noexcept { return OutputName; }
	inline const D3D12_RESOURCE_STATES GetResourceState() const { return State; }

	void SetTarget(std::string passName, std::string outputName);

	virtual void Bind(PassOutputBase& out) = 0;
	virtual void Validate() const = 0;
	virtual std::optional<UniquePtr<TransitionBase>> GetResourceTransition(D3D12_RESOURCE_STATES prev) const = 0;
	virtual const void* const GetRscPtr() const = 0;

protected:
	PassInputBase(std::string&& name, D3D12_RESOURCE_STATES state);

protected:
	D3D12_RESOURCE_STATES State;

private:
	std::string Name;
	std::string PassName;
	std::string OutputName;

};

template<ResourceType T>
class PassInput : public PassInputBase
{
public:
	PassInput(std::string&& name, SharedPtr<T>& resource, D3D12_RESOURCE_STATES state)
		:PassInputBase(std::move(name), state), Resource(resource)
	{}

	PassInput(std::string&& name, SharedPtr<T>& resource) requires (std::is_base_of_v<ID3D12DescriptorHeapPtr, T>)
		: PassInputBase(std::move(name), (D3D12_RESOURCE_STATES)0), Resource(resource)
	{}

	virtual void Validate() const override
	{
		if (!IsLinked) throw std::runtime_error("Unlinked input");
	}

	virtual void Bind(PassOutputBase& out) override
	{
		if (auto* outPtr = dynamic_cast<PassOutput<T>*>(&out))
		{
			Resource = outPtr->GetResource();
			IsLinked = true;
		}
		else
			throw std::bad_cast();
	}

	inline const D3D12_RESOURCE_STATES GetResourceState() const { return State; }

	virtual std::optional<UniquePtr<TransitionBase>> GetResourceTransition(D3D12_RESOURCE_STATES prev) const override
	{
		return GetResourceTransitionImpl(prev);
	}

	const void* const GetRscPtr() const override { return reinterpret_cast<void*>(Resource.get()); }

private:
	std::optional<UniquePtr<TransitionBase>> GetResourceTransitionImpl(D3D12_RESOURCE_STATES prev) const
	{
		return MakeUnique<Transition<T>>(Resource, prev, State);
	}

	std::optional<UniquePtr<TransitionBase>> GetResourceTransitionImpl(D3D12_RESOURCE_STATES prev) const 
		requires (std::is_base_of_v<ID3D12DescriptorHeapPtr, T>)
	{
		return std::nullopt;
	}

private:
	SharedPtr<T>& Resource;
	bool IsLinked = false;
};

class PassOutputBase
{
public:
	virtual ~PassOutputBase() = default;

	const std::string& GetName() const noexcept { return Name; }
	inline const D3D12_RESOURCE_STATES GetResourceState() const { return State; }
	virtual void Validate() const = 0;
	virtual const void* const GetRscPtr() const = 0;

protected:
	PassOutputBase(std::string&& name, D3D12_RESOURCE_STATES state);

protected:
	D3D12_RESOURCE_STATES State;

private:
	std::string Name;
};

template<ResourceType T>
class PassOutput : public PassOutputBase
{
public:
	PassOutput(std::string&& name, SharedPtr<T>& resource, D3D12_RESOURCE_STATES state)
		:PassOutputBase(std::move(name), state), Resource(resource)
	{}

	PassOutput(std::string&& name, SharedPtr<T>& resource) requires(std::is_base_of_v<ID3D12DescriptorHeapPtr, T>)
		: PassOutputBase(std::move(name), (D3D12_RESOURCE_STATES)0), Resource(resource)
	{}

	virtual void Validate() const override {}

	SharedPtr<T>& GetResource() const
	{
		if (IsLinked)
			throw std::runtime_error("Resource " + GetName() + " bound twice");

		IsLinked = true;
		return Resource;
	}

	const void* const GetRscPtr() const override { return reinterpret_cast<void*>(Resource.get()); }

private:
	SharedPtr<T>& Resource;
	mutable bool IsLinked = false;
};

class RenderPass
{
	using TransitionsArray = std::vector<UniquePtr<TransitionBase>>;

public:
	RenderPass(std::string&& name);
	virtual ~RenderPass() = default;
	
	void Init(ID3D12Device5Ptr device);
	// Submit Render Pass commands to cmdList - Does not include cmdList execution
	virtual void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) = 0;

	inline ID3D12PipelineStatePtr GetPSO() { return PipelineState; }
	inline const std::string& GetName() const noexcept { return Name; }
	inline const std::vector<UniquePtr<PassInputBase>>& GetInputs() const { return Inputs; }
	inline const std::vector<UniquePtr<PassOutputBase>>& GetOutputs() const { return Outputs; }

	PassInputBase& GetInput(const std::string& name) const;
	PassOutputBase& GetOutput(const std::string& name) const;

	void SetInput(const std::string& inputName, const std::string& targetName);
	virtual void Validate();

	// For resources that are not propagated further they must be transitioned back to where they were
	// to be in proper state for the next frame iteration through the graph
	std::vector<UniquePtr<TransitionBase>> GetInverseTransitions(const TransitionsArray& transitions);

protected:
	virtual void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const;
	virtual void InitResources(ID3D12Device5Ptr device);
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
	DescriptorHeapComposite Heaps;

	SharedPtr<ID3D12ResourcePtr> RTVBuffer{};
	SharedPtr<ID3D12ResourcePtr> DSVBuffer{};

	std::vector<UniquePtr<PassInputBase>> Inputs;
	std::vector<UniquePtr<PassOutputBase>> Outputs;
};

class ForwardRenderPass final : public RenderPass
{
public:
	ForwardRenderPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
};

class ClearPass final : public RenderPass
{
public:
	ClearPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const override;
	inline void InitResources(ID3D12Device5Ptr device) override {}
	void InitRootSignature() override {}
	void InitPipelineState() override {}
};

class GUIPass final : public RenderPass
{
public:
	GUIPass(std::string&& name, ImGuiLayer& layer);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	inline void InitResources(ID3D12Device5Ptr device) override {}
	void InitRootSignature() override {}
	void InitPipelineState() override {}
private:
	ImGuiLayer& Layer;
};

class GeometryPass final : public RenderPass
{
public:
	GeometryPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
private:
	SharedPtr<ID3D12ResourcePtr> Positions;
	SharedPtr<ID3D12ResourcePtr> Normals;
	SharedPtr<ID3D12ResourcePtr> Diffuse;
	SharedPtr<ID3D12ResourcePtr> Specular;

	// GBuffers
	SharedPtr<ID3D12DescriptorHeapPtr> RTVHeap{}; // to be used in this pass as RTVs
	SharedPtr<ID3D12DescriptorHeapPtr> SRVHeap{}; // to be used in lighting pass as SRVs
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 4> RTVHandles;
};

class LightingPass final : public RenderPass
{
public:
	LightingPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const override;
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
private:
	SharedPtr<ID3D12ResourcePtr> Positions;
	SharedPtr<ID3D12ResourcePtr> Normals;
	SharedPtr<ID3D12ResourcePtr> Diffuse;
	SharedPtr<ID3D12ResourcePtr> Specular;

	SharedPtr<ID3D12DescriptorHeapPtr> SRVHeap{};
};