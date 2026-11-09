#pragma once
#include <luxel/types.h>
#include <luxel/swapchain.h>
#include <cstdint>

namespace luxel
{
	enum class Backend : uint8_t { Vulkan };

	struct DeviceDescriptor
	{
		Backend backend{ Backend::Vulkan };
		bool debug{ true };
	};

	class Device : public RefCounted
	{
	public:
		virtual ~Device() = default;

		virtual Backend backend() const = 0;
		virtual Ref<Swapchain> create_swapchain(const SwapchainDescriptor&) = 0;
	};

	Ref<Device> create_device(const DeviceDescriptor& device_desc);
	
} // namespace luxel
