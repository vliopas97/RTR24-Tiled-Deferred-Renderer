#define HLSL
#include "..\HLSLCompat.h"

Texture2D<float> Depth : register(t0);
Texture2D<float3> RandomTexture : register(t1);
Texture2D<float4> Normals : register(t2); // From GBuffer Pass
StructuredBuffer<float3> Offsets : register(t3);

SamplerState smplr : register(s0);

PipelineConstants globalConstants : register(b0);

struct PSInput
{
    float4 positionClip : positionClip;
    float2 texCoords : texCoord;
    float4 positionViewport : SV_Position;
};

float3 DecodeSphereMap(float2 e)
{
    float2 tmp = e - e * e;
    float f = tmp.x + tmp.y;
    float m = sqrt(4.0f * f - 1.0f);
    
    float3 n;
    n.xy = m * (e * 4.0f - 2.0f);
    n.z = 3.0f - 8.0f * f;
    return n;
}

float3 ComputePositionViewFromZ(uint2 screenSize, uint2 coords, float zbuffer)
{
    mat4x4 projectionMatrix = globalConstants.Projection;
    float2 screenPixelOffset = float2(2.0f, -2.0f) / float2(screenSize.x, screenSize.y);
    float2 positionScreen = (float2(coords.xy) + 0.5f) * screenPixelOffset.xy + float2(-1.0f, 1.0f);
    float viewSpaceZ = projectionMatrix._43 / (zbuffer - projectionMatrix._33);
	
	
    float2 screenSpaceRay = float2(positionScreen.x / projectionMatrix._11, positionScreen.y / projectionMatrix._22);
    float3 positionView;
    positionView.z = viewSpaceZ;
    positionView.xy = screenSpaceRay.xy * positionView.z;
    
    return positionView;
}

float4 main(float4 position : SV_Position) : SV_TARGET
{
    uint width, height, noMips;
    Normals.GetDimensions(0, width, height, noMips);
    uint2 screenSize = uint2(width, height);
    
    float2 texCoords = float2(position.x / width, position.y / height);

    float centerZBuffer = Depth.Sample(smplr, texCoords).r;
    float3 centerDepthPos = ComputePositionViewFromZ(screenSize, uint2(position.xy), centerZBuffer);
    float3 normal = DecodeSphereMap(Normals.Sample(smplr, texCoords).xy);
    
    float2 noiseScale = float2(width / 8.0f, height / 8.0f);
    
    float3 randomVector = RandomTexture.Sample(smplr, texCoords * noiseScale).xyz;
    float3 tangent = normalize(randomVector - normal * dot(randomVector, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 transformMat = float3x3(tangent, bitangent, normal);
    
    float occlusion = 0.0f;
    float radius = 0.3f;
    float power = 1.0f;
    
    for (int i = 0; i < 16; i++)
    {
        float3 samplePosition = mul(transformMat, Offsets[i]); // swap?
        samplePosition = samplePosition * radius + centerDepthPos;
        
        float3 sampleDirection = normalize(samplePosition - centerDepthPos);
        float4 offset = mul(globalConstants.Projection, float4(samplePosition, 1.0f)); // swap?
        offset.xy /= offset.w;
        
        float sampleDepth = Depth.Sample(smplr, float2(offset.x * 0.5f + 0.5f, -offset.y * 0.5f + 0.5f)).r;
        sampleDepth = ComputePositionViewFromZ(screenSize, offset.xy, sampleDepth).z;
        
        float rangeCheck = smoothstep(0.0f, 1.0f, radius / abs(centerDepthPos.z - sampleDepth));
        occlusion += rangeCheck * step(sampleDepth, samplePosition.z) * max(dot(normal, sampleDirection), 0.0f);
    }
    
    occlusion = 1.0f - (occlusion / 16); // kernel size
    float occlusionOut = pow(occlusion, power);
    
    return float4(float3(occlusionOut, occlusionOut, occlusionOut), 1.0f);
}