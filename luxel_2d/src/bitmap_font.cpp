#include <luxel/bitmap_font.h>
#include "utf8.h"

#include <algorithm>
#include <stdexcept>

namespace luxel
{
	BitmapFont::BitmapFont(Device& device, const BitmapFontDescriptor& desc)
		: m_first_codepoint(desc.first_codepoint)
		, m_cell_width(static_cast<float>(desc.cell_width))
		, m_cell_height(static_cast<float>(desc.cell_height))
		, m_padding(static_cast<float>(desc.padding))
		, m_line_height(desc.line_height)
	{
		// --- Validación: mejor un error claro aquí que letras raras en pantalla ---
		if (desc.atlas_width == 0 || desc.atlas_height == 0 || desc.cell_width == 0 ||
		    desc.cell_height == 0 || desc.columns == 0) {
			throw std::invalid_argument("BitmapFont: tamaños a 0");
		}
		if (desc.atlas_pixels.size() != static_cast<size_t>(desc.atlas_width) * desc.atlas_height * 4) {
			throw std::invalid_argument("BitmapFont: atlas_pixels no mide ancho * alto * 4 (RGBA8)");
		}
		if (desc.columns * desc.cell_width > desc.atlas_width) {
			throw std::invalid_argument("BitmapFont: las columnas no caben en el ancho del atlas");
		}
		const auto glyph_count = static_cast<uint32_t>(desc.advances.size());
		const uint32_t rows = (glyph_count + desc.columns - 1) / desc.columns;
		if (glyph_count == 0 || rows * desc.cell_height > desc.atlas_height) {
			throw std::invalid_argument("BitmapFont: hay más avances que celdas en el atlas (o ninguno)");
		}
		if (desc.line_height <= 0.0f) {
			throw std::invalid_argument("BitmapFont: line_height debe ser > 0");
		}
		if (desc.fallback < desc.first_codepoint || desc.fallback - desc.first_codepoint >= glyph_count) {
			throw std::invalid_argument("BitmapFont: el carácter de fallback no está en la fuente");
		}

		// --- Textura. UNorm y no SRGB: RGB es blanco (igual en las dos) y el alfa
		// (la cobertura) siempre es lineal. ---
		m_texture = device.create_texture({
			.width        = desc.atlas_width,
			.height       = desc.atlas_height,
			.format       = Format::R8G8B8A8_UNorm,
			.usage        = TextureUsage::Sampled,
			.mip_levels   = 1,
			.initial_data = desc.atlas_pixels
		});

		// --- Tabla de glifos: la UV de cada celda, calculada una sola vez ---
		const float atlas_w = static_cast<float>(desc.atlas_width);
		const float atlas_h = static_cast<float>(desc.atlas_height);
		m_glyphs.resize(glyph_count);
		for (uint32_t g = 0; g < glyph_count; ++g)
		{
			const float x = static_cast<float>((g % desc.columns) * desc.cell_width);
			const float y = static_cast<float>((g / desc.columns) * desc.cell_height);
			m_glyphs[g] = Glyph{
				.uv_min  = { x / atlas_w, y / atlas_h },
				.uv_max  = { (x + m_cell_width) / atlas_w, (y + m_cell_height) / atlas_h },
				.advance = desc.advances[g]
			};
		}
		m_fallback_index = static_cast<uint32_t>(desc.fallback - desc.first_codepoint);
	}

	bool BitmapFont::has_glyph(char32_t codepoint) const
	{
		return codepoint >= m_first_codepoint && codepoint - m_first_codepoint < m_glyphs.size();
	}

	const BitmapFont::Glyph& BitmapFont::glyph(char32_t codepoint) const
	{
		return has_glyph(codepoint) ? m_glyphs[codepoint - m_first_codepoint] : m_glyphs[m_fallback_index];
	}

	float BitmapFont::measure(std::u8string_view text) const
	{
		float widest = 0.0f;
		float line = 0.0f;
		for (std::size_t i = 0; i < text.size();)
		{
			const char32_t cp = detail::next_codepoint(text, i);
			if (cp == U'\n') {
				widest = std::max(widest, line);
				line = 0.0f;
				continue;
			}
			line += glyph(cp).advance;
		}
		return std::max(widest, line);
	}

} // namespace luxel
