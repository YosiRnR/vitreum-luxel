#include "vulkan_platform.h"
#include <cstdint>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#endif

namespace luxel::vulkan::platform
{
	std::vector<const char*> get_platform_instance_extensions()
	{
#if defined(_WIN32)
		return { vk::KHRSurfaceExtensionName, vk::KHRWin32SurfaceExtensionName };
#elif defined(__linux__)
		return { vk::KHRSurfaceExtensionName, vk::KHRXlibSurfaceExtensionName };
#else
		return { vk::KHRSurfaceExtensionName };
#endif
	}

	vk::raii::SurfaceKHR create_surface_for_window(const vk::raii::Instance& instance,
		const WindowHandle& window_handle)
	{
		using enum PlatformType;

		switch (window_handle.platform) {
		case Windows:
#if defined(_WIN32)
		{
			if (!window_handle.windows.hwnd) {
				return nullptr;
			}
			vk::Win32SurfaceCreateInfoKHR create_info{
				.hinstance = static_cast<HINSTANCE>(window_handle.windows.hinstance),
				.hwnd = static_cast<HWND>(window_handle.windows.hwnd)
			};
			return instance.createWin32SurfaceKHR(create_info);
		}
#else
			return nullptr;
#endif

		case Linux:
#if defined(__linux__)
		{
			if (!window_handle.x11.display) {
				return nullptr;
			}
			vk::XlibSurfaceCreateInfoKHR create_info{
				.dpy = static_cast<Display*>(window_handle.x11.display),
				.window = static_cast<::Window>(window_handle.x11.window)
			};
			return instance.createXlibSurfaceKHR(create_info);
		}
#else
			return nullptr;
#endif

			//		case Linux2:
			//#if defined(__linux__)
			//		{
			//			if (!window_handle.wayland.display || !window_handle.wayland.surface) {
			//				return nullptr;
			//			}
			//			// wl_surface en un union con pointers opacos: hay que reinterpretar
			//			// porque el union de WindowHandle almacena void*.
			//			vk::WaylandSurfaceCreateInfoKHR create_info{
			//				.display = static_cast<wl_display*>(window_handle.wayland.display),
			//				.surface = static_cast<wl_surface*>(window_handle.wayland.surface)
			//			};
			//			return instance.createWaylandSurfaceKHR(create_info);
			//		}
			//#else
			//			return nullptr;
			//#endif

		case Unknown:
		default:
			return nullptr;
		}
	}

	bool queue_family_can_present(const vk::raii::PhysicalDevice& physical_device, uint32_t index)
	{
#if defined(_WIN32)
		return physical_device.getWin32PresentationSupportKHR(index);
#elif defined(__linux__)
		return false;
#endif
	}

} // namespace luxel::vulkan::platform
