#include <luxel/ui/painter.h>

namespace luxel::ui
{
	void Painter::box(const Rect& rect, const BoxStyle& style)
	{
		m_renderer.draw_rounded_rect({
			.top_left         = rect.top_left(),
			.size             = rect.size(),
			.corner_radius    = style.corner_radius,
			.color            = style.color,
			.border_thickness = style.border_thickness,
			.border_color     = style.border_color
		});
	}

	void Painter::text(std::u8string_view text, math::Vector2f top_left, float size, Color color)
	{
		if (m_theme.font != nullptr) {
			m_renderer.draw_text(*m_theme.font, text, top_left, size, color);
		}
	}

	math::Vector2f Painter::measure(std::u8string_view text, float size) const
	{
		if (m_theme.font == nullptr) {
			return { 0.0f, size };
		}
		// measure() da píxeles de la fuente; la escala es la misma que en draw_text.
		return { m_theme.font->measure(text) * (size / m_theme.font->line_height()), size };
	}

	void Painter::push_clip(const Rect& rect) { m_renderer.push_clip_rect(rect.top_left(), rect.size()); }
	void Painter::pop_clip() { m_renderer.pop_clip_rect(); }

} // namespace luxel::ui
