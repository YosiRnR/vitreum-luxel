// Pixel shader del Renderer2D: color de la textura * color del quad y, si el
// quad tiene estilo, la forma redondeada (SDF).

// Debe coincidir con Renderer2D::PushConstants (renderer2d.cpp) y con
// quad_vs.hlsl. Aquí solo se usan styles_slot y styles_offset, pero la struct
// tiene que ser la misma para que cada campo caiga en su byte.
struct PushConstants
{
    column_major float4x4 view_proj;
    uint instances_slot;
    uint instances_offset;
    float y_sign;
    uint styles_slot;       // hueco bindless del buffer con la tabla de estilos de este draw
    uint styles_offset;     // byte donde empieza
};

[[vk::push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
ByteAddressBuffer bindless_buffers[];

[[vk::binding(1, 0)]]
SamplerState bindless_samplers[];

[[vk::binding(2, 0)]]
Texture2D<float4> bindless_textures[];

// Tamaño de StyleRecord en bytes (static_assert en renderer2d.cpp).
static const uint k_style_size = 16;

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    nointerpolation uint texture_index : TEXCOORD1;
    nointerpolation uint sampler_index : TEXCOORD2;
    float2 local    : TEXCOORD3;                    // dónde está este píxel dentro del quad: (0,0) = centro
    nointerpolation float2 half_size : TEXCOORD4;   // la mitad del tamaño del quad
    nointerpolation uint style_index : TEXCOORD5;   // 0 = sin estilo
};

// ----------------------------------------------------------------------------
// SDF de un rectángulo redondeado centrado en (0,0).
//
//   p          el punto (respecto al centro del rectángulo)
//   half_size  la mitad del tamaño: un botón de 100x40 tiene (50, 20)
//   radius     el radio de las esquinas
//   devuelve   la distancia al borde: < 0 dentro, 0 en el borde, > 0 fuera
//
// Valores para comprobarla (half_size = (50, 20), radius = 8):
//   p = ( 0,  0) -> -20      centro: a 20 del borde de arriba (y del de abajo)
//   p = (50,  0) ->   0      en el borde derecho
//   p = (50, 20) ->  +3.31   el pico: fuera, se lo come la esquina redonda
//   p = (45, 15) ->  -3.76   cerca de la esquina, pero dentro
// ----------------------------------------------------------------------------
float sd_rounded_box(float2 p, float2 half_size, float radius)
{
    float2 q = abs(p) - half_size + radius;
    return length(max(q, 0)) + min(max(q.x, q.y), 0) - radius;
}

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

    float4 color = tex.Sample(smp, input.uv) * input.color;

    // Cuántas unidades del lote mide un píxel de pantalla. ddx/ddy dicen cuánto
    // cambia un valor al pasar al píxel de la derecha / de abajo. En la UI,
    // local.x aumenta 1 por píxel: ddx = 1, ddy = 0 -> 1 unidad por píxel. Con
    // el quad girado el cambio se reparte entre los dos, y length lo recompone.
    // Va ANTES del if: las derivadas comparan píxeles vecinos, y dentro de un
    // if unos vecinos podrían entrar y otros no.
    const float units_per_pixel = length(float2(ddx(input.local.x), ddy(input.local.x)));

    // Sprites, letras y quads normales acaban aquí: no leen la tabla.
    if (input.style_index == 0) {
        return color;
    }

    // --- Leer el estilo (mismo orden que StyleRecord en renderer2d.cpp) --------
    // Se lee aquí y no en el vertex shader porque es igual en todo el quad: no
    // tiene sentido pasarlo por los interpoladores. Los estilos empiezan en 1.
    // El slot no lleva NonUniformResourceIndex: viene de las push constants.
    ByteAddressBuffer styles = bindless_buffers[push_constants.styles_slot];
    const uint base = push_constants.styles_offset + (input.style_index - 1) * k_style_size;
    const float4 shape = asfloat(styles.Load4(base));   // x = radio, yzw = relleno

    // --- Forma ------------------------------------------------------------------
    const float d = sd_rounded_box(input.local, input.half_size, shape.x);

    // Cobertura: cuánto del píxel cae dentro, según la distancia de su centro al
    // borde medida en píxeles. A medio píxel hacia dentro, 1; justo en el borde,
    // 0.5; a medio píxel hacia fuera, 0. Esa rampa de un píxel es el suavizado.
    const float coverage = saturate(0.5f - d / units_per_pixel);
    color.a *= coverage;
    return color;
}
