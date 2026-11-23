cmake_minimum_required(VERSION 3.21)

project(test_1 LANGUAGES CXX)

add_executable(test_1
    src/test_1.cpp
 "src/stb_image.h")

target_link_libraries(test_1
    PRIVATE
        vitreum
        luxel
)

target_compile_features(test_1 PRIVATE cxx_std_20)

target_compile_definitions(test_1 PRIVATE
    UNICODE
    _UNICODE
    LUXEL_SHADER_DIR="${CMAKE_BINARY_DIR}/luxel/shaders"
)

set_target_properties(test_1 PROPERTIES
    CXX_EXTENSIONS OFF
)
