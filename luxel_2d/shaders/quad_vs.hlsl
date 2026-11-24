// Vertex shader del Renderer2D.
//
// No hay vertex buffer "de verdad": el C++ hace draw(6 * n) y cada invocación
// de este shader recibe solo su número de vértice (SV_VertexID = 0 .. 6n-1).
// Con ese número averigua a qué quad pertenece y qué esquina es, y lee los
// datos del quad de un buffer bindless (vertex pulling).
//
// Ejemplo con 3 quads (18 vértices): el vértice 13 es
//     quad   = 13 / 6 = 2   -> lee el tercer QuadInstance
//     vertex = 13 % 6 = 1   -> segundo vértice del primer triángulo de ese quad

// Debe coincidir con Renderer2D::PushConstants (renderer2d.cpp).
struct PushConstants
{
    // column_major: el C++ guarda la matriz por columnas (Matrix4x4 es
    // column-major), así que la leemos tal cual, sin transponer. Es el valor por
    // defecto de HLSL, pero se escribe para que quede claro que es a propósito.
    column_major float4x4 view_proj;
    uint instances_slot;    // hueco bindless del buffer donde están los quads
    uint instances_offset;  // byte donde empieza este lote dentro de ese buffer
    float y_sign;           // +1: la Y crece hacia arriba (mundo); -1: hacia abajo (UI en píxeles)
    uint styles_slot;       // tabla de estilos: solo la usa el pixel shader
    uint styles_offset;
};

[[vk::push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
ByteAddressBuffer bindless_buffers[];

// Tamaño de QuadInstance en bytes (static_assert en renderer2d.cpp).
static const uint k_instance_size = 64;

// Dos triángulos por quad: (0,1,2) y (0,2,3), con las esquinas numeradas así:
//
//     3 ---- 2        tal como se VEN: 3 y 2 arriba
//     |    / |
//     |  /   |
//     0 ---- 1
//
// La tabla dice qué esquina corresponde a cada uno de los 6 vértices.
static const uint k_corner_of_vertex[6] = { 0, 1, 2, 0, 2, 3 };

// Posición de cada esquina en un quad de lado 1 centrado en el origen.
static const float2 k_corner_position[4] =
{
    float2(-0.5f, -0.5f), // 0: abajo izquierda
    float2( 0.5f, -0.5f), // 1: abajo derecha
    float2( 0.5f,  0.5f), // 2: arriba derecha
    float2(-0.5f,  0.5f)  // 3: arriba izquierda
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    // nointerpolation: los índices son iguales en los 3 vértices del
    // triángulo, pero el rasterizador interpolaría cualquier cosa que le
    // pasemos. Con enteros además es obligatorio: no se pueden interpolar.
    nointerpolation uint texture_index : TEXCOORD1;
    nointerpolation uint sampler_index : TEXCOORD2;
    float2 local    : TEXCOORD3;                    // posición dentro del quad, sin girar: (0,0) = centro
    nointerpolation float2 half_size : TEXCOORD4;
    nointerpolation uint style_index : TEXCOORD5;   // 0 = sin estilo (ver style_table.h)
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    const uint quad_index = vertex_id / 6;
    const uint corner     = k_corner_of_vertex[vertex_id % 6];

    // --- Leer el QuadInstance (64 bytes = 4 bloques de 16) ------------------
    // Mismo orden de campos que la struct QuadInstance de renderer2d.cpp.
    ByteAddressBuffer instances = bindless_buffers[push_constants.instances_slot];
    const uint base = push_constants.instances_offset + quad_index * k_instance_size;

    const float4 position_size = asfloat(instances.Load4(base +  0)); // xy = position, zw = size
    const float4 uv_rect       = asfloat(instances.Load4(base + 16)); // xy = uv_min,   zw = uv_max
    const float4 color         = asfloat(instances.Load4(base + 32));
    const uint4  extra         =         instances.Load4(base + 48);  // x = rotation (bits de float), y = textura, z = sampler, w = estilo

    const float2 position = position_size.xy;
    const float2 size     = position_size.zw;
    const float  rotation = asfloat(extra.x);

    // --- Esquina en el mundo ------------------------------------------------
    // 1) Escalar la esquina unitaria al tamaño del quad.
    const float2 local = k_corner_position[corner] * size;

    // 2) Girarla alrededor del centro del quad. Matriz de rotación 2D
    //    antihoraria (con +Y arriba):  x' = c*x - s*y ;  y' = s*x + c*y
    float s, c;
    sincos(rotation, s, c);
    const float2 rotated = float2(c * local.x - s * local.y,
                                  s * local.x + c * local.y);

    // Hasta aquí todo se ha calculado "con +Y arriba", tal como se ve: la
    // esquina 3 es la de arriba a la izquierda y el giro es antihorario.
    //
    // 3) Pasar al espacio del lote y llevarla a la posición del quad. En la UI
    //    subir en pantalla es RESTAR Y, así que se cambia el signo de la Y.
    //    Ejemplo: botón de 100x40 centrado en (60, 30) píxeles. Su esquina 3 es
    //    local (-50, +20); con y_sign = -1 queda (-50, -20) y cae en (10, 10),
    //    arriba a la izquierda. Es lo único que cambia: la UV de abajo ni se entera.
    const float2 world = position + float2(rotated.x, rotated.y * push_constants.y_sign);

    // --- UV -----------------------------------------------------------------
    // La esquina local va de -0.5 a 0.5. Pasarla a [0,1] en cada eje, con la
    // V dada la vuelta: en luxel la UV (0,0) es ARRIBA a la izquierda, pero la
    // Y del mundo crece hacia arriba. Así la esquina 3 (arriba izquierda) toma
    // uv_min y la 1 (abajo derecha) toma uv_max.
    const float2 corner01 = float2(k_corner_position[corner].x + 0.5f,
                                   0.5f - k_corner_position[corner].y);
    const float2 uv = lerp(uv_rect.xy, uv_rect.zw, corner01);

    VSOutput output;
    output.position      = mul(push_constants.view_proj, float4(world, 0.0f, 1.0f));
    output.uv            = uv;
    output.color         = color;
    output.texture_index = extra.y;
    output.sampler_index = extra.z;
    output.local         = local;        // antes de girar: la SDF no tiene que saber nada del giro
    output.half_size     = size * 0.5f;
    output.style_index   = extra.w;
    return output;
}
