#include <luxel/ui/widgets.h>
#include <cmath>

namespace luxel::ui
{
	// Una línea de texto dentro de rect, centrada en vertical.
	static void draw_line(Painter& painter, const Rect& rect, std::u8string_view text, Align align)
	{
		const float size = painter.theme().text_size;
		const math::Vector2f needed = painter.measure(text, size);

		// Lo que sobra a los lados: todo a la derecha (Start), repartido (Center)
		// o todo a la izquierda (End).
		const float spare = rect.width - needed.x;
		float x = rect.x;
		if (align == Align::Center) { x += spare * 0.5f; }
		else if (align == Align::End) { x += spare; }
		const float y = rect.y + (rect.height - needed.y) * 0.5f;

		// A píxel entero: a medio píxel, las letras saldrían borrosas.
		painter.text(text, { std::round(x), std::round(y) }, size, painter.theme().text_color);
	}

	void Panel::draw(Painter& painter) const
	{
		painter.box(rect(), hovered() && m_hover_style ? *m_hover_style : m_style);
	}

	Label::Label(std::u8string text, Align align)
		: m_text(std::move(text)), m_align(align)
	{
		set_ignores_mouse(true);
	}

	void Label::draw(Painter& painter) const
	{
		draw_line(painter, rect(), m_text, m_align);
	}

	math::Vector2f Label::measure_content(const Theme& theme)
	{
		return measure_text(theme, m_text, theme.text_size);
	}

	math::Vector2f Button::measure_content(const Theme& theme)
	{
		const math::Vector2f text = measure_text(theme, m_text, theme.text_size);
		return { text.x + 2.0f * theme.button_padding.x, text.y + 2.0f * theme.button_padding.y };
	}

	void Button::draw(Painter& painter) const
	{
		const ButtonStyle& style = m_style ? *m_style : painter.theme().button;
		const BoxStyle* box = &style.normal;
		switch (state())
		{
		case ButtonState::Normal:         box = &style.normal;  break;
		case ButtonState::Hovered:        box = &style.hover;   break;
		case ButtonState::Pressed:        box = &style.pressed; break;
		case ButtonState::PressedOutside: box = &style.hover;   break;   // sigue siendo suyo, pero "levantado"
		}
		painter.box(rect(), *box);
		draw_line(painter, rect(), m_text, Align::Center);
	}

} // namespace luxel::ui
