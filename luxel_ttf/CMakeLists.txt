cmake_minimum_required(VERSION 3.21)

project(luxel_ttf LANGUAGES CXX)

# luxel_ttf: lector y rasterizador de fuentes TrueType hecho a mano
# (docs/ttf-design.md). Solo usa la biblioteca estándar: no enlaza con luxel ni con
# luxel_2d. Recibe los bytes del .ttf (no lee ficheros) y devuelve medidas, contornos
# y, más adelante, el atlas que se le da a BitmapFont.
add_library(luxel_ttf STATIC
    "include/luxel/ttf/font.h"
    "include/luxel/ttf/outline.h"
    "include/luxel/ttf/raster.h"
    "src/reader.h"
    "src/tables.h"
    "src/tables.cpp"
    "src/cmap.h"
    "src/cmap.cpp"
    "src/cmap_format4.cpp"
    "src/glyf.h"
    "src/glyf.cpp"
    "src/glyf_component.cpp"
    "src/outline_builder.h"
    "src/outline_builder.cpp"
    "src/rasterizer.h"
    "src/rasterizer.cpp"
    "src/coverage_piece.cpp"
    "src/font.cpp"
)

target_include_directories(luxel_ttf
    PUBLIC
        include
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(luxel_ttf PUBLIC cxx_std_20)

set_target_properties(luxel_ttf PROPERTIES
    CXX_EXTENSIONS OFF
)
