#include "..\Common.hlsli"

struct PSInput
{
    float3 posWorld : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 texCoords : TEXCOORD;
    float4 position : SV_POSITION;
};

PSInput main(float3 position : POSITION,  float3 normal : NORMAL, float3 tangent : TANGENT, float3 bitangent : BITANGENT, 
float2 texCoords : TEXCOORD)
{
    PSInput result;
    float4x4 modelView = mul(globalConstants.View, actorData.Model);
    float4 posView = mul(modelView, float4(position, 1.0f));
    
    result.posWorld = posView.xyz;
    result.normal = mul((float3x3) modelView, normal);
    result.tangent = mul((float3x3) modelView, tangent);
    result.bitangent = mul((float3x3) modelView, bitangent);
    result.position = mul(globalConstants.ViewProjection, mul(actorData.Model, float4(position, 1.0f)));
    result.texCoords = texCoords;
    
    return result;
}