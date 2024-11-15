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
    float4x4 modelView = mul(globalConstants.View, actorData.Model);
    float4 posWorld = mul(modelView, float4(position, 1.0f));
    
    result.posWorld = posWorld.xyz;
    result.normal = mul((float3x3) modelView, normal);
    result.position = mul(globalConstants.ViewProjection, mul(actorData.Model, float4(position, 1.0f)));
    
    return result;
}