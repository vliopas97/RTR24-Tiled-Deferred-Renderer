#pragma once
#include "Core/Core.h"
#include "Buffer.h"
#include "Shaders/HLSLCompat.h"

#include <type_traits>

struct GlobalResources
{
	ID3D12DescriptorHeapPtr SRVHeap{};
	ID3D12DescriptorHeapPtr UAVHeap{};
	ID3D12DescriptorHeapPtr CBVHeap{};
	ID3D12DescriptorHeapPtr SamplerHeap{};
	ID3D12DescriptorHeapPtr LightsHeap{};
	ConstantBuffer<PipelineConstants> CBGlobalConstants{};

	ID3D12ResourcePtr RTVBuffer{};
	ID3D12ResourcePtr DSVBuffer{};

	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle{};

	ID3D12CommandQueuePtr CmdQueue{};
	ID3D12CommandAllocatorPtr CmdAllocator{};

	glm::uvec2 WindowDimensions{1920, 1080};

	ID3D12FencePtr Fence{};
	HANDLE FenceEvent{};
	uint64_t FenceValue = 0;
};

extern GlobalResources Globals;

namespace GlobalResManager
{

	void SetRTV(ID3D12ResourcePtr res, D3D12_CPU_DESCRIPTOR_HANDLE handle);
	void SetDSV(ID3D12ResourcePtr res, D3D12_CPU_DESCRIPTOR_HANDLE handle);
	void SetCmdAllocator(ID3D12CommandAllocatorPtr cmdAllocator);
}

template <typename T>
inline constexpr bool is_com_ptr_v = false;

template <typename T>
inline constexpr bool is_com_ptr_v<_com_ptr_t<T>> = true;

template <typename T>
concept ResourceType = is_com_ptr_v<T>;

class PassOutputBase;

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
	virtual D3D12_RESOURCE_BARRIER GetResourceTransition(D3D12_RESOURCE_STATES prev) const = 0;

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

	virtual void Validate() const override
	{
		if (!IsLinked) throw std::runtime_error("Unlinked input");
	}

	virtual void Bind(PassOutputBase& out) override;
	inline const D3D12_RESOURCE_STATES GetResourceState() const { return State; }

	virtual D3D12_RESOURCE_BARRIER GetResourceTransition(D3D12_RESOURCE_STATES prev) const override
	{
		return CD3DX12_RESOURCE_BARRIER::Transition(*Resource, prev, State);
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

	virtual void Validate() const override {}

	SharedPtr<T> GetResource() const
	{
		if (IsLinked)
			throw std::runtime_error("Resource " + GetName() + " bound twice");

		IsLinked = true;
		return Resource;
	}

private:
	SharedPtr<T>& Resource;
	mutable bool IsLinked = false;
};

template<ResourceType T>
void PassInput<T>::Bind(PassOutputBase& out)
{
	if (auto* outPtr = dynamic_cast<PassOutput<T>*>(&out))
	{
		*Resource = *(outPtr->GetResource());
		IsLinked = true;
	}
	else
		throw std::bad_cast();
}