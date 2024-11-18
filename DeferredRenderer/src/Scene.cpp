#include "Scene.h"
#include "Camera.h"
#include "Rendering/Actors/Model.h"
#include <filesystem>
#include <unordered_map>

Scene::Scene(ID3D12Device5Ptr device, const Camera& camera)
	:Device(device), DebugMode(true)
{
	//Actors.emplace_back(Cube{ device, camera });
	//Actors[0].SetPosition({ 2, 0, 0 });
	//Actors.emplace_back(Cube{ device, camera });
	//Actors[1].SetPosition({ 0, 0, 3 });
	InitializeTextureIndices();// Texture indices before Loading the Models - IMPORTANT
	LoadModels(camera);
	Lights.emplace_back(DirectionalLight{});
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

	for (auto& light : Lights)
	{
		light.Tick();
		light.GUI();
	}
}

void Scene::CreateShaderResources(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue, PipelineStateBindings& pipelineStateBindings)
{
	auto uavHandle = pipelineStateBindings.UAVHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvHandle = pipelineStateBindings.SRVHeap->GetCPUDescriptorHandleForHeapStart();
	auto cbvHandle = pipelineStateBindings.CBVHeap->GetCPUDescriptorHandleForHeapStart();
	auto lightsHandle = pipelineStateBindings.LightsHeap->GetCPUDescriptorHandleForHeapStart();

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

	std::filesystem::path solutionPath = std::filesystem::current_path().parent_path();
	auto filepath = solutionPath.string() + (DebugMode ? "\\Content\\Model\\Nanosuit\\" : "\\Content\\Model\\Sponza\\");

	for (const auto& pair : TextureIndexMap)
	{
		TextureResources.emplace_back(MakeUnique<Texture>(device, cmdQueue, srvHandle, filepath + pair.first));
		srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}

void Scene::LoadModels(const Camera& camera)
{
	auto findTextureIndex = [this](aiString& filename)
		{
			auto it = TextureIndexMap.find(filename.C_Str());
			filename.Clear();
			return it != TextureIndexMap.end() ? it->second : -1; // Return -1 if not found
		};

	Assimp::Importer importer;
	auto objFilename = DebugMode ? "\\Content\\Model\\Nanosuit\\nanosuit.obj" : "\\Content\\Model\\Sponza\\sponza.obj";
	float scalingFactor = DebugMode ? 0.15f : 0.05f;

	std::filesystem::path solutionPath = std::filesystem::current_path().parent_path();
	auto scene = importer.ReadFile(solutionPath.string() + objFilename,
								   aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_FixInfacingNormals);

	assert(scene->HasMaterials() && "expected that loaded scene has materials");
	auto* materials = scene->mMaterials;

	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		const auto mesh = scene->mMeshes[i];
		Actors.emplace_back(Model{ Device, camera, *mesh });
		auto& actor = Actors.back();
		actor.SetScale({ scalingFactor, scalingFactor, scalingFactor });

		bool hasMaterial = mesh->mMaterialIndex >= 0;
		if (!hasMaterial)
			return;

		aiString filename;
		auto& material = materials[mesh->mMaterialIndex];
		
		material->GetTexture(aiTextureType_DIFFUSE, 0, &filename);
		actor.ActorInfo.CPUData.KdID = findTextureIndex(filename);
		material->GetTexture(aiTextureType_NORMALS, 0, &filename);
		actor.ActorInfo.CPUData.KnID = findTextureIndex(filename);
		material->GetTexture(aiTextureType_SPECULAR, 0, &filename);
		actor.ActorInfo.CPUData.KsID = findTextureIndex(filename);
	}
}

void Scene::InitializeTextureIndices()
{
	auto filename = std::string(DebugMode ? "\\Content\\Model\\Nanosuit\\texturesList.txt" : "\\Content\\Model\\Sponza\\texturesList.txt");
	std::filesystem::path solutionPath = std::filesystem::current_path().parent_path();
	filename = solutionPath.string() + filename;

		std::ifstream file(filename);
	if (file.is_open())
	{
		std::string line;
		int line_number = 0;

		while (std::getline(file, line))
		{
			TextureIndexMap[line] = line_number; // 0-based indexing
			line_number++;
		}

		file.close();
	}
	else assert(false && "could not open file");
	
}
