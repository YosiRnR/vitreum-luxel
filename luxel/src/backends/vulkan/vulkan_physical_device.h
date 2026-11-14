#pragma once

#include <luxel/device.h>
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

	struct PhysicalDeviceRate
	{
		vk::raii::PhysicalDevice device;
		uint32_t score{ 0 };
		QueueFamilyIndices indices{};
	};

	QueueFamilyIndices find_queue_families(const vk::raii::PhysicalDevice&);
	PhysicalDeviceRate select_physical_device(const vk::raii::Instance&, const RequestedDeviceMaximums&);
	void print_physical_device_info(const vk::PhysicalDevice&);

} // namespace luxel::vulkan
