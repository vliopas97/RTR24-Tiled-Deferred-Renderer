#define HLSL
#include "..\HLSLCompat.h"

Texture2D<float4> OriginalColor;
Texture2D<float4> ReflectionColor;

SamplerState smplr : register(s0);

ConstantBuffer<PipelineConstants> glConstants[] : register(b0, space0);
static PipelineConstants globalConstants = glConstants[0];

float4 main(float4 position : SV_Position) : SV_TARGET
{
    uint width, height, noMips;
    OriginalColor.GetDimensions(0, width, height, noMips);
    uint2 screenSize = uint2(width, height);
    
    float2 texCoords = float2(position.x / width, position.y / height);
    
    float4 orgColor = OriginalColor.Sample(smplr, texCoords);
    float4 reflColor = ReflectionColor.Sample(smplr, texCoords);
    
    if(!globalConstants.SSREnabled)
        return orgColor;

    float3 color = lerp(orgColor.rgb, reflColor.rgb, reflColor.a);
    return float4(color, 1.0f);

}