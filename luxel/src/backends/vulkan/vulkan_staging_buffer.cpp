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

		// Memoria del buffer (host-visible y coherente, ya mapeada en el bloque)
		vk::MemoryRequirements mem_requirements = m_buffer.getMemoryRequirements();
		vk::MemoryPropertyFlags properties      = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

		m_buffer_memory = vk_device.memory_allocator().allocate(mem_requirements, properties, ResourceKind::Buffer);
		m_buffer.bindMemory(m_buffer_memory.memory(), m_buffer_memory.offset());
	}

	void VulkanStagingBuffer::write_data(std::span<const std::byte> data)
	{
		if (data.size() > m_size) {
			throw std::runtime_error("El tamaño del buffer a escribir excede el tamaño reservado");
		}
		
		if (!m_buffer_memory.mapped()) {
			throw std::runtime_error("El bloque de memoria del staging buffer no está mapeado");
		}

		std::memcpy(m_buffer_memory.mapped(), data.data(), data.size());
	}

} // namespace luxel::vulkan
