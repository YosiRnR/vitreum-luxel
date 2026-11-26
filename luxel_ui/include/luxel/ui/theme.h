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

	// Una caja de texto: el fondo según el estado, el cursor (la rayita donde se escribe)
	// y el margen del texto al canto.
	struct TextFieldStyle
	{
		BoxStyle normal{  .color = { 0.03f, 0.04f, 0.07f, 0.90f }, .corner_radius = 6.0f,
		                  .border_thickness = 1.0f, .border_color = { 1.0f, 1.0f, 1.0f, 0.35f } };
		BoxStyle hover{   .color = { 0.03f, 0.04f, 0.07f, 0.90f }, .corner_radius = 6.0f,
		                  .border_thickness = 1.0f, .border_color = { 1.0f, 1.0f, 1.0f, 0.70f } };
		BoxStyle focused{ .color = { 0.03f, 0.04f, 0.07f, 0.95f }, .corner_radius = 6.0f,
		                  .border_thickness = 2.0f, .border_color = { 1.0f, 0.85f, 0.35f, 1.0f } };
		Color caret{ 1.0f, 0.85f, 0.35f, 1.0f };
		float caret_width{ 2.0f };
		math::Vector2f padding{ 10.0f, 6.0f };
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
		TextFieldStyle text_field{};

		// El marco que rodea al widget con el foco cuando no tiene otra forma de
		// mostrarlo (un botón al que se llega con Tab), a "focus_ring_gap" px de él.
		BoxStyle focus_ring{ .color = { 0.0f, 0.0f, 0.0f, 0.0f }, .corner_radius = 9.0f,
		                     .border_thickness = 2.0f, .border_color = { 1.0f, 0.85f, 0.35f, 1.0f } };
		float focus_ring_gap{ 3.0f };
		float scroll_step{ 60.0f };          // píxeles que se mueve una lista por cada muesca de la rueda
	};

} // namespace luxel::ui
