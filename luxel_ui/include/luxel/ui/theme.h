#pragma once

#include <luxel/bitmap_font.h>
#include <luxel/renderer2d.h>

namespace luxel::ui
{
	// El aspecto común de toda la UI (D7 de docs/ui-design.md). Uno por Ui.
	struct Theme
	{
		const BitmapFont* font{ nullptr };   // no es suya: debe vivir más que la Ui. nullptr = no se pinta texto
		float text_size{ 30.0f };            // alto de línea, en píxeles
		Color text_color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

} // namespace luxel::ui
