#include "cmap.h"
#include <algorithm>
#include <iterator>

namespace luxel::ttf::detail
{
	// ------------------------------------------------------------------------
	// TODO (T2): tu parte. El glifo de un carácter en una subtabla de formato 4.
	//
	// Ya lo hiciste en truetype/ (CmapTable::read_format4), pero al revés: allí
	// recorrías TODOS los caracteres de cada tramo para meterlos en un mapa. Aquí te
	// dan UN carácter y hay que encontrar su tramo y su glifo.
	//
	// Pasos:
	//   1. Si el carácter pasa de 0xFFFF, no está (el formato 4 no llega más allá).
	//   2. Su tramo: el PRIMERO cuyo end_code es >= codepoint. end_codes está ordenado:
	//      no hace falta recorrerlo entero (pista: <algorithm> tiene una función que
	//      hace justo eso en log2(n) pasos; 184 tramos de Segoe UI -> 8 pasos).
	//      Si no hay ninguno, o el tramo empieza DESPUÉS del carácter (cae en un
	//      hueco entre tramos), no está.
	//   3. Con id_range_offset == 0: glifo = codepoint + id_delta, "módulo 65536" (la
	//      suma da la vuelta en 16 bits: -29 se suma como 65507).
	//   4. Si no, el glifo se lee de glyph_id_array. Tu fórmula de truetype/ está
	//      bien (con i = número de tramo y n = número de tramos):
	//
	//          índice = id_range_offset[i] / 2 + (codepoint - start_code[i]) - (n - i)
	//
	//      Si el índice se sale de glyph_id_array, no está. Si lo leído es 0, no
	//      está; si no, se le suma id_delta (también módulo 65536).
	//
	// Por qué esa fórmula tan rara: en el fichero, id_range_offset[i] es la distancia
	// en BYTES desde ESA casilla de idRangeOffset hasta el glifo. Desde la casilla i
	// hasta el final de idRangeOffset quedan (n - i) casillas, y glyphIdArray va justo
	// detrás. Así que, contando en uint16 desde el principio de glyphIdArray, hay
	// que restar esas (n - i).
	//
	// Escenarios (caso 6 de test_ttf): en Segoe UI negrita 'A' -> 36, 'ñ' -> 120,
	// '€' -> 189, U+1F600 -> 0. Y el caso 7 compara tu formato 4 con el formato 12 de
	// Arial en los 65.536 caracteres del plano básico: tienen que coincidir todos.
	// ------------------------------------------------------------------------

	uint16_t Format4::lookup(char32_t codepoint) const
	{
		if (codepoint > 0xFFFF) {
			return 0;
		}

		auto tramo = std::ranges::lower_bound(end_codes, codepoint);

		if (tramo == end_codes.end()) {
			return 0;
		}

		std::size_t i = static_cast<std::size_t>(std::distance(end_codes.begin(), tramo));

		if (codepoint < start_codes[i]) {
			return 0;
		}

		uint16_t glyph_id{ 0 };
		if (id_range_offsets[i] != 0)
		{
			auto idx = static_cast<std::ptrdiff_t>(id_range_offsets[i] / 2) +
				       static_cast<std::ptrdiff_t>((codepoint - start_codes[i])) -
				       static_cast<std::ptrdiff_t>((end_codes.size() - i));

			if (idx < 0 || idx >= std::ssize(glyph_id_array)) {
				return 0;
			}

			glyph_id = glyph_id_array[idx];

			if (glyph_id != 0) {
				glyph_id = static_cast<uint16_t>(glyph_id + id_deltas[i]);
			}
		}
		else {
			glyph_id = static_cast<uint16_t>(codepoint + id_deltas[i]);
		}

		return glyph_id;
	}

} // namespace luxel::ttf::detail
