#include "..\Common.hlsli"

SamplerState smplr : register(s0);

static DirLightData Sun = glLights[0];

static const float3 ambient = { 0.15f, 0.15f, 0.15f };
static const float3 diffuseColor = { 1.0f, 1.0f, 1.0f };
static const float diffuseIntensity = 1.0f;
static const float attConst = 1.0f;
static const float attLin = 0.05f;
static const float attQuad = 0.01f;

float3 normalPreprocess(float3 n, float3 t, float3 b, float2 texCoords)
{
    int ID = actorData.KnID;    
    
    if (ID < 0)
        return normalize(n); // if no normal map available
    
    float3x3 TBN = float3x3(normalize(t), normalize(b), normalize(n));
    Texture2D<float4> normalMap = getTexture(ID);
    const float3 normalSample = normalMap.Sample(smplr, texCoords).xyz;
    n.x = normalSample.x * 2.0f - 1.0f;
    n.y = normalSample.y * 2.0f - 1.0f;
    n.z = normalSample.z * 2.0f - 1.0f;
    n = -1.0f * mul(transpose(TBN), n);
    return n;
}

float3 calcSpecular(in float3 posView, in float3 lightDir, in float3 normal)
{
    float3 viewDir = normalize(posView);
    float3 reflectDir = reflect(lightDir, viewDir);
    float specular = dot(viewDir, reflectDir);
    specular = clamp(specular, 0, 1.0f);
    specular = dot(lightDir, viewDir) >= 0.0f ? specular : 0.0f;
    specular = pow(specular, actorData.Material.Shininess);
    
    return specular;
}

float3 getSpecularFromTexture(in float3 posView, in float3 lightDir, in float3 normal, in float2 texCoords)
{
    int ID = actorData.KsID;
    Texture2D<float4> specTexture = getTexture(ID);

    float4 sample = specTexture.Sample(smplr, texCoords);
    float3 specColor = sample.rgb;
    float specPower = sample.a;
    
    float3 r = reflect(lightDir, normal);
    float3 viewDir = normalize(posView);
    float3 specular = pow(max(0.0f, dot(normalize(r), normalize(viewDir))), specPower) * specColor;
    return specular;
}