#pragma once
#include "Core/Core.h"
#include "Rendering/Actors/Actor.h"
#include "Rendering/Actors/Lights.h"
#include "Rendering/Shader.h"
#include "Rendering/Texture.h"
#include "Rendering/RootSignature.h"


class Scene
{
public:
	Scene(ID3D12Device5Ptr device, const class Camera& camera);

	template<typename Pass>
	requires std::is_base_of_v<RenderPass, Pass>
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
	{
		for (const auto& actor : Actors)
			actor.Bind<Pass>(cmdList);
	}
	
	void Tick();

	void CreateShaderResources(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue);

private:
	void InitializeTextures(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue);
	void LoadModels(const Camera& camera);
	void InitializeTextureIndices();

private:
	std::vector<Actor> Actors;
	std::vector<DirectionalLight> Lights;
	ID3D12Device5Ptr Device;

	std::vector<std::pair<std::string, uint32_t>> TextureIndexMap;
	std::vector<UniquePtr<Texture>> TextureResources;
	
	bool DebugMode;
	std::string FilesLocation;
};

template<>
inline void Scene::Bind<GUIPass>(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	for (auto& light : Lights)
		light.GUI();
}

