#pragma once

#include <luxel/types.h>
#include <luxel/device.h>
#include "vulkan_context.h"

namespace luxel::vulkan
{
	class VulkanDevice : public Device
	{
	public:
		explicit VulkanDevice(const DeviceDescriptor&);
		~VulkanDevice() override = default;

		Backend backend() const override { return Backend::Vulkan; }

	private:
		Ref<VulkanContext> m_vk_context;
	};

} // namespace luxel::vulkan
