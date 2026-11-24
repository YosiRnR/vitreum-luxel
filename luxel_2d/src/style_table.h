#pragma once

// Tabla de estilos del Renderer2D (hito R4b). Aparte y solo con std para poder
// probarla sin GPU en tests/test_renderer2d.
//
// Un "estilo" es lo que un sprite normal no necesita: el radio de las esquinas
// (y, más adelante, el borde y el recorte). En vez de meterlo en cada quad, va en
// una tabla aparte y el quad solo lleva su número:
//
//     quads:    [sprite 0] [botón 1] [botón 1] [panel 2] [letra 0]
//     estilos:  1: radio 8     2: radio 16
//
// El 0 significa "sin estilo": ni siquiera se mira la tabla. Los demás
// empiezan en 1, así que el estilo n está en la posición n - 1 del array.

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace luxel::detail
{
	inline constexpr uint32_t k_no_style = 0;

	// Añade un estilo a la tabla del lote y devuelve su número. Si es igual que
	// el último, lo reutiliza: 5 botones iguales seguidos comparten un estilo.
	// Solo se mira el último (y no toda la tabla) para que añadir sea inmediato.
	template <typename Style>
	uint32_t add_style(std::vector<Style>& styles, const Style& style)
	{
		if (styles.empty() || !(styles.back() == style)) {
			styles.push_back(style);
		}
		return static_cast<uint32_t>(styles.size());   // posición + 1
	}

	// Prepara la tabla de UN draw: deja en "draw_styles" solo los estilos que usan
	// estos quads y cambia el style_index de cada quad para que apunte ahí.
	//
	// Por qué hace falta: un lote enorme se parte en varios draws, y lo que
	// sube cada uno tiene que caber en 1 MB. Escenario: 40.000 botones con 40.000
	// radios distintos. La tabla entera no cabe, pero cada draw lleva 16.384 quads
	// como mucho, así que usa 16.384 estilos como mucho, y esos sí caben.
	//
	//     quads del draw:  estilos 7, 0, 9, 7
	//     draw_styles:     { estilo 7, estilo 9 }
	//     quads después:   1, 0, 2, 1
	//
	// remap es memoria de trabajo (se reutiliza entre llamadas): remap[n] es el
	// número nuevo del estilo n, o 0 si aún no ha salido en este draw.
	template <typename Quad, typename Style>
	void compact_styles(std::span<Quad> quads, const std::vector<Style>& styles,
	                    std::vector<uint32_t>& remap, std::vector<Style>& draw_styles)
	{
		draw_styles.clear();
		remap.assign(styles.size() + 1, k_no_style);

		for (Quad& quad : quads)
		{
			if (quad.style_index == k_no_style) {
				continue;
			}
			uint32_t& new_index = remap[quad.style_index];
			if (new_index == k_no_style) {
				draw_styles.push_back(styles[quad.style_index - 1]);
				new_index = static_cast<uint32_t>(draw_styles.size());
			}
			quad.style_index = new_index;
		}
	}

} // namespace luxel::detail
