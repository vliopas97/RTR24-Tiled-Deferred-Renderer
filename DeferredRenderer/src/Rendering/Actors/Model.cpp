#include "Model.h"
#include <filesystem>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>

Model::Model(ID3D12Device5Ptr device, const Camera& camera)
	:Actor(device, camera)
{
	std::filesystem::path solutionPath = std::filesystem::current_path().parent_path();
	Assimp::Importer importer;
	auto model = importer.ReadFile(solutionPath.string() + "\\Content\\Model\\Nanosuit\\nanosuit.obj",
								   aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_FixInfacingNormals);

	struct VertexElementNorm : VertexElement
	{
		glm::float3 Normal;
	};

	std::vector<VertexElementNorm> vertices;
	vertices.reserve(model->mMeshes[0]->mNumVertices* model->mNumMeshes);
	std::vector<uint32_t> indices;
	indices.reserve(model->mMeshes[0]->mNumFaces * 3 * model->mNumMeshes);

	unsigned int offset = 0;

	for (unsigned int j = 0; j < model->mNumMeshes; j++)
	{
		auto& mesh = model->mMeshes[j];
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			vertices.push_back({
				glm::float3{ mesh->mVertices[i].x,mesh->mVertices[i].y,mesh->mVertices[i].z },
				*reinterpret_cast<glm::float3*>(&mesh->mNormals[i])
							   });
			vertices.back().Normal *= -1.0f;
		}
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			const auto& face = mesh->mFaces[i];
			assert(face.mNumIndices == 3);
			indices.push_back(face.mIndices[0] + offset);
			indices.push_back(face.mIndices[1] + offset);
			indices.push_back(face.mIndices[2] + offset);
		}
		offset += mesh->mNumVertices;
	}

	BufferLayout layout{ {"POSITION", DataType::float3},
				{"NORMAL", DataType::float3} };

	VBuffer.Init(device, vertices, layout);
	IBuffer.Init(device, indices);
}
