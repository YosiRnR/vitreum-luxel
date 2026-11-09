#include "vulkan_command_list.h"
#include <luxel/command_list.h>
#include "vulkan_common.h"
#include <cassert>

namespace luxel::vulkan
{
	void VulkanCommandList::reset(vk::raii::CommandBuffer& cmd_buffer, vk::Image image, const vk::raii::ImageView& view, vk::Extent2D extent)
	{
		m_command_buffer = &cmd_buffer;
		m_image          = image;
		m_image_view     = &view;
		m_extent         = extent;
	}

	void VulkanCommandList::begin_rendering(const RenderingDescriptor& desc)
	{
		assert(m_command_buffer != nullptr);

		vk::RenderingAttachmentInfo color_attachment{
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.imageView   = **m_image_view,
			.loadOp      = vk::AttachmentLoadOp::eClear,
			.storeOp     = vk::AttachmentStoreOp::eStore,
			.clearValue  = vk::ClearColorValue{ desc.clear.r, desc.clear.g, desc.clear.b, desc.clear.a }
		};

		vk::RenderingInfo rendering_info{
			.renderArea           = vk::Rect2D{ .offset = { 0, 0 }, .extent = m_extent },
			.layerCount           = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments    = &color_attachment
		};

		m_command_buffer->beginRendering(rendering_info);
	}

	void VulkanCommandList::end_rendering()
	{
		assert(m_command_buffer != nullptr);

		m_command_buffer->endRendering();
	}

} // namespace luxel::vulkan
