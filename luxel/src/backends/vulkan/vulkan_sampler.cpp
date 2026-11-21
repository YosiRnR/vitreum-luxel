#include "vulkan_sampler.h"
#include "vulkan_common.h"
#include "vulkan_device.h"
#include <luxel/sampler.h>
#include <utility>

namespace luxel::vulkan
{
	namespace
	{
		vk::Filter to_vk_sampler_filter(SamplerFilter filter)
		{
			using enum SamplerFilter;
			switch (filter)
			{
			case Linear:  return vk::Filter::eLinear;
			case Nearest: return vk::Filter::eNearest;
			}

			return vk::Filter::eLinear;
		}

		vk::SamplerAddressMode to_vk_sampler_address_mode(SamplerAddressMode address_mode)
		{
			using enum SamplerAddressMode;
			switch (address_mode)
			{
			case Repeat:      return vk::SamplerAddressMode::eRepeat;
			case ClampToEdge: return vk::SamplerAddressMode::eClampToEdge;
			}

			return vk::SamplerAddressMode::eRepeat;
		}

	} // namespace

	VulkanSampler::VulkanSampler(const Ref<VulkanDevice>& vk_device, const SamplerDescriptor& sampler_desc)
		: Sampler(sampler_desc)
		, m_ref_device(vk_device)
		, m_bindless_table(&vk_device->bindless_table())
	{
		vk::SamplerCreateInfo sampler_info{
			.magFilter    = to_vk_sampler_filter(sampler_desc.mag_filter),
			.minFilter    = to_vk_sampler_filter(sampler_desc.min_filter),
			.mipmapMode   = vk::SamplerMipmapMode::eLinear, // De momento
			.addressModeU = to_vk_sampler_address_mode(sampler_desc.address_mode_u),
			.addressModeV = to_vk_sampler_address_mode(sampler_desc.address_mode_v),
			.addressModeW = to_vk_sampler_address_mode(sampler_desc.address_mode_w),
			.anisotropyEnable = vk::False, // De momento
			.minLod = 0,
			.maxLod = 0,
		};
		m_sampler = vk::raii::Sampler(m_ref_device->logical_device(), sampler_info);

		m_slot_index = m_bindless_table->allocate_sampler_slot(*m_sampler);
	}

	VulkanSampler::~VulkanSampler()
	{
		if (m_slot_index > 0 && m_bindless_table) {
			m_bindless_table->free_sampler_slot(m_slot_index);
		}
	}

	void VulkanSampler::on_zero_refs() const
	{
		auto* self = const_cast<VulkanSampler*>(this);

		// Sacamos el Ref del miembro a un local ANTES de encolar: ese local debe
		// ser lo ultimo que se destruye, porque si era la ultima referencia al
		// device, su destructor vaciara la cola y hara delete de "this".
		Ref<VulkanDevice> device = std::move(self->m_ref_device);
		device->enqueue_destroy(self);
	}

} // namespace luxel::vulkan
