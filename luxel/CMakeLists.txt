cmake_minimum_required(VERSION 3.21)

project(luxel LANGUAGES CXX)

find_package(Vulkan REQUIRED COMPONENTS dxc)
message(STATUS "DXC library: ${Vulkan_dxc_LIBRARY}")
message(STATUS "DXC executable: ${Vulkan_dxc_EXECUTABLE}")

file(GLOB lista_hlsl_shaders CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.hlsl")

set(shader_spv_files)
set(shader_header_files)

foreach(hlsl_shader IN LISTS lista_hlsl_shaders)
    get_filename_component(shader_name "${hlsl_shader}" NAME_WE)

    if(shader_name MATCHES "_(vs|ps)$")
        set(shader_stage "${CMAKE_MATCH_1}")
    else()
        message(STATUS "Ignorando shader sin sufijo _vs/_ps: ${hlsl_shader}")
        continue()
    endif()

    if(shader_stage STREQUAL "vs")
        set(shader_profile "vs_6_0")
        set(shader_entry   "vs_main")
    elseif(shader_stage STREQUAL "ps")
        set(shader_profile "ps_6_0")
        set(shader_entry   "ps_main")
    else()
        message(STATUS "Ignorando shader sin prefijo vs/ps: ${hlsl_shader}")
        continue()
    endif()

    set(shader_output "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_name}.spv")
    list(APPEND shader_spv_files "${shader_output}")
    message(STATUS "Shader: ${hlsl_shader} -> ${shader_output}")

    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${Vulkan_dxc_EXECUTABLE}"
                -spirv
                -T "${shader_profile}"
                -E "${shader_entry}"
                "-fspv-target-env=vulkan1.3"
                -Fo "${shader_output}"
                "${hlsl_shader}"
        DEPENDS "${hlsl_shader}"
        VERBATIM
    )

    set(shader_header "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_name}_spv.h")
    list(APPEND shader_header_files "${shader_header}")

    add_custom_command(
        OUTPUT "${shader_header}"
        COMMAND "${CMAKE_COMMAND}"
                "-DINPUT=${shader_output}"
                "-DOUTPUT=${shader_header}"
                "-DNAME=${shader_name}_spv"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        DEPENDS "${shader_output}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
        VERBATIM
    )
endforeach()

add_custom_target(compile_shaders ALL
    DEPENDS ${shader_header_files}
    SOURCES ${lista_hlsl_shaders}
)

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

add_dependencies(luxel compile_shaders)

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
