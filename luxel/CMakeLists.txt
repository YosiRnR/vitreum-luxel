cmake_minimum_required(VERSION 3.21)

project(luxel LANGUAGES CXX)

# Define la función luxel_embed_shaders() (y busca el SDK de Vulkan con dxc).
# luxel ya no tiene shaders propios: cada target (luxel_2d, los tests...) llama
# a la función con los suyos. Las funciones de CMake son globales, así que los
# CMakeLists que se procesen después de este pueden usarla.
include(cmake/luxel_shaders.cmake)

add_library(luxel STATIC
    
    "src/backends/vulkan/vulkan_platform.cpp"
    "src/backends/vulkan/vulkan_swapchain.cpp"
    "include/luxel/luxel.h"
    "include/luxel/device.h"
    "src/device.cpp"
    "src/backends/vulkan/vulkan_device.h"
    "src/backends/vulkan/vulkan_device.cpp"
    "src/backends/vulkan/vulkan_physical_device.h"
    "src/backends/vulkan/vulkan_physical_device.cpp"
    "include/luxel/swapchain.h"
    "include/luxel/frame.h"
    "include/luxel/command_list.h"
    "src/backends/vulkan/vulkan_frame.h"
    "src/backends/vulkan/vulkan_command_list.h"
    "src/backends/vulkan/vulkan_command_list.cpp"
    "src/slot_allocator.h"
    "src/backends/vulkan/vulkan_bindless_table.h"
    "src/backends/vulkan/vulkan_bindless_table.cpp"
    "include/luxel/graphics_pipeline.h"
    "src/backends/vulkan/vulkan_graphics_pipeline.h"
    "src/backends/vulkan/vulkan_graphics_pipeline.cpp"
    "src/backends/vulkan/vulkan_format.h"
    "include/luxel/buffer.h"
    "include/luxel/math/vectors.h"
    "include/luxel/math/matrix4.h"
    "src/backends/vulkan/vulkan_buffer.h"
    "src/backends/vulkan/vulkan_buffer.cpp"
    "src/backends/vulkan/vulkan_staging_buffer.h"
    "src/backends/vulkan/vulkan_staging_buffer.cpp"
    "src/backends/vulkan/vulkan_frame_allocator.h"
    "src/backends/vulkan/vulkan_frame_allocator.cpp"
    "src/backends/vulkan/vulkan_frame.cpp"
    "include/luxel/sampler.h"
    "include/luxel/texture.h"
    "include/luxel/flags.h"
    "src/backends/vulkan/vulkan_sampler.h"
    "src/backends/vulkan/vulkan_sampler.cpp"
    "src/backends/vulkan/vulkan_texture.h"
    "src/backends/vulkan/vulkan_texture.cpp"
    "src/free_list_allocator.h"
    "src/free_list_allocator.cpp"
    "src/backends/vulkan/vulkan_memory_block.h"
    "src/backends/vulkan/vulkan_memory_block.cpp"
    "src/backends/vulkan/vulkan_memory_allocator.h"
    "src/backends/vulkan/vulkan_memory_allocator.cpp"
    "include/luxel/cameras.h"
    "src/cameras.cpp"
)

target_include_directories(luxel
    PUBLIC
        include
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(luxel PRIVATE Vulkan::Vulkan)

target_compile_features(luxel PUBLIC cxx_std_20)

target_compile_definitions(luxel PUBLIC UNICODE _UNICODE)

set_target_properties(luxel PROPERTIES
    CXX_EXTENSIONS OFF
)
