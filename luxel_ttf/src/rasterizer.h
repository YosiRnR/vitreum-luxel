#pragma once

// El rasterizador por dentro (F5 de docs/ttf-design.md). Dos pasos:
//
// 1. Cada recta deja en un buffer de float, fila a fila, "cuánto CAMBIA la cobertura"
//    al pasar de un píxel al siguiente, con signo: + si la recta baja, - si sube.
// 2. Una suma acumulada por fila convierte esos cambios en cobertura.
//
// Escenario: un cuadrado de x = 1 a x = 3 en una fila de 5 píxeles. Su lado derecho
// baja y su lado izquierdo sube (o al revés: da igual, al final hay valor absoluto):
//
//     cambios       0   +1    0   -1    0    (el lado de x = 1 y el de x = 3)
//     suma          0    1    1    0    0    -> los píxeles 1 y 2, tapados
//
// Si el lado izquierdo estuviera en x = 1,5, en vez de +1 en el píxel 1 dejaría +0,5
// en el 1 y +0,5 en el 2: suma 0 | 0,5 | 1 | 0 | 0. El píxel 1, medio tapado.

#include <luxel/ttf/outline.h>
#include <cstdint>
#include <span>
#include <vector>

namespace luxel::ttf::detail
{
	// TU PARTE (src/coverage_piece.cpp): un trozo de recta que cruza UNA fila,
	// entrando por arriba en x = x_top y saliendo por abajo en x = x_bottom, deja sus
	// cambios en "row". "height" es la altura del trozo dentro de la fila (de 0 a 1:
	// 1 si la cruza entera) con el signo de la recta (+ baja, - sube).
	//
	// Lo que debe cumplirse, para cada píxel i: la suma row[0] + ... + row[i] crece en
	// height * (la parte del píxel i que queda A LA DERECHA del trozo). Así, lo que
	// queda a la derecha del todo crece en height, y la suma de todo lo que deja es
	// exactamente height.
	//
	// Garantías de quien llama: 0 <= x_top, x_bottom <= row.size() - 2 (siempre hay
	// una casilla más a la derecha del último píxel que toca).
	void accumulate_piece(std::span<float> row, float x_top, float x_bottom, float height);

	class Rasterizer
	{
	public:
		Rasterizer(uint32_t width, uint32_t height);

		// Coordenadas en píxeles de la imagen: (0, 0) es la esquina de arriba a la
		// izquierda del píxel (0, 0), y (width, height), la de abajo a la derecha.
		void draw_line(Point from, Point to);
		void draw_quadratic(Point from, Point control, Point to);

		// Paso 2: suma por filas, valor absoluto, tope en 1 y a bytes.
		std::vector<uint8_t> coverage() const;

		uint32_t width() const { return m_width; }
		uint32_t height() const { return m_height; }

	private:
		std::span<float> row(uint32_t y);

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_stride;               // width + 2: dos casillas de más por fila
		std::vector<float> m_changes;    // m_stride * height
	};

} // namespace luxel::ttf::detail
