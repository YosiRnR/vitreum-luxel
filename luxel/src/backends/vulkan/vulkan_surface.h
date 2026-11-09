#pragma once

#include <luxel/types.h>
#include "vulkan_common.h"
#include "vulkan_device.h"

namespace luxel::vulkan
{
	class VulkanSurface
	{
	public:
		VulkanSurface(const Ref<VulkanDevice> vk_device);
		~VulkanSurface();

	private:
		vk::raii::SurfaceKHR m_surface{ nullptr };

		const Ref<VulkanDevice> m_ref_device;
	};

} // namespace luxel::vulkan