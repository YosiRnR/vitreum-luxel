#pragma once
#include <luxel/types.h>
#include <luxel/swapchain.h>
#include <luxel/frame.h>
#include <cstdint>

namespace luxel
{
	enum class Backend : uint8_t { Vulkan };

	struct RequestedDeviceMaximums
	{
		uint32_t max_textures{ 8192 };
		uint32_t max_storage_buffers{ 2048 };
		uint32_t max_samplers{ 128 };
	};

	struct DeviceDescriptor
	{
		static constexpr uint32_t k_max_textures_engine_limit         = 65536;
		static constexpr uint32_t k_max_storage_buffers_engine_limit  = 16384;
		static constexpr uint32_t k_max_samplers_engine_limit         =  1024;

		Backend backend{ Backend::Vulkan };
		bool debug{ true };

		RequestedDeviceMaximums device_maximus{};
	};


	class Device : public RefCounted
	{
	public:
		virtual ~Device() = default;

		virtual Backend backend() const = 0;
		virtual Ref<Swapchain> create_swapchain(const SwapchainDescriptor&) = 0;
		virtual Frame* begin_frame(Swapchain&) = 0; // nullptr = no se pinta en este frame
		virtual void end_frame(Frame&) = 0;
		virtual void wait_idle() = 0;

	};

	Ref<Device> create_device(const DeviceDescriptor& device_desc);
	
} // namespace luxel
