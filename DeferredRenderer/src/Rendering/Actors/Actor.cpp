#include "Actor.h"
#include "Primitives.h"
#include "Rendering/Shader.h"
#include "Rendering/RootSignature.h"
#include "Camera.h"

Actor::Actor(ID3D12Device5Ptr device, const Camera& camera)
    : SceneCamera(camera),
    Position(0.0f, 0.0f, 0.0f),
    Rotation(0.0f, 0.0f, 0.0f),
    Scale(1.0f, 1.0f, 1.0f)
{
    ActorInfo.CPUData.Material.MatericalColor = glm::vec3(1.0f, 1.0f, 1.0f);
    ActorInfo.CPUData.Material.SpecularIntensity = 1.0f;
    ActorInfo.CPUData.Material.Shininess = 12.0f;
}

void Actor::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetVertexBuffers(0, 1, &VBuffer.GetView());
	cmdList->IASetIndexBuffer(&IBuffer.GetView());
    cmdList->SetGraphicsRootConstantBufferView(LightCBuffer + 1, ActorInfo.GetGPUVirtualAddress());
	cmdList->DrawIndexedInstanced(IBuffer.GetIndexCount(), 1, 0, 0, 0);
}

void Actor::Tick()
{
    ActorInfo.CPUData.Model = glm::translate(Position) * glm::mat4_cast(glm::quat(glm::radians(Rotation))) * glm::scale(Scale);
    ActorInfo.CPUData.ModelView = SceneCamera.GetView() * ActorInfo.CPUData.Model;
    ActorInfo.Tick();
}

void Actor::SetUpGPUResources(ID3D12Device5Ptr device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor)
{
    ActorInfo.Init(device, destDescriptor);
}

Cube::Cube(ID3D12Device5Ptr device, const Camera& camera)
    :Actor(device, camera)
{
    struct VertexElementNorm : VertexElement
    {
        glm::float3 Normal;
    };

    auto data = Primitives::Cube::CreateWNormals<VertexElementNorm>();

    BufferLayout layout{ {"POSITION", DataType::float3},
                    {"NORMAL", DataType::float3} };

    VBuffer.Init(device, data.Vertices, layout);
    IBuffer.Init(device, data.Indices);
}

Sphere::Sphere(ID3D12Device5Ptr device, const Camera& camera)
    :Actor(device, camera)
{
    Scale = glm::vec3(0.1f, 0.1f, 0.1f);
    
    auto data = Primitives::Sphere::Create<VertexElement>();

    BufferLayout layout{ {"POSITION", DataType::float3},
                    {"NORMAL", DataType::float3}};

    VBuffer.Init(device, data.Vertices, layout);
    IBuffer.Init(device, data.Indices);
}
