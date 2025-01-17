struct PSInput
{
    float4 positionClip : positionClip;
    float2 texCoords : texCoord;
    float4 positionViewport : SV_Position;
};

PSInput main(uint vertexID : SV_VertexID)
{
    PSInput output;

    output.texCoords = float2((vertexID << 1) & 2, vertexID & 2);
    output.positionClip = float4(output.texCoords * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.positionViewport = output.positionClip;
    
    return output;
}