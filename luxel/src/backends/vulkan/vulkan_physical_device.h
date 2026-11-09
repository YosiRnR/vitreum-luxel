#pragma once

#include "vulkan_common.h"
#include <optional>
#include <cstdint>
#include <vector>
#include <array>

namespace luxel::vulkan
{
	inline constexpr std::array k_required_device_extensions = { vk::KHRSwapchainExtensionName };

	using RequiredFeatures = vk::StructureChain<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan12Features,
		vk::PhysicalDeviceVulkan13Features>;

	RequiredFeatures make_required_features();

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphics_family;
		std::optional<uint32_t> present_family;
		bool is_complete() const {
			return graphics_family.has_value() && present_family.has_value();
		}
	};

	struct SwapchainSupportDetails
	{
		vk::SurfaceCapabilitiesKHR       capabilities;
		std::vector<vk::SurfaceFormatKHR>     formats;
		std::vector<vk::PresentModeKHR> present_modes;

		// Un swapchain es adecuado si tiene al menos un formato de color y un modo de presentación compatibles
		bool is_adequate() const {
			return !formats.empty() && !present_modes.empty();
		}
	};

	struct PhysicalDeviceRate
	{
		vk::raii::PhysicalDevice device;
		uint32_t score{ 0 };
		QueueFamilyIndices indices{};
	};

	QueueFamilyIndices find_queue_families(const vk::raii::PhysicalDevice&, const vk::raii::SurfaceKHR&);
	SwapchainSupportDetails query_swapchain_support(const vk::raii::PhysicalDevice&, const vk::raii::SurfaceKHR&);
	PhysicalDeviceRate select_physical_device(const vk::raii::Instance&, const vk::raii::SurfaceKHR&);
	void print_physical_device_info(const vk::PhysicalDevice&);

} // namespace luxel::vulkan
