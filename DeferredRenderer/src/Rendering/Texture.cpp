#include "Texture.h"
#include "Core/Exception.h"
#include "ResourceUploadBatch.h"

#include <source_location>
#include <filesystem>
#include <cstdlib>

#include <iostream>
#include <fstream>

using namespace DirectX;

Texture::Texture(ID3D12Device5Ptr device, ID3D12CommandQueuePtr CmdQueue, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor, const std::string& filename)
{
	wchar_t wideName[512];
	mbstowcs_s(nullptr, wideName, filename.c_str(), _TRUNCATE);
	GRAPHICS_ASSERT(DirectX::LoadFromWICFile(wideName, DirectX::WIC_FLAGS_NONE, nullptr, Image));

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    bool isNormalMap = filename.find("ddn") != std::string::npos ||
        filename.find("NRM") != std::string::npos;
    bool isPNG = filename.find(".png") != std::string::npos;

    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (isNormalMap)
    {
        if (!isPNG || filename.find("arch_ddn") != std::string::npos)
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
        else
            format = DXGI_FORMAT_B8G8R8A8_UNORM;
    }

    auto resDesc = CD3DX12_RESOURCE_DESC(
        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
        GetWidth(), GetHeight(), 1, isNormalMap ? 1 : 5,
        format,
        1, 0,
        D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE);
    
    GRAPHICS_ASSERT(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&TextureResource)));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = resDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = isNormalMap ? 1 : 5;

    device->CreateShaderResourceView(TextureResource, &srvDesc, destDescriptor);

  
    D3D12_SUBRESOURCE_DATA textureData = { Image.GetPixels(), Image.GetImage(0, 0, 0)->rowPitch, 0};
    resourceUpload.Upload(TextureResource, 0, &textureData, 1);

    if (!isNormalMap)
    {
        resourceUpload.Transition(
            TextureResource,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE  );

        resourceUpload.GenerateMips(TextureResource );

        resourceUpload.Transition(
            TextureResource,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    }

    auto uploadResourcesFinished = resourceUpload.End(CmdQueue);
    uploadResourcesFinished.wait();

}
