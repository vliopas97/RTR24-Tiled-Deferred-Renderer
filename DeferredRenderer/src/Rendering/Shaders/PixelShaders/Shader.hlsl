// pixel.hlsl
#define HLSL
#include "..\Common.hlsli"
#include "core.hlsli"

struct PSInput
{
    float3 posWorld : POSITION;
    float3 normal : NORMAL;
    float3x3 TBN : TBN;
    float2 texCoords : TEXCOORD;
    float4 position : SV_POSITION;
};

float4 main(float3 posView : POSITION, float3 normal : Normal, float3x3 TBN : TBN, float2 texCoords : TEXCOORD) : SV_TARGET
{
    Texture2D<float4> tex = getTexture(actorData.KdID);
    float4 texSample = tex.Sample(smplr, texCoords);
    
    if (texSample.a < 0.1f)
        discard;
    
    normal = normalPreprocess(normal, TBN, texCoords);
    
    float3 lightPos = mul((float3x3) globalConstants.View, Sun.Position);
    float3 lightDir = mul((float3x3) globalConstants.View, Sun.Direction);
    
    float3 diffuse = Sun.DiffuseColor * Sun.DiffuseIntensity * 1.0f * max(0.0f, dot(lightDir, normal));
    
    float3 specular = (actorData.KsID < 0) ? calcSpecular(posView, lightDir, normal) : getSpecularFromTexture(posView, lightDir, normal, texCoords);
    
    return float4(saturate(diffuse + Sun.Ambient) * texSample.rgb + attLin * specular, 1.0f);
}