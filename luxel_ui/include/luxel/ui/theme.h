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

	// Una barra de desplazamiento: el carril y el pulgar (lo que se arrastra).
	struct ScrollBarStyle
	{
		BoxStyle track{         .color = { 0.00f, 0.00f, 0.00f, 0.35f }, .corner_radius = 6.0f };
		BoxStyle thumb{         .color = { 0.32f, 0.35f, 0.41f, 1.00f }, .corner_radius = 6.0f };
		BoxStyle thumb_hover{   .color = { 0.48f, 0.52f, 0.60f, 1.00f }, .corner_radius = 6.0f };
		BoxStyle thumb_pressed{ .color = { 0.75f, 0.78f, 0.85f, 1.00f }, .corner_radius = 6.0f };
		float width{ 12.0f };
		float min_thumb{ 24.0f };   // en una lista larguísima el pulgar mediría 1 px: no habría quien lo agarrase
	};

	// El aspecto común de toda la UI (D7 de docs/ui-design.md). Uno por Ui.
	struct Theme
	{
		const BitmapFont* font{ nullptr };   // no es suya: debe vivir más que la Ui. nullptr = no se pinta texto
		float text_size{ 30.0f };            // alto de línea, en píxeles
		Color text_color{ 1.0f, 1.0f, 1.0f, 1.0f };
		math::Vector2f button_padding{ 24.0f, 9.0f };   // del texto al canto del botón: a los lados y arriba/abajo
		ButtonStyle button{};                // el de los botones que no traen uno propio
		ScrollBarStyle scroll_bar{};
		float scroll_step{ 60.0f };          // píxeles que se mueve una lista por cada muesca de la rueda
	};

} // namespace luxel::ui
