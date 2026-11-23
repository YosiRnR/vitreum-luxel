// Pixel shader del Renderer2D: color de la textura * color del quad.
//
// No usa push constants: todo lo que necesita (índices de textura y sampler)
// le llega del vertex shader, porque cambia de un quad a otro dentro del
// mismo draw. Eso es lo que permite que un lote mezcle texturas.

[[vk::binding(1, 0)]]
SamplerState bindless_samplers[];

[[vk::binding(2, 0)]]
Texture2D<float4> bindless_textures[];

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    nointerpolation uint texture_index : TEXCOORD1;
    nointerpolation uint sampler_index : TEXCOORD2;
};

float4 ps_main(PSInput input) : SV_Target
{
    // NonUniformResourceIndex es OBLIGATORIO aquí y no un adorno: la GPU
    // ejecuta los píxeles en grupos (32 o 64 a la vez) y, en un mismo grupo,
    // unos píxeles pueden ser de un quad con una textura y otros de otro quad
    // con otra. Sin esta marca el compilador puede suponer que todos usan el
    // mismo índice y leer la textura equivocada (fallos raros, que dependen de
    // la GPU). En el shader de test_1 el índice venía de push constants, que sí
    // es igual para todo el draw.
    Texture2D<float4> tex = bindless_textures[NonUniformResourceIndex(input.texture_index)];
    SamplerState smp      = bindless_samplers[NonUniformResourceIndex(input.sampler_index)];

    return tex.Sample(smp, input.uv) * input.color;
}
