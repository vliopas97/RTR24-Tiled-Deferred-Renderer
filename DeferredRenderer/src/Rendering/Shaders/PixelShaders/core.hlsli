#include "..\Common.hlsli"

static DirLightData Sun = glLights[0];

static const float3 ambient = { 0.15f, 0.15f, 0.15f };
static const float3 diffuseColor = { 1.0f, 1.0f, 1.0f };
static const float diffuseIntensity = 1.0f;
static const float attConst = 1.0f;
static const float attLin = 0.05f;
static const float attQuad = 0.01f;

float3 calcSunlight(in float3 posWorld, in float3 normal)
{
    normal = normalize(normal);
    float3 lightPos = mul((float3x3) globalConstants.View, Sun.Position);
    float3 lightDir = mul((float3x3) globalConstants.View, Sun.Direction);
    
    const float3 diffuse = Sun.DiffuseColor * Sun.DiffuseIntensity * 1.0f * max(0.0f, dot(lightDir, normal));
    
    float3 viewDir = normalize(posWorld);
    float3 reflectDir = reflect(-lightDir, viewDir);
    float specular = dot(viewDir, reflectDir);
    specular = clamp(specular, 0, 1.0f);
    specular = dot(lightDir, normal) != 0.0 ? specular : 0.0;
    specular = pow(specular, actorData.Material.Shininess);
    
    return saturate((diffuse + float3(specular, specular, specular) + Sun.Ambient) * actorData.Material.MatericalColor);
}