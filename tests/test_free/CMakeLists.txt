cmake_minimum_required(VERSION 3.21)

project(test_free LANGUAGES CXX)

find_package(Vulkan REQUIRED)

add_executable(test_free
    src/test_free.cpp
)

target_include_directories(test_free
    PRIVATE
        "$<TARGET_PROPERTY:luxel,SOURCE_DIR>/src"
)

target_link_libraries(test_free
    PRIVATE
        luxel
)

target_compile_features(test_free PRIVATE cxx_std_20)

target_compile_definitions(test_free PRIVATE UNICODE _UNICODE)

set_target_properties(test_free PROPERTIES
    CXX_EXTENSIONS OFF
)

add_test(NAME test_free COMMAND test_free)
