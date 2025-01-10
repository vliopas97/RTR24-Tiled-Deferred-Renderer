#define HLSL
#include "..\HLSLCompat.h"

Texture2D<float4> Textures[] : register(t0, space0);
ConstantBuffer<PipelineConstants> glConstants[] : register(b0, space0);
ConstantBuffer<ActorData> actorData : register(b0, space200);

static PipelineConstants globalConstants = glConstants[0];
SamplerState smplr : register(s0);
    
Texture2D<float4> getTexture(uint texID)
{
    return Textures[0 + texID];
}

float3 normalPreprocess(float3 n, float3x3 TBN, float2 texCoords)
{
    int ID = actorData.KnID;
    
    if (ID < 0)
        return normalize(-1.0f * n); // if no normal map available
    
    Texture2D<float4> normalMap = getTexture(ID);
    const float3 normalSample = normalMap.Sample(smplr, texCoords).xyz;
    
    n = 2.0f * normalSample.xyz - 1.0f;
    n.xz = -n.xz;
    n = mul(TBN, n);
    
    return normalize(n);
}

struct PSOutput
{
    float4 Positions : SV_Target0;
    float4 Normals : SV_Target1;
    float4 Diffuse : SV_Target2;
    float4 Specular : SV_Target3;
};

PSOutput main(float3 posView : POSITION, float3 normal : Normal, float3x3 TBN : TBN, float2 texCoords : TEXCOORD)
{
    PSOutput output;
    
    Texture2D<float4> tex = getTexture(actorData.KdID);
    float4 texSample = tex.Sample(smplr, texCoords);
    
    if (texSample.a < 0.1f)
        discard;
    
    normal = normalPreprocess(normal, TBN, texCoords);
    
    output.Positions = float4(posView, 1.0f);
    output.Normals = float4(normal, 1.0f);
    output.Diffuse = texSample;
    output.Specular = (actorData.KsID < 0) ? float4(0, 0, 0, actorData.Material.Shininess) : getTexture(actorData.KsID).Sample(smplr, texCoords);
    
    return output;
}