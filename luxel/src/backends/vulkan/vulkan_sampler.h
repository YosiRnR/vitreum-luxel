#pragma once

#include <luxel/types.h>
#include <luxel/sampler.h>
#include "vulkan_common.h"
#include "vulkan_device.h"
#include <cstdint>

namespace luxel::vulkan
{
	class VulkanBindlessTable;

	class VulkanSampler : public Sampler
	{
	public:
		static Ref<Sampler> create(const Ref<VulkanDevice>& vk_device, const SamplerDescriptor& sampler_desc) {
			return Ref<Sampler>(new VulkanSampler(vk_device, sampler_desc));
		}

		// Destructor
		~VulkanSampler() override;

		uint32_t bindless_index() const override { return m_slot_index; }

	private:
		VulkanSampler(const Ref<VulkanDevice>& vk_device, const SamplerDescriptor& sampler_desc);

		// Declarados en orden inverso de destrucción
		Ref<VulkanDevice> m_ref_device;
		vk::raii::Sampler m_sampler{ nullptr };
		uint32_t m_slot_index{};
		VulkanBindlessTable* m_bindless_table{ nullptr };

		void on_zero_refs() const override;
	};

} // namespace luxel::vulkan
