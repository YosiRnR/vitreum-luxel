#include <luxel/types.h>
#include <luxel/device.h>
#include "vulkan_device.h"
#include "vulkan_context.h"

namespace luxel::vulkan
{
	VulkanDevice::VulkanDevice(const DeviceDescriptor& device_desc)
	{
		// Creamos el objeto con el constructor por defecto para que ponga los valores por defecto
		// para app_name, app_version, engine_name, engine_version y api_version
		// aunque algunos de estos campos deberían ser seleccionables
		VulkanContext::Config config{};

		config.window_handle.platform = device_desc.window.platform;
		config.window_handle = device_desc.window;
		config.enable_validation_layers = device_desc.debug;

		m_vk_context = Ref<VulkanContext>(new VulkanContext(config));
	}

} // namespace luxel::vulkan
