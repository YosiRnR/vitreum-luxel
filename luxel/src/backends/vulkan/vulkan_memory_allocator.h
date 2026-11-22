#pragma once

#include "vulkan_common.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <array>
#include <utility>

namespace luxel::vulkan
{
	class VulkanMemoryBlock;
	class VulkanDevice;

	enum class ResourceKind : uint8_t
	{
		Buffer = 0,
		Image  = 1,
		Count
	};

	class MemoryAllocation
	{
	public:
		MemoryAllocation() = default;
		MemoryAllocation(
			vk::DeviceMemory memory,
			uint64_t offset,
			uint64_t size,
			std::byte* mapped,
			VulkanMemoryBlock* block) noexcept
			: m_memory(memory)
			, m_offset(offset)
			, m_size(size)
			, m_mapped(mapped)
			, m_block(block)
		{
		}
		~MemoryAllocation() {
			reset();
		}

		// No copiable: solo puede haber un dueño de este trozo de memoria
		MemoryAllocation(const MemoryAllocation&) = delete;
		MemoryAllocation& operator=(const MemoryAllocation&) = delete;

		// Movible: transfiere la propiedad y deja el objeto origen vacío
		MemoryAllocation(MemoryAllocation&& other) noexcept
			: m_memory(std::exchange(other.m_memory, vk::DeviceMemory{}))
			, m_offset(std::exchange(other.m_offset, 0))
			, m_size(std::exchange(other.m_size, 0))
			, m_mapped(std::exchange(other.m_mapped, nullptr))
			, m_block(std::exchange(other.m_block, nullptr))
		{
		}
		MemoryAllocation& operator=(MemoryAllocation&& other) noexcept
		{
			if (this != &other)
			{
				reset();
				m_memory = std::exchange(other.m_memory, vk::DeviceMemory{});
				m_offset = std::exchange(other.m_offset, 0);
				m_size   = std::exchange(other.m_size, 0);
				m_mapped = std::exchange(other.m_mapped, nullptr);
				m_block  = std::exchange(other.m_block, nullptr);
			}
			return *this;
		}

		void reset();

		// Accesores públicos
		[[nodiscard]] vk::DeviceMemory   memory() const noexcept { return m_memory; }
		[[nodiscard]] uint64_t           offset() const noexcept { return m_offset; }
		[[nodiscard]] uint64_t           size()   const noexcept { return m_size;   }
		[[nodiscard]] std::byte*         mapped() const noexcept { return m_mapped; }

		[[nodiscard]] explicit operator bool() const noexcept {
			return m_block != nullptr && m_size > 0;
		}

	private:
		vk::DeviceMemory       m_memory{};
		uint64_t               m_offset{ 0 };
		uint64_t               m_size{ 0 };
		std::byte*             m_mapped{ nullptr };
		VulkanMemoryBlock*     m_block{ nullptr };
	};

	class VulkanMemoryAllocator
	{
	public:
		static constexpr uint64_t k_default_block_size  = 64ull * 1024 * 1024; // 64 MB
		static constexpr size_t   k_resource_kind_count = static_cast<size_t>(ResourceKind::Count);
		static constexpr size_t   k_bucket_count        = k_resource_kind_count * VK_MAX_MEMORY_TYPES;

		explicit VulkanMemoryAllocator(const VulkanDevice& vk_device);
		~VulkanMemoryAllocator();

		VulkanMemoryAllocator(const VulkanMemoryAllocator&) = delete;
		VulkanMemoryAllocator& operator=(const VulkanMemoryAllocator&) = delete;
		VulkanMemoryAllocator(VulkanMemoryAllocator&&) = delete;
		VulkanMemoryAllocator& operator=(VulkanMemoryAllocator&&) = delete;

		MemoryAllocation allocate(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties, ResourceKind kind);

		[[nodiscard]] size_t block_count() const noexcept;
		[[nodiscard]] size_t block_count(ResourceKind kind, uint32_t memory_type_index) const noexcept;

	private:
		const VulkanDevice& m_ref_device;
		std::array<std::vector<std::unique_ptr<VulkanMemoryBlock>>, k_bucket_count> m_blocks;

		[[nodiscard]] static constexpr size_t bucket_index(ResourceKind kind, uint32_t memory_type_index) noexcept {
			return static_cast<size_t>(kind) * VK_MAX_MEMORY_TYPES + memory_type_index;
		}
	};

} // namespace luxel::vulkan
