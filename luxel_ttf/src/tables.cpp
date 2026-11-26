#include "tables.h"
#include <stdexcept>

namespace luxel::ttf::detail
{
	namespace
	{
		[[noreturn]] void invalid(const char* problem)
		{
			throw std::runtime_error(std::string("ttf: ") + problem);
		}
	}

	// head: 54 bytes. Los campos que no se usan se saltan, pero se cuenta cuántos
	// bytes ocupan para llegar a los que sí.
	Head read_head(Reader reader)
	{
		reader.skip(4 + 4 + 4);                 // version, fontRevision, checksumAdjustment
		if (reader.u32() != 0x5F0F3CF5) {       // magicNumber: si no está, no es una 'head' de verdad
			invalid("'head' sin el número mágico");
		}
		reader.skip(2);                         // flags
		Head head{};
		head.units_per_em = reader.u16();
		reader.skip(8 + 8 + 4 * 2 + 2 + 2 + 2); // created, modified, bbox, macStyle, lowestRecPPEM, fontDirectionHint
		head.index_to_loc_format = reader.i16();

		// La especificación dice de 16 a 16384. Un 0 haría dividir entre cero al escalar.
		if (head.units_per_em < 16 || head.units_per_em > 16384) {
			invalid("'head' con units_per_em fuera de rango");
		}
		if (head.index_to_loc_format != 0 && head.index_to_loc_format != 1) {
			invalid("'head' con un formato de 'loca' desconocido");
		}
		return head;
	}

	uint16_t read_maxp(Reader reader)
	{
		reader.skip(4);   // version (0.5 en fuentes CFF, 1.0 en TrueType)
		const uint16_t glyph_count = reader.u16();
		if (glyph_count == 0) {
			invalid("'maxp' sin glifos");   // el 0 (.notdef) tiene que existir siempre
		}
		return glyph_count;
	}

	Hhea read_hhea(Reader reader)
	{
		reader.skip(4);                    // version
		Hhea hhea{};
		hhea.ascent   = reader.i16();
		hhea.descent  = reader.i16();
		hhea.line_gap = reader.i16();
		reader.skip(2 + 2 + 2 + 2 + 2 + 2 + 2 + 4 * 2 + 2);   // advanceWidthMax ... metricDataFormat
		hhea.number_of_h_metrics = reader.u16();
		return hhea;
	}

	// hmtx tiene dos partes. Escenario: una fuente monoespaciada de 3000 glifos en la
	// que todos avanzan lo mismo. Repetir el avance 3000 veces es tirar bytes, así que:
	//
	//     [avance, lsb] x number_of_h_metrics     (las primeras, completas)
	//     [lsb]         x el resto                (estos repiten el ÚLTIMO avance)
	//
	// Consolas, por ejemplo, podría tener number_of_h_metrics = 1. (Tu truetype/ leía
	// solo la primera parte y, para el resto, usaba el último avance con lsb 0.)
	std::vector<HorizontalMetrics> read_hmtx(Reader reader, uint16_t number_of_h_metrics, uint16_t glyph_count)
	{
		if (number_of_h_metrics == 0 || number_of_h_metrics > glyph_count) {
			invalid("'hhea' con un número de entradas de 'hmtx' imposible");
		}
		std::vector<HorizontalMetrics> metrics(glyph_count);
		for (uint16_t i = 0; i < number_of_h_metrics; ++i)
		{
			metrics[i].advance           = reader.u16();
			metrics[i].left_side_bearing = reader.i16();
		}
		for (uint32_t i = number_of_h_metrics; i < glyph_count; ++i)
		{
			metrics[i].advance           = metrics[number_of_h_metrics - 1].advance;
			metrics[i].left_side_bearing = reader.i16();
		}
		return metrics;
	}

	// loca: glyph_count + 1 offsets dentro de 'glyf'. El glifo i ocupa los bytes
	// [loca[i], loca[i + 1]); si son iguales, no tiene contornos (el espacio).
	//
	// Copiada TAL CUAL de tu truetype/ (LocaTable::read). Es el reto de T1: hay un
	// fallo que con Segoe UI no se nota, y el caso 3 de test_ttf lo encuentra.
	std::vector<uint32_t> read_loca(Reader reader, int16_t index_to_loc_format, uint16_t glyph_count)
	{
		std::vector<uint32_t> loca_offsets(static_cast<size_t>(glyph_count) + 1);

		// short offsets
		if (index_to_loc_format == 0)
		{
			for (uint32_t i = 0; i <= glyph_count; ++i) {
				loca_offsets[i] = static_cast<uint32_t>(reader.u16()) * 2;
			}
		}
		// long offsets
		else
		{
			for (uint32_t i = 0; i <= glyph_count; ++i) {
				loca_offsets[i] = reader.u32();
			}
		}
		return loca_offsets;
	}

} // namespace luxel::ttf::detail
