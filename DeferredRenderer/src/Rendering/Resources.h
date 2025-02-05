#pragma once
#include "Core/Core.h"
#include "Buffer.h"
#include "Shaders/HLSLCompat.h"

#include <type_traits>

class RenderPass;

// Concepts

template <typename T>
inline constexpr bool is_com_ptr_v = false;

template <typename T>
inline constexpr bool is_com_ptr_v<_com_ptr_t<T>> = true;

template <typename T>
concept ResourceType = is_com_ptr_v<T>;

template <typename T>
concept HasGetGPUVirtualAddress =
	requires(T t) { { t->GetGPUVirtualAddress() } -> std::convertible_to<uint64_t>; } ||
	requires(T t) { { t.GetGPUVirtualAddress() } -> std::convertible_to<uint64_t>; };

template < template <typename...> class base, typename derived>
struct is_base_of_template_impl
{
	template<typename... Ts>
	static constexpr std::true_type  test(const base<Ts...>*);
	static constexpr std::false_type test(...);
	using type = decltype(test(std::declval<derived*>()));
};

template < template <typename...> class base, typename derived>
using is_base_of_template = typename is_base_of_template_impl<base, derived>::type;

// Concepts End

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
	ID3D12GraphicsCommandList4Ptr CmdList{};

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

struct TransitionBase
{
	virtual ~TransitionBase() = default;
	virtual void Apply(ID3D12GraphicsCommandList4Ptr cmdList) = 0;

	virtual const void* const GetResource() const = 0;
	virtual D3D12_RESOURCE_STATES GetPrevious() const = 0;
	virtual D3D12_RESOURCE_STATES GetNext() const = 0;
};

template<ResourceType T>
struct Transition : public TransitionBase
{
	Transition(SharedPtr<T>& resource, D3D12_RESOURCE_STATES previous, D3D12_RESOURCE_STATES next)
		:Resource(resource), Previous(previous), Next(next)
	{}

	void Apply(ID3D12GraphicsCommandList4Ptr cmdList) override
	{
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			*Resource,
			Previous,
			Next
		));
	}

	const void* const GetResource() const { return reinterpret_cast<void*>(Resource.get()); }
	D3D12_RESOURCE_STATES GetPrevious() const { return Previous; }
	D3D12_RESOURCE_STATES GetNext() const { return Next; }

	SharedPtr<T>& Resource;
	D3D12_RESOURCE_STATES Previous{};
	D3D12_RESOURCE_STATES Next{};
};

// To be used by actors to bind non-global variables to PSOs of the appropriate Render Pass
struct ResourceGPUBase
{
	virtual ~ResourceGPUBase() = default;
	virtual void Bind(ID3D12GraphicsCommandList4Ptr cmdList, UINT slot) const = 0;
	virtual void BindCompute(ID3D12GraphicsCommandList4Ptr cmdList, UINT slot) const = 0;
};

template<typename T>
requires HasGetGPUVirtualAddress<T>
struct ResourceGPU : public ResourceGPUBase
{
	ResourceGPU() = default;
	virtual ~ResourceGPU() = default;

	uint64_t GetGPUVirtualAddress() const
	{
		if constexpr (std::is_pointer_v<T> || is_com_ptr_v<T>)
			return Resource->GetGPUVirtualAddress();
		else
			return Resource.GetGPUVirtualAddress();
	}

	T Resource;
};

template<typename T>
requires is_base_of_template<ConstantBuffer, T>::value
struct ResourceGPU_ConstantBufferView : public ResourceGPU<T>
{
	using ResourceGPU::ResourceGPU;
	virtual void Bind(ID3D12GraphicsCommandList4Ptr cmdList, UINT slot) const override
	{
		cmdList->SetGraphicsRootConstantBufferView(slot, GetGPUVirtualAddress());
	}

	virtual void BindCompute(ID3D12GraphicsCommandList4Ptr cmdList, UINT slot) const override
	{
		cmdList->SetComputeRootConstantBufferView(slot, GetGPUVirtualAddress());
	}
};

template<typename T>
using ResourceGPU_CBV = ResourceGPU_ConstantBufferView<ConstantBuffer<T>>;

struct DescriptorHeapComposite
{
public:
	virtual ~DescriptorHeapComposite() = default;
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const;
	void BindCompute(ID3D12GraphicsCommandList4Ptr cmdList) const;
	void PushBack(ID3D12DescriptorHeapPtr heap);

private:
	void BindDescriptorHeap(ID3D12GraphicsCommandList4Ptr cmdList, ID3D12DescriptorHeapPtr heap) const;

private:
	std::vector<ID3D12DescriptorHeapPtr> Heaps;
};