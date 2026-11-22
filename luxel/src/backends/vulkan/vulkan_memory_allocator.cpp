#include "vulkan_memory_allocator.h"
#include "vulkan_device.h"
#include "vulkan_memory_block.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <cstdint>
#include <memory>
#include <cassert>
#include <vector>

namespace luxel::vulkan
{
	void MemoryAllocation::reset()
	{
		if (m_block && m_size > 0) {
			m_block->free(m_offset, m_size);
		}
		m_memory    = vk::DeviceMemory{};
		m_offset    = 0;
		m_size      = 0;
		m_mapped    = nullptr;
		m_block     = nullptr;
	}

	VulkanMemoryAllocator::VulkanMemoryAllocator(const VulkanDevice& vk_device)
		: m_ref_device(vk_device)
	{
	}

	VulkanMemoryAllocator::~VulkanMemoryAllocator() = default;

	MemoryAllocation VulkanMemoryAllocator::allocate(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties, ResourceKind kind)
	{
		const uint32_t type_index = m_ref_device.find_memory_type(requirements.memoryTypeBits, properties);
		assert(type_index < VK_MAX_MEMORY_TYPES && "memoryTypeIndex fuera de rango");

		auto& bucket = m_blocks[bucket_index(kind, type_index)];
		for (auto& block : bucket)
		{
			if (auto range = block->allocate(requirements.size, requirements.alignment)) {
				return MemoryAllocation(
					block->memory(),
					range->offset,
					range->size,
					block->mapped() ? block->mapped() + range->offset : nullptr,
					block.get()
				);
			}
		}

		// Ningún bloque tiene hueco suficiente: creamos uno nuevo (de 64 MB o mayor si el recurso lo exige)
		const uint64_t new_block_size = std::max<uint64_t>(k_default_block_size, requirements.size);
		auto new_block = std::make_unique<VulkanMemoryBlock>(m_ref_device, new_block_size, type_index);

		auto range = new_block->allocate(requirements.size, requirements.alignment);
		if (!range) {
			throw std::runtime_error("VulkanMemoryAllocator: fallo inesperado al reservar en un bloque recién creado");
		}

		MemoryAllocation alloc(
			new_block->memory(),
			range->offset,
			range->size,
			new_block->mapped() ? new_block->mapped() + range->offset : nullptr,
			new_block.get()
		);

		bucket.push_back(std::move(new_block));
		return alloc;
	}

	void VulkanMemoryAllocator::trim()
	{
		for (auto& bucket : m_blocks)
		{
			bool kept_standard_empty = false;

			std::erase_if(bucket, [&](const std::unique_ptr<VulkanMemoryBlock>& block) {
				if (!block->is_empty()) {
					return false;
				}

				if (!kept_standard_empty && block->size() <= k_default_block_size) {
					kept_standard_empty = true;
					return false;
				}

				return true;
			});
		}
	}

	size_t VulkanMemoryAllocator::block_count() const noexcept
	{
		size_t total = 0;
		for (const auto& bucket : m_blocks) {
			total += bucket.size();
		}
		return total;
	}

	size_t VulkanMemoryAllocator::block_count(ResourceKind kind, uint32_t memory_type_index) const noexcept
	{
		assert(memory_type_index < VK_MAX_MEMORY_TYPES);
		return m_blocks[bucket_index(kind, memory_type_index)].size();
	}

} // namespace luxel::vulkan
