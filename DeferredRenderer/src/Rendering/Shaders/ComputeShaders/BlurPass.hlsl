// Workgroup size
#define HLSL
#include "..\HLSLCompat.h"

#define GROUP_SIZE 12
#define MAX_RADIUS 16
#define MAX_KERNEL_SIZE (2 * MAX_RADIUS + 1)
#define CACHE_SIZE (GROUP_SIZE + 2 * MAX_RADIUS)

Texture2D<float4> InputImage : register(t0);
RWTexture2D<float4> OutputImage : register(u0);
StructuredBuffer<Kernel> Filters : register(t1);

struct RadiusCB
{
    uint Radius;
};
ConstantBuffer<RadiusCB> RadiusBuffer : register(b0);


uint radiusToFilterID(RadiusCB radius)
{
    return radius.Radius - 1;
}

static const uint filterID = radiusToFilterID(RadiusBuffer);
static const float coeffs[MAX_KERNEL_SIZE] = Filters[filterID].Coeffs;
static const uint kernelSize = 2 * RadiusBuffer.Radius + 1;

// Reduce shared memory usage: Use row and column buffers instead of full 2D cache
groupshared float4 cache[CACHE_SIZE][CACHE_SIZE];

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void main(uint3 globalID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{ 
    uint width, height, noMips;
    InputImage.GetDimensions(0, width, height, noMips);
    uint2 imageSize = uint2(width, height);
    // Compute the top-left position of the workgroup in global coordinates
    uint2 localPos = localID.xy + uint2(MAX_RADIUS, MAX_RADIUS); // Shift position to exclude invalid edges
    uint2 workgroupOrigin = groupID.xy * GROUP_SIZE - uint2(MAX_RADIUS, MAX_RADIUS);
    uint2 screenPos = groupID.xy * GROUP_SIZE + localID.xy;

    // Load to shared memory
    for (uint i = localID.y; i < CACHE_SIZE; i += GROUP_SIZE)
    {
        for (uint j = localID.x; j < CACHE_SIZE; j += GROUP_SIZE)
        {
            uint2 texPos = workgroupOrigin + uint2(j, i);
            if (texPos.x < imageSize.x && texPos.y < imageSize.y)
                cache[i][j] = InputImage.Load(uint3(texPos, 0)).rgba;
            else
                cache[i][j] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    GroupMemoryBarrierWithGroupSync();
    
    if (localPos.x >= CACHE_SIZE - MAX_RADIUS || localPos.y >= CACHE_SIZE - MAX_RADIUS)
        return;

    // Horizontal Pass
    float4 blurH = float4(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < kernelSize; i++)
    {
        blurH += cache[localPos.y][localPos.x + i - RadiusBuffer.Radius] * coeffs[i];
    }

    if (screenPos.x < imageSize.x && screenPos.y < imageSize.y)
        OutputImage[screenPos] = blurH;

    GroupMemoryBarrierWithGroupSync();
    
     // Reload Horizontally Blurred Data
    for (uint i = localID.y; i < CACHE_SIZE; i += GROUP_SIZE)
    {
        for (uint j = localID.x; j < CACHE_SIZE; j += GROUP_SIZE)
        {
            uint2 texPos = workgroupOrigin + uint2(j, i);
            if (texPos.x < imageSize.x && texPos.y < imageSize.y)
                cache[i][j] = OutputImage.Load(uint3(texPos, 0)).rgba;
            else
                cache[i][j] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Vertical Pass
    float4 blurV = float4(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < kernelSize; i++)
    {
        blurV += cache[localPos.y + i - RadiusBuffer.Radius][localPos.x] * coeffs[i];
    }
    
    if (screenPos.x < imageSize.x && screenPos.y < imageSize.y)
        OutputImage[screenPos] = blurV;
}
