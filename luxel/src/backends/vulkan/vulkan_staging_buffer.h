#pragma once

#include "vulkan_common.h"
#include "vulkan_memory_allocator.h"
#include <cstddef>
#include <span>

namespace luxel::vulkan
{
	class VulkanDevice;

	class VulkanStagingBuffer
	{
		// Declarados en orden inverso de destrucción
		MemoryAllocation m_buffer_memory{};
		vk::raii::Buffer m_buffer{ nullptr };
		size_t m_size{};

	public:
		VulkanStagingBuffer(VulkanDevice& device, size_t size);

		void write_data(std::span<const std::byte> data);

		// Accesores públicos
		const vk::raii::Buffer& buffer() const { return m_buffer; }
	};

} // namespace luxel::vulkan
