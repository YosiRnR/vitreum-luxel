#include <vitreum/vitreum.h>
#include <luxel/luxel.h>
// Si lo incluyes no compila porque no encuentra el archivo de origen, así que Vulkan está oculto al test
//#include <vulkan/vulkan.hpp>

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
		.debug   = true,
		.window  = to_luxel(main_wnd.native_handle())
	});

	main_wnd.show();

	while (!main_wnd.should_close())
	{
		main_wnd.poll_events();

		while (main_wnd.has_pending_events())
		{
			auto event = main_wnd.pop_event();

			if (event && event->type == vitreum::WinEventType::Close) {
				main_wnd.cancel_close(vitreum::message_box_okcancel(main_wnd.native_handle(), "¿Desea cerrar la ventana?", "WARNING"));
			}
		}
	}
}
