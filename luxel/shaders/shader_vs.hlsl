
float4 vs_main(uint id : SV_VertexID) : SV_Position
{
    float2 positions[3] =
    {
        float2(0.0f, -0.5f),
        float2(0.5f, 0.5f),
        float2(-0.5f, 0.5f)
    };
    
    float4 output = float4(positions[id], 0.0f, 1.0f);

    return output;
}
