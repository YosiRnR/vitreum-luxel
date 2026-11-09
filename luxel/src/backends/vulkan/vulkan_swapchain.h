#pragma once

#include <luxel/types.h>
#include <luxel/swapchain.h>
#include "vulkan_common.h"
#include "vulkan_device.h"
#include <vector>

namespace luxel::vulkan
{
	class VulkanSwapchain : public Swapchain
	{
	public:
		VulkanSwapchain(const Ref<VulkanDevice>& vk_device, const SwapchainDescriptor&);
		~VulkanSwapchain() override;

		Size2D extent() const override;
		Format color_format() const override;
		void   resize(Size2D) override;

	private:
		// Declarados en orden inverso de destrucción
		Ref<VulkanDevice> m_ref_device;
		vk::raii::SurfaceKHR m_surface{ nullptr };
		vk::raii::SwapchainKHR m_swapchain{ nullptr };

		std::vector<vk::Image>           m_images;
		std::vector<vk::raii::ImageView> m_image_views;

		vk::Format   m_format{};
		vk::Extent2D m_extent{};

	};

} // namespace luxel::vulkan