cmake_minimum_required(VERSION 3.21)

project(test_1 LANGUAGES CXX)

add_executable(test_1
    src/test_1.cpp
    "src/stb_image.h"
)

# Compila e incrusta los shaders que usa este test: genera shader_vs_spv.h y
# shader_ps_spv.h en una carpeta privada de test_1 (ver luxel/cmake/luxel_shaders.cmake).
# Los .hlsl viven en tests/shaders porque test_deferred también los usa.
luxel_embed_shaders(test_1
    ../shaders/shader_vs.hlsl
    ../shaders/shader_ps.hlsl
)

target_link_libraries(test_1
    PRIVATE
        vitreum
        luxel
)

target_compile_features(test_1 PRIVATE cxx_std_20)

target_compile_definitions(test_1 PRIVATE
    UNICODE
    _UNICODE
)

set_target_properties(test_1 PROPERTIES
    CXX_EXTENSIONS OFF
)
