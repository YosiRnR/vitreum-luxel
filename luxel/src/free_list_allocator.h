#pragma once

#include <optional>
#include <cstdint>
#include <vector>

namespace luxel
{
	struct MemoryRange
	{
		uint64_t offset{};
		uint64_t size{};
	};

	class FreeListAllocator
	{
	public:
		explicit FreeListAllocator(uint64_t size)
			: m_size(size)
		{
			m_free_ranges.push_back({ 0, size });
		}

		std::optional<MemoryRange> allocate(uint64_t size, uint64_t alignment);
		void free(uint64_t offset, uint64_t size);

		[[nodiscard]] uint64_t capacity() const noexcept { return m_size; }
		[[nodiscard]] uint64_t free_size() const noexcept;
		[[nodiscard]] bool is_empty() const noexcept;

	private:
		uint64_t m_size{};
		std::vector<MemoryRange> m_free_ranges;
	};

} // namespace luxel
