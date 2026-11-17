#include "vulkan_staging_buffer.h"
#include "vulkan_device.h"
#include <stdexcept>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <span>

namespace luxel::vulkan
{
	VulkanStagingBuffer::VulkanStagingBuffer(VulkanDevice& vk_device, size_t size)
		: m_size(size)
	{
		// Buffer
		vk::BufferCreateInfo buffer_info{
			.size        = size,
			.usage       = vk::BufferUsageFlagBits::eTransferSrc,
			.sharingMode = vk::SharingMode::eExclusive
		};
		m_buffer = vk::raii::Buffer(vk_device.logical_device(), buffer_info);

		// Buffer Memory
		vk::MemoryRequirements mem_requirements = m_buffer.getMemoryRequirements();
		vk::MemoryPropertyFlags properties      = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

		uint32_t memory_type_index = vk_device.find_memory_type(mem_requirements.memoryTypeBits, properties);

		vk::MemoryAllocateInfo alloc_info{
			.allocationSize  = mem_requirements.size,
			.memoryTypeIndex = memory_type_index
		};
		m_buffer_memory = vk::raii::DeviceMemory(vk_device.logical_device(), alloc_info);

		m_buffer.bindMemory(*m_buffer_memory, 0);
	}

	void VulkanStagingBuffer::write_data(std::span<const std::byte> data)
	{
		if (data.size() > m_size) {
			throw std::runtime_error("El tamaño del buffer a escribir excede el tamaño reservado");
		}
		
		void* mapped{ nullptr };
		mapped = m_buffer_memory.mapMemory(0, vk::WholeSize, vk::MemoryMapFlags{});
		std::memcpy(static_cast<char*>(mapped), data.data(), data.size());
		m_buffer_memory.unmapMemory();
	}

} // namespace luxel::vulkan
