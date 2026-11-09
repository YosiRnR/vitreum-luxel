#include "vulkan_physical_device.h"
#include <array>
#include <format>
#include <cstdint>
#include <utility>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace luxel::vulkan
{
	namespace
	{
		// Tabla de Features de Vulkan 1.2
		constexpr std::array k_features12 = {
			&vk::PhysicalDeviceVulkan12Features::descriptorIndexing,
			&vk::PhysicalDeviceVulkan12Features::runtimeDescriptorArray,
			&vk::PhysicalDeviceVulkan12Features::shaderSampledImageArrayNonUniformIndexing,
			&vk::PhysicalDeviceVulkan12Features::shaderStorageBufferArrayNonUniformIndexing,
			&vk::PhysicalDeviceVulkan12Features::descriptorBindingPartiallyBound,
			&vk::PhysicalDeviceVulkan12Features::descriptorBindingStorageBufferUpdateAfterBind,
			&vk::PhysicalDeviceVulkan12Features::descriptorBindingSampledImageUpdateAfterBind,
			&vk::PhysicalDeviceVulkan12Features::descriptorBindingStorageImageUpdateAfterBind
		};

		// Tabla de Features de Vulkan 1.3
		constexpr std::array k_features13 = {
			&vk::PhysicalDeviceVulkan13Features::dynamicRendering,
			&vk::PhysicalDeviceVulkan13Features::synchronization2
		};

		// Habilita todos los campos de la tabla poniéndolos a vk::True
		template <typename Struct, size_t N>
		void enable_all(Struct& s, const std::array<vk::Bool32 Struct::*, N>& table)
		{
			for (auto member : table) {
				s.*member = vk::True;
			}
		}

		// Comprueba que todos los campos de la tabla estén a vk::True en el dispositivo
		template <typename Struct, size_t N>
		bool all_supported(const Struct& s, const std::array<vk::Bool32 Struct::*, N>& table)
		{
			return std::ranges::all_of(table, [&](auto member) {
				return s.*member == vk::True;
			});
		}


		bool check_device_extension_support(const vk::raii::PhysicalDevice& physical_device)
		{
			auto available_extensions = physical_device.enumerateDeviceExtensionProperties();

			return std::ranges::all_of(k_required_device_extensions, [&available_extensions](std::string_view required_ext)
			{
				return std::ranges::any_of(available_extensions, [required_ext](const vk::ExtensionProperties& ext)
				{
					return std::string_view(ext.extensionName) == required_ext;
				});
			});
		}

		bool supports_required_features(const vk::raii::PhysicalDevice& physical_device)
		{
			auto feature_chain = physical_device.getFeatures2<
				vk::PhysicalDeviceFeatures2,
				vk::PhysicalDeviceVulkan12Features,
				vk::PhysicalDeviceVulkan13Features
			>();

			return all_supported(feature_chain.get<vk::PhysicalDeviceVulkan12Features>(), k_features12) &&
				   all_supported(feature_chain.get<vk::PhysicalDeviceVulkan13Features>(), k_features13);
		}

		PhysicalDeviceRate rate_physical_device(const vk::raii::PhysicalDevice& physical_device, const vk::raii::SurfaceKHR& surface)
		{
			PhysicalDeviceRate device_rate{
				.device = physical_device,
				.score = 0,
				.indices = {}
			};

			auto device_props = physical_device.getProperties2();

			if (device_props.properties.apiVersion < vk::ApiVersion13) {
				return device_rate;
			}

			if (bool extensions_supported = check_device_extension_support(physical_device); !extensions_supported) {
				return device_rate;
			}

			if (bool features_supported = supports_required_features(physical_device); !features_supported) {
				return device_rate;
			}

			device_rate.indices = find_queue_families(physical_device, surface);
			if (!device_rate.indices.is_complete()) {
				return device_rate;
			}

			if (bool swapchain_adequate = query_swapchain_support(physical_device, surface).is_adequate(); !swapchain_adequate) {
				return device_rate;
			}

			if (device_props.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
				device_rate.score += 1000;
			}
			else if (device_props.properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
				device_rate.score += 500;
			}

			return device_rate;
		}

	} // namespace


	RequiredFeatures make_required_features()
	{
		RequiredFeatures device_features_chain{};

		enable_all(device_features_chain.get<vk::PhysicalDeviceVulkan12Features>(), k_features12);
		enable_all(device_features_chain.get<vk::PhysicalDeviceVulkan13Features>(), k_features13);

		return device_features_chain;
	}

	QueueFamilyIndices find_queue_families(const vk::raii::PhysicalDevice& physical_device, const vk::raii::SurfaceKHR& surface)
	{
		QueueFamilyIndices indices{};
		auto queue_families = physical_device.getQueueFamilyProperties();

		for (uint32_t i = 0; i < queue_families.size(); ++i)
		{
			if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
				indices.graphics_family = i;
			}

			if (physical_device.getSurfaceSupportKHR(i, *surface)) {
				indices.present_family = i;
			}

			// Si encontramos una familia de colas que soporte gráficos y present juntos, ya tenemos la mejor
			if (indices.graphics_family.has_value() &&
				indices.present_family.has_value()  &&
				indices.present_family.value() == indices.graphics_family.value())
			{
				break;
			}
		}

		return indices;
	}

	SwapchainSupportDetails query_swapchain_support(const vk::raii::PhysicalDevice& physical_device, const vk::raii::SurfaceKHR& surface)
	{
		SwapchainSupportDetails details;
		auto surface_handle = *surface;

		details.capabilities  = physical_device.getSurfaceCapabilitiesKHR(surface_handle);
		details.formats       = physical_device.getSurfaceFormatsKHR(surface_handle);
		details.present_modes = physical_device.getSurfacePresentModesKHR(surface_handle);

		return details;
	}

	PhysicalDeviceRate select_physical_device(const vk::raii::Instance& vk_instance, const vk::raii::SurfaceKHR& surface)
	{
		auto physical_devices = vk_instance.enumeratePhysicalDevices();

		if (physical_devices.empty()) {
			throw std::runtime_error("Failed to find GPUs with Vulkan support."); // NOSONAR
		}

		PhysicalDeviceRate best_device{ .device = nullptr, .score = 0, .indices = {} };

		for (const auto& device : physical_devices)
		{
			auto cur_device = rate_physical_device(device, surface);

			if (cur_device.score > best_device.score) {
				best_device = std::move(cur_device);
			}
		}

		if (best_device.score == 0) {
			throw std::runtime_error("Failed to find a suitable GPU with Luxel requirements."); // NOSONAR
		}

		return best_device;
	}

	void print_physical_device_info(const vk::PhysicalDevice& physical_device)
	{
		auto properties = physical_device.getProperties2();

		auto tipo_dispositivo = [](vk::PhysicalDeviceType type)
			{
				using enum vk::PhysicalDeviceType;
				switch (type)
				{
				case eIntegratedGpu: return "GPU integrada";
				case eDiscreteGpu: return "GPU dedicada";
				case eVirtualGpu: return "GPU virtual";
				case eCpu: return "CPU";
				default: return "Dispositivo desconocido";
				}
			};

		std::cout << std::format("Dispositivo: {}\n", properties.properties.deviceName.data());
		std::cout << std::format("Tipo:        {}\n", tipo_dispositivo(properties.properties.deviceType));
		std::cout << std::format("Vendor ID:   0x{:04X}\n", properties.properties.vendorID);
		std::cout << std::format("Device ID:   0x{:04X}\n", properties.properties.deviceID);
		std::cout << std::format("Vulkan:      {}.{}.{}\n", vk::versionMajor(properties.properties.apiVersion),
			vk::versionMinor(properties.properties.apiVersion),
			vk::versionPatch(properties.properties.apiVersion));
		std::cout << std::format("Driver:      {}\n", properties.properties.driverVersion);
	}

} // namespace luxel::vulkan
