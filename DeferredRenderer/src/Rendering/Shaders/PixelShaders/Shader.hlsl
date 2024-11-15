// pixel.hlsl
#include "..\Common.hlsli"
#include "core.hlsli"

struct PSInput
{
    float3 posWorld : POSITION;
    float3 normal : NORMAL;
    float4 position : SV_POSITION;
};

SamplerState smplr : register(s0);

float4 main(float3 posWorld : POSITION, float3 n : Normal) : SV_TARGET
{    
    return float4(calcSunlight(n), 1.0f);
}