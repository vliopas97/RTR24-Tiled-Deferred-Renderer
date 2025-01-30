#define HLSL
#include "..\HLSLCompat.h"

Texture2D<float4> Positions : register(t0);
Texture2D<float3> RandomTexture : register(t1);
Texture2D<float4> Normals : register(t2); // From GBuffer Pass
StructuredBuffer<float3> Offsets : register(t3);

SamplerState smplr : register(s0);

PipelineConstants globalConstants : register(b0);

float4 main(float4 position : SV_Position) : SV_TARGET
{
    uint width, height, noMips;
    Normals.GetDimensions(0, width, height, noMips);
    uint2 screenSize = uint2(width, height);
    
    float2 texCoords = float2(position.x / width, position.y / height);

    float3 centerDepthPos = Positions.Sample(smplr, texCoords).xyz;
    float3 normal = Normals.Sample(smplr, texCoords).xyz;
    float2 noiseScale = float2(width / 8.0f, height / 8.0f);
    
    float3 randomVector = RandomTexture.Sample(smplr, texCoords * noiseScale).xyz;
    
    float3 tangent = normalize(randomVector - normal * dot(randomVector, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);
    
    float occlusion = 0.0f;
    float radius = globalConstants.RadiusSSAO;
    float intensity = globalConstants.IntensitySSAO;
    
    for (int i = 0; i < 16; i++)
    {
        float3 samplePosition = mul(transpose(TBN), Offsets[i]);
        samplePosition = samplePosition * radius + centerDepthPos;
        
        float4 offset = mul(globalConstants.Projection, float4(samplePosition, 1.0f));
        offset.xy /= offset.w;

        float sampleDepth = Positions.Sample(smplr, float2(offset.x * 0.5f + 0.5f, -offset.y * 0.5f + 0.5f)).z;

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(samplePosition.z - sampleDepth));
        occlusion += rangeCheck * step(sampleDepth +1e-4, samplePosition.z);
    }
    
    occlusion = 1.0f - (occlusion / 16.0f); // kernel size
    float occlusionOut = pow(occlusion, intensity);
    return float4(float3(occlusionOut, occlusionOut, occlusionOut), 1.0f);
}