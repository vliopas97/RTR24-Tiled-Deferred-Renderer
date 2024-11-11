#define HLSL
#include "HLSLCompat.h"

ConstantBuffer<PipelineConstants> glConstants[] : register(b0, space0);
ConstantBuffer<ActorData> actorData : register(b0, space200);

static PipelineConstants globalConstants = glConstants[0];