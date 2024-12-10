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

struct TransitionBase
{
	virtual ~TransitionBase() = default;
	virtual void Apply(ID3D12GraphicsCommandList4Ptr cmdList) = 0;
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

	SharedPtr<T>& Resource;
	D3D12_RESOURCE_STATES Previous{};
	D3D12_RESOURCE_STATES Next{};
};

struct GlobalRenderPassResources
{
public:
	virtual ~GlobalRenderPassResources() = default;
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList);
	virtual void Setup(ID3D12Device5Ptr device);

private:
	void BindDescriptorHeap(ID3D12GraphicsCommandList4Ptr cmdList, ID3D12DescriptorHeapPtr heap);

public:
	std::vector<ID3D12DescriptorHeapPtr> Heaps;
};