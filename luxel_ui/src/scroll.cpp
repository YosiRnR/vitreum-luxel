#include <luxel/ui/scroll.h>
#include "scroll_math.h"
#include <algorithm>
#include <cmath>

namespace luxel::ui
{
	// ------------------------------------------------------------ ScrollBar
	// Aquí solo se reparten los eventos y se pinta. Las cuentas están en scroll_math.

	void ScrollBar::set_values(float view, float content, float scroll)
	{
		m_view    = view;
		m_content = content;
		m_scroll  = scroll;
	}

	Rect ScrollBar::thumb_rect() const
	{
		// thumb_for_scroll cuenta desde el principio del carril; aquí, en la ventana.
		const Rect& track = rect();
		const Thumb thumb = thumb_for_scroll({ m_view, m_content, track.height, m_min_thumb }, m_scroll);
		return { track.x, track.y + thumb.start, track.width, thumb.length };
	}

	math::Vector2f ScrollBar::measure_content(const Theme& theme)
	{
		m_min_thumb = theme.scroll_bar.min_thumb;
		return { theme.scroll_bar.width, 0.0f };
	}

	bool ScrollBar::on_mouse_down(const Event& event)
	{
		if (event.button != MouseButton::Left) {
			return false;
		}
		m_dragging = true;

		// En el pulgar: se agarra por donde está el ratón y no se mueve nada.
		// En el carril: el pulgar salta hasta quedar centrado en el ratón, y se sigue
		// arrastrando desde ahí.
		const Rect thumb = thumb_rect();
		if (thumb.contains(event.position)) {
			m_grab = event.position.y - thumb.y;
		}
		else {
			m_grab = thumb.height * 0.5f;
			drag_to(event.position.y);
		}
		return true;
	}

	void ScrollBar::on_mouse_drag(const Event& event)
	{
		if (m_dragging) {
			drag_to(event.position.y);
		}
	}

	void ScrollBar::drag_to(float mouse_y)
	{
		const Rect& track = rect();
		const float start = mouse_y - m_grab - track.y;   // dónde tendría que empezar el pulgar
		if (on_change) {
			on_change(scroll_for_thumb({ m_view, m_content, track.height, m_min_thumb }, start));
		}
	}

	void ScrollBar::draw(Painter& painter) const
	{
		const ScrollBarStyle& style = painter.theme().scroll_bar;
		const BoxStyle* thumb = &style.thumb;
		if (m_dragging)     { thumb = &style.thumb_pressed; }
		else if (hovered()) { thumb = &style.thumb_hover; }
		painter.box(rect(), style.track);
		painter.box(thumb_rect(), *thumb);
	}

	// ----------------------------------------------------------- ScrollView

	ScrollView::ScrollView()
		: m_viewport(add<Widget>())
		, m_content(m_viewport.add<Column>())
		, m_bar(add<ScrollBar>())
	{
		m_viewport.set_clips_children(true);
		m_bar.on_change = [this](float scroll) { set_scroll(scroll); };   // this vale: la barra es suya
	}

	void ScrollView::set_scroll(float scroll)
	{
		m_scroll = std::clamp(scroll, 0.0f, m_max_scroll);
		m_bar.set_values(m_viewport.rect().height, m_viewport.rect().height + m_max_scroll, m_scroll);
	}

	math::Vector2f ScrollView::measure_content(const Theme& theme)
	{
		m_step = theme.scroll_step;
		m_viewport.measure(theme);   // y este, a content
		m_bar.measure(theme);

		// Cuenta siempre con la barra: si no, el ancho cambiaría al aparecer.
		return { m_content.desired_size().x + m_bar.desired_size().x, 0.0f };
	}

	void ScrollView::arrange_children()
	{
		Rect view = rect();
		const float wanted = m_content.desired_size().y;

		// Escenario: estabas al final de la lista y agrandan la ventana (o quitan filas).
		// Ahora se puede bajar menos: hay que recortar, o quedaría un hueco debajo.
		m_max_scroll = std::max(0.0f, wanted - view.height);
		m_scroll     = std::clamp(m_scroll, 0.0f, m_max_scroll);

		m_bar.set_visible(m_max_scroll > 0.0f);
		if (m_bar.visible())
		{
			const float width = std::min(m_bar.desired_size().x, view.width);
			view.width -= width;
			m_bar.set_values(view.height, wanted, m_scroll);
			m_bar.arrange({ view.x + view.width, view.y, width, view.height });
		}

		// La columna, subida "scroll" píxeles (enteros: si no, las letras saldrían
		// borrosas). El Widget normal deja a sus hijos con el rect que tengan, así que
		// basta con ponérselo antes de colocar la ventana.
		m_content.set_rect({ view.x, view.y - std::round(m_scroll), view.width, std::max(wanted, view.height) });
		m_viewport.arrange(view);
	}

	bool ScrollView::on_mouse_wheel(const Event& event)
	{
		// La rueda hacia arriba es positiva, y subir es bajar menos: de ahí el menos.
		const float before = m_scroll;
		set_scroll(m_scroll - event.wheel * m_step);

		// Si ya estaba en el tope no es para él: que lo intente la lista de fuera.
		return m_scroll != before;
	}

} // namespace luxel::ui
