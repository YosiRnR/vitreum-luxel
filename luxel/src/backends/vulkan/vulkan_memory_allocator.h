#pragma once

#include "vulkan_common.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <array>

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

	struct MemoryAllocation
	{
		vk::DeviceMemory memory{};
		uint64_t offset{ 0 };
		uint64_t size{ 0 };
		std::byte* mapped{ nullptr };
		VulkanMemoryBlock* block{ nullptr };

		[[nodiscard]] explicit operator bool() const noexcept {
			return block != nullptr && size > 0;
		}
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
		void free(const MemoryAllocation& allocation);

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
