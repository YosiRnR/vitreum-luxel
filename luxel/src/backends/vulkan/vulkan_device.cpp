#include <luxel/types.h>
#include <luxel/device.h>
#include "vulkan_device.h"
#include "vulkan_platform.h"
#include "vulkan_physical_device.h"
#include <set>
#include <ranges>
#include <vector>
#include <format>
#include <string>
#include <cstring>
#include <cstdint>
#include <utility>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <string_view>
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

	VulkanDevice::VulkanDevice(const DeviceDescriptor& device_desc)
	{
		// Creamos el objeto con el constructor por defecto para que ponga los valores por defecto
		// para app_name, app_version, engine_name, engine_version y api_version
		// aunque algunos de estos campos deberían ser seleccionables
		InstanceConfig config{};

		config.window_handle = device_desc.window;
		config.enable_validation_layers = device_desc.debug;

		create_instance(config);
		setup_debug_messenger();
		create_surface(config);
		pick_physical_device();
		create_logical_device();
	}

	VulkanDevice::~VulkanDevice() = default;

	vk::DebugUtilsMessengerCreateInfoEXT VulkanDevice::debug_utils_create_info()
	{
		using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
		using enum vk::DebugUtilsMessageTypeFlagBitsEXT;

		vk::DebugUtilsMessengerCreateInfoEXT debug_create_info{
			.messageSeverity = eVerbose | eWarning | eError,
			.messageType     = eGeneral | eValidation | ePerformance,
			.pfnUserCallback = debug_callback,
			.pUserData       = this
		};
		return debug_create_info;
	}

	void VulkanDevice::create_instance(const InstanceConfig& config)
	{
		if (config.api_version < vk::ApiVersion13) {
			throw std::runtime_error(std::format("Required minimum API 1.3 for dynamic rendering, selected is: {}", config.api_version));
		}

		std::vector<char const*> enabled_layers{};
		std::vector<char const*> enabled_extensions{};

		auto layer_properties = m_context.enumerateInstanceLayerProperties();
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

		auto debug_messenger_create_info = debug_utils_create_info();

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

	void VulkanDevice::setup_debug_messenger()
	{
		if (!m_enable_validation_layers) {
			return;
		}

		auto debug_messenger_create_info = debug_utils_create_info();
		m_debug_messenger = vk::raii::DebugUtilsMessengerEXT(m_instance, debug_messenger_create_info);
	}

	void VulkanDevice::create_surface(const InstanceConfig& config)
	{
		m_surface = platform::create_surface_for_window(m_instance, config.window_handle);

		if (!*m_surface) {
			throw std::runtime_error("No se pudo crear la surface para la ventana");
		}
	}

	void VulkanDevice::pick_physical_device()
	{
		auto best_device = select_physical_device(m_instance, m_surface);

		// Guardamos los índices de las familias de colas del dispositivo elegido.
		// Son necesarios para crear el dispositivo lógico y el command pool.
		m_graphics_queue_family_index = best_device.indices.graphics_family.value();
		m_present_queue_family_index  = best_device.indices.present_family.value();

		m_physical_device = std::move(best_device.device);

		// Opcional, mostrar info por consola sobre el dispositivo físico seleccionado
		print_physical_device_info(m_physical_device);
	}

	void VulkanDevice::create_logical_device()
	{
		std::set<uint32_t> unique_queue_families = { m_graphics_queue_family_index, m_present_queue_family_index };
		std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
		float queue_priority = 1.0f;

		for (uint32_t queue_family : unique_queue_families)
		{
			queue_create_infos.push_back(vk::DeviceQueueCreateInfo{
				.queueFamilyIndex = queue_family,
				.queueCount       = 1,
				.pQueuePriorities = &queue_priority
			});
		}

		auto device_features_chain = make_required_features();

		vk::DeviceCreateInfo create_info{
			.pNext                   = &device_features_chain.get<vk::PhysicalDeviceFeatures2>(),
			.queueCreateInfoCount    = static_cast<uint32_t>(queue_create_infos.size()),
			.pQueueCreateInfos       = queue_create_infos.data(),
			.enabledExtensionCount   = static_cast<uint32_t>(k_required_device_extensions.size()),
			.ppEnabledExtensionNames = k_required_device_extensions.data(),
			.pEnabledFeatures        = nullptr // Usamos features extendidas vía pNext
		};

		m_logical_device = vk::raii::Device(m_physical_device, create_info);

		// Crear las colas de gráficos y presentación
		m_graphics_queue = vk::raii::Queue(m_logical_device, m_graphics_queue_family_index, 0);

		if (m_graphics_queue_family_index == m_present_queue_family_index) {
			m_present_queue = m_graphics_queue;
		}
		else {
			m_present_queue = vk::raii::Queue(m_logical_device, m_present_queue_family_index, 0);
		}
	}

	VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDevice::debug_callback
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
