#pragma once
#include "Core/Core.h"
#include "Rendering/Buffer.h"
#include "Rendering/Shaders/HLSLCompat.h"

// For the sun
class DirectionalLight
{
public:
	DirectionalLight();

	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const;
	void GUI() const;
	void Tick();

	void SetUpGPUResources(ID3D12Device5Ptr device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor);

private:
	mutable glm::vec3 Position;
	mutable glm::vec3 Direction;

	mutable ConstantBuffer<DirLightData> Info;
};

