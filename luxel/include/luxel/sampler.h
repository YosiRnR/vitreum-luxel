#pragma once

#include <luxel/types.h>
#include <cstdint>

namespace luxel
{
	enum class SamplerFilter { Nearest, Linear };
	enum class SamplerAddressMode { Repeat, ClampToEdge };

	struct SamplerDescriptor
	{
		SamplerFilter filter{ SamplerFilter::Linear };
		SamplerAddressMode address_mode{ SamplerAddressMode::Repeat };
	};

	class Sampler : public RefCounted
	{
	public:
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
