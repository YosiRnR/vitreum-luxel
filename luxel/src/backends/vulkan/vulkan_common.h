#pragma once

#ifndef VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#endif

#ifndef VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#endif

#if defined(_WIN32)
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#elif defined(__linux__)
#ifndef VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#endif
#endif

#include <vulkan/vulkan_raii.hpp>