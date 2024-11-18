#pragma once
#include "Core/Core.h"
#include "Actor.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class Model : public Actor
{
public:
	Model(ID3D12Device5Ptr device, const class Camera& camera, const aiMesh& mesh);

};

