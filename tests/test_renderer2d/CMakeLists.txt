cmake_minimum_required(VERSION 3.21)

project(test_renderer2d LANGUAGES CXX)

# Pruebas de la parte de CPU del Renderer2D (orden por capa y troceo en draws),
# sin GPU ni ventana. Es de "caja blanca": incluye una cabecera privada de
# luxel_2d (src/batch_order.h), que solo usa std. Por eso no enlaza con nada:
# basta con poder ver esa carpeta.
add_executable(test_renderer2d
    src/test_renderer2d.cpp
)

# $<TARGET_PROPERTY:luxel_2d,SOURCE_DIR> es una "expresión generadora": CMake
# la sustituye al generar el proyecto por la carpeta de luxel_2d. Así no se
# escribe la ruta a mano (si se mueve luxel_2d, esto sigue funcionando).
target_include_directories(test_renderer2d PRIVATE
    "$<TARGET_PROPERTY:luxel_2d,SOURCE_DIR>/src"
)

target_compile_features(test_renderer2d PRIVATE cxx_std_20)

set_target_properties(test_renderer2d PROPERTIES
    CXX_EXTENSIONS OFF
)

add_test(NAME test_renderer2d COMMAND test_renderer2d)
