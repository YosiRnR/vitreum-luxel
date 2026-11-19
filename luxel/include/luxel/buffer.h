#pragma once

#include <luxel/types.h>
#include <luxel/flags.h>
#include <cstdint>
#include <cstddef>
#include <ranges>
#include <span>

namespace luxel
{
	enum class BufferUsage : uint32_t
	{
		None        = 0, 
		Storage     = 1 << 0,
		Index       = 1 << 1
	};

	template<>
	inline constexpr bool enable_bitmask_operators<BufferUsage> = true;

	struct BufferDescriptor
	{
		size_t size{};
		BufferUsage usage{ BufferUsage::Storage };
		std::span<const std::byte> initial_data{};

		template<std::ranges::contiguous_range R>
		static constexpr BufferDescriptor from_data(const R& data, BufferUsage usage = BufferUsage::Storage) noexcept
		{
			using ElementType = std::ranges::range_value_t<R>;
			static_assert(std::is_trivially_copyable_v<ElementType>, "El Buffer de datos debe ser copiable de forma trivial");

			auto data_span = std::span{ data };
			return {
				.size  = data_span.size_bytes(),
				.usage = usage,
				.initial_data = std::as_bytes(data_span)
			};
		}
	};

	class Buffer : public RefCounted
	{
	public:
		virtual ~Buffer() = default;

		size_t size() const { return m_size; }
		BufferUsage usage() const { return m_usage; }
		virtual uint32_t bindless_index() const = 0; // Si no es BufferUsage::Storage devuelve 0 (sin slot)

	protected:
		Buffer(size_t size, BufferUsage usage)
			: m_size(size)
			, m_usage(usage)
		{
		}

	private:
		size_t m_size{};
		BufferUsage m_usage{ BufferUsage::None };
	};

} // namespace luxel
