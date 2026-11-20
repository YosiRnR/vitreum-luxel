#pragma once

#include <luxel/types.h>
#include <cstdint>

namespace luxel
{
	enum class SamplerFilter { Nearest, Linear };
	enum class SamplerAddressMode { Repeat, ClampToEdge };

	struct SamplerDescriptor
	{
		SamplerFilter mag_filter{ SamplerFilter::Linear };
		SamplerFilter min_filter{ SamplerFilter::Linear };
		SamplerAddressMode address_mode_u{ SamplerAddressMode::Repeat };
		SamplerAddressMode address_mode_v{ SamplerAddressMode::Repeat };
		SamplerAddressMode address_mode_w{ SamplerAddressMode::Repeat };
	};

	class Sampler : public RefCounted
	{
	public:
		virtual ~Sampler() = default;

		virtual uint32_t bindless_index() const = 0;

		// Accesores públicos
		const SamplerDescriptor& sampler_desc() const { return m_sampler_desc; }

	protected:
		Sampler(const SamplerDescriptor& sampler_desc)
			: m_sampler_desc(sampler_desc)
		{
		}

	private:
		SamplerDescriptor m_sampler_desc;
	};

} // namespace luxel
