#pragma once

#include <luxel/types.h>
#include <type_traits>
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

	constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) noexcept {
		return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}
	
	constexpr BufferUsage operator&(BufferUsage a, BufferUsage b) noexcept {
		return static_cast<BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}

	constexpr BufferUsage& operator|=(BufferUsage& a, BufferUsage b) noexcept {
		return a = a | b;
	}

	constexpr bool has_flag(BufferUsage flags, BufferUsage bit) noexcept {
		return (flags & bit) == bit;
	}

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
