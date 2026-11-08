#pragma once
#include "vulkan_common.h"
#include <luxel/types.h>
#include "vulkan_context.h"

namespace luxel::vulkan
{
	class VulkanSurface
	{
	public:
		VulkanSurface(const Ref<VulkanContext> vk_context);
		~VulkanSurface();

	private:
		vk::raii::SurfaceKHR m_surface{ nullptr };

		const Ref<VulkanContext> m_ref_context;
	};

} // namespace luxel::vulkan