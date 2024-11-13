#pragma once
#include "Core/Core.h"
#include "DirectXTex.h"

class Texture
{
public:
	Texture(ID3D12Device5Ptr device, ID3D12CommandQueuePtr cmdQueue, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor, const std::string& filename);

	inline uint32_t GetWidth() const { return (uint32_t)Image.GetMetadata().width; }
	inline uint32_t GetHeight() const { return (uint32_t)Image.GetMetadata().height; }
	inline ID3D12ResourcePtr GetResource() { return TextureResource; }

private:
	DirectX::ScratchImage Image;
	ID3D12ResourcePtr TextureResource;

};

