struct PushConstants
{
    uint vertex_buffer_slot;
    uint transform_matrix_buffer_slot;
    uint transform_matrix_buffer_offset;
    
    uint texture_slot;
    uint sampler_slot;
};

[[vk::binding(1, 0)]]
SamplerState bindless_samplers[];

[[vk::binding(2, 0)]]
Texture2D<float4> bindless_textures[];

[[vk::push_constant]]
PushConstants push_constants;

float4 ps_main(float2 uv : TEXCOORD0) : SV_Target
{
    Texture2D<float4> texture = bindless_textures[NonUniformResourceIndex(push_constants.texture_slot)];

    SamplerState texture_sampler = bindless_samplers[NonUniformResourceIndex(push_constants.sampler_slot)];
    
    return texture.Sample(texture_sampler, uv);
}
