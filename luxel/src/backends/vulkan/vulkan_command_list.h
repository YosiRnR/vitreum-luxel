#pragma once

#include <luxel/types.h>
#include <luxel/command_list.h>
#include "vulkan_common.h"
#include <cstdint>
#include <cstddef>
#include <span>

namespace luxel
{
	class GraphicsPipeline;
	class Buffer;
}

namespace luxel::vulkan
{

	class VulkanCommandList : public CommandList
	{
	public:
		VulkanCommandList() = default;

		void reset(vk::raii::CommandBuffer&, vk::Image, const vk::raii::ImageView&, vk::Extent2D, vk::PipelineLayout);

		void begin_rendering(const RenderingDescriptor&) override;
		void end_rendering() override;

		void bind_pipeline(GraphicsPipeline&) override;
		void draw(uint32_t num_vertices) override;
		void bind_constants(std::span<const std::byte>) override;
		void bind_index_buffer(Buffer&, IndexType) override;
		void draw_indexed(uint32_t index_count) override;

	private:
		vk::raii::CommandBuffer* m_command_buffer{ nullptr };
		vk::Image m_image{};
		const vk::raii::ImageView* m_image_view{ nullptr };
		vk::Extent2D m_extent{};

		vk::PipelineLayout m_pipeline_layout{};
	};

} // namespace luxel::vulkan
