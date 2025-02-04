#include "Resources.h"

GlobalResources Globals{};

void GlobalResManager::SetRTV(ID3D12ResourcePtr res, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	Globals.RTVBuffer = res;
	Globals.RTVHandle = handle;
}

void GlobalResManager::SetDSV(ID3D12ResourcePtr res, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	Globals.DSVBuffer = res;
	Globals.DSVHandle = handle;
}

void GlobalResManager::SetCmdAllocator(ID3D12CommandAllocatorPtr cmdAllocator)
{
	Globals.CmdAllocator = cmdAllocator;
}

void DescriptorHeapComposite::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	UINT rootParameterIndex = 0;
	for (const auto& heap : Heaps)
	{
		BindDescriptorHeap(cmdList, heap);
		cmdList->SetGraphicsRootDescriptorTable(rootParameterIndex++, heap->GetGPUDescriptorHandleForHeapStart());
	}
}

void DescriptorHeapComposite::BindCompute(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	UINT rootParameterIndex = 0;
	for (const auto& heap : Heaps)
	{
		BindDescriptorHeap(cmdList, heap);
		cmdList->SetComputeRootDescriptorTable(rootParameterIndex++, heap->GetGPUDescriptorHandleForHeapStart());
	}
}

void DescriptorHeapComposite::PushBack(ID3D12DescriptorHeapPtr heap)
{
	Heaps.push_back(heap);
}

void DescriptorHeapComposite::BindDescriptorHeap(ID3D12GraphicsCommandList4Ptr cmdList, ID3D12DescriptorHeapPtr heap) const
{
	std::array<ID3D12DescriptorHeap*, 1> heaps = { heap };
	cmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
}
