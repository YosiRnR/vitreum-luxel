#include <luxel/ui/widgets.h>
#include <cmath>

namespace luxel::ui
{
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
		const float size = painter.theme().text_size;
		const math::Vector2f needed = painter.measure(m_text, size);

		// Lo que sobra a los lados: todo a la derecha (Start), repartido (Center)
		// o todo a la izquierda (End).
		const float spare = rect().width - needed.x;
		float x = rect().x;
		if (m_align == Align::Center) { x += spare * 0.5f; }
		else if (m_align == Align::End) { x += spare; }
		const float y = rect().y + (rect().height - needed.y) * 0.5f;

		// A píxel entero: a medio píxel, las letras saldrían borrosas.
		painter.text(m_text, { std::round(x), std::round(y) }, size, painter.theme().text_color);
	}

} // namespace luxel::ui
