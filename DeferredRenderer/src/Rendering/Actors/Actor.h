#pragma once
#include "Core/Core.h"
#include "Rendering/Buffer.h"
#include "Rendering/Shaders/HLSLCompat.h"
#include "Rendering/Resources.h"
#include "Rendering/RenderPass.h"

#include <typeindex>

#include <iostream>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <typeinfo>
#include <memory>

constexpr uint32_t MaxResourcesPerInstance = 10;

class Actor
{
	using Resources2RenderPassMap = std::unordered_map<std::type_index, std::vector<LocalRenderPassBase*>>;

public:
	Actor(ID3D12Device5Ptr device, const class Camera& camera);
	Actor(const Actor&) = delete;
	Actor& operator=(const Actor&) = delete;

	Actor(Actor&&) noexcept = default;
	Actor& operator=(Actor&&) noexcept = default;

	virtual ~Actor() = default;

	template<typename Pass>
	requires std::is_base_of_v<RenderPass, Pass>
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const;

	void Tick();

    void SetPosition(const glm::vec3& position) { Position = position; }
    void SetRotation(const glm::vec3& rotation) { Rotation = rotation; }
    void SetScale(const glm::vec3& scale) { Scale = scale; }

	void SetUpGPUResources(ID3D12Device5Ptr device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor);

protected:
	template<typename Pass, typename T>
	requires std::is_base_of_v<LocalRenderPassBase, T> && std::is_base_of_v<RenderPass, Pass>
	void AddResourceToMap(T& rscPtr)
	{
		auto& resources = ResourceMap[std::type_index(typeid(Pass))];
		resources.push_back(&rscPtr);
	}

private:
	template<typename Pass>
	requires std::is_base_of_v<RenderPass, Pass>
	void BindLocalResources(ID3D12GraphicsCommandList4Ptr cmdList) const
	{
		auto it = ResourceMap.find(std::type_index(typeid(Pass)));
		if (it != ResourceMap.end())
		{
			for (const auto& resource : it->second)
				resource->Bind(cmdList);
		}
	}

protected:
	const Camera& SceneCamera;
	VertexBuffer VBuffer;
	IndexBuffer IBuffer;

	glm::vec3 Position;  
	glm::vec3 Rotation;  
	glm::vec3 Scale;

	//ConstantBuffer<ActorData> ActorInfo;
	Resources2RenderPassMap ResourceMap;

	UniquePtr<LocalRenderPass_CBV<ActorData>> ActorInfo;

	friend class Scene;
};

template<typename Pass>
requires std::is_base_of_v<RenderPass, Pass>
void Actor::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	ASSERT(false, "No appropriate Binding Scheme for this type of Render Pass");
}

template<>
inline void Actor::Bind<ForwardRenderPass>(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetVertexBuffers(0, 1, &VBuffer.GetView());
	cmdList->IASetIndexBuffer(&IBuffer.GetView());
	BindLocalResources<ForwardRenderPass>(cmdList);
	cmdList->DrawIndexedInstanced(IBuffer.GetIndexCount(), 1, 0, 0, 0);
}

class Cube : public Actor
{
public:
	Cube(ID3D12Device5Ptr device, const Camera& camera);
};

class Sphere : public Actor
{
public:
	Sphere(ID3D12Device5Ptr device, const Camera& camera);
};
