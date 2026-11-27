#include "glyf.h"

namespace luxel::ttf::detail
{
	// ------------------------------------------------------------------------
	// Dos funciones pequeñas para los glifos compuestos (escritas por el usuario en T3).
	//
	// 1. f2dot14: las escalas de las piezas vienen como F2Dot14, un número con coma
	//    FIJA en 16 bits: los 2 bits de arriba son la parte entera (con signo) y los 14
	//    de abajo, los decimales. Dicho de otra forma: el número de verdad es el entero
	//    de 16 bits con signo dividido entre 2^14. Escenarios:
	//
	//        0x4000 ->  1.0          0x2000 -> 0.5
	//        0xC000 -> -1.0          0x7FFF -> 1.99993896...
	//
	// 2. append_component: añade a "parent" los puntos de "child" pasados por la
	//    transformación (la fórmula está en ComponentTransform, en glyf.h) y sus
	//    contornos.
	//
	//    La trampa está en contour_ends: guardan NÚMEROS DE PUNTO, y los puntos del
	//    hijo ya no empiezan en 0 dentro del padre. Escenario, la "ñ":
	//
	//        la 'n' (18 puntos, 1 contorno):         ends = { 17 }
	//        la virgulilla (20 puntos, 1 contorno):  ends = { 19 }
	//        la 'ñ' = n + virgulilla (38 puntos):    ends = { 17, ??? }
	//
	// Los prueba el caso 10 de test_ttf (piezas hechas a mano, con escala y sin ella) y
	// el 9, que compara la "ñ" con la "n" y su virgulilla.
	// ------------------------------------------------------------------------

	float f2dot14(int16_t raw)
	{
		return static_cast<float>(raw) / static_cast<float>(1 << 14);
	}

	void append_component(GlyphShape& parent, const GlyphShape& child, const ComponentTransform& t)
	{
		auto start_point = parent.points.size();

		for (const auto& p : child.points)
		{
			const float x = t.a * p.x + t.c * p.y + t.dx;
			const float y = t.b * p.x + t.d * p.y + t.dy;

			parent.points.push_back({ x, y, p.on_curve });
		}

		for (const auto& contour_end : child.contour_ends) {
			parent.contour_ends.push_back(start_point + contour_end);
		}
	}

} // namespace luxel::ttf::detail
