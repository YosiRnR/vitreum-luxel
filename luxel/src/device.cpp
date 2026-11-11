#include <luxel/types.h>
#include <luxel/device.h>
#include "backends/vulkan/vulkan_device.h"
#include <stdexcept>

namespace luxel
{
	Ref<Device> create_device(const DeviceDescriptor& device_desc)
	{
		switch (device_desc.backend)
		{
		case Backend::Vulkan:
			return vulkan::VulkanDevice::create(device_desc);
		}

		// No debería ser alcanzable hasta que no se añadan más Backends en la enumeración
		throw std::runtime_error("Backend no soportado");
	}

} // namespace luxel
