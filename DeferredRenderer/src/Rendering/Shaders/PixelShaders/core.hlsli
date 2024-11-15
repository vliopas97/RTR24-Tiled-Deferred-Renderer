#include "..\Common.hlsli"

DirLightData Sun()
{
    return glLights[0];
}

static const float3 materialColor = { 0.7f, 0.7f, 0.9f };
static const float3 ambient = { 0.15f, 0.15f, 0.15f };
static const float3 diffuseColor = { 1.0f, 1.0f, 1.0f };
static const float diffuseIntensity = 1.0f;
static const float attConst = 1.0f;
static const float attLin = 0.05f;
static const float attQuad = 0.01f;

float3 calcSunlight(in float3 normal)
{
    const float3 diffuse = diffuseColor * diffuseIntensity * 1.0f * max(0.0f, dot(Sun().Direction, normal));
    
    return saturate((diffuse + ambient) * materialColor);
}