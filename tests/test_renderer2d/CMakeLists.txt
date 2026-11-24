cmake_minimum_required(VERSION 3.21)

project(test_renderer2d LANGUAGES CXX)

# Pruebas de la parte de CPU del Renderer2D (orden por capa, troceo en draws,
# UTF-8, proyección de la UI), sin GPU ni ventana. Es de "caja blanca": incluye
# cabeceras privadas de luxel_2d (src/*.h). Esas cabeceras solo usan std y las
# matemáticas de luxel, que también son solo cabeceras; por eso no enlaza con
# nada: basta con poder ver esas carpetas.
add_executable(test_renderer2d
    src/test_renderer2d.cpp
)

# $<TARGET_PROPERTY:luxel_2d,SOURCE_DIR> es una "expresión generadora": CMake
# la sustituye al generar el proyecto por la carpeta de luxel_2d. Así no se
# escribe la ruta a mano (si se mueve luxel_2d, esto sigue funcionando).
target_include_directories(test_renderer2d PRIVATE
    "$<TARGET_PROPERTY:luxel_2d,SOURCE_DIR>/src"
    "$<TARGET_PROPERTY:luxel,SOURCE_DIR>/include"   # luxel/math/*.h
)

target_compile_features(test_renderer2d PRIVATE cxx_std_20)

set_target_properties(test_renderer2d PROPERTIES
    CXX_EXTENSIONS OFF
)

add_test(NAME test_renderer2d COMMAND test_renderer2d)
