#pragma once

#include <cstdint>

namespace vitreum
{
	struct Size
	{
		int width{};
		int height{};
	};

	struct Position
	{
		int xpos{};
		int ypos{};
	};

	struct Rect
	{
		int left{};
		int right{};
		int top{};
		int bottom{};
	};

	enum class NativePlatform{ Win32, X11, Wayland };

	struct NativeWindowHandle
	{
		NativePlatform platform;

		union
		{
			struct {
				void* hwnd;
				void* hinstance;
			} win32;

			struct {
				void* display;
				uint64_t window;
			} x11;

			struct {
				void* display;
				void* surface;
			} wayland;
		};
	};

} // namespace vitreum

