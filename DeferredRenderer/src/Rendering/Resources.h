#pragma once
#include "Core/Core.h"
#include "Buffer.h"
#include "Shaders/HLSLCompat.h"

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
};

extern GlobalResources Globals;

namespace GlobalResManager
{

	void SetRTV(ID3D12ResourcePtr res, D3D12_CPU_DESCRIPTOR_HANDLE handle);
	void SetDSV(ID3D12ResourcePtr res, D3D12_CPU_DESCRIPTOR_HANDLE handle);

}

