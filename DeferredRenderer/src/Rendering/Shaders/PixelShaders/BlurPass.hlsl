#define HLSL
#include "..\HLSLCompat.h"

Texture2D TextureToBlur : register(t0);
SamplerState smplr : register(s0);
ConstantBuffer<BlurPassControls> Controls : register(b0);

static const uint kernelSize = 15;

static const float offsets[kernelSize] =
{
    -7,
    -6,
    -5,
    -4,
    -3,
    -2,
    -1,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7
};

static const float weights[kernelSize] =
{
    0.009032585254438357,
    0.018475821052165713,
    0.03385119150683613,
    0.05555529465395964,
    0.08167005483822282,
    0.10754484603742742,
    0.12685407401442889,
    0.13403226528504197,
    0.12685407401442889,
    0.10754484603742742,
    0.08167005483822282,
    0.05555529465395964,
    0.03385119150683613,
    0.018475821052165713,
    0.009032585254438357
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
