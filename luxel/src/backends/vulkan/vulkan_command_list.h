#pragma once

#include <luxel/command_list.h>
#include "vulkan_common.h"

namespace luxel::vulkan
{
	class VulkanCommandList : public CommandList
	{
	public:
		VulkanCommandList() = default;

		void reset(vk::raii::CommandBuffer&, vk::Image, const vk::raii::ImageView&, vk::Extent2D);

		void begin_rendering(const RenderingDescriptor&) override;
		void end_rendering() override;

		vk::raii::CommandBuffer* m_command_buffer{ nullptr };
		vk::Image m_image{};
		const vk::raii::ImageView* m_image_view{ nullptr };
		vk::Extent2D m_extent{};
	};

} // namespace luxel::vulkan
