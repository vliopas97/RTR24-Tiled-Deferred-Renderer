#include "dxUtils.h"
#include "Core/Exception.h"
#include "DX12/dxTexture.h"

D3D12_SRV_DIMENSION DX::to_D3D12_SRV_DIMENSION(VIEW_DIMENSION viewDim)
{
    return static_cast<D3D12_SRV_DIMENSION>(viewDim);
}

D3D12_UAV_DIMENSION DX::to_D3D12_UAV_DIMENSION(VIEW_DIMENSION viewDim)
{
    ASSERT(viewDim < 9, "No suitable conversion to D3D12_UAV_DIMENSION for TEXTURE_CUBE, TEXTURE_CUBEARRAY, RAYTRACING_ACCELERATION_STRUCTURE");
    return static_cast<D3D12_UAV_DIMENSION>(viewDim);
}

D3D12_RTV_DIMENSION DX::to_D3D12_RTV_DIMENSION(VIEW_DIMENSION viewDim)
{
    ASSERT(viewDim < 9, "No suitable conversion to D3D12RTV_DIMENSION for TEXTURE_CUBE, TEXTURE_CUBEARRAY, RAYTRACING_ACCELERATION_STRUCTURE");
    return static_cast<D3D12_RTV_DIMENSION>(viewDim);
}

D3D12_RESOURCE_DIMENSION DX::to_D3D12_RESOURCE_DIMENSION(RESOURCE_DIMENSION resDim)
{
    return static_cast<D3D12_RESOURCE_DIMENSION>(resDim);
}

VIEW_DIMENSION DX::from_D3D12_SRV_DIMENSION(D3D12_SRV_DIMENSION srvDim)
{
    return static_cast<VIEW_DIMENSION>(srvDim);
}

VIEW_DIMENSION DX::from_D3D12_UAV_DIMENSION(D3D12_UAV_DIMENSION uavDim)
{
    return static_cast<VIEW_DIMENSION>(uavDim);
}

VIEW_DIMENSION DX::from_D3D12_RTV_DIMENSION(D3D12_RTV_DIMENSION rtvDim)
{
    return static_cast<VIEW_DIMENSION>(rtvDim);
}

RESOURCE_DIMENSION DX::from_D3D12_RESOURCE_DIMENSION(D3D12_RESOURCE_DIMENSION resDim)
{
    return static_cast<RESOURCE_DIMENSION>(resDim);
}

DX::Texture DX::CreateRenderTarget(ID3D12Device5Ptr device, ID3D12ResourcePtr backBuffer)
{
    DX::Texture output{};
    auto desc = backBuffer->GetDesc();
    output.Resource = backBuffer;
    output.Device = device;
    output.State = D3D12_RESOURCE_STATE_PRESENT;
    output.ResourceFlags = desc.Flags;
    output.ViewDimension = VIEW_DIMENSION_TEXTURE2D;
    return output;
}
