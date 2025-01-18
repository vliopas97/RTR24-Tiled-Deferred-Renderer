#define HLSL
#include "..\HLSLCompat.h"

Texture2D TextureToBlur : register(t0);
SamplerState smplr : register(s0);
ConstantBuffer<BlurPassControls> Controls : register(b0);

static const uint kernelSize = 5;

static const float offsets[kernelSize] =
{
    -2,
    -1,
    0,
    1,
    2
};

static const float weights[kernelSize] =
{
    0.15338835280702454,
    0.22146110682534667,
    0.2503010807352574,
    0.22146110682534667,
    0.15338835280702454
};

float4 main(float4 position : SV_Position) : SV_Target
{
    uint width, height, noMips;
    TextureToBlur.GetDimensions(0, width, height, noMips);
    uint2 screenSize = uint2(width, height);
    
    // Calculate normalized texture coordinates
    float2 texCoords = float2(position.x / width, position.y / height);
    
    // Determine the direction of the blur based on the Controls structure
    float2 dir = Controls.IsHorizontal ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);

    float4 output = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    for (int i = 0; i < kernelSize; i++)
    {
        float2 offset = dir * offsets[i] / screenSize;
        float weight = weights[i];
        output += TextureToBlur.Sample(smplr, texCoords + offset) * weight;
    }
    
    return output;
}
