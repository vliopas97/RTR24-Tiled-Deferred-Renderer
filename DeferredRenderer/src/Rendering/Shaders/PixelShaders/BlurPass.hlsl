#define HLSL
#include "..\HLSLCompat.h"

Texture2D TextureToBlur : register(t0);
SamplerState smplr : register(s0);
ConstantBuffer<BlurPassControls> Controls : register(b0);

static const uint kernelSize = 9;

static const float offsets[kernelSize] =
{
    -7.304547036499911,
    -5.35308381175656,
    -3.4048471718931532,
    -1.4588111840004858,
    0.4862426846689484,
    2.431625915613778,
    4.378621204796657,
    6.328357272092126,
    8
};

static const float weights[kernelSize] =
{
    0.012886119174695622,
    0.0519163052253057,
    0.1361482870984158,
    0.23255915602238483,
    0.2588386792559968,
    0.18772977983330918,
    0.08870474727392855,
    0.027292496709325292,
    0.003924429406638234
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
