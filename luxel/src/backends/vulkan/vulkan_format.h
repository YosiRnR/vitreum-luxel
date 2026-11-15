#pragma once

#include "vulkan_common.h"
#include <luxel/types.h>

namespace luxel::vulkan
{
	// Esto también está en vulkan_swapchain.cpp, lo tengo que poner accesible en un solo sitio
	inline vk::Format to_vk_format(Format format)
	{
		switch (format)
		{
		case Format::R8G8B8A8_UNorm:    return vk::Format::eR8G8B8A8Unorm;
		case Format::R8G8B8A8_SRGB:     return vk::Format::eR8G8B8A8Srgb;
		case Format::B8G8R8A8_UNorm:    return vk::Format::eB8G8R8A8Unorm;
		case Format::B8G8R8A8_SRGB:     return vk::Format::eB8G8R8A8Srgb;
		case Format::D32_Float:         return vk::Format::eD32Sfloat;
		case Format::D24_UNorm_S8_UInt: return vk::Format::eD24UnormS8Uint;
		default:                        return vk::Format::eUndefined;
		}
	}

	inline Format from_vk_format(vk::Format format)
	{
		switch (format)
		{
		case vk::Format::eR8G8B8A8Unorm:  return Format::R8G8B8A8_UNorm;
		case vk::Format::eR8G8B8A8Srgb:   return Format::R8G8B8A8_SRGB;
		case vk::Format::eB8G8R8A8Unorm:  return Format::B8G8R8A8_UNorm;
		case vk::Format::eB8G8R8A8Srgb:   return Format::B8G8R8A8_SRGB;
		case vk::Format::eD32Sfloat:      return Format::D32_Float;
		case vk::Format::eD24UnormS8Uint: return Format::D24_UNorm_S8_UInt;
		default:                          return Format::Undefined;
		}
	}

} // namespace luxel::vulkan
