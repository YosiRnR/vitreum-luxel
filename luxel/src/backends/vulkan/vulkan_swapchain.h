#pragma once

#include <luxel/types.h>
#include <luxel/swapchain.h>
#include "vulkan_common.h"
#include "vulkan_device.h"
#include <vector>
#include <cstdint>
#include <optional>

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

		std::optional<uint32_t> acquire(const vk::raii::Semaphore&);
		void present(const vk::raii::Queue&, uint32_t);

		// Accesores públicos
		vk::Image            image(uint32_t index) const { return m_images[index]; }
		const vk::raii::ImageView& image_view(uint32_t index) const { return m_image_views[index]; }
		const vk::raii::Semaphore& render_finished(uint32_t index) const { return m_render_finished[index]; }
		vk::Format           vk_format() const { return m_format; }
		vk::Extent2D         vk_extent() const { return m_extent; }
		bool                 needs_recreate() const { return m_needs_recreate; }

	private:
		// Declarados en orden inverso de destrucción
		Ref<VulkanDevice> m_ref_device;
		vk::raii::SurfaceKHR m_surface{ nullptr };
		vk::raii::SwapchainKHR m_swapchain{ nullptr };

		std::vector<vk::Image>           m_images;
		std::vector<vk::raii::ImageView> m_image_views;

		std::vector<vk::raii::Semaphore> m_render_finished;

		bool m_needs_recreate{ false };

		vk::Format   m_format{};
		vk::Extent2D m_extent{};

	};

} // namespace luxel::vulkan