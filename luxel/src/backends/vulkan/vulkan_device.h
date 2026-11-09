#pragma once

#include <luxel/types.h>
#include <luxel/swapchain.h>
#include <luxel/device.h>
#include "vulkan_common.h"
#include <string>
#include <cstdint>

namespace luxel::vulkan
{
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

	public:
		explicit VulkanDevice(const DeviceDescriptor&);
		~VulkanDevice() override;

		Backend backend() const override { return Backend::Vulkan; }
		Ref<Swapchain> create_swapchain(const SwapchainDescriptor&) override;


		const vk::raii::Instance& instance() const { return m_instance; }
		const vk::raii::PhysicalDevice& physical_device() const { return m_physical_device; }
		uint32_t present_family_index() const { return m_present_queue_family_index; }
		uint32_t graphics_family_index() const { return m_graphics_queue_family_index; }
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
	};

} // namespace luxel::vulkan
