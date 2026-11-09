#pragma once

#include <luxel/types.h>
#include <cstdint>

namespace luxel
{
	// Formatos portables. Cada backend los traduce a su equivalente nativo.
	enum class Format : uint8_t
	{
		Undefined = 0,
		R8G8B8A8_UNorm,
		R8G8B8A8_SRGB,
		B8G8R8A8_UNorm,
		B8G8R8A8_SRGB,
		D32_Float,
		D24_UNorm_S8_UInt
	};

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
	};

} // namespace luxel
