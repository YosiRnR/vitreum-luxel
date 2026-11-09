#pragma once

#include <luxel/types.h>
#include <luxel/device.h>
#include <luxel/swapchain.h>
#include <luxel/frame.h>
#include "vulkan_common.h"
#include "vulkan_frame.h"
#include <array>
#include <string>
#include <cstdint>

namespace luxel::vulkan
{
	inline constexpr uint32_t k_frames_in_flight = 2;

	class VulkanDevice : public Device
	{
		struct InstanceConfig
		{
			std::string    app_name{ "Luxel Vulkan Application" };
			uint32_t    app_version{ 1 };
			std::string engine_name{ "Luxel Engine" };
			uint32_t engine_version{ 1 };
			uint32_t    api_version{ vk::ApiVersion13 };

			bool enable_validation_layers{ true };
		};

		struct FrameResources
		{
			vk::raii::CommandPool command_pool{ nullptr };
			vk::raii::CommandBuffer command_buffer{ nullptr };
			vk::raii::Semaphore image_available{ nullptr };
			vk::raii::Fence in_flight{ nullptr };
		};

	public:
		explicit VulkanDevice(const DeviceDescriptor&);
		~VulkanDevice() override;

		Backend backend() const override { return Backend::Vulkan; }
		Ref<Swapchain> create_swapchain(const SwapchainDescriptor&) override;
		Frame* begin_frame(Swapchain&) override; // nullptr = no se pinta en este frame
		void end_frame(Frame&) override;
		void wait_idle() override;

		// Accesores públicos
		const vk::raii::Instance& instance() const { return m_instance; }
		const vk::raii::PhysicalDevice& physical_device() const { return m_physical_device; }
		uint32_t present_family_index() const { return m_present_queue_family_index; }
		uint32_t graphics_family_index() const { return m_graphics_queue_family_index; }
		const vk::raii::Queue& graphics_queue() const { return m_graphics_queue;  }
		const vk::raii::Queue& present_queue() const { return m_present_queue; }
		const vk::raii::Device& logical_device() const { return m_logical_device; }

	private:
		// Vulkan RAII Context
		vk::raii::Context m_context{};

		bool m_enable_validation_layers{ false };

		// Vulkan RAII objects
		vk::raii::Instance  m_instance{ nullptr };
		vk::raii::DebugUtilsMessengerEXT m_debug_messenger{ nullptr };

		vk::raii::PhysicalDevice m_physical_device{ nullptr };
		vk::raii::Device          m_logical_device{ nullptr };

		uint32_t m_graphics_queue_family_index{};
		uint32_t  m_present_queue_family_index{};

		vk::raii::Queue m_graphics_queue{ nullptr };
		vk::raii::Queue  m_present_queue{ nullptr };

		std::array<FrameResources, k_frames_in_flight> m_frames_resources;
		uint32_t m_frame_index{ 0 };

		VulkanFrame m_frame;


		// Función estática para imprimir por consola mensajes de Debug
		static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback
		(
			vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
			vk::DebugUtilsMessageTypeFlagsEXT message_type,
			const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
			void* user_data
		);
		vk::DebugUtilsMessengerCreateInfoEXT debug_utils_create_info();
		void create_instance(const InstanceConfig&);
		void setup_debug_messenger();
		void pick_physical_device();
		void create_logical_device();
		void create_frames();
	};

} // namespace luxel::vulkan
