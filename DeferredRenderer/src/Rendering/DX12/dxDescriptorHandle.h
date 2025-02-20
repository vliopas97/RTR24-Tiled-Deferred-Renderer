#pragma once
#include "Core/Core.h"

namespace DX
{
	struct DescriptorHeap;

	struct DescriptorHandle
	{
		const DX::DescriptorHeap* Parent = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle{};
		D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle{};
	};
}