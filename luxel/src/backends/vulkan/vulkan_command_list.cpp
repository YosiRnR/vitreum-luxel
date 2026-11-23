#include "vulkan_command_list.h"
#include <luxel/types.h>
#include <luxel/flags.h>
#include <luxel/command_list.h>
#include <luxel/buffer.h>
#include <luxel/graphics_pipeline.h>
#include "vulkan_bindless_table.h"
#include "vulkan_graphics_pipeline.h"
#include "vulkan_buffer.h"
#include "vulkan_common.h"
#include <stdexcept>
#include <cstdint>
#include <cassert>
#include <cstddef>
#include <format>
#include <span>

namespace luxel::vulkan
{
	namespace
	{
		vk::IndexType to_vk_index_type(IndexType index_type)
		{
			switch (index_type)
			{
			case IndexType::Uint16: return vk::IndexType::eUint16;
			case IndexType::Uint32: return vk::IndexType::eUint32;
			}

			return vk::IndexType::eUint16;
		}

	} // namespace 

	void VulkanCommandList::reset(vk::raii::CommandBuffer& cmd_buffer,
		                          vk::Image image, const vk::raii::ImageView& view,
		                          vk::Extent2D extent,
		                          vk::PipelineLayout pipeline_layout)
	{
		m_command_buffer  = &cmd_buffer;
		m_image           = image;
		m_image_view      = &view;
		m_extent          = extent;
		m_pipeline_layout = pipeline_layout;
	}

	void VulkanCommandList::begin_rendering(const RenderingDescriptor& desc)
	{
		assert(m_command_buffer != nullptr);

		vk::RenderingAttachmentInfo color_attachment{
			.imageView   = **m_image_view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
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

		m_command_buffer->setViewport(
			0,
			vk::Viewport{
				.x = 0.0f,
				.y = static_cast<float>(m_extent.height),
				.width  = static_cast<float>(m_extent.width),
				.height = -static_cast<float>(m_extent.height), // La Y del mundo crece hacía arriba
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			}
		);

		m_command_buffer->setScissor(
			0,
			vk::Rect2D{
				.offset = { 0, 0 },
				.extent = m_extent
			}
		);

		m_command_buffer->beginRendering(rendering_info);
	}

	void VulkanCommandList::end_rendering()
	{
		assert(m_command_buffer != nullptr);

		m_command_buffer->endRendering();
	}

	void VulkanCommandList::bind_pipeline(GraphicsPipeline& pipeline)
	{
		VulkanGraphicsPipeline& vk_pipeline = static_cast<VulkanGraphicsPipeline&>(pipeline);

		m_command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, vk_pipeline.pipeline());
	}

	void VulkanCommandList::draw(uint32_t num_vertices)
	{
		assert(m_command_buffer != nullptr);

		m_command_buffer->draw(num_vertices, 1, 0, 0);
	}

	void VulkanCommandList::bind_constants(std::span<const std::byte> data)
	{
		assert(m_command_buffer != nullptr);

		if (data.size() == 0 || data.size() % 4 != 0 || data.size() > VulkanBindlessTable::k_push_constant_range.size) {
			throw std::runtime_error(std::format("Buffer de Push Constants incompatible. Debe cumplir (tamaño > 0, múltiplo de 4, máximo {})", VulkanBindlessTable::k_push_constant_range.size));
		}

		m_command_buffer->pushConstants(
			m_pipeline_layout,
			VulkanBindlessTable::k_push_constant_range.stageFlags,
			VulkanBindlessTable::k_push_constant_range.offset,
			static_cast<uint32_t>(data.size()),
			data.data()
		);
	}

	void VulkanCommandList::bind_index_buffer(Buffer& index_buffer, IndexType index_type)
	{
		assert(m_command_buffer != nullptr);

		if (!has_flag(index_buffer.usage(), BufferUsage::Index)) {
			throw std::runtime_error("El buffer que se intenta utilizar como Index Buffer, no está creado con el tipo de BufferUsage::Index");
		}

		VulkanBuffer& vk_buffer = static_cast<VulkanBuffer&>(index_buffer);

		m_command_buffer->bindIndexBuffer(vk_buffer.buffer(), 0, to_vk_index_type(index_type));
	}

	void VulkanCommandList::draw_indexed(uint32_t index_count)
	{
		assert(m_command_buffer != nullptr);

		m_command_buffer->drawIndexed(index_count, 1, 0, 0, 0);
	}

} // namespace luxel::vulkan
