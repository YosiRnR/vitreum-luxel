#include "vulkan_frame_allocator.h"
#include "vulkan_device.h"
#include <luxel/frame.h>
#include <stdexcept>
#include <cstring>
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

		// Memoria visible del buffer (en CPU)
		vk::MemoryRequirements mem_requirements = m_buffer.getMemoryRequirements();
		vk::MemoryPropertyFlags properties      = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

		uint32_t memory_type_index = vk_device.find_memory_type(mem_requirements.memoryTypeBits, properties);

		vk::MemoryAllocateInfo alloc_info{
			.allocationSize  = mem_requirements.size,
			.memoryTypeIndex = memory_type_index
		};
		m_buffer_memory = vk::raii::DeviceMemory(vk_device.logical_device(), alloc_info);
		
		m_buffer.bindMemory(*m_buffer_memory, 0);

		m_mapped = m_buffer_memory.mapMemory(0, vk::WholeSize, vk::MemoryMapFlags{});

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

		if (buffer_data.size() == 0 || m_mapped == nullptr) {
			return {};
		}

		auto* byte_mapped = static_cast<std::byte*>(m_mapped);
		byte_mapped += m_cursor;

		std::memcpy(static_cast<void*>(byte_mapped) , buffer_data.data(), buffer_data.size());
		auto cursor_offset = m_cursor;
		m_cursor += static_cast<uint32_t>(buffer_data.size());

		return {
			.slot_index = m_slot_index,
			.offset     = cursor_offset
		};
	}


} // namespace luxel::vulkan
