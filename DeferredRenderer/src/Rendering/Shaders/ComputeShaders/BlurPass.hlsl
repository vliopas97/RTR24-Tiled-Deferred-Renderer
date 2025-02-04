// Workgroup size
#define GROUP_SIZE 16
#define M 16
#define N (2 * M + 1)
#define CACHE_SIZE (GROUP_SIZE + 2 * M)

Texture2D<float4> InputImage : register(t0);
RWTexture2D<float4> OutputImage : register(u0);

// Gaussian kernel weights (sigma = 10)
static const float coeffs[N] =
{
    0.0123181, 0.0143815, 0.0166235, 0.0190241, 0.0215548, 0.0241795, 0.026854, 0.029528,
    0.0321453, 0.0346468, 0.0369717, 0.0390603, 0.0408566, 0.0423107, 0.0433808, 0.0440359,
    0.0442566, 0.0440359, 0.0433808, 0.0423107, 0.0408566, 0.0390603, 0.0369717, 0.0346468,
    0.0321453, 0.029528, 0.026854, 0.0241795, 0.0215548, 0.0190241, 0.0166235, 0.0143815, 0.0123181
};

// Reduce shared memory usage: Use row and column buffers instead of full 2D cache
groupshared float3 cache[CACHE_SIZE][CACHE_SIZE];

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void main(uint3 globalID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{ 
    uint width, height, noMips;
    InputImage.GetDimensions(0, width, height, noMips);
    uint2 imageSize = uint2(width, height);

    // Compute the top-left position of the workgroup in global coordinates
    uint2 localPos = localID.xy + uint2(M, M); // Shift position to exclude invalid edges
    uint2 workgroupOrigin = groupID.xy * GROUP_SIZE - uint2(M, M);
    uint2 screenPos = groupID.xy * GROUP_SIZE + localID.xy;

    // Load to shared memory
    for (uint i = localID.y; i < CACHE_SIZE; i += GROUP_SIZE)
    {
        for (uint j = localID.x; j < CACHE_SIZE; j += GROUP_SIZE)
        {
            uint2 texPos = workgroupOrigin + uint2(j, i);
            if (texPos.x < imageSize.x && texPos.y < imageSize.y)
                cache[i][j] = InputImage.Load(uint3(texPos, 0)).rgb;
            else
                cache[i][j] = float3(0.0f, 0.0f, 0.0f);
        }
    }

    GroupMemoryBarrierWithGroupSync();
    
    if (localPos.x >= CACHE_SIZE - M || localPos.y >= CACHE_SIZE - M)
        return;

    // Horizontal Pass
    float3 blurH = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < N; i++)
    {
        blurH += cache[localPos.y][localPos.x + i - M] * coeffs[i];
    }

    if (screenPos.x < imageSize.x && screenPos.y < imageSize.y)
        OutputImage[screenPos] = float4(blurH, 1.0f);

    GroupMemoryBarrierWithGroupSync();
    
     // Reload Horizontally Blurred Data
    for (uint i = localID.y; i < CACHE_SIZE; i += GROUP_SIZE)
    {
        for (uint j = localID.x; j < CACHE_SIZE; j += GROUP_SIZE)
        {
            uint2 texPos = workgroupOrigin + uint2(j, i);
            if (texPos.x < imageSize.x && texPos.y < imageSize.y)
                cache[i][j] = OutputImage.Load(uint3(texPos, 0)).rgb; // Reload from horizontal pass
            else
                cache[i][j] = float3(0.0f, 0.0f, 0.0f);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Vertical Pass
    float3 blurV = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < N; i++)
    {
        blurV += cache[localPos.y + i - M][localPos.x] * coeffs[i];
    }
    
    if (screenPos.x < imageSize.x && screenPos.y < imageSize.y)
        OutputImage[screenPos] = float4(blurV, 1.0f);
}
