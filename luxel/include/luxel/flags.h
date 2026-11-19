#pragma once

#include <type_traits>

namespace luxel
{
	template<typename T>
	inline constexpr bool enable_bitmask_operators = false;


	template<typename T>
		requires(std::is_enum_v<T>&& enable_bitmask_operators<T>)
	constexpr T operator|(T a, T b) noexcept
	{
		using U = std::underlying_type_t<T>;
		return static_cast<T>(static_cast<U>(a) | static_cast<U>(b));
	}

	template<typename T>
		requires(std::is_enum_v<T>&& enable_bitmask_operators<T>)
	constexpr T operator&(T a, T b) noexcept
	{
		using U = std::underlying_type_t<T>;
		return static_cast<T>(static_cast<U>(a) & static_cast<U>(b));
	}

	template<typename T>
		requires(std::is_enum_v<T>&& enable_bitmask_operators<T>)
	constexpr T& operator|=(T& a, T b) noexcept {
		return a = a | b;
	}

	template<typename T>
		requires(std::is_enum_v<T>&& enable_bitmask_operators<T>)
	constexpr T& operator&=(T& a, T b) noexcept {
		return a = a & b;
	}

	template<typename T>
		requires(std::is_enum_v<T>&& enable_bitmask_operators<T>)
	constexpr bool has_flag(T flags, T bit) noexcept
	{
		// Con bit == 0 (None) no hay nada que comprobar: has_flag(x, None) devuelve x == None.
		if (bit == static_cast<T>(0)) {
			return flags == static_cast<T>(0);
		}
		return (flags & bit) == bit;
	}

} // namespace luxel
