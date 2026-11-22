#pragma once

#include "vulkan_common.h"
#include "vulkan_memory_allocator.h"
#include <luxel/frame.h>
#include <cstdint>
#include <cstddef>
#include <span>

namespace luxel::vulkan
{
	class VulkanDevice;
	class VulkanBindlessTable;

	constexpr size_t k_frame_allocator_capacity = 1024 * 1024; // 1 MB fijo por ahora

	class VulkanFrameAllocator
	{
		uint32_t m_slot_index{};
		uint32_t m_cursor{};
		size_t m_capacity{ k_frame_allocator_capacity };

		// Declarados en orden inverso de destrucción
		MemoryAllocation m_buffer_memory{};
		vk::raii::Buffer m_buffer{ nullptr };

		VulkanBindlessTable* m_bindless_table{ nullptr };

	public:
		VulkanFrameAllocator(VulkanDevice& device);
		~VulkanFrameAllocator();

		BufferAllocation allocate(std::span<const std::byte> buffer_data);
		void reset_cursor();
	};

} // namespace luxel::vulkan
