#include "cmap.h"
#include <algorithm>
#include <stdexcept>

namespace luxel::ttf::detail
{
	namespace
	{
		[[noreturn]] void invalid(const char* problem)
		{
			throw std::runtime_error(std::string("ttf: 'cmap' ") + problem);
		}

		// ¿Es una subtabla de Unicode? Plataforma 0 (Unicode) con cualquier codificación,
		// o plataforma 3 (Windows) con la 1 (plano básico) o la 10 (todo Unicode). Las
		// demás (Mac antiguo, símbolos...) usan otras numeraciones.
		bool is_unicode(uint16_t platform, uint16_t encoding)
		{
			return platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10));
		}

		// La subtabla de formato 4, desde su primer byte (el del formato):
		//
		//     format, length, language        uint16 x 3
		//     segCountX2                      uint16  (el número de tramos, por 2)
		//     searchRange, entrySelector,     uint16 x 3  (para buscar más rápido; no se usan)
		//     rangeShift
		//     endCode[n]                      uint16 x n
		//     reservedPad                     uint16
		//     startCode[n], idDelta[n],       uint16 x n cada uno
		//     idRangeOffset[n]
		//     glyphIdArray[...]               uint16 hasta el final de la subtabla
		Format4 read_format4(Reader reader)
		{
			reader.skip(6);                                   // format, length, language
			const uint16_t seg_count_x2 = reader.u16();
			if (seg_count_x2 == 0 || seg_count_x2 % 2 != 0) {
				invalid("formato 4 con un número de tramos imposible");
			}
			const std::size_t n = seg_count_x2 / 2u;
			reader.skip(6);                                   // searchRange, entrySelector, rangeShift

			Format4 table;
			table.end_codes.resize(n);
			table.start_codes.resize(n);
			table.id_deltas.resize(n);
			table.id_range_offsets.resize(n);
			for (auto& v : table.end_codes)        { v = reader.u16(); }
			reader.skip(2);                                   // reservedPad
			for (auto& v : table.start_codes)      { v = reader.u16(); }
			for (auto& v : table.id_deltas)        { v = reader.i16(); }
			for (auto& v : table.id_range_offsets) { v = reader.u16(); }

			// Lo que queda hasta el final de la subtabla es glyphIdArray. (Tu truetype/
			// calculaba lo mismo con offset + length - tell.)
			table.glyph_id_array.resize((reader.size() - reader.tell()) / 2);
			for (auto& v : table.glyph_id_array)   { v = reader.u16(); }

			// La búsqueda binaria necesita los finales en orden; y un tramo al revés no
			// tiene sentido.
			if (!std::ranges::is_sorted(table.end_codes)) {
				invalid("formato 4 con los tramos desordenados");
			}
			return table;
		}

		// Formato 12:
		//
		//     format, reserved                uint16 x 2
		//     length, language, numGroups     uint32 x 3
		//     numGroups x { startCharCode, endCharCode, startGlyphID }   uint32 x 3
		Format12 read_format12(Reader reader)
		{
			reader.skip(2 + 2 + 4 + 4);
			const uint32_t group_count = reader.u32();
			if (group_count > (reader.size() - reader.tell()) / 12) {
				invalid("formato 12 con más grupos de los que caben");   // antes de reservar memoria
			}
			Format12 table;
			table.groups.resize(group_count);
			for (auto& g : table.groups)
			{
				g.start       = reader.u32();
				g.end         = reader.u32();
				g.start_glyph = reader.u32();
			}
			for (std::size_t i = 0; i < table.groups.size(); ++i)
			{
				const auto& g = table.groups[i];
				if (g.start > g.end || (i > 0 && g.start <= table.groups[i - 1].end)) {
					invalid("formato 12 con grupos desordenados o que se pisan");
				}
			}
			return table;
		}
	}

	uint32_t Format12::lookup(char32_t codepoint) const
	{
		// El primer grupo que acaba en el código o después. Si empieza antes, es el suyo.
		const auto it = std::ranges::lower_bound(groups, static_cast<uint32_t>(codepoint), {}, &Group::end);
		if (it == groups.end() || it->start > codepoint) {
			return 0;
		}
		return it->start_glyph + (static_cast<uint32_t>(codepoint) - it->start);
	}

	uint16_t CharacterMap::glyph_index(char32_t codepoint, uint16_t glyph_count) const
	{
		const uint32_t glyph = std::visit([&](const auto& table) -> uint32_t { return table.lookup(codepoint); }, m_table);
		return glyph < glyph_count ? static_cast<uint16_t>(glyph) : uint16_t{ 0 };
	}

	// La cabecera de 'cmap':
	//
	//     version, numTables              uint16 x 2
	//     numTables x { platformID uint16, encodingID uint16, offset uint32 }
	//
	// "offset" cuenta desde el principio de 'cmap'.
	std::optional<CharacterMap> read_cmap(Reader cmap, uint16_t wanted_format)
	{
		cmap.skip(2);   // version
		const uint16_t subtable_count = cmap.u16();
		for (uint16_t i = 0; i < subtable_count; ++i)
		{
			const uint16_t platform = cmap.u16();
			const uint16_t encoding = cmap.u16();
			const uint32_t offset   = cmap.u32();
			if (!is_unicode(platform, encoding)) {
				continue;
			}
			// Hasta el final de 'cmap': el 'length' del formato 4 es de 16 bits y hay
			// fuentes que lo traen mal; el Reader ya impide salirse.
			Reader subtable = cmap.sub(offset, cmap.size() - std::min<std::size_t>(offset, cmap.size()), "'cmap'");
			Reader peek = subtable;
			const uint16_t format = peek.u16();
			if (format != wanted_format) {
				continue;
			}
			if (format == 4) {
				// En el formato 4 sí se usa el length para saber dónde acaba glyphIdArray.
				Reader header = subtable;
				header.skip(2);
				const uint16_t length = header.u16();
				return CharacterMap(read_format4(subtable.sub(0, std::min<std::size_t>(length, subtable.size()), "'cmap'")));
			}
			return CharacterMap(read_format12(subtable));
		}
		return std::nullopt;
	}

	CharacterMap read_best_cmap(Reader cmap)
	{
		if (auto full = read_cmap(cmap, 12)) {
			return *std::move(full);
		}
		if (auto basic = read_cmap(cmap, 4)) {
			return *std::move(basic);
		}
		invalid("sin subtabla Unicode de formato 4 ni 12");
	}

} // namespace luxel::ttf::detail
