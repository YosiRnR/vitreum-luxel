#include <luxel/types.h>
#include <luxel/device.h>
#include "backends/vulkan/vulkan_device.h"
#include <stdexcept>
#include <format>

namespace luxel
{
	Ref<Device> create_device(const DeviceDescriptor& device_desc)
	{
		if (device_desc.device_maximus.max_textures > DeviceDescriptor::k_max_textures_engine_limit) {
			throw std::runtime_error(std::format("El número máximo de texturas solicitado supera el límite de {}", DeviceDescriptor::k_max_textures_engine_limit));
		}
		if (device_desc.device_maximus.max_storage_buffers > DeviceDescriptor::k_max_storage_buffers_engine_limit) {
			throw std::runtime_error(std::format("El número máximo de bufers de almacenamiento solicitado supera el límite de {}", DeviceDescriptor::k_max_storage_buffers_engine_limit));
		}
		if (device_desc.device_maximus.max_samplers > DeviceDescriptor::k_max_samplers_engine_limit) {
			throw std::runtime_error(std::format("El número máximo de samplers de texturas solicitado supera el límite de {}", DeviceDescriptor::k_max_samplers_engine_limit));
		}

		switch (device_desc.backend)
		{
		case Backend::Vulkan:
			return vulkan::VulkanDevice::create(device_desc);
		}

		// No debería ser alcanzable hasta que no se añadan más Backends en la enumeración
		throw std::runtime_error("Backend no soportado");
	}

} // namespace luxel
