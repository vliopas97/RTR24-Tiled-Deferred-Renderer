#pragma once
#include "Core/Core.h"
#include "dxCore.h"
#include "dxDescriptorHandle.h"
#include "dxDescriptorHeap.h"

#include <optional>

namespace DX
{
	struct TextureCreateInfo
	{
		ID3D12Device5Ptr device = nullptr;
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t depth = 1;;
		bool generateMipMaps = false;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		RESOURCE_DIMENSION resDim = RESOURCE_DIMENSION_UNKNOWN;
		VIEW_DIMENSION viewDim = VIEW_DIMENSION_UNKNOWN;
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON;
		D3D12_CLEAR_VALUE clearValue{};
	};

	struct Texture
	{
		Texture() = default;
		Texture(TextureCreateInfo* textureCreateInfo);
		Texture(TextureCreateInfo* textureCreateInfo, const std::string& filename);

		void TransitionTo(ID3D12GraphicsCommandList4Ptr cmdList, D3D12_RESOURCE_STATES nextState);

		D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc() const;
		D3D12_RENDER_TARGET_VIEW_DESC GetRTVDesc() const;
		D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc() const;

		void Upload(void* data, uint32_t rowPitch, uint32_t slicePitch);
		void GenerateMipMaps();

		ID3D12ResourcePtr Resource;

	private:
		void InitResource(TextureCreateInfo* textureCreateInfo);

	private:
		ID3D12Device5Ptr Device;
		D3D12_RESOURCE_STATES State;
		D3D12_RESOURCE_DESC ResourceDesc;// might get rid of this wastes memory
		D3D12_RESOURCE_FLAGS ResourceFlags;
		VIEW_DIMENSION ViewDimension;

		friend Texture CreateRenderTarget(ID3D12Device5Ptr device, ID3D12ResourcePtr backBuffer);
	};
}