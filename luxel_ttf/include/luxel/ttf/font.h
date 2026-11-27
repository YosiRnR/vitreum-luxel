#pragma once

// luxel_ttf: lee fuentes TrueType (.ttf) a mano (docs/ttf-design.md). Solo std: no
// sabe de GPU ni de ficheros. La aplicación lee los bytes y se los da.
//
// Un .ttf es un índice de TABLAS, cada una con 4 letras de nombre:
//
//     head  datos generales: unidades por em, formato de loca...
//     maxp  cuántos glifos hay
//     hhea  medidas de la línea (ascent, descent...) y cuántas entradas tiene hmtx
//     hmtx  avance y margen izquierdo de cada glifo
//     loca  dónde empieza cada glifo dentro de glyf
//     glyf  los contornos (T3)
//     cmap  de carácter Unicode a número de glifo (T2)
//
// Todo va en "unidades de la fuente": Segoe UI usa 2048 por em. Para pasar a
// píxeles a 48 px: multiplicar por 48 / 2048.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace luxel::ttf
{
	namespace detail { class CharacterMap; }
	// Nombre de tabla de 4 letras como un entero: tag("head") == 0x68656164.
	constexpr uint32_t tag(const char (&name)[5])
	{
		return (uint32_t(uint8_t(name[0])) << 24) | (uint32_t(uint8_t(name[1])) << 16) |
		       (uint32_t(uint8_t(name[2])) << 8)  |  uint32_t(uint8_t(name[3]));
	}

	// Medidas horizontales de un glifo, en unidades de la fuente.
	struct HorizontalMetrics
	{
		uint16_t advance{};            // cuánto avanza el lápiz tras él
		int16_t left_side_bearing{};   // del lápiz al borde izquierdo del dibujo
	};

	// Dónde están los bytes de un glifo dentro de la tabla 'glyf'. Con length 0 no
	// tiene contornos (el espacio): no hay nada que pintar, pero sí avance.
	struct GlyphLocation
	{
		uint32_t offset{};
		uint32_t length{};
	};

	class Font
	{
	public:
		// Copia los bytes (no hace falta mantenerlos vivos) y lee las tablas básicas.
		// Lanza std::runtime_error si el fichero no es un TrueType válido o está roto.
		explicit Font(std::span<const std::byte> data);
		~Font();

		// Solo movible: la 'cmap' va en un unique_ptr (su tipo es privado de luxel_ttf).
		Font(Font&&) noexcept;
		Font& operator=(Font&&) noexcept;
		Font(const Font&) = delete;
		Font& operator=(const Font&) = delete;

		uint16_t units_per_em() const { return m_units_per_em; }
		uint16_t glyph_count() const { return m_glyph_count; }

		// De la línea base hacia arriba (positivo) y hacia abajo (negativo), y el
		// espacio extra entre líneas. Alto de línea = ascent - descent + line_gap.
		int16_t ascent() const { return m_ascent; }
		int16_t descent() const { return m_descent; }
		int16_t line_gap() const { return m_line_gap; }

		// El glifo de un carácter Unicode, o 0 (.notdef, el "tofu") si la fuente no lo
		// tiene. Escenario: en Segoe UI negrita, 'A' -> 36 y U+1F600 (un emoji) -> 0.
		uint16_t glyph_index(char32_t codepoint) const;

		// Lanzan std::out_of_range si glyph >= glyph_count().
		HorizontalMetrics metrics(uint16_t glyph) const;
		GlyphLocation location(uint16_t glyph) const;

		// Los bytes de una tabla, o vacío si la fuente no la tiene.
		std::span<const std::byte> table(uint32_t table_tag) const;
		bool has_table(uint32_t table_tag) const { return !table(table_tag).empty(); }

	private:
		struct TableRecord
		{
			uint32_t tag;
			uint32_t offset;
			uint32_t length;
		};

		void read_directory();
		std::span<const std::byte> required(uint32_t table_tag, const char* name) const;

		std::vector<std::byte> m_data;
		std::vector<TableRecord> m_tables;
		std::vector<uint32_t> m_loca;                  // glyph_count + 1 offsets: el glifo i va de [i] a [i+1]
		std::vector<HorizontalMetrics> m_metrics;      // uno por glifo
		std::unique_ptr<const detail::CharacterMap> m_cmap;

		uint16_t m_units_per_em{};
		uint16_t m_glyph_count{};
		int16_t m_ascent{};
		int16_t m_descent{};
		int16_t m_line_gap{};
	};

} // namespace luxel::ttf
