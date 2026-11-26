#include <luxel/ui/text_field.h>
#include <luxel/ui/painter.h>
#include "utf8_edit.h"
#include <algorithm>
#include <cmath>

namespace luxel::ui
{
	TextField::TextField(std::u8string text)
		: m_text(std::move(text)), m_caret(m_text.size())
	{
		set_focus_mode(FocusMode::All);
	}

	void TextField::set_text(std::u8string text)
	{
		m_text  = std::move(text);
		m_caret = m_text.size();
	}

	void TextField::changed()
	{
		if (on_change) {
			on_change(m_text);
		}
	}

	// ------------------------------------------------------------------ teclado

	void TextField::on_focus_gained()
	{
		// Con Tab, al final, para seguir escribiendo. Con un clic, on_mouse_down (que llega
		// justo después) lo pondrá donde toque.
		m_caret = m_text.size();
	}

	bool TextField::on_key_down(const Event& event)
	{
		switch (event.key)
		{
		case Key::Left:  m_caret = previous_boundary(m_text, m_caret); break;
		case Key::Right: m_caret = next_boundary(m_text, m_caret);     break;
		case Key::Home:  m_caret = 0;                                   break;
		case Key::End:   m_caret = m_text.size();                       break;

		case Key::Backspace:   // borra el carácter de DELANTE del cursor (puede ser de 2 bytes)
			if (m_caret > 0)
			{
				const std::size_t start = previous_boundary(m_text, m_caret);
				m_text.erase(start, m_caret - start);
				m_caret = start;
				changed();
			}
			break;

		case Key::Delete:      // borra el de DETRÁS: el cursor no se mueve
			if (m_caret < m_text.size())
			{
				m_text.erase(m_caret, next_boundary(m_text, m_caret) - m_caret);
				changed();
			}
			break;

		case Key::Enter:
			if (on_submit) {
				on_submit(m_text);
			}
			break;

		case Key::Escape:
			return false;   // que Ui le quite el foco

		default:
			break;          // las letras llegan por on_text; aquí solo se tragan
		}
		return true;
	}

	bool TextField::on_text(const Event& event)
	{
		if (m_max_length != 0 && count_codepoints(m_text) >= m_max_length) {
			return true;   // lleno: la letra se pierde, pero sigue sin ser del juego
		}
		const std::u8string bytes = encode_utf8(event.codepoint);
		m_text.insert(m_caret, bytes);
		m_caret += bytes.size();
		changed();
		return true;
	}

	// -------------------------------------------------------------------- ratón

	bool TextField::on_mouse_down(const Event& event)
	{
		if (event.button != MouseButton::Left) {
			return false;
		}
		// La frontera más cercana al ratón. Escenario: clic en la mitad derecha de la "ñ"
		// -> el cursor queda detrás de ella; en la mitad izquierda, delante.
		const float x = event.position.x - (rect().x + m_padding.x - m_scroll);
		float best = -1.0f;
		for (const Stop& stop : m_stops)
		{
			const float distance = std::abs(stop.x - x);
			if (best < 0.0f || distance < best)
			{
				best    = distance;
				m_caret = stop.byte;
			}
		}
		return true;
	}

	// ------------------------------------------------------------------- layout

	math::Vector2f TextField::measure_content(const Theme& theme)
	{
		m_padding     = theme.text_field.padding;
		m_caret_width = theme.text_field.caret_width;

		// Una parada por frontera, de la primera (0) a la última (size): "aña" da 4.
		m_stops.clear();
		std::size_t byte = 0;
		while (true)
		{
			const float x = measure_text(theme, std::u8string_view(m_text).substr(0, byte), theme.text_size).x;
			m_stops.push_back({ byte, x });
			if (byte >= m_text.size()) {
				break;
			}
			byte = next_boundary(m_text, byte);
		}
		m_text_width = m_stops.back().x;

		// Ancho: solo los márgenes. Si pidiese el del texto, la caja crecería al escribir.
		return { 2.0f * m_padding.x, theme.text_size + 2.0f * m_padding.y };
	}

	float TextField::caret_x() const
	{
		for (const Stop& stop : m_stops) {
			if (stop.byte >= m_caret) {
				return stop.x;
			}
		}
		return m_text_width;
	}

	void TextField::arrange_children()
	{
		// Que el cursor se vea. Escenario: caja de 200 px y texto de 300 con el cursor al
		// final -> se esconden los 100 primeros (más el ancho del cursor). Al volver con
		// Inicio, el texto se desplaza a la derecha hasta enseñar el principio.
		const float inner = std::max(0.0f, rect().width - 2.0f * m_padding.x);
		const float caret = caret_x();
		if (caret + m_caret_width - m_scroll > inner) {
			m_scroll = caret + m_caret_width - inner;
		}
		if (caret < m_scroll) {
			m_scroll = caret;
		}
		// Si se ha borrado, no dejar hueco a la derecha mientras haya texto escondido.
		m_scroll = std::clamp(m_scroll, 0.0f, std::max(0.0f, m_text_width + m_caret_width - inner));
		m_scroll = std::round(m_scroll);   // a píxel entero, o las letras saldrían borrosas

		Widget::arrange_children();
	}

	// -------------------------------------------------------------------- dibujo

	void TextField::draw(Painter& painter) const
	{
		const Theme& theme = painter.theme();
		const TextFieldStyle& style = theme.text_field;
		const BoxStyle& box = focused() ? style.focused : (hovered() ? style.hover : style.normal);
		painter.box(rect(), box);

		const Rect inner{ rect().x + m_padding.x, rect().y + m_padding.y,
		                  std::max(0.0f, rect().width - 2.0f * m_padding.x),
		                  std::max(0.0f, rect().height - 2.0f * m_padding.y) };
		const float size = theme.text_size;
		const float y    = std::round(inner.y + (inner.height - size) * 0.5f);
		const float left = inner.x - m_scroll;

		painter.push_clip(inner);
		painter.text(m_text, { std::round(left), y }, size, theme.text_color);
		if (focused())
		{
			BoxStyle caret{ .color = style.caret };
			painter.box({ std::round(left + caret_x()), y + size * 0.1f, m_caret_width, size * 0.8f }, caret);
		}
		painter.pop_clip();
	}

} // namespace luxel::ui
