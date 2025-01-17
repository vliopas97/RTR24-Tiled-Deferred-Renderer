#define HLSL
#include "..\HLSLCompat.h"

Texture2D SceneTexture : register(t0);
SamplerState smplr : register(s0);
ConstantBuffer<BlurPassControls> Controls : register(b0);

float4 main(float2 texCoords : TEXCOORD) : SV_Target
{
    const float offset[5] = { 0.0, 1.0, 2.0, 3.0, 4.0 };
    const float weight[5] =
    {
        0.2270270270, 0.1945945946, 0.1216216216,
        0.0540540541, 0.0162162162
    };
    
    float2 dir = Controls.IsHorizontal ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);

    float4 output = SceneTexture.Sample(smplr, texCoords) * weight[0];
    float3 FragmentColor = float3(0.0, 0.0, 0.0);

    for (int i = 1; i < 5; i++)
    {
        FragmentColor += SceneTexture.Sample(smplr,
                        texCoords + float2(dir.x * offset[i], dir.y * offset[i])).rgb * weight[i];
        FragmentColor += SceneTexture.Sample(smplr,
                        texCoords - float2(dir.x * offset[i], dir.y * offset[i])).rgb * weight[i];
    }

    output.rgb += FragmentColor;

    return float4(output.rgb, 1.0);
    
    //float accAlpha = 0.0f;
    //float3 maxColor = float3(0.0f, 0.0f, 0.0f);
    
    //for (int i = 0; i < 5; i++)
    //{
    //    const float2 tc = texCoords + float2(dir.x * i, dir.y * i);
    //    const float4 s = SceneTexture.Sample(smplr, tc).rgba;
    //    const float coef = coefficients[i];
    //    accAlpha += s.a * coef;
    //    maxColor = max(s.rgb, maxColor);
    //}
    //return float4(maxColor, accAlpha);
}
