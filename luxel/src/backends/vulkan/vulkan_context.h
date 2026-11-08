#pragma once

#include "vulkan_common.h"
#include <luxel/types.h>
#include <cstdint>
#include <string>

namespace luxel::vulkan
{
	class VulkanContext : public RefCounted
	{
	public:
		struct Config
		{
			std::string    app_name{ "Luxel Vulkan Application" };
			uint32_t    app_version{ 1 };
			std::string engine_name{ "Luxel Engine" };
			uint32_t engine_version{ 1 };
			uint32_t    api_version{ vk::ApiVersion13 };

			WindowHandle window_handle{};
			bool enable_validation_layers{ true };
		};

		explicit VulkanContext(const Config& config);
		~VulkanContext();

	private:
		// Con RAII, el orden de declaración importa, determina el orden de destrucción, que será el inverso al de construcción.
		vk::raii::Context m_context;

		vk::raii::Instance                      m_instance{ nullptr };
		vk::raii::DebugUtilsMessengerEXT m_debug_messenger{ nullptr };
		vk::raii::SurfaceKHR                     m_surface{ nullptr };

		bool m_enable_validation_layers{ false };

		// Función estática para imprimir por consola mensajes de Debug
		static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback
		(
			vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
			vk::DebugUtilsMessageTypeFlagsEXT message_type,
			const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
			void* user_data
		);

		void create_instance(const Config& config);
		void setup_debug_messenger();
		void create_surface(const Config& config);
	};

} // namespace luxel::vulkan
