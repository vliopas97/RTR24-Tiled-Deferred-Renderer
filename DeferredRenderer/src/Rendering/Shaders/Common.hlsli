#define HLSL
#include "HLSLCompat.h"

Texture2D<float4> Textures[] : register(t0, space0);

ConstantBuffer<PipelineConstants> glConstants[] : register(b0, space0);
ConstantBuffer<ActorData> actorData : register(b0, space200);

static PipelineConstants globalConstants = glConstants[0];

Texture2D<float4> getTexture(uint texID)
{
    return Textures[0 + texID];
}