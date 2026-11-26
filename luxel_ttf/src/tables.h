#pragma once

// Las tablas básicas, cada una leída con su propio Reader (que no puede salirse de
// ella). Solo se guardan los campos que se usan: en tu truetype/ se leían todos,
// y algunos ni existen en todas las versiones (la 'maxp' de versión 0.5 solo tiene
// num_glyphs; leer los demás sería leer fuera).

#include "reader.h"
#include <luxel/ttf/font.h>
#include <cstdint>
#include <vector>

namespace luxel::ttf::detail
{
	struct Head
	{
		uint16_t units_per_em{};
		int16_t index_to_loc_format{};   // 0 = 'loca' corta (uint16), 1 = larga (uint32)
	};

	struct Hhea
	{
		int16_t ascent{};
		int16_t descent{};
		int16_t line_gap{};
		uint16_t number_of_h_metrics{};   // cuántas entradas completas tiene 'hmtx'
	};

	Head read_head(Reader reader);
	uint16_t read_maxp(Reader reader);   // el número de glifos
	Hhea read_hhea(Reader reader);
	std::vector<HorizontalMetrics> read_hmtx(Reader reader, uint16_t number_of_h_metrics, uint16_t glyph_count);
	std::vector<uint32_t> read_loca(Reader reader, int16_t index_to_loc_format, uint16_t glyph_count);

} // namespace luxel::ttf::detail
