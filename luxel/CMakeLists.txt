cmake_minimum_required(VERSION 3.21)

project(luxel LANGUAGES CXX)

add_library(luxel STATIC
    
)

target_include_directories(vitreum
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(luxel PUBLIC cxx_std_20)

target_compile_definitions(luxel PUBLIC UNICODE _UNICODE)

set_target_properties(luxel PROPERTIES
    CXX_EXTENSIONS OFF
)
