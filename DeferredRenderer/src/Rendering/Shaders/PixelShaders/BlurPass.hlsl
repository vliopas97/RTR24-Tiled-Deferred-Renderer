#define HLSL
#include "..\HLSLCompat.h"

Texture2D TextureToBlur : register(t0);
SamplerState smplr : register(s0);
ConstantBuffer<BlurPassControls> Controls : register(b0);

static const uint kernelSize = 5;

static const float offsets[kernelSize] =
{
    -3.4048471718931532,
    -1.4588111840004858,
    0.48624268466894843,
    2.431625915613778,
    4
};

static const float weights[kernelSize] =
{
    0.15642123799829394,
    0.26718801880015064,
    0.29738065394682034,
    0.21568339342709997,
    0.06332669582763516
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
