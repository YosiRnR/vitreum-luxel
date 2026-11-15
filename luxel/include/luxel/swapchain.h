#pragma once

#include <luxel/types.h>
#include <cstdint>

namespace luxel
{
	struct SwapchainDescriptor
	{
		WindowHandle window;
		Size2D size;
		bool vsync{ true };
	};

	class Swapchain : public RefCounted
	{
	public:
		virtual ~Swapchain() = default;

		virtual Size2D extent() const = 0;
		virtual Format color_format() const = 0;
		virtual void   resize(Size2D) = 0;

		virtual bool   recreate() = 0;
	};

} // namespace luxel
