#include "vulkan_surface.h"
#include "vulkan_device.h"
#include "vulkan_platform.h"

namespace luxel::vulkan
{
	VulkanSurface::VulkanSurface(const Ref<VulkanDevice> vk_context)
		: m_ref_device(vk_context)
	{
	}

	VulkanSurface::~VulkanSurface() = default;

} // namespace luxel::vulkan
