#pragma once

// Las cuentas que dependen de hacia dónde crece la Y (hito R4a). Aparte y sin
// GPU para poder probarlas en tests/test_renderer2d.
//
// El Renderer2D trabaja en dos espacios:
//   - mundo (begin):  +Y hacia ARRIBA. y_sign = +1
//   - UI (begin_ui):  píxeles, (0,0) arriba a la izquierda, +Y hacia ABAJO. y_sign = -1
//
// Un solo número los distingue: y_sign es "cuánto cambia la Y al subir una
// unidad en pantalla". Por eso "subir" es siempre  y += y_sign * cantidad,
// y "bajar"  y -= y_sign * cantidad, en los dos espacios.

#include <luxel/math/vectors.h>
#include <luxel/math/matrix4.h>

namespace luxel::detail
{
	inline constexpr float k_y_up   = +1.0f;
	inline constexpr float k_y_down = -1.0f;

	// Proyección de la UI: del píxel (0,0)-(width,height) al NDC de luxel
	// (+Y arriba). Con 1280x720:
	//     (0, 0)       -> (-1, +1)   arriba a la izquierda
	//     (640, 360)   -> ( 0,  0)   centro
	//     (1280, 720)  -> (+1, -1)   abajo a la derecha
	// El truco es solo decir que "bottom" vale height y "top" vale 0: la Y sale
	// dada la vuelta sin ninguna cuenta aparte.
	inline math::Matrix4x4 ui_projection(float width, float height)
	{
		return math::Matrix4x4::orthographic(0.0f, width, height, 0.0f);
	}

	// Centro de un rectángulo a partir de su esquina de arriba a la izquierda.
	inline math::Vector2f rect_center(math::Vector2f top_left, math::Vector2f size, float y_sign)
	{
		return { top_left.x + size.x * 0.5f, top_left.y - y_sign * size.y * 0.5f };   // bajar medio alto
	}

	// Centro de la celda de una letra. pen es la esquina de arriba a la
	// izquierda del hueco de la letra; la celda empieza "pad" más a la izquierda
	// y "pad" más ARRIBA (ver el dibujo de bitmap_font.h).
	inline math::Vector2f glyph_center(math::Vector2f pen, float pad, float cell_w, float cell_h, float y_sign)
	{
		return rect_center({ pen.x - pad, pen.y + y_sign * pad }, { cell_w, cell_h }, y_sign);
	}

} // namespace luxel::detail
