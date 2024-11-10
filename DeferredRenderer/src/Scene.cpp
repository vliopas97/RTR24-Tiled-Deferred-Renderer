#include "Scene.h"

Scene::Scene(ID3D12Device5Ptr device)
	:Device(device)
{
	Actors.emplace_back( device);
}

void Scene::Bind(ID3D12GraphicsCommandList4Ptr cmdList)
{
	for (const auto& actor : Actors)
		actor.Bind(cmdList);
}
