#pragma once

#include <luxel/frame.h>
#include <luxel/command_list.h>
#include "vulkan_command_list.h"
#include <cstdint>

namespace luxel::vulkan
{
	class VulkanSwapchain;

	class VulkanFrame : public Frame
	{
	public:
		VulkanFrame() = default;

		CommandList& commands() override { return m_command_list; }

		void reset(
			vk::raii::CommandBuffer& cmd_buffer,
			vk::Image image, const vk::raii::ImageView& view,
			vk::Extent2D extent,
			VulkanSwapchain& vk_swapchain, uint32_t image_index
		)
		{
			m_swapchain = &vk_swapchain;
			m_image_index = image_index;
			m_command_list.reset(cmd_buffer, image, view, extent);
		}

		// Accesores públicos
		VulkanSwapchain& swapchain() const { return *m_swapchain; }
		uint32_t image_index() const { return m_image_index; }

	private:
		VulkanCommandList m_command_list;
		VulkanSwapchain* m_swapchain{ nullptr };
		uint32_t m_image_index{ 0 };
	};

} // namespace luxel::vulkan
