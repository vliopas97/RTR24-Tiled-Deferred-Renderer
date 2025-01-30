#include "core.hlsli"

struct PSInput
{
    float3 posView : POSITION;
    float3 normal : NORMAL;
    float3x3 TBN : TBN;
    float2 texCoords : TEXCOORD;
    float4 position : SV_POSITION;
};

PSInput main(float3 position : POSITION,  float3 normal : NORMAL, float3 tangent : TANGENT, float3 bitangent : BITANGENT, 
float2 texCoords : TEXCOORD)
{
    PSInput result;
    float4x4 modelView = actorData.ModelView;
    float4 posView = mul(modelView, float4(position, 1.0f));
    
    result.posView = posView.xyz;
    
    normal = mul((float3x3) modelView, normal);
    tangent = mul((float3x3) modelView, tangent);
    bitangent = mul((float3x3) modelView, bitangent);
    
    normal = normalize(normal);
    tangent = normalize(tangent);
    bitangent = normalize(bitangent);
    
    result.normal = normal;
    
    result.TBN = calcTBNmatrix(normal, tangent, bitangent);
    result.texCoords = texCoords;
    result.position = mul(globalConstants.ViewProjection, mul(actorData.Model, float4(position, 1.0f)));
    return result;
}