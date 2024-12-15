#include "Scene.h"
#include "Camera.h"
#include "Rendering/Actors/Model.h"
#include "Rendering/Resources.h"

#include <filesystem>
#include <unordered_map>
#include <iostream>

Scene::Scene(ID3D12Device5Ptr device, const Camera& camera)
	:Device(device), DebugMode(true)
{
	FilesLocation = std::filesystem::current_path().parent_path().string()
		+ std::string(DebugMode ? "\\Content\\Model\\Nanosuit\\" : "\\Content\\Model\\Sponza\\");

	InitializeTextureIndices();// Texture indices before Loading the Models - IMPORTANT
	LoadModels(camera);
	Lights.emplace_back(DirectionalLight{});
}

void Scene::Tick()
{
	for (auto& actor : Actors)
		actor.Tick();

	for (auto& light : Lights)
	{
		light.Tick();
		//light.GUI();
	}
}

void Scene::CreateShaderResources(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue)
{
	auto uavHandle = Globals.UAVHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvHandle = Globals.SRVHeap->GetCPUDescriptorHandleForHeapStart();
	auto cbvHandle = Globals.CBVHeap->GetCPUDescriptorHandleForHeapStart();
	auto lightsHandle = Globals.LightsHeap->GetCPUDescriptorHandleForHeapStart();

	cbvHandle.ptr += Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); // 1st element of desc table occupied
	for (auto& actor : Actors)
	{
		actor.SetUpGPUResources(Device, cbvHandle);
		cbvHandle.ptr += Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	for (auto& light : Lights)
	{
		light.SetUpGPUResources(device, lightsHandle);
		lightsHandle.ptr += Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	InitializeTextures(device, cmdQueue);

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
	CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(Globals.SamplerHeap->GetCPUDescriptorHandleForHeapStart());
	device->CreateSampler(&samplerDesc, samplerHandle);

}

void Scene::InitializeTextures(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue)
{
	// Create Texture
	auto srvHandle = Globals.SRVHeap->GetCPUDescriptorHandleForHeapStart();

	for (const auto& pair : TextureIndexMap)
	{
		TextureResources.emplace_back(MakeUnique<Texture>(device, cmdQueue, srvHandle, FilesLocation + pair.first));
		srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}

void Scene::LoadModels(const Camera& camera)
{
	Assimp::Importer importer;
	auto objFilename = FilesLocation + (DebugMode ? "nanosuit.obj" : "sponza.obj");
	float scalingFactor = DebugMode ? 0.15f : 0.05f;

	std::filesystem::path solutionPath = std::filesystem::current_path().parent_path();
	auto scene = importer.ReadFile(objFilename,
								   aiProcess_Triangulate 
								   | aiProcess_ConvertToLeftHanded 
								   | aiProcess_FixInfacingNormals
								   | aiProcess_JoinIdenticalVertices
								   | aiProcess_GenNormals |
								   aiProcess_CalcTangentSpace);

	ASSERT(scene, "Scene importer returns NULL. Scene not found in location " + objFilename + " .Check directory or file name");
	
	ASSERT(scene->HasMaterials(), "Expected that loaded scene has materials");
	auto* materials = scene->mMaterials;

	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		const auto mesh = scene->mMeshes[i];
		Actors.emplace_back(Model{ Device, camera, *mesh, materials, TextureIndexMap });
		auto& actor = Actors.back();
		actor.SetScale({ scalingFactor, scalingFactor, scalingFactor });
	}
}

void Scene::InitializeTextureIndices()
{
	auto filename = FilesLocation + "texturesList.txt";

		std::ifstream file(filename);
	if (file.is_open())
	{
		std::string line;
		int line_number = 0;

		while (std::getline(file, line))
		{
			TextureIndexMap.emplace_back(std::pair<std::string, uint32_t>{ line, line_number });
			line_number++;
		}

		file.close();
	}
	else ASSERT(false, 
				"Could not open file for initializing the texture indices for dynamic indexing.\n Not found in location: " 
				+ FilesLocation + " Check directory or file name.");
}
