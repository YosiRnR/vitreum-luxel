#pragma once

// El contorno de un glifo listo para rasterizar (T4): rectas y curvas cuadráticas en
// PÍXELES, con +Y hacia abajo (como la UI y las imágenes) y el origen en el lápiz:
// x = 0 donde empieza la letra e y = 0 en la línea de base. Así la parte de arriba
// de una "l" tiene y negativa y la cola de una "p", positiva.
//
//        y = -35,5 ─  ██         "l" de Segoe UI negrita a 48 px por em
//                     ██         (en la fuente: x de 133 a 449, y de 0 a 1516,
//                     ██          con 2048 unidades por em: escala 48 / 2048)
//      y = 0 (base) ─ ██ ─────
//                     x = 3,1

#include <cstdint>
#include <vector>

namespace luxel::ttf
{
	struct Point
	{
		float x{};
		float y{};

		bool operator==(const Point&) const = default;
	};

	// Una recta de "from" a "to" o, si is_curve, una Bézier cuadrática que sale de
	// "from" hacia "control" y acaba en "to" (sin pasar por "control").
	struct OutlineSegment
	{
		Point from;
		Point control;   // solo si is_curve
		Point to;
		bool is_curve{ false };
	};

	struct Outline
	{
		std::vector<OutlineSegment> segments;
		// Dónde acaba cada contorno: el contorno i son segments[ends[i-1], ends[i]).
		// Cada uno es cerrado: el "to" del último es el "from" del primero.
		std::vector<uint32_t> contour_ends;
		// La caja que lo contiene (puntos de control incluidos), en píxeles.
		Point min{};
		Point max{};

		bool empty() const { return segments.empty(); }
	};

} // namespace luxel::ttf
