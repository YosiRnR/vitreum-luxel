#pragma once

#include "vulkan_common.h"
#include "../../free_list_allocator.h"
#include <optional>
#include <cstdint>
#include <cstddef>

namespace luxel::vulkan
{
	class VulkanDevice;

	class VulkanMemoryBlock
	{
	public:
		VulkanMemoryBlock(const VulkanDevice& vk_device, uint64_t size, uint32_t memory_type_index);
		~VulkanMemoryBlock();

		VulkanMemoryBlock(const VulkanMemoryBlock&) = delete;
		VulkanMemoryBlock& operator=(const VulkanMemoryBlock&) = delete;
		VulkanMemoryBlock(VulkanMemoryBlock&&) = delete;
		VulkanMemoryBlock& operator=(VulkanMemoryBlock&&) = delete;

		std::optional<MemoryRange> allocate(uint64_t size, uint64_t alignment);
		void free(uint64_t offset, uint64_t size);

		// Accesores públicos
		vk::DeviceMemory memory() const { return *m_memory; }
		uint32_t memory_type_index() const { return m_memory_type_index; }
		std::byte* mapped() const { return m_mapped; }

	private:
		vk::raii::DeviceMemory m_memory{ nullptr };
		FreeListAllocator m_allocator;
		uint32_t m_memory_type_index{};
		std::byte* m_mapped{ nullptr };
	};

} // namespace luxel::vulkan
