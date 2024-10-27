#pragma once
#include "Core/Core.h"

namespace D3D
{
	ID3D12DescriptorHeapPtr CreateDescriptorHeap(ID3D12Device5Ptr device,
												 uint32_t count,
												 D3D12_DESCRIPTOR_HEAP_TYPE type,
												 bool shaderVisible);

	ID3D12CommandQueuePtr CreateCommandQueue(ID3D12Device5Ptr device);

	D3D12_CPU_DESCRIPTOR_HANDLE CreateRTV(ID3D12Device5Ptr device,
										  ID3D12ResourcePtr resource,
										  ID3D12DescriptorHeapPtr heap,
										  uint32_t& usedHeapEntries,
										  DXGI_FORMAT format);

	void ResourceBarrier(ID3D12GraphicsCommandList4Ptr cmdList,
						 ID3D12ResourcePtr resource,
						 D3D12_RESOURCE_STATES prevState,
						 D3D12_RESOURCE_STATES nextState);

	uint64_t SubmitCommandList(ID3D12GraphicsCommandList4Ptr cmdList,
							   ID3D12CommandQueuePtr cmdQueue,
							   ID3D12FencePtr fence,
							   uint64_t fenceValue);
}