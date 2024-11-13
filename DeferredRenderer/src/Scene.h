#pragma once
#include "Core/Core.h"
#include "Rendering/Actors/Actor.h"
#include "Rendering/Shader.h"
#include "Rendering/Texture.h"


class Scene
{
public:
	Scene(ID3D12Device5Ptr device);
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList);
	void Tick();

	void CreateShaderResources(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue, PipelineStateBindings& pipelineStateBindings);

private:
	void InitializeTextures(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue, PipelineStateBindings& pipelineStateBindings);

private:
	std::vector<Actor> Actors;
	ID3D12Device5Ptr Device;

	UniquePtr<Texture> tex;
};

