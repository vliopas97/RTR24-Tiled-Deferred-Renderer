#define HLSL
#include "HLSLCompat.h"

ConstantBuffer<PipelineConstants> glConstHeap[] : register(b0);

static PipelineConstants globalConstants = glConstHeap[0];