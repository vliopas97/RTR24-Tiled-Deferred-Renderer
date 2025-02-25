#include "dxTexture.h"
#include "Core/Exception.h"
#include "Resources.h"

#include "dxUtils.h"

#include "DirectXTex.h"
#include "ResourceUploadBatch.h"

using namespace DirectX;

namespace DX
{

	Texture::Texture(TextureCreateInfo* textureCreateInfo)
	{
        InitResource(textureCreateInfo);
        if (textureCreateInfo->generateMipMaps)
            GenerateMipMaps();
	}

    Texture::Texture(TextureCreateInfo* textureCreateInfo, const std::string& filename)
    {
        DirectX::ScratchImage Image;
        wchar_t wideName[512];

        mbstowcs_s(nullptr, wideName, filename.c_str(), _TRUNCATE);
        //GRAPHICS_ASSERT(DirectX::LoadFromWICFile(wideName, DirectX::WIC_FLAGS_NONE, nullptr, Image));
        DirectX::LoadFromWICFile(wideName, DirectX::WIC_FLAGS_NONE, nullptr, Image);

        textureCreateInfo->width = (uint32_t)Image.GetMetadata().width;
        textureCreateInfo->height = (uint32_t)Image.GetMetadata().height;

        InitResource(textureCreateInfo);

        Upload(Image.GetPixels(), Image.GetImage(0, 0, 0)->rowPitch, 0);

        if (textureCreateInfo->generateMipMaps)
            GenerateMipMaps();
    }

    void Texture::TransitionTo(ID3D12GraphicsCommandList4Ptr cmdList, D3D12_RESOURCE_STATES nextState)
    {
        //ASSERT(State != nextState, "Best avoid transition where previous and next state are the same");
        if (State == nextState) return;

        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Resource.Get(), State, nextState));
        State = nextState;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC Texture::GetSRVDesc() const
    {
        ASSERT(!(ResourceFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE), "Can't return SRVDesc. Shader Resource Denied");
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = ResourceDesc.Format;
        desc.ViewDimension = to_D3D12_SRV_DIMENSION(ViewDimension);
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Texture2D.MipLevels = ResourceDesc.MipLevels;
        return desc;
    }

    D3D12_RENDER_TARGET_VIEW_DESC Texture::GetRTVDesc() const
    {
        ASSERT(ResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, "Can't return RTVDesc. Render Target View not allowed");
        D3D12_RENDER_TARGET_VIEW_DESC desc = {};
        desc.Format = ResourceDesc.Format;
        desc.ViewDimension = to_D3D12_RTV_DIMENSION(ViewDimension);
        desc.Texture2D.MipSlice = 0;
        return desc;
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC Texture::GetUAVDesc() const
    {
        ASSERT(ResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "Can't return UAVDesc. Unordered Access not allowed");
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
        desc.Format = ResourceDesc.Format;
        desc.ViewDimension = to_D3D12_UAV_DIMENSION(ViewDimension);
        desc.Texture2D.MipSlice = 0;
        return desc;
    }

    void Texture::Upload(void* data, uint32_t rowPitch, uint32_t slicePitch)
    {
        ResourceUploadBatch resourceUpload(Device);
        resourceUpload.Begin();
        D3D12_SUBRESOURCE_DATA subResourceData = { data, rowPitch, slicePitch };

        if (State != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            resourceUpload.Transition( Resource, State, D3D12_RESOURCE_STATE_COPY_DEST);
            resourceUpload.Upload(Resource.Get(), 0, &subResourceData, 1);
            resourceUpload.Transition( Resource, D3D12_RESOURCE_STATE_COPY_DEST, State);
        }
        else
            resourceUpload.Upload(Resource, 0, &subResourceData, 1);


        resourceUpload.End(Globals.CmdQueue).wait();
    }

    void Texture::GenerateMipMaps()
    {
        ResourceUploadBatch resourceUpload(Device);
        resourceUpload.Begin();

        if (State != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        {
            resourceUpload.Transition(Resource, State, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            resourceUpload.GenerateMips(Resource);
            resourceUpload.Transition( Resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, State);
        }
        else
            resourceUpload.GenerateMips(Resource);

        resourceUpload.End(Globals.CmdQueue).wait();
    }

    void Texture::InitResource(TextureCreateInfo* textureCreateInfo)
    {
        Device = textureCreateInfo->device;
        State = textureCreateInfo->initialResourceState;
        ResourceFlags = textureCreateInfo->flags;
        ViewDimension = textureCreateInfo->viewDim;

        D3D12_CLEAR_VALUE clearValue = textureCreateInfo->clearValue;

        ResourceDesc = CD3DX12_RESOURCE_DESC(
            to_D3D12_RESOURCE_DIMENSION(textureCreateInfo->resDim),
            D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
            textureCreateInfo->width, textureCreateInfo->height, textureCreateInfo->depth,
            textureCreateInfo->generateMipMaps ? uint32_t(floor(log2(float(std::min(textureCreateInfo->width, textureCreateInfo->height))))) + 1 : 1,
            textureCreateInfo->format,
            1, 0,
            D3D12_TEXTURE_LAYOUT_UNKNOWN,
            textureCreateInfo->flags);

        bool useClearValue = (ResourceFlags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) &&
            (textureCreateInfo->clearValue.Format != DXGI_FORMAT_UNKNOWN);

        GRAPHICS_ASSERT(
            textureCreateInfo->device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &ResourceDesc,
                State,
                useClearValue? &clearValue : nullptr,
                IID_PPV_ARGS(&Resource)));
    }

}