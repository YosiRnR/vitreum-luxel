#pragma once

#include <luxel/bitmap_font.h>
#include <luxel/renderer2d.h>

namespace luxel::ui
{
	// Aspecto de un rectángulo: lo que ofrece Renderer2D::draw_rounded_rect.
	struct BoxStyle
	{
		Color color{ 0.0f, 0.0f, 0.0f, 0.0f };
		float corner_radius{ 0.0f };
		float border_thickness{ 0.0f };
		Color border_color{ 0.0f, 0.0f, 0.0f, 1.0f };
	};

	// Los tres aspectos de un botón.
	struct ButtonStyle
	{
		BoxStyle normal{  .color = { 0.22f, 0.24f, 0.28f, 1.0f }, .corner_radius = 6.0f };
		BoxStyle hover{   .color = { 0.32f, 0.35f, 0.41f, 1.0f }, .corner_radius = 6.0f };
		BoxStyle pressed{ .color = { 0.13f, 0.14f, 0.17f, 1.0f }, .corner_radius = 6.0f };
	};

	// El aspecto común de toda la UI (D7 de docs/ui-design.md). Uno por Ui.
	struct Theme
	{
		const BitmapFont* font{ nullptr };   // no es suya: debe vivir más que la Ui. nullptr = no se pinta texto
		float text_size{ 30.0f };            // alto de línea, en píxeles
		Color text_color{ 1.0f, 1.0f, 1.0f, 1.0f };
		ButtonStyle button{};                // el de los botones que no traen uno propio
	};

} // namespace luxel::ui
