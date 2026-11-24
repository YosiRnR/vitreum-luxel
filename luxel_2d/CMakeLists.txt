cmake_minimum_required(VERSION 3.21)

project(luxel_2d LANGUAGES CXX)

# luxel_2d: renderers 2D por encima del RHI (docs/renderers-design.md).
#
# Es una biblioteca APARTE de luxel a propósito: enlaza con luxel igual que lo
# haría una aplicación, así que solo ve su API pública (luxel/include). Si algún
# día alguien escribe aquí #include "backends/vulkan/..." el compilador no lo
# encontrará: la regla "los renderers no ven Vulkan" la vigila el compilador.
#
# En Visual Studio sería otro proyecto .vcxproj de tipo "Biblioteca estática"
# dentro de la misma solución, con una referencia al proyecto luxel.
add_library(luxel_2d STATIC
    "include/luxel/renderer2d.h"
    "src/renderer2d.cpp"
    "include/luxel/bitmap_font.h"
    "src/bitmap_font.cpp"
    "src/batch_order.h"
    "src/ui_space.h"
    "src/utf8.h"
)

# Shaders del Renderer2D, incrustados como cabeceras (quad_vs_spv.h y
# quad_ps_spv.h). Quedan en una carpeta PRIVADA de luxel_2d: quien use el
# Renderer2D no los ve ni tiene que saber que existen.
luxel_embed_shaders(luxel_2d
    shaders/quad_vs.hlsl
    shaders/quad_ps.hlsl
)

# include/ es PUBLIC: quien enlace con luxel_2d puede hacer
# #include <luxel/renderer2d.h>. (Comparte el prefijo "luxel/" con las cabeceras
# del RHI; son carpetas distintas en disco pero se incluyen igual.)
target_include_directories(luxel_2d
    PUBLIC
        include
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# PUBLIC (no PRIVATE) porque renderer2d.h incluye cabeceras de luxel
# (device.h, math...): quien incluya renderer2d.h también necesita verlas.
# Con PUBLIC, enlazar con luxel_2d arrastra luxel automáticamente.
target_link_libraries(luxel_2d PUBLIC luxel)

target_compile_features(luxel_2d PUBLIC cxx_std_20)

set_target_properties(luxel_2d PROPERTIES
    CXX_EXTENSIONS OFF
)
