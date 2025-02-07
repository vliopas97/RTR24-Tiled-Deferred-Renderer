#include "Model.h"
#include <filesystem>

uint RoughnessToKernel(float roughness)
{
	return static_cast<uint>(1.0f + roughness * (16.0f - 1.0f));
}


Model::Model(ID3D12Device5Ptr device, 
			 const Camera& camera, 
			 const aiMesh& mesh,
			 aiMaterial** materials,
			 const std::vector<std::pair<std::string, uint32_t>>& textureIndexMap)
	:Actor(device, camera)
{
	auto findTextureIndex = [&textureIndexMap](aiString& filename)
		{
			int result = -1;
			for (const auto& pair : textureIndexMap)
				if (pair.first == filename.C_Str())
				{ 
					result = (int)pair.second;  // Return the index if found
					break;
				}

			filename.Clear();
			return result;  // Return -1 if not found
		};

	BufferLayout layout{ {"POSITION", DataType::float3},
						{"NORMAL", DataType::float3},
						{"TANGENT", DataType::float3},
						{"BITANGENT", DataType::float3},
						{"TEXCOORD", DataType::float2}, };

	struct VertexElementNorm : VertexElement
	{
		glm::float3 Normal;
		glm::float3 Tangent;
		glm::float3 Bitangent;
		glm::float2 TexCoords;
	};

	aiString filename;
	auto& material = materials[mesh.mMaterialIndex];
	auto& data = ActorInfo->Resource.CPUData;

	material->GetTexture(aiTextureType_DIFFUSE, 0, &filename);

	std::string file = filename.C_Str();
	if (file.find("floor") != std::string::npos) 
	{
		data.Material.Reflectiveness = 0.85f;
		Roughness.Resource.CPUData = RoughnessToKernel(0.7f);
	}

	data.KdID = findTextureIndex(filename);
	material->GetTexture(aiTextureType_NORMALS, 0, &filename);
	data.KnID = findTextureIndex(filename);
	material->GetTexture(aiTextureType_SPECULAR, 0, &filename);
	data.KsID = findTextureIndex(filename);

	if (data.KsID < 0) material->Get(AI_MATKEY_SHININESS, data.Material.Shininess);

	std::vector<VertexElementNorm> vertices;
	vertices.reserve(mesh.mNumVertices);
	std::vector<uint32_t> indices;
	indices.reserve(mesh.mNumFaces * 3);

	unsigned int offset = 0;
	std::vector<glm::float2> tt;

	for (unsigned int i = 0; i < mesh.mNumVertices; i++)
	{
		glm::float3 vertex = *reinterpret_cast<glm::float3*>(&mesh.mVertices[i]);
		glm::float3 normal = *reinterpret_cast<glm::float3*>(&mesh.mNormals[i]);
		glm::float3 tangent = *reinterpret_cast<glm::float3*>(&mesh.mTangents[i]);
		glm::float3 bitangent = *reinterpret_cast<glm::float3*>(&mesh.mBitangents[i]);
		glm::float2 texCoords = data.KdID >= 0 ? glm::float2{mesh.mTextureCoords[0][i].x, mesh.mTextureCoords[0][i].y} : glm::float2{0, 0};
		tt.push_back(texCoords);
		vertices.push_back({vertex, normal, tangent, bitangent, texCoords});
	}

	for (unsigned int i = 0; i < mesh.mNumFaces; i++)
	{
		const auto& face = mesh.mFaces[i];
		assert(face.mNumIndices == 3);
		indices.push_back(face.mIndices[0]);
		indices.push_back(face.mIndices[1]);
		indices.push_back(face.mIndices[2]);
	}

	VBuffer.Init(device, vertices, layout);
	IBuffer.Init(device, indices);
}
