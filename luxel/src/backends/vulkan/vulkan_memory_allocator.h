#pragma once

#include "vulkan_common.h"
#include <cstdint>
#include <cstddef>

namespace luxel::vulkan
{
	class VulkanMemoryBlock;

	struct MemoryAllocation
	{
		vk::DeviceMemory memory{};
		uint64_t offset{ 0 };
		uint64_t size{ 0 };
		std::byte* mapped{ nullptr };
		VulkanMemoryBlock* block{ nullptr };
	};

} // namespace luxel::vulkan
