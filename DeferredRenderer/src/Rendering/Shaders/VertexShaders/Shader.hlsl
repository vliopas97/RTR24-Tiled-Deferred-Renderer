#include "..\Common.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput main(float3 position : POSITION, float4 color : COLOR)
{
    PSInput result;
    result.position = mul(actorData.Model, float4(position, 1.0f));
    result.position = mul(globalConstants.ViewProjection, result.position);
    result.color = color;
    return result;
}