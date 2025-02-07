#define HLSL
#include "..\HLSLCompat.h"

Texture2D<float4> Positions : register(t0); // XYZ -> PositionsView.xyz A->Reflectiveness
Texture2D<float4> Normals : register(t1); // XYZ -> NormalsView.xyz A->Roughness (might not use rougness)
Texture2D<float4> PixelsColor : register(t2);

ConstantBuffer<PipelineConstants> glConstants[] : register(b0, space0);
static PipelineConstants globalConstants = glConstants[0];

SamplerState smplr : register(s0);

#define MAX_THICKNESS 0.05f
#define MAX_ITERATIONS 256

void computeReflection(in float3 position, in float3 normal, in uint2 screenDims, out float4 positionScreen, out float3 reflectionDirScreen, out float maxDistance)
{
    positionScreen = mul(globalConstants.Projection, float4(position, 1.0f));
    positionScreen /= positionScreen.w;
    float3 viewDirView = normalize(position);
    float4 reflectionView = float4(reflect(viewDirView, normal), 0.0f);

    float4 reflectionEndView = float4(position, 1.0f) + reflectionView * 1000.0f;
    reflectionEndView /= (reflectionEndView.z < 0) ? reflectionEndView.z : 1.0f;

    float4 reflectionEndScreen = mul(globalConstants.Projection, float4(reflectionEndView.xyz, 1.0f));
    reflectionEndScreen /= reflectionEndScreen.w;
    reflectionDirScreen = normalize(reflectionEndScreen.xyz - positionScreen.xyz);

    positionScreen.xy *= float2(0.5f, -0.5f);
    positionScreen.xy += float2(0.5f, 0.5f);

    reflectionDirScreen.xy *= float2(0.5f, -0.5f);

    maxDistance = reflectionDirScreen.x >= 0 ? (1 - positionScreen.x) / reflectionDirScreen.x : -positionScreen.x / reflectionDirScreen.x;
    maxDistance = min(maxDistance, reflectionDirScreen.y < 0 ? (-positionScreen.y / reflectionDirScreen.y) : ((1 - positionScreen.y) / reflectionDirScreen.y));
    maxDistance = min(maxDistance, reflectionDirScreen.z < 0 ? (-positionScreen.z / reflectionDirScreen.z) : ((1 - positionScreen.z) / reflectionDirScreen.z));
}

bool traceIntersection(in float3 positionScreen, in float3 reflectionDirScreen, in float maxDistance, in uint2 screenDims, out float3 intersection)
{
    float3 reflectionEndPosScreen = positionScreen + reflectionDirScreen * maxDistance;
    float3 dp = reflectionEndPosScreen - positionScreen;
    int2 sampleScreenPos = int2(positionScreen.xy * screenDims);
    int2 endScreenPos = int2(reflectionEndPosScreen.xy * screenDims);
    int2 dp2 = endScreenPos - sampleScreenPos;
    const int maxDist = max(abs(dp2.x), abs(dp2.y));
    dp /= maxDist;
    
    float4 rayPosScreen = float4(positionScreen + dp, 0.0f);
    float4 rayDirScreen = float4(dp.xyz, 0);
    float4 rayStartPos = rayPosScreen;

    int hitIndex = -1;
    for (int i = 0; i < maxDist && i < MAX_ITERATIONS; i+=4)
    {
        float depth0 = 0;
        float depth1 = 0;
        float depth2 = 0;
        float depth3 = 0;
        
        float4 rayPosScreen0 = rayPosScreen + rayDirScreen * 0;
        float4 rayPosScreen1 = rayPosScreen + rayDirScreen * 1;
        float4 rayPosScreen2 = rayPosScreen + rayDirScreen * 2;
        float4 rayPosScreen3 = rayPosScreen + rayDirScreen * 3;
        
        float4 sampleInScreenSpace0 = mul(globalConstants.Projection, float4(Positions.Sample(smplr, rayPosScreen0.xy).xyz, 1.0f));
        float4 sampleInScreenSpace1 = mul(globalConstants.Projection, float4(Positions.Sample(smplr, rayPosScreen1.xy).xyz, 1.0f));
        float4 sampleInScreenSpace2 = mul(globalConstants.Projection, float4(Positions.Sample(smplr, rayPosScreen2.xy).xyz, 1.0f));
        float4 sampleInScreenSpace3 = mul(globalConstants.Projection, float4(Positions.Sample(smplr, rayPosScreen3.xy).xyz, 1.0f));
        
        depth0 = (sampleInScreenSpace0 / sampleInScreenSpace0.w).z;
        depth1 = (sampleInScreenSpace1 / sampleInScreenSpace1.w).z;
        depth2 = (sampleInScreenSpace2 / sampleInScreenSpace2.w).z;
        depth3 = (sampleInScreenSpace3 / sampleInScreenSpace3.w).z;
        
        {
            float thickness = rayPosScreen0.z - depth0;
            if (thickness >= 0 && thickness < MAX_THICKNESS)
            {
                hitIndex = i + 0;
                break;
            }
        }
	    {
            float thickness = rayPosScreen0.z - depth1;
            if (thickness >= 0 && thickness < MAX_THICKNESS)
            {
                hitIndex = i + 1;
                break;
            }
        }
	    {
            float thickness = rayPosScreen0.z - depth2;
            if (thickness >= 0 && thickness < MAX_THICKNESS)
            {
                hitIndex = i + 2;
                break;
            }
        }
	    {
            float thickness = rayPosScreen0.z - depth3;
            if (thickness >= 0 && thickness < MAX_THICKNESS)
            {
                hitIndex = i + 3;
                break;
            }
        }
        
        rayPosScreen = rayPosScreen3 + rayDirScreen;
    }

    bool intersected = hitIndex >= 0;
    intersection = rayStartPos.xyz + rayDirScreen.xyz * hitIndex;
    
    return intersected;
}

float4 computeReflectedColor(bool intersects, float3 intersection, float4 originalColor)
{
    float4 reflectionColor = PixelsColor.Sample(smplr, intersection.xy);
   
    if (intersects)
        return reflectionColor;

    return originalColor;
}

float4 main(float4 position : SV_Position) : SV_Target
{
    float4 color = float4(1, 1, 1, 1);
    uint width, height, noMips;
    Normals.GetDimensions(0, width, height, noMips);
    
    uint2 screenDims = uint2(width, height);
    float2 texCoords = float2(position.x / width, position.y / height);
    
    float3 positionView = Positions.Sample(smplr, texCoords).rgb;
    float3 normalView = Normals.Sample(smplr, texCoords).rgb;
    float reflectiveness = Positions.Sample(smplr, texCoords).a;
    float4 originalColor = PixelsColor.Sample(smplr, texCoords);
    
    if (reflectiveness == 0 || length(normalView) == 0.0f || !globalConstants.SSREnabled)
        discard;
    
    float4 positionScreen = float4(0, 0, 0, 0);
    float3 reflectionScreen = float3(0, 0, 0);
    float maxDistance = 0;
    
    computeReflection(positionView, normalView, screenDims, positionScreen, reflectionScreen, maxDistance);
    
    float3 intersection = float3(0, 0, 0);
    bool intersects = traceIntersection(positionScreen.xyz, reflectionScreen, maxDistance, screenDims, intersection);
    
    float4 reflectionColor = computeReflectedColor(intersects, intersection, originalColor);
    reflectionColor.a = reflectiveness;
    //color = (1.0f - reflectiveness) * originalColor + reflectiveness * reflectionColor;
    
    return reflectionColor;;
}