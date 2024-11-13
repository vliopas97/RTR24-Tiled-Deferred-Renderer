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

void Scene::CreateShaderResources(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue, PipelineStateBindings& pipelineStateBindings)
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

	InitializeTextures(device, cmdQueue, pipelineStateBindings);

	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // Linear filtering
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // Wrap texture coordinates
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

	// Get the handle to the sampler heap
	CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(pipelineStateBindings.SamplerHeap->GetCPUDescriptorHandleForHeapStart());
}

void Scene::InitializeTextures(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue, PipelineStateBindings& pipelineStateBindings)
{
	// Create Texture
	auto srvHandle = pipelineStateBindings.SRVHeap->GetCPUDescriptorHandleForHeapStart();
	tex = MakeUnique<Texture>(device, cmdQueue, srvHandle, "Img\\sample.jpg");

	// Set tex IDs for each object
	for (auto& actor : Actors)
		actor.Model.CPUData.TextureID = 0;
}
