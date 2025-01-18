#define HLSL
#include "..\HLSLCompat.h"

ConstantBuffer<PipelineConstants> glConstants[] : register(b0, space0);
ConstantBuffer<DirLightData> glLights[] : register(b0, space100);

SamplerState smplr : register(s0);
static PipelineConstants globalConstants = glConstants[0];
static DirLightData Sun = glLights[0];

// attenuation constants for sunlight
static const float attConst = 1.0f;
static const float attLin = 0.05f;
static const float attQuad = 0.01f;

Texture2D<float4> Positions : register(t0);
Texture2D<float4> Normals : register(t1);
Texture2D<float4> Diffuse : register(t2);
Texture2D<float4> Specular : register(t3);
Texture2D<float4> AmbientOcclusion : register(t4);

float3 calcSpecular(in float3 posView, in float3 lightDir, in float3 normal, in float shininess)
{
    float3 viewDir = normalize(posView);
    float3 reflectDir = reflect(lightDir, viewDir);
    float specular = dot(viewDir, reflectDir);
    specular = clamp(specular, 0, 1.0f);
    specular = dot(lightDir, viewDir) >= 0.0f ? specular : 0.0f;
    specular = pow(specular, shininess);
    
    return specular;
}

float3 getSpecularFromTexture(in float3 posView, in float3 lightDir, in float3 normal, in float4 spec)
{
    float3 specColor = spec.rgb;
    float specPower = spec.a;
    
    float3 r = reflect(lightDir, normal);
    float3 viewDir = normalize(posView);
    float3 specular = pow(max(0.0f, dot(normalize(r), normalize(viewDir))), specPower) * specColor;
    return specular;
}

float4 main(float4 position : SV_Position) : SV_TARGET
{
    uint width, height, noMips;
    Normals.GetDimensions(0, width, height, noMips);
    
    float2 texCoords = float2(position.x / width, position.y / height);
    
    float3 normal = Normals.Sample(smplr, texCoords).rgb;
    if (length(normal) == 0.0f)
        discard;
    
    float4 posView = Positions.Sample(smplr, texCoords).rgba;
    float4 matDiffuse = Diffuse.Sample(smplr, texCoords).rgba;
    float4 matSpecular = Specular.Sample(smplr, texCoords).rgba;
    
    float3 lightPos = mul(globalConstants.View, float4(Sun.Position, 1.0f)).xyz;
    float3 lightDir = mul(globalConstants.View, float4(Sun.Direction, 0.0f)).xyz;
    
    float3 diffuse = Sun.DiffuseColor * Sun.DiffuseIntensity * 1.0f * max(0.0f, dot(lightDir, normal));
    float3 specular = length(matSpecular.xyz) == 0.0f ? calcSpecular(posView.xyz, lightDir, normal, matSpecular.a) 
    : getSpecularFromTexture(posView.xyz, lightDir, normal, matSpecular);
    
    float4 occlusion = AmbientOcclusion.Sample(smplr, texCoords);
    
    if (globalConstants.SSAOEnabled)
        return float4(saturate(diffuse + Sun.Ambient) * matDiffuse.rgb + attLin * specular, 1.0f) * occlusion;
    
    return float4(saturate(diffuse + Sun.Ambient) * matDiffuse.rgb + attLin * specular, 1.0f);
}