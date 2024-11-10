#include "Actor.h"

Actor::Actor(ID3D12Device5Ptr device)
{
    std::vector<VertexElement> cubeVertices =
    {
        // Front face (Z = 1.0f)
        { { -1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }, // Top-left front
        { {  1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } }, // Top-right front
        { {  1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } }, // Bottom-right front
        { { -1.0f, -1.0f,  1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } }, // Bottom-left front

        // Back face (Z = -1.0f)
        { { -1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f } }, // Top-left back
        { {  1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } }, // Top-right back
        { {  1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 0.5f, 0.5f } }, // Bottom-right back
        { { -1.0f, -1.0f, -1.0f }, { 0.5f, 0.5f, 0.5f, 1.0f } }  // Bottom-left back
    };

    std::vector<uint32_t> cubeIndices = {
        // Front face
        0, 1, 2,
        0, 2, 3,

        // Right face
        1, 5, 6,
        1, 6, 2,

        // Back face
        5, 4, 7,
        5, 7, 6,

        // Left face
        4, 0, 3,
        4, 3, 7,

        // Top face
        4, 5, 1,
        4, 1, 0,

        // Bottom face
        3, 2, 6,
        3, 6, 7
    };


	BufferLayout layout{ {"POSITION", DataType::float3},
						{"COLOR", DataType::float4} };
	VBuffer.Init(device, cubeVertices, layout);
	IBuffer.Init(device, cubeIndices);
	
}

void Actor::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetVertexBuffers(0, 1, &VBuffer.GetView());
	cmdList->IASetIndexBuffer(&IBuffer.GetView());
	cmdList->DrawIndexedInstanced(IBuffer.GetIndexCount(), 1, 0, 0, 0);
}
