#include "..\Common.hlsli"

void flipNormalForBackfaces(inout float3 normal, float4 posView)
{
    if (dot(normal, (float3) posView) >= 0.0f)
        normal *= -1.0f;
}

float3x3 calcTBNmatrix(in float3 normal, in float3 tangent, float3 bitangent)
{   
    // check if mesh is left handed or right to fix the normals being incorrect due to mirroring
    float handedness = (dot(cross(tangent, normal), bitangent) < 0.0f) ? -1.0f : 1.0f;
    
    float3x3 TBN = float3x3(handedness * normalize(tangent), normalize(bitangent), normalize(normal));
    TBN = transpose(TBN);
    return TBN;
}