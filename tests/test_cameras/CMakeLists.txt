cmake_minimum_required(VERSION 3.21)

project(test_cameras LANGUAGES CXX)

# Test de caja negra: solo usa la API publica de luxel (cameras.h y math),
# asi que no necesita la carpeta src de luxel ni Vulkan.
add_executable(test_cameras
    src/test_cameras.cpp
)

target_link_libraries(test_cameras
    PRIVATE
        luxel
)

target_compile_features(test_cameras PRIVATE cxx_std_20)

set_target_properties(test_cameras PROPERTIES
    CXX_EXTENSIONS OFF
)

add_test(NAME test_cameras COMMAND test_cameras)
