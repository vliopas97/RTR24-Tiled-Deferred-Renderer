#include "..\Common.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORDS;
};

PSInput main(float3 position : POSITION, float4 color : COLOR, float2 tex : TEXCOORDS)
{
    PSInput result;
    result.position = mul(actorData.Model, float4(position, 1.0f));
    result.position = mul(globalConstants.ViewProjection, result.position);
    result.tex = tex;
    return result;
}