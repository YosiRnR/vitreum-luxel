#include "vulkan_frame.h"
#include "vulkan_frame_allocator.h"
#include <luxel/frame.h>
#include <cstddef>
#include <span>

namespace luxel::vulkan
{
	BufferAllocation VulkanFrame::allocate(std::span<const std::byte> buffer_data) {
		return m_frame_allocator->allocate(buffer_data);
	}

} // namespace luxel::vulkan
