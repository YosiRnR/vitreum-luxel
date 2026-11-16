#pragma once

#include <luxel/types.h>
#include "vulkan_device.h"

namespace luxel::vulkan
{
	class VulkanGraphicsPipeline : public GraphicsPipeline
	{
	public:
		static Ref<GraphicsPipeline> create(const Ref<VulkanDevice>& vk_device, const GraphicsPipelineDescriptor& desc) {
			return Ref<GraphicsPipeline>(new VulkanGraphicsPipeline(vk_device, desc));
		}

		// Accesores públicos
		const vk::raii::Pipeline& pipeline() const { return m_pipeline; }

	private:
		VulkanGraphicsPipeline(const Ref<VulkanDevice>& vk_device, const GraphicsPipelineDescriptor&);

		// Declarados en orden inverso de destrucción
		Ref<VulkanDevice> m_ref_device;
		vk::raii::Pipeline m_pipeline{ nullptr };

		void on_zero_refs() const override;
	};

} // namespace luxel::vulkan
