#include "Model.h"
#include <filesystem>

Model::Model(ID3D12Device5Ptr device, const Camera& camera, const aiMesh& mesh)
	:Actor(device, camera)
{
	struct VertexElementNorm : VertexElement
	{
		glm::float3 Normal;
	};

	std::vector<VertexElementNorm> vertices;
	vertices.reserve(mesh.mNumVertices);
	std::vector<uint32_t> indices;
	indices.reserve(mesh.mNumFaces * 3);

	unsigned int offset = 0;

	for (unsigned int i = 0; i < mesh.mNumVertices; i++)
	{
		vertices.push_back({
			glm::float3{ mesh.mVertices[i].x,mesh.mVertices[i].y,mesh.mVertices[i].z },
			*reinterpret_cast<glm::float3*>(&mesh.mNormals[i])
							});
		vertices.back().Normal *= -1.0f;
	}

	for (unsigned int i = 0; i < mesh.mNumFaces; i++)
	{
		const auto& face = mesh.mFaces[i];
		assert(face.mNumIndices == 3);
		indices.push_back(face.mIndices[0]);
		indices.push_back(face.mIndices[1]);
		indices.push_back(face.mIndices[2]);
	}

	BufferLayout layout{ {"POSITION", DataType::float3},
				{"NORMAL", DataType::float3} };

	VBuffer.Init(device, vertices, layout);
	IBuffer.Init(device, indices);
}
