#pragma once

// El rasterizador (T4): de un Outline (rectas y cuadráticas en píxeles) a una imagen
// de cobertura, un byte por píxel: 0 = la letra no lo toca, 255 = lo tapa entero,
// 128 = tapa la mitad de su área. Es lo que irá en el canal alfa del atlas (T5).
//
// La cobertura es EXACTA (el área de cada píxel que queda dentro de la letra), no
// muestras: ver F5 en docs/ttf-design.md. Relleno "non-zero": dos contornos que se
// pisan no se restan, y un agujero (el de la "o") va en sentido contrario y sí resta.

#include <luxel/ttf/outline.h>
#include <cstdint>
#include <vector>

namespace luxel::ttf
{
	// La imagen justa que contiene la letra. "left" y "top" dicen dónde cae su píxel
	// (0, 0) respecto al lápiz, con las mismas reglas que Outline (+Y abajo, y = 0 en
	// la línea de base). Escenario: la "l" de Segoe UI negrita a 48 px va de x = 3,1 a
	// 10,5 y de y = -35,5 a 0, así que left = 3, top = -36, 8 x 36 píxeles.
	struct GlyphBitmap
	{
		uint32_t width{};
		uint32_t height{};
		int32_t left{};
		int32_t top{};
		std::vector<uint8_t> coverage;   // width * height, fila a fila desde arriba

		bool empty() const { return coverage.empty(); }
		uint8_t at(uint32_t x, uint32_t y) const { return coverage[y * width + x]; }
	};

	// Un Outline vacío (el espacio) da un GlyphBitmap vacío.
	GlyphBitmap rasterize(const Outline& outline);

} // namespace luxel::ttf
