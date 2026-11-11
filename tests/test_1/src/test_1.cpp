#include <vitreum/vitreum.h>
#include <luxel/luxel.h>
#include <iostream>
#include <cstdint>
#include <format>

static luxel::WindowHandle to_luxel(const vitreum::NativeWindowHandle& win_handle)
{
	luxel::WindowHandle result{};

	if (win_handle.platform == vitreum::NativePlatform::Win32)
	{
		result.platform = luxel::PlatformType::Windows;
		result.windows.hinstance = win_handle.win32.hinstance;
		result.windows.hwnd = win_handle.win32.hwnd;
	}
	else if (win_handle.platform == vitreum::NativePlatform::X11)
	{
		result.platform = luxel::PlatformType::Linux;
		result.linux.display = win_handle.x11.display;
		result.linux.window = win_handle.x11.window;
	}
	else
	{
	}

	return result;
}

int main(int argc, char** argv)
{
	vitreum::Window main_wnd{};
	main_wnd.create({ "Vitreum Window",{ 1920, 1080 } });

	auto render_device = luxel::create_device(luxel::DeviceDescriptor{
		.backend = luxel::Backend::Vulkan,
		.debug   = true
	});

	auto swapchain = render_device->create_swapchain({
		.window = to_luxel(main_wnd.native_handle()),
		.size   = { static_cast<uint32_t>(main_wnd.width()), static_cast<uint32_t>(main_wnd.height()) },
		.vsync  = true
	});

	auto extent       = swapchain->extent();
	auto color_format = swapchain->color_format();

	std::cout << std::format("      Extent: {} x {}\n", extent.width, extent.height);
	std::cout << std::format("Color Format: {}\n", static_cast<uint8_t>(color_format));

	main_wnd.set_size({ 800, 600 });

	main_wnd.show();

	while (!main_wnd.should_close())
	{
		main_wnd.poll_events();

		while (main_wnd.has_pending_events())
		{
			auto event = main_wnd.pop_event();

			if (event && event->type == vitreum::WinEventType::Close) {
				main_wnd.cancel_close(
					vitreum::message_box_okcancel(main_wnd.native_handle(), "¿Desea cerrar la ventana?", "WARNING")
				);
			}
			else if (event && event->type == vitreum::WinEventType::Resize) {
				swapchain->resize({ static_cast<uint32_t>(event->size.width), static_cast<uint32_t>(event->size.height) });
			}
		}

		auto* frame = render_device->begin_frame(*swapchain);
		if (frame == nullptr) {
			continue;
		}

		frame->commands().begin_rendering({ .clear = { 1.0f, 0.0f, 0.0f, 1.0f } });
		frame->commands().end_rendering();
		render_device->end_frame(*frame);

		static luxel::Size2D last{};
		auto now = swapchain->extent();
		if (now.width != last.width || now.height != last.height) {
			std::cout << std::format("Swapchain: {} x {}\n", now.width, now.height);
			last = now;
		}
	}

	render_device->wait_idle();
}
