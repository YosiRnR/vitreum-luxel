#pragma once
#include <luxel/command_list.h>
#include <cstdint>
#include <cstddef>
#include <span>

namespace luxel
{
	struct BufferAllocation
	{
		uint32_t slot_index{};
		uint32_t offset{};
	};

	class Frame
	{
	public:
		virtual ~Frame() = default;

		virtual CommandList& commands() = 0;
		virtual BufferAllocation allocate(std::span<const std::byte> buffer_data) = 0;
	};

} // namespace luxel
