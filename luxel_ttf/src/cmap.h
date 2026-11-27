#pragma once

// 'cmap': de carácter Unicode a número de glifo. 'A' (U+0041) no es "el glifo 65":
// cada fuente numera sus glifos a su manera (en Segoe UI la A es el 36; en Consolas,
// el 4). Esta tabla dice cuál es cuál.
//
// Una 'cmap' trae varias SUBTABLAS, una por plataforma y codificación (y cada una en
// un formato). Nos interesan dos:
//
//     formato 4   el plano básico (U+0000 a U+FFFF), por tramos. Lo tienen todas.
//     formato 12  todo Unicode (hasta U+10FFFF), por grupos. Arial lo tiene; Segoe UI
//                 y Consolas no (les basta con el 4: no traen nada más allá de U+FFFF).
//
// Se guarda la subtabla tal cual (sus tramos) y se busca en ella. Tu truetype/ la
// recorría entera al cargar y metía cada código en un unordered_map: decenas de miles
// de entradas para usar unas pocas.

#include "reader.h"
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace luxel::ttf::detail
{
	// Formato 4. Escenario con dos tramos (en una fuente de verdad hay cientos):
	//
	//     tramo  start   end     id_delta   id_range_offset
	//       0    0x0020  0x007E   -29        0          'espacio' a '~' seguidos
	//       1    0x00A0  0x00FF    0         X          los Latin-1 sueltos: en glyph_id_array
	//       2    0xFFFF  0xFFFF    1         0          el último, obligatorio: cierra la tabla
	//
	//   'A' = 0x41 -> tramo 0 (0x41 <= 0x7E) y 0x41 >= start -> 0x41 + (-29) = 36
	//
	// Con id_range_offset != 0 el glifo no sale de una suma: se lee de glyph_id_array.
	struct Format4
	{
		std::vector<uint16_t> end_codes;          // ordenados: se puede buscar con búsqueda binaria
		std::vector<uint16_t> start_codes;
		std::vector<int16_t>  id_deltas;
		std::vector<uint16_t> id_range_offsets;
		std::vector<uint16_t> glyph_id_array;     // lo que haya detrás de id_range_offsets

		// El glifo del carácter, o 0 (.notdef) si no está. TU PARTE (cmap_format4.cpp).
		uint16_t lookup(char32_t codepoint) const;
	};

	// Formato 12: grupos de caracteres seguidos con glifos seguidos.
	//   { start 0x1DF25, end 0x1DF2A, start_glyph 4645 }: U+1DF27 -> 4645 + 2 = 4647
	struct Format12
	{
		struct Group
		{
			uint32_t start;
			uint32_t end;
			uint32_t start_glyph;
		};
		std::vector<Group> groups;   // ordenados y sin solaparse

		uint32_t lookup(char32_t codepoint) const;
	};

	class CharacterMap
	{
	public:
		explicit CharacterMap(Format4 table) : m_table(std::move(table)) {}
		explicit CharacterMap(Format12 table) : m_table(std::move(table)) {}

		uint16_t format() const { return std::holds_alternative<Format4>(m_table) ? 4 : 12; }

		// 0 si no está o si la tabla apunta a un glifo que la fuente no tiene.
		uint16_t glyph_index(char32_t codepoint, uint16_t glyph_count) const;

	private:
		std::variant<Format4, Format12> m_table;
	};

	// La subtabla Unicode de ese formato (4 o 12), o vacío si la fuente no la tiene.
	std::optional<CharacterMap> read_cmap(Reader cmap, uint16_t wanted_format);

	// La mejor que haya: la 12 si existe (llega más lejos) y si no, la 4. Lanza si no
	// hay ninguna de las dos.
	CharacterMap read_best_cmap(Reader cmap);

} // namespace luxel::ttf::detail
