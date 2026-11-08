#include "vulkan_surface.h"
#include "vulkan_context.h"
#include "vulkan_platform.h"

namespace luxel::vulkan
{
	VulkanSurface::VulkanSurface(const Ref<VulkanContext> vk_context)
		: m_ref_context(vk_context)
	{
	}

	VulkanSurface::~VulkanSurface() = default;

} // namespace luxel::vulkan
