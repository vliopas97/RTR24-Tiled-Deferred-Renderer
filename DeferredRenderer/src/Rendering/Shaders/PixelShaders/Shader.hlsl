// pixel.hlsl
#include "..\Common.hlsli"
#include "core.hlsli"

struct PSInput
{
    float3 posWorld : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 texCoords : TEXCOORD;
    float4 position : SV_POSITION;
};

float4 main(float3 posView : POSITION, float3 normal : Normal, float3 tangent : TANGENT, float3 bitangent : BITANGENT,
float2 texCoords : TEXCOORD) : SV_TARGET
{
    normal = normalPreprocess(normal, tangent, bitangent, texCoords);
    
    float3 lightPos = mul((float3x3) globalConstants.View, Sun.Position);
    float3 lightDir = mul((float3x3) globalConstants.View, Sun.Direction);
    
    float3 diffuse = Sun.DiffuseColor * Sun.DiffuseIntensity * 1.0f * max(0.0f, dot(lightDir, normal));
    
    float3 specular = float3(0, 0, 0);
    if (actorData.KsID < 0)
        specular = calcSpecular(posView, lightDir, normal);
    else
        specular = getSpecularFromTexture(posView, lightDir, normal, texCoords);
    
    Texture2D<float4> tex = getTexture(actorData.KdID);
    
    return float4(saturate(diffuse + ambient) * tex.Sample(smplr, texCoords).rgb + attLin * specular, 1.0f);
}