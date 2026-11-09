#pragma once

#include <vector>
#include <luxel/types.h>
#include "vulkan_common.h"

namespace luxel::vulkan::platform
{
	// Extensiones de instance necesarias para poder crear una surface en la plataforma actual.
	///
	std::vector<const char*> get_platform_instance_extensions();

	// Crea la surface nativa a partir del handle de ventana opaco.
	///
	vk::raii::SurfaceKHR create_surface_for_window(const vk::raii::Instance& instance, const WindowHandle& window_handle);

	bool queue_family_can_present(const vk::raii::PhysicalDevice&, uint32_t);

} // namespace luxel::vulkan::platform
