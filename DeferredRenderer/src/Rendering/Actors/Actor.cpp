#include "Actor.h"
#include "Primitives.h"

Actor::Actor(ID3D12Device5Ptr device)
    : Position(0.0f, 0.0f, 0.0f),
    Rotation(0.0f, 0.0f, 0.0f),
    Scale(1.0f, 1.0f, 1.0f)
{
}

void Actor::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetVertexBuffers(0, 1, &VBuffer.GetView());
	cmdList->IASetIndexBuffer(&IBuffer.GetView());
    cmdList->SetGraphicsRootConstantBufferView(3, Model.GetGPUVirtualAddress());
	cmdList->DrawIndexedInstanced(IBuffer.GetIndexCount(), 1, 0, 0, 0);
}

void Actor::Tick()
{
    Model.CPUData.Model = glm::translate(Position) * glm::mat4_cast(glm::quat(Rotation)) * glm::scale(Scale);
    Model.Tick();
}

void Actor::SetUpGPUResources(ID3D12Device5Ptr device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor)
{
    Model.Init(device, destDescriptor);
}

Cube::Cube(ID3D12Device5Ptr device)
    :Actor(device)
{
    auto data = Primitives::Cube::Create<VertexElement>();
    for (auto& d : data.Vertices)
        d.Color = glm::float4(1.0f, 1.0f, 1.0f, 1.0f);

    BufferLayout layout{ {"POSITION", DataType::float3},
                        {"COLOR", DataType::float4} };
    VBuffer.Init(device, data.Vertices, layout);
    IBuffer.Init(device, data.Indices);
}