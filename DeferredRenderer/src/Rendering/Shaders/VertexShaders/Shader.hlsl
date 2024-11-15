#include "..\Common.hlsli"

struct PSInput
{
    float3 posWorld : POSITION;
    float3 normal : NORMAL;
    float4 position : SV_POSITION;
};

PSInput main(float3 position : POSITION,  float3 normal : NORMAL)
{
    PSInput result;
    float4 posWorld = mul(actorData.Model, float4(position, 1.0f));
    
    result.posWorld = posWorld.xyz;
    result.normal = mul((float3x3) actorData.Model, normal);
    result.position = mul(globalConstants.ViewProjection, posWorld);
    
    return result;
}