cmake_minimum_required(VERSION 3.21)

project(luxel LANGUAGES CXX)

find_package(Vulkan REQUIRED)

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
    "include/luxel/swapchain.h" "include/luxel/frame.h" "include/luxel/command_list.h" "src/backends/vulkan/vulkan_frame.h" "src/backends/vulkan/vulkan_command_list.h" "src/backends/vulkan/vulkan_command_list.cpp"  "src/slot_allocator.h" "src/backends/vulkan/vulkan_bindless_table.h" "src/backends/vulkan/vulkan_bindless_table.cpp")

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
