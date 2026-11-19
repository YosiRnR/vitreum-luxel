#pragma once

#include <luxel/types.h>
#include <luxel/flags.h>
#include <cstdint>
#include <cstddef>
#include <span>

namespace luxel
{
	enum class TextureUsage : uint32_t
	{
		None = 0,
		Sampled = 1 << 0
	};

	template<>
	inline constexpr bool enable_bitmask_operators<TextureUsage> = true;


	struct TextureDescriptor
	{
		uint32_t width{};
		uint32_t height{};
		Format format{ Format::Undefined };
		TextureUsage usage{ TextureUsage::Sampled };
		uint32_t mip_levels{ 1 };
		std::span<const std::byte> initial_data;
	};

	class Texture : public RefCounted
	{
	public:
		virtual uint32_t bindless_index() const = 0;

		// Accesores públicos
		uint32_t width() const { return m_width; }
		uint32_t height() const { return m_height; }
		Format format() const { return m_format; }
		uint32_t mip_levels() const { return m_mip_levels; }
		TextureUsage usage() const { return m_usage; }

	protected:
		Texture(uint32_t width, uint32_t height, Format format, uint32_t mip_levels, TextureUsage usage)
			: m_width(width)
			, m_height(height)
			, m_format(format)
			, m_mip_levels(mip_levels)
			, m_usage(usage)
		{
		}

	private:
		uint32_t m_width{};
		uint32_t m_height{};
		Format m_format{ Format::Undefined };
		uint32_t m_mip_levels{};
		TextureUsage m_usage{ TextureUsage::None };
	};

} // namespace luxel