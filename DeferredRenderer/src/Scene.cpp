#include "Scene.h"
#include "Camera.h"
#include "Rendering/Actors/Model.h"
#include "Rendering/Resources.h"

#include <filesystem>
#include <unordered_map>
#include <iostream>

Scene::Scene(ID3D12Device5Ptr device, const Camera& camera)
	:Device(device), DebugMode(false)
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
	}
}

void Scene::CreateShaderResources(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue)
{
	auto uavHeapIt = Globals.UAVHeap.begin();
	auto srvHeapIt = Globals.SRVHeap.begin();
	auto cbvHeapIt = Globals.CBVHeap.begin();
	auto lightsHeapIt = Globals.LightsHeap.begin();

	for (auto& actor : Actors)
	{
		actor.SetUpGPUResources(Device, (++cbvHeapIt)->CPUHandle);
	}

	for (auto& light : Lights)
	{
		light.SetUpGPUResources(device, lightsHeapIt->CPUHandle);
		lightsHeapIt++;
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
	CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(Globals.SamplerHeap.begin()->CPUHandle);
	device->CreateSampler(&samplerDesc, samplerHandle);

}

void Scene::InitializeTextures(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue)
{
	// Create Texture
	auto srvHeapIt = Globals.SRVHeap.begin();

	for (const auto& pair : TextureIndexMap)
	{
		auto filename = pair.first;

		DX::TextureCreateInfo texCreateInfo{};
		texCreateInfo.device = device;
		texCreateInfo.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		texCreateInfo.initialResourceState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		texCreateInfo.viewDim = VIEW_DIMENSION_TEXTURE2D;
		texCreateInfo.resDim = RESOURCE_DIMENSION_TEXTURE2D;

		bool isNormalMap = filename.find("ddn") != std::string::npos ||
			filename.find("NRM") != std::string::npos;
		bool isPNG = filename.find(".png") != std::string::npos;

		if (isNormalMap)
		{
			if (!isPNG || filename.find("arch_ddn") != std::string::npos)
				texCreateInfo.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			else
				texCreateInfo.format = DXGI_FORMAT_B8G8R8A8_UNORM;
		}

		texCreateInfo.generateMipMaps = !isNormalMap;

		UniquePtr<DX::Texture> t = MakeUnique<DX::Texture>(&texCreateInfo, FilesLocation + filename);
		auto desc = t->GetSRVDesc();
		device->CreateShaderResourceView(t->Resource.Get(), &desc, (srvHeapIt++)->CPUHandle);

		TextureResources.emplace_back(std::move(t));
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
