#include <luxel/ttf/font.h>
#include "cmap.h"
#include "glyf.h"
#include "outline_builder.h"
#include "reader.h"
#include "tables.h"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace luxel::ttf
{
	Font::Font(std::span<const std::byte> data)
		: m_data(data.begin(), data.end())
	{
		read_directory();

		const detail::Head head = detail::read_head(Reader(required(tag("head"), "'head'"), "'head'"));
		m_units_per_em = head.units_per_em;
		m_glyph_count  = detail::read_maxp(Reader(required(tag("maxp"), "'maxp'"), "'maxp'"));

		const detail::Hhea hhea = detail::read_hhea(Reader(required(tag("hhea"), "'hhea'"), "'hhea'"));
		m_ascent   = hhea.ascent;
		m_descent  = hhea.descent;
		m_line_gap = hhea.line_gap;
		m_metrics  = detail::read_hmtx(Reader(required(tag("hmtx"), "'hmtx'"), "'hmtx'"),
		                               hhea.number_of_h_metrics, m_glyph_count);

		m_loca = detail::read_loca(Reader(required(tag("loca"), "'loca'"), "'loca'"),
		                           head.index_to_loc_format, m_glyph_count);

		// Que 'loca' tenga sentido: cada glifo empieza donde acaba el anterior o más
		// allá (nunca antes), y el último no se sale de 'glyf'. Así location() ya no
		// tiene que comprobar nada.
		const std::size_t glyf_size = required(tag("glyf"), "'glyf'").size();
		if (!std::ranges::is_sorted(m_loca) || m_loca.back() > glyf_size) {
			throw std::runtime_error("ttf: 'loca' con offsets imposibles");
		}

		m_cmap = std::make_unique<const detail::CharacterMap>(
			detail::read_best_cmap(Reader(required(tag("cmap"), "'cmap'"), "'cmap'")));
	}

	// Aquí y no en la cabecera: para destruir un unique_ptr<CharacterMap> hace falta
	// el tipo completo, que solo se ve en este .cpp.
	Font::~Font() = default;
	Font::Font(Font&&) noexcept = default;
	Font& Font::operator=(Font&&) noexcept = default;

	uint16_t Font::glyph_index(char32_t codepoint) const
	{
		return m_cmap->glyph_index(codepoint, m_glyph_count);
	}

	// El principio del fichero:
	//
	//     sfntVersion   uint32   0x00010000 (TrueType) u 'OTTO' (CFF: otro formato de curvas)
	//     numTables     uint16
	//     (6 bytes que solo sirven para buscar más rápido)
	//     numTables x { tag uint32, checksum uint32, offset uint32, length uint32 }
	void Font::read_directory()
	{
		Reader reader(m_data, "el directorio de tablas");
		const uint32_t version = reader.u32();
		if (version == tag("OTTO")) {
			throw std::runtime_error("ttf: fuente con curvas CFF (.otf): no soportada");
		}
		if (version != 0x00010000 && version != tag("true")) {
			throw std::runtime_error("ttf: no es una fuente TrueType");
		}
		const uint16_t table_count = reader.u16();
		reader.skip(6);   // searchRange, entrySelector, rangeShift

		m_tables.reserve(table_count);
		for (uint16_t i = 0; i < table_count; ++i)
		{
			TableRecord record{};
			record.tag = reader.u32();
			reader.skip(4);   // checksum
			record.offset = reader.u32();
			record.length = reader.u32();

			// Que no se salga del fichero (sin sumar, para no desbordar).
			if (record.offset > m_data.size() || record.length > m_data.size() - record.offset) {
				throw std::runtime_error("ttf: una tabla se sale del fichero");
			}
			m_tables.push_back(record);
		}
	}

	std::span<const std::byte> Font::table(uint32_t table_tag) const
	{
		for (const TableRecord& record : m_tables) {
			if (record.tag == table_tag) {
				return std::span<const std::byte>(m_data).subspan(record.offset, record.length);
			}
		}
		return {};
	}

	std::span<const std::byte> Font::required(uint32_t table_tag, const char* name) const
	{
		const auto bytes = table(table_tag);
		if (bytes.empty()) {
			throw std::runtime_error(std::string("ttf: falta la tabla ") + name);
		}
		return bytes;
	}

	HorizontalMetrics Font::metrics(uint16_t glyph) const
	{
		if (glyph >= m_glyph_count) {
			throw std::out_of_range("ttf: glifo fuera de la fuente");
		}
		return m_metrics[glyph];
	}

	Outline Font::outline(uint16_t glyph, float pixels_per_em) const
	{
		if (glyph >= m_glyph_count) {
			throw std::out_of_range("ttf: glifo fuera de la fuente");
		}
		const detail::GlyphShape shape = detail::read_glyph_shape(table(tag("glyf")), m_loca, glyph);
		return detail::build_outline(shape, pixels_per_em / static_cast<float>(m_units_per_em));
	}

	GlyphLocation Font::location(uint16_t glyph) const
	{
		if (glyph >= m_glyph_count) {
			throw std::out_of_range("ttf: glifo fuera de la fuente");
		}
		return { m_loca[glyph], m_loca[glyph + 1u] - m_loca[glyph] };
	}

} // namespace luxel::ttf
