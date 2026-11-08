#include "vulkan_context.h"
#include "vulkan_platform.h"
#include <format>
#include <vector>
#include <ranges>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <type_traits>

namespace luxel::vulkan
{
	namespace
	{
		template<typename Properties>
		std::vector<const char*> filter_supported(std::vector<const char*> const& requested, std::vector<Properties> const& available)
		{
			auto filtered = requested | std::views::filter([&available](const char* req)
				{
					return std::ranges::any_of(available, [req](const Properties& avail)
						{
							if constexpr (std::is_same_v<Properties, vk::LayerProperties>) {
								return std::strcmp(req, avail.layerName) == 0;
							}
							else if constexpr (std::is_same_v<Properties, vk::ExtensionProperties>) {
								return std::strcmp(req, avail.extensionName) == 0;
							}
							else {
								static_assert(std::is_same_v<Properties, vk::LayerProperties> ||
									std::is_same_v<Properties, vk::ExtensionProperties>,
									"Unsupported Properties type");
							}
						});
				});

			return std::vector<const char*>(filtered.begin(), filtered.end());
		}

	} // namespace

	VulkanContext::VulkanContext(const Config& config)
		: m_context{}
	{
		create_instance(config);
		setup_debug_messenger();
		create_surface(config);
	}

	VulkanContext::~VulkanContext() = default;

	void VulkanContext::create_instance(const Config& config)
	{
		std::vector<char const*> enabled_layers{};
		std::vector<char const*> enabled_extensions{};

		auto layer_properties     = m_context.enumerateInstanceLayerProperties();
		auto extension_properties = m_context.enumerateInstanceExtensionProperties();

		// Filtrar las capas de validación requeridas según las capas disponibles
		if (config.enable_validation_layers) {
			enabled_layers = filter_supported<vk::LayerProperties>({ "VK_LAYER_KHRONOS_validation" }, layer_properties);
		}
		m_enable_validation_layers = !enabled_layers.empty();

		// Filtrar las extensiones requeridas según las extensiones disponibles
		std::vector<char const*> required_extensions = platform::get_platform_instance_extensions(config.window_handle.platform);
		if (m_enable_validation_layers) {
			required_extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		enabled_extensions = filter_supported<vk::ExtensionProperties>(required_extensions, extension_properties);
		if (enabled_extensions.size() != required_extensions.size())
		{
			std::vector<const char*> missing_extensions;
			for (const char* req_ext : required_extensions)
			{
				if (std::find(enabled_extensions.begin(), enabled_extensions.end(), req_ext) == enabled_extensions.end()) {
					missing_extensions.push_back(req_ext);
				}
			}

			auto join = [](const std::vector<const char*>& names, std::string_view delimiter)
				{
					std::string result;
					for (const auto* name : names)
					{
						if (!result.empty()) {
							result += delimiter;
						}
						result += name;
					}
					return result;
				};
			throw std::runtime_error(std::format("Missing required Vulkan extensions: {}", join(missing_extensions, ", ")));
		}

		// Crear la instancia de Vulkan con las capas y extensiones habilitadas
		vk::ApplicationInfo app_info{
			.pApplicationName   = config.app_name.c_str(),
			.applicationVersion = config.app_version,
			.pEngineName        = config.engine_name.c_str(),
			.engineVersion      = config.engine_version,
			.apiVersion         = config.api_version
		};

		using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
		using enum vk::DebugUtilsMessageTypeFlagBitsEXT;

		vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{
			.messageSeverity = eVerbose | eWarning | eError,
			.messageType     = eGeneral | eValidation | ePerformance,
			.pfnUserCallback = debug_callback,
			.pUserData       = this
		};

		vk::InstanceCreateInfo create_info{
			.pNext                   = m_enable_validation_layers ? &debug_messenger_create_info : nullptr,
			.pApplicationInfo        = &app_info,
			.enabledLayerCount       = static_cast<uint32_t>(enabled_layers.size()),
			.ppEnabledLayerNames     = enabled_layers.data(),
			.enabledExtensionCount   = static_cast<uint32_t>(enabled_extensions.size()),
			.ppEnabledExtensionNames = enabled_extensions.data()
		};
		m_instance = vk::raii::Instance(m_context, create_info);
	}

	void VulkanContext::setup_debug_messenger()
	{
		if (!m_enable_validation_layers) {
			return;
		}

		using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
		using enum vk::DebugUtilsMessageTypeFlagBitsEXT;

		vk::DebugUtilsMessengerCreateInfoEXT debug_create_info{
			.messageSeverity = eVerbose | eWarning | eError,
			.messageType     = eGeneral | eValidation | ePerformance,
			.pfnUserCallback = debug_callback,
			.pUserData       = this
		};
		m_debug_messenger = vk::raii::DebugUtilsMessengerEXT(m_instance, debug_create_info);
	}

	void VulkanContext::create_surface(const Config& config)
	{
		m_surface = platform::create_surface_for_window(m_instance, config.window_handle);

		if (!*m_surface) {
			throw std::runtime_error("No se pudo crear la surface para la ventana");
		}
	}

	VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanContext::debug_callback
	(
		vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
		vk::DebugUtilsMessageTypeFlagsEXT message_type,
		const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
		void* user_data // NOSONAR - void* user_data es requerido así por la API de Vulkan, aunque no lo usaremos aquí.
	)
	{
		std::string severity;
		if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
			severity = "VERBOSE";
		}
		else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
			severity = "INFO";
		}
		else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
			severity = "WARNING";
		}
		else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
			severity = "ERROR";
		}
		std::string type;
		if (message_type & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral) {
			type = "GENERAL";
		}
		else if (message_type & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation) {
			type = "VALIDATION";
		}
		else if (message_type & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance) {
			type = "PERFORMANCE";
		}
		std::string message(callback_data->pMessage);
		std::cerr << std::format("[Vulkan][{}][{}] {}\n", severity, type, message);

		return vk::False; // Indica que no se debe interrumpir la ejecución del programa
	}

} // namespace luxel::vulkan
