#include "vulkan_frame_allocator.h"
#include "vulkan_device.h"
#include <luxel/frame.h>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <span>

namespace luxel::vulkan
{
	void VulkanFrameAllocator::reset_cursor()
	{
		m_cursor = 0;
	}

	VulkanFrameAllocator::VulkanFrameAllocator(VulkanDevice& vk_device)
		: m_bindless_table(&vk_device.bindless_table())
	{
		// Buffer
		vk::BufferCreateInfo buffer_info{
			.size        = m_capacity,
			.usage       = vk::BufferUsageFlagBits::eStorageBuffer,
			.sharingMode = vk::SharingMode::eExclusive
		};
		m_buffer = vk::raii::Buffer(vk_device.logical_device(), buffer_info);

		// Memoria visible del buffer (ya mapeada en el bloque)
		vk::MemoryRequirements mem_requirements = m_buffer.getMemoryRequirements();
		vk::MemoryPropertyFlags properties      = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

		m_buffer_memory = vk_device.memory_allocator().allocate(mem_requirements, properties, ResourceKind::Buffer);
		m_buffer.bindMemory(m_buffer_memory.memory(), m_buffer_memory.offset());

		m_slot_index = m_bindless_table->allocate_buffer_slot(m_buffer, {}, vk::WholeSize);
	}

	VulkanFrameAllocator::~VulkanFrameAllocator()
	{
		if (m_slot_index > 0 && m_bindless_table) {
			m_bindless_table->free_buffer_slot(m_slot_index);
		}
	}

	BufferAllocation VulkanFrameAllocator::allocate(std::span<const std::byte> buffer_data)
	{
		m_cursor = (m_cursor + 15) & ~15u;

		if (m_cursor + buffer_data.size() > m_capacity) {
			throw std::runtime_error("VulkanFrameAllocator overflow!");
		}

		std::byte* byte_mapped = m_buffer_memory.mapped();
		if (buffer_data.size() == 0 || byte_mapped == nullptr) {
			return {};
		}

		std::memcpy(byte_mapped + m_cursor, buffer_data.data(), buffer_data.size());
		auto cursor_offset = m_cursor;
		m_cursor += static_cast<uint32_t>(buffer_data.size());

		return {
			.slot_index = m_slot_index,
			.offset     = cursor_offset
		};
	}

} // namespace luxel::vulkan
