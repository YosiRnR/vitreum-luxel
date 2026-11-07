cmake_minimum_required(VERSION 3.21)

project(vitreum LANGUAGES CXX)

add_library(vitreum STATIC
    src/win32/window_win32.cpp
)

target_include_directories(vitreum
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(vitreum PUBLIC cxx_std_20)

target_compile_definitions(vitreum PUBLIC UNICODE _UNICODE)

set_target_properties(vitreum PROPERTIES
    CXX_EXTENSIONS OFF
)
