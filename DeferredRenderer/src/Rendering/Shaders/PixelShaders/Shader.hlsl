// pixel.hlsl
#include "..\Common.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORDS;
};

SamplerState smplr : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    return getTexture(actorData.TextureID).Sample(smplr, input.tex);
    //return float4(1, 0, 0, 1);
}