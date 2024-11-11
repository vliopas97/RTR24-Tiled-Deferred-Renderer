#include "Scene.h"

Scene::Scene(ID3D12Device5Ptr device)
	:Device(device)
{
	Actors.emplace_back(Cube{ device });
	Actors[0].SetPosition({ 2, 0, 0 });
	Actors.emplace_back(Cube{ device });
	Actors[1].SetPosition({ 0, 0, 3 });
}

void Scene::Bind(ID3D12GraphicsCommandList4Ptr cmdList)
{
	for (const auto& actor : Actors)
		actor.Bind(cmdList);
}

void Scene::Tick()
{
	for (auto& actor : Actors)
		actor.Tick();
}

void Scene::CreateShaderResources(PipelineStateBindings& pipelineStateBindings)
{
	auto uavHandle = pipelineStateBindings.UAVHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvHandle = pipelineStateBindings.SRVHeap->GetCPUDescriptorHandleForHeapStart();
	auto cbvHandle = pipelineStateBindings.CBVHeap->GetCPUDescriptorHandleForHeapStart();

	cbvHandle.ptr += Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (auto& actor : Actors)
	{
		actor.SetUpGPUResources(Device, cbvHandle);
		cbvHandle.ptr += Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}
