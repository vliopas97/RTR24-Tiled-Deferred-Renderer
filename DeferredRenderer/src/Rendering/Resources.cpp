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

void GlobalRenderPassResources::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	UINT rootParameterIndex = 0;
	for (const auto& heap : Heaps)
	{
		BindDescriptorHeap(cmdList, heap);
		cmdList->SetGraphicsRootDescriptorTable(rootParameterIndex++, heap->GetGPUDescriptorHandleForHeapStart());
	}

}

void GlobalRenderPassResources::Setup(ID3D12Device5Ptr device)
{
	Heaps.push_back(Globals.SRVHeap);
	Heaps.push_back(Globals.UAVHeap);
	Heaps.push_back(Globals.CBVHeap);
	Heaps.push_back(Globals.SamplerHeap);
	Heaps.push_back(Globals.LightsHeap);
}

void GlobalRenderPassResources::BindDescriptorHeap(ID3D12GraphicsCommandList4Ptr cmdList, ID3D12DescriptorHeapPtr heap) const
{
	std::array<ID3D12DescriptorHeap*, 1> heaps = { heap };
	cmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
}
