#include <luxel/ui/widget.h>
#include <luxel/ui/painter.h>
#include <algorithm>
#include <cassert>
#include <cmath>

namespace luxel::ui
{
	void Widget::measure(const Theme& theme)
	{
		// Se redondea hacia arriba: un texto de 81,3 px necesita 82. Así todo el layout
		// trabaja con píxeles enteros.
		const math::Vector2f content = measure_content(theme);
		m_desired = { std::ceil(std::max(content.x, m_min_size.x)), std::ceil(std::max(content.y, m_min_size.y)) };
	}

	void Widget::arrange(const Rect& rect)
	{
		m_rect = rect;
		arrange_children();
	}

	math::Vector2f Widget::measure_content(const Theme& theme)
	{
		// No depende de sus hijos, pero ellos sí necesitan medirse: puede haber un
		// contenedor más abajo.
		for (const auto& child : m_children) {
			child->measure(theme);
		}
		return { 0.0f, 0.0f };
	}

	void Widget::arrange_children()
	{
		for (const auto& child : m_children) {
			child->arrange(child->rect());
		}
	}

	void Widget::draw_tree(Painter& painter) const
	{
		if (!m_visible) {
			return;
		}

		draw(painter);

		// Los hijos van después, así quedan encima. Los recortes se anidan solos:
		// la pila del Renderer2D ya guarda la intersección.
		if (m_clips_children) {
			painter.push_clip(m_rect);
		}
		for (const auto& child : m_children) {
			child->draw_tree(painter);
		}
		if (m_clips_children) {
			painter.pop_clip();
		}
	}

	void Widget::remove()
	{
		assert(m_parent != nullptr && "la raíz no se puede quitar");
		m_removed = true;
		m_visible = false;   // ni se pinta ni recibe el ratón: para el usuario ya no está

		// Deja un rastro hasta la raíz para que Ui sepa por dónde bajar a buscarlo.
		for (Widget* up = m_parent; up != nullptr; up = up->m_parent) {
			up->m_removed_below = true;
		}
	}

	void Widget::set_hovered(bool hovered)
	{
		if (m_hovered == hovered) {
			return;
		}
		m_hovered = hovered;
		if (hovered) {
			on_mouse_enter();
		}
		else {
			on_mouse_leave();
		}
	}

} // namespace luxel::ui
