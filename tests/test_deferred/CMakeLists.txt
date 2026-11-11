cmake_minimum_required(VERSION 3.21)

project(test_deferred LANGUAGES CXX)

find_package(Vulkan REQUIRED)

add_executable(test_deferred
    src/test_deferred.cpp
)

# Prueba de "caja blanca": ve las cabeceras privadas de luxel (src/) para poder
# hablar con VulkanDevice. Solo este ejecutable tiene ese permiso; el resto del
# mundo sigue viendo luxel por su API publica.
target_include_directories(test_deferred
    PRIVATE
        "$<TARGET_PROPERTY:luxel,SOURCE_DIR>/src"
)

target_link_libraries(test_deferred
    PRIVATE
        vitreum
        luxel
        Vulkan::Vulkan
)

target_compile_features(test_deferred PRIVATE cxx_std_20)

target_compile_definitions(test_deferred PRIVATE UNICODE _UNICODE)

set_target_properties(test_deferred PROPERTIES
    CXX_EXTENSIONS OFF
)

add_test(NAME test_deferred COMMAND test_deferred)
