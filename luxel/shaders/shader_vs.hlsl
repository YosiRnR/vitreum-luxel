struct PushConstants
{
    uint vertex_buffer_slot;
    uint transform_matrix_buffer_slot;
    uint transform_matrix_buffer_offset;
};

[[vk::push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
ByteAddressBuffer bindless_buffers[];


float4 vs_main(uint id : SV_VertexID) : SV_Position
{
    ByteAddressBuffer vertex_buffer = bindless_buffers[push_constants.vertex_buffer_slot];
    
    float2 position = asfloat(vertex_buffer.Load2(id * 8));
    
    ByteAddressBuffer transform_matrix_buffer = bindless_buffers[push_constants.transform_matrix_buffer_slot];
    
    uint offset = push_constants.transform_matrix_buffer_offset;
    
    float4 row0 = asfloat(transform_matrix_buffer.Load4(offset + 0));
    float4 row1 = asfloat(transform_matrix_buffer.Load4(offset + 16));
    float4 row2 = asfloat(transform_matrix_buffer.Load4(offset + 32));
    float4 row3 = asfloat(transform_matrix_buffer.Load4(offset + 48));
    
    matrix mat = transpose(matrix(row0, row1, row2, row3));
    
    float4 output = mul(mat, float4(position, 0.0f, 1.0f));

    return output;
}
