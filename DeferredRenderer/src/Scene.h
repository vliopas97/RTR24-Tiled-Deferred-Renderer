#pragma once
#include "Core/Core.h"
#include "Rendering/Actors/Actor.h"

class Scene
{
public:
	Scene(ID3D12Device5Ptr device);
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList);

private:
	std::vector<Actor> Actors;
	ID3D12Device5Ptr Device;
};

