cmake_minimum_required(VERSION 3.21)

project(luxel_ttf LANGUAGES CXX)

# luxel_ttf: lector y rasterizador de fuentes TrueType hecho a mano
# (docs/ttf-design.md). Solo usa la biblioteca estándar: no enlaza con luxel ni con
# luxel_2d. Recibe los bytes del .ttf (no lee ficheros) y devuelve medidas, contornos
# y, más adelante, el atlas que se le da a BitmapFont.
add_library(luxel_ttf STATIC
    "include/luxel/ttf/font.h"
    "src/reader.h"
    "src/tables.h"
    "src/tables.cpp"
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
