#pragma once
#include "Core/Core.h"
#include "Actor.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <unordered_map>

class Model : public Actor
{
public:
	Model(ID3D12Device5Ptr device, 
		  const class Camera& camera, 
		  const aiMesh& mesh,
		  aiMaterial** materials,
		  const std::vector<std::pair<std::string, uint32_t>>& textureIndexMap);

};

