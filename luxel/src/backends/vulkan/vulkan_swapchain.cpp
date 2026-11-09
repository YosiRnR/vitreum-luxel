#include <luxel/types.h>
#include "vulkan_swapchain.h"
#include "vulkan_device.h"
#include "vulkan_platform.h"
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <limits>

namespace luxel::vulkan
{
	namespace
	{
		inline vk::Format to_vk_format(Format format)
		{
			switch (format)
			{
			case Format::R8G8B8A8_UNorm:    return vk::Format::eR8G8B8A8Unorm;
			case Format::R8G8B8A8_SRGB:     return vk::Format::eR8G8B8A8Srgb;
			case Format::B8G8R8A8_UNorm:    return vk::Format::eB8G8R8A8Unorm;
			case Format::B8G8R8A8_SRGB:     return vk::Format::eB8G8R8A8Srgb;
			case Format::D32_Float:         return vk::Format::eD32Sfloat;
			case Format::D24_UNorm_S8_UInt: return vk::Format::eD24UnormS8Uint;
			default:                        return vk::Format::eUndefined;
			}
		}

		inline Format from_vk_format(vk::Format format)
		{
			switch (format)
			{
			case vk::Format::eR8G8B8A8Unorm:  return Format::R8G8B8A8_UNorm;
			case vk::Format::eR8G8B8A8Srgb:   return Format::R8G8B8A8_SRGB;
			case vk::Format::eB8G8R8A8Unorm:  return Format::B8G8R8A8_UNorm;
			case vk::Format::eB8G8R8A8Srgb:   return Format::B8G8R8A8_SRGB;
			case vk::Format::eD32Sfloat:      return Format::D32_Float;
			case vk::Format::eD24UnormS8Uint: return Format::D24_UNorm_S8_UInt;
			default:                          return Format::Undefined;
			}
		}

		vk::SurfaceFormatKHR choose_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats)
		{
			for (const auto& available_format : available_formats)
			{
				if ((available_format.format == vk::Format::eB8G8R8A8Srgb || available_format.format == vk::Format::eR8G8B8A8Srgb) &&
					available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
					return available_format;
				}
			}

			// Si no encuentra el ideal, nos conformamos con el primero que ofrezca la pantalla
			return available_formats[0];
		}

		vk::PresentModeKHR choose_present_mode(const std::vector<vk::PresentModeKHR>& available_present_modes, bool vsync)
		{
			if (vsync)
			{
				for (const auto& available_mode : available_present_modes)
				{
					if (available_mode == vk::PresentModeKHR::eFifo) {
						return available_mode;
					}
				}
			}
			else {
				for (const auto& available_mode : available_present_modes)
				{
					if (available_mode == vk::PresentModeKHR::eMailbox) {
						return available_mode;
					}
				}
			}

			return vk::PresentModeKHR::eFifo;
		}

		vk::Extent2D choose_extent(const vk::SurfaceCapabilitiesKHR& capabilities, Size2D window_size)
		{
			if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
				return capabilities.currentExtent;
			}
			else
			{
				// Si la pantalla nos da flexibilidad, forzamos los píxeles reales adaptándonos 
				// al Size2D que nos ha enviado la ventana cliente, respetando los límites de la GPU.
				vk::Extent2D actual_extent{ window_size.width, window_size.height };

				actual_extent.width  = std::clamp(actual_extent.width , capabilities.minImageExtent.width , capabilities.maxImageExtent.width);
				actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

				return actual_extent;
			}
		}

	} // namespace

	VulkanSwapchain::VulkanSwapchain(const Ref<VulkanDevice>& vk_device, const SwapchainDescriptor& swapchain_desc)
		: m_ref_device(vk_device)
	{
		m_surface = platform::create_surface_for_window(m_ref_device->instance(), swapchain_desc.window);

		if (!*m_surface) {
			throw std::runtime_error("No se pudo crear la surface para la ventana");
		}

		if (!m_ref_device->physical_device().getSurfaceSupportKHR(m_ref_device->present_family_index(), *m_surface)) {
			throw std::runtime_error("La cola de presentación no está disponible");
		}

		auto capabilities  = m_ref_device->physical_device().getSurfaceCapabilitiesKHR(*m_surface);
		auto formats       = m_ref_device->physical_device().getSurfaceFormatsKHR(*m_surface);
		auto present_modes = m_ref_device->physical_device().getSurfacePresentModesKHR(*m_surface);

		vk::SurfaceFormatKHR surface_format = choose_surface_format(formats);
		m_format = surface_format.format;
		vk::PresentModeKHR present_mode = choose_present_mode(present_modes, swapchain_desc.vsync);
		m_extent = choose_extent(capabilities, swapchain_desc.size);

		uint32_t image_count = capabilities.maxImageCount > 0 ? std::min(capabilities.minImageCount + 1, capabilities.maxImageCount) : capabilities.minImageCount + 1;

		// Configurar la estructura de creación del Swapchain
		vk::SwapchainCreateInfoKHR create_info{
			.surface          = *m_surface,
			.minImageCount    = image_count,
			.imageFormat      = surface_format.format,
			.imageColorSpace  = surface_format.colorSpace,
			.imageExtent      = m_extent,
			.imageArrayLayers = 1,
			.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment
		};

		if (m_ref_device->present_family_index() == m_ref_device->graphics_family_index()) {
			create_info.imageSharingMode = vk::SharingMode::eExclusive;
		}
		else {
			create_info.imageSharingMode = vk::SharingMode::eConcurrent;
		}

		create_info.preTransform   = capabilities.currentTransform;
		create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		create_info.presentMode    = present_mode;
		create_info.clipped        = vk::True;

		// Crear el swapchain real
		m_swapchain = vk::raii::SwapchainKHR(m_ref_device->logical_device(), create_info);

		m_images = m_swapchain.getImages();

		for (const auto& image : m_images)
		{
			vk::ImageViewCreateInfo view_info{
				.image    = image,
				.viewType = vk::ImageViewType::e2D,
				.format   = m_format,
				.components = {
					.r = vk::ComponentSwizzle::eIdentity,
					.g = vk::ComponentSwizzle::eIdentity,
					.b = vk::ComponentSwizzle::eIdentity,
					.a = vk::ComponentSwizzle::eIdentity
				},
				.subresourceRange = {
					.aspectMask     = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel   = 0,
					.levelCount     = 1,
					.baseArrayLayer = 0,
					.layerCount     = 1
				}
			};
			m_image_views.emplace_back(m_ref_device->logical_device(), view_info);
		}
	}

	VulkanSwapchain::~VulkanSwapchain() = default;

	Size2D VulkanSwapchain::extent() const {
		return Size2D{ .width = m_extent.width, .height = m_extent.height };
	}

	Format VulkanSwapchain::color_format() const {
		return from_vk_format(m_format);
	}

	void VulkanSwapchain::resize(Size2D)
	{
	}

} // namespace luxel::vulkan
