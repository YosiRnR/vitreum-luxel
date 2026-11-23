struct PushConstants
{
    uint vertex_buffer_slot;
    uint transform_matrix_buffer_slot;
    uint transform_matrix_buffer_offset;
    
    uint texture_slot;
    uint sampler_slot;
};

[[vk::push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
ByteAddressBuffer bindless_buffers[];

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput vs_main(uint id : SV_VertexID)
{
    ByteAddressBuffer vertex_buffer = bindless_buffers[push_constants.vertex_buffer_slot];
    
    float2 position = asfloat(vertex_buffer.Load2(id * 16));
    float2 uv = asfloat(vertex_buffer.Load2(id * 16 + 8));  // ¿como se pasan al pixel/fragment shader?
    
    ByteAddressBuffer transform_matrix_buffer = bindless_buffers[push_constants.transform_matrix_buffer_slot];
    
    uint offset = push_constants.transform_matrix_buffer_offset;
    
    float4 row0 = asfloat(transform_matrix_buffer.Load4(offset + 0));
    float4 row1 = asfloat(transform_matrix_buffer.Load4(offset + 16));
    float4 row2 = asfloat(transform_matrix_buffer.Load4(offset + 32));
    float4 row3 = asfloat(transform_matrix_buffer.Load4(offset + 48));
    
    matrix mat = transpose(matrix(row0, row1, row2, row3));
    
    float4 xform_pos = mul(mat, float4(position, 0.0f, 1.0f));

    VSOutput output;
    output.position = xform_pos;
    output.uv = uv;
    return output;
}
