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
static const uint k_style_size = 48;

// Qué partes del estilo hay que aplicar (mismos valores que en renderer2d.cpp).
static const uint k_style_shape = 1;   // forma redondeada y borde (draw_rounded_rect)
static const uint k_style_clip  = 2;   // recorte (push_clip_rect)

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
    float2 world    : TEXCOORD6;                    // dónde está este píxel en unidades del lote
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

// Pone "top" encima de "bottom" (los dos con alfa, sin premultiplicar).
//
// Es lo mismo que hace el blending de la GPU con lo que ya hay en pantalla, pero
// aquí entre el borde y el relleno, y sin saber aún qué hay detrás. Cada color
// pesa lo que se ve de él: top, su alfa; bottom, lo que top deja ver del suyo.
//
// Escenario: borde blanco a la mitad (alfa 0.5) sobre un relleno "negro, alfa 0":
//   mezclando sin pesos:  (blanco + negro) / 2 = gris       <- halo sucio
//   con pesos:            (1*0.5 + 0*0) / 0.5  = blanco, con alfa 0.5   <- bien
// Un relleno que no se ve no puede ensuciar, sea cual sea su RGB.
float4 blend_over(float4 top, float4 bottom)
{
    const float bottom_weight = bottom.a * (1.0f - top.a);
    const float alpha = top.a + bottom_weight;
    // max: si los dos son transparentes, alpha es 0 y no se puede dividir (el
    // color da igual: no se va a ver).
    const float3 rgb = (top.rgb * top.a + bottom.rgb * bottom_weight) / max(alpha, 0.0001f);
    return float4(rgb, alpha);
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
    const uint4  block0       = styles.Load4(base);                 // x = radio, y = grosor (bits de float), z = flags
    const float4 border_color = asfloat(styles.Load4(base + 16));
    const float4 clip         = asfloat(styles.Load4(base + 32));   // xy = min, zw = max

    const float corner_radius    = asfloat(block0.x);
    const float border_thickness = asfloat(block0.y);
    const uint  flags            = block0.z;

    if (flags & k_style_shape)
    {
        // --- Forma --------------------------------------------------------------
        const float d = sd_rounded_box(input.local, input.half_size, corner_radius);

        // Cobertura: cuánto del píxel cae dentro, según la distancia de su centro
        // al borde medida en píxeles. A medio píxel hacia dentro, 1; justo en el
        // borde, 0.5; a medio píxel hacia fuera, 0. Esa rampa de un píxel es el
        // suavizado.
        const float coverage = saturate(0.5f - d / units_per_pixel);

        // --- Borde --------------------------------------------------------------
        // El borde es la franja entre d = -grosor y d = 0: crece hacia DENTRO, así
        // que el botón sigue midiendo lo mismo. "in_border" es la misma rampa de
        // antes, pero colocada en el canto interior (d = -grosor) y al revés: 0 en
        // el relleno, 1 en la franja. Con un borde de 2 px:
        //     d = -5   -> saturate(0.5 - 3)   = 0     relleno
        //     d = -2   -> saturate(0.5 + 0)   = 0.5   justo en el canto interior
        //     d = -1   -> saturate(0.5 + 1)   = 1     borde
        // Sin borde (grosor 0) el C++ manda border_color con alfa 0 y blend_over
        // devuelve el relleno tal cual: no hace falta un if.
        const float in_border = saturate(0.5f + (d + border_thickness) / units_per_pixel);
        color = blend_over(float4(border_color.rgb, border_color.a * in_border), color);

        // La forma va después del borde: vale para el relleno y para el borde.
        color.a *= coverage;
    }

    if (flags & k_style_clip)
    {
        // --- Recorte ------------------------------------------------------------
        // Cuánto se sale el píxel por cada lado, como en la SDF: negativo dentro.
        // Por la izquierda o arriba, min - p; por la derecha o abajo, p - max; y
        // manda el lado por el que más se sale. Ejemplo: recorte de x = 100 a 300;
        // en x = 310 -> max(100 - 310, 310 - 300) = +10, fuera.
        // La rampa es la misma: en un recorte a píxeles enteros el corte es nítido
        // y en uno a medio píxel queda suavizado.
        const float2 outside = max(clip.xy - input.world, input.world - clip.zw);
        color.a *= saturate(0.5f - max(outside.x, outside.y) / units_per_pixel);
    }

    return color;
}
