#include "vulkan_memory_block.h"
#include "vulkan_device.h"
#include <optional>
#include <cstdint>
#include <cstddef>
#include <cassert>

namespace luxel::vulkan
{
	VulkanMemoryBlock::VulkanMemoryBlock(const VulkanDevice& vk_device, uint64_t size, uint32_t memory_type_index)
		: m_allocator(size)
		, m_memory_type_index(memory_type_index)
	{
		vk::MemoryAllocateInfo alloc_info{
			.allocationSize  = size,
			.memoryTypeIndex = m_memory_type_index
		};
		m_memory = vk::raii::DeviceMemory(vk_device.logical_device(), alloc_info);

		using enum vk::MemoryPropertyFlagBits;
		vk::MemoryPropertyFlags properties                = eHostVisible | eHostCoherent;
		vk::PhysicalDeviceMemoryProperties mem_properties = vk_device.physical_device().getMemoryProperties();
		if ((mem_properties.memoryTypes[m_memory_type_index].propertyFlags & properties) == properties)
		{
			m_mapped = static_cast<std::byte*>(m_memory.mapMemory(0, vk::WholeSize, vk::MemoryMapFlags{}));
		}
	}

	VulkanMemoryBlock::~VulkanMemoryBlock() {
		assert(m_allocator.is_empty() && "VulkanMemoryBlock destruido con asignaciones activas");
	}

	std::optional<MemoryRange> VulkanMemoryBlock::allocate(uint64_t size, uint64_t alignment) {
		return m_allocator.allocate(size, alignment);
	}

	void VulkanMemoryBlock::free(uint64_t offset, uint64_t size) {
		m_allocator.free(offset, size);
	}

} // namespace luxel::vulkan
