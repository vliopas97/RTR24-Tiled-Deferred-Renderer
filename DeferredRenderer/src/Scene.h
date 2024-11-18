#pragma once
#include "Core/Core.h"
#include "Rendering/Actors/Actor.h"
#include "Rendering/Actors/Lights.h"
#include "Rendering/Shader.h"
#include "Rendering/Texture.h"


class Scene
{
public:
	Scene(ID3D12Device5Ptr device, const class Camera& camera);
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList);
	void Tick();

	void CreateShaderResources(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue, PipelineStateBindings& pipelineStateBindings);

private:
	void InitializeTextures(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue, PipelineStateBindings& pipelineStateBindings);
	void LoadModels(const Camera& camera);
	void InitializeTextureIndices();

private:
	std::vector<Actor> Actors;
	std::vector<DirectionalLight> Lights;
	ID3D12Device5Ptr Device;

	std::unordered_map<std::string, uint32_t> TextureIndexMap;
	std::vector<UniquePtr<Texture>> TextureResources;
	bool DebugMode;
};

