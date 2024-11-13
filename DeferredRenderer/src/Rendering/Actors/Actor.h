#pragma once
#include "Core/Core.h"
#include "Rendering/Buffer.h"
#include "Rendering/Shaders/HLSLCompat.h"

class Actor
{
public:
	Actor(ID3D12Device5Ptr device);
	virtual ~Actor() = default;

	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const;
	void Tick();

    void SetPosition(const glm::vec3& position) { Position = position; }
    void SetRotation(const glm::vec3& rotation) { Rotation = rotation; }
    void SetScale(const glm::vec3& scale) { Scale = scale; }

	void SetUpGPUResources(ID3D12Device5Ptr device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor);

protected:
	VertexBuffer VBuffer;
	IndexBuffer IBuffer;

	glm::vec3 Position;  
	glm::vec3 Rotation;  
	glm::vec3 Scale;

	ConstantBuffer<ActorData> Model;

	friend class Scene;
};

class Cube : public Actor
{
public:
	Cube(ID3D12Device5Ptr device);
};