#pragma once

#include <luxel/ui/event.h>
#include <luxel/ui/widget.h>
#include <vitreum/vitreum.h>
#include <optional>

// El puente entre la ventana y la UI: convierte los eventos de vitreum en los de
// luxel_ui. Vive en la aplicación porque es la única que conoce a las dos (igual
// que to_luxel() con la ventana). Vacío = a la UI no le interesa ese evento.

inline std::optional<luxel::ui::MouseButton> to_ui(vitreum::MouseButton button)
{
	switch (button)
	{
	case vitreum::MouseButton::Left:   return luxel::ui::MouseButton::Left;
	case vitreum::MouseButton::Middle: return luxel::ui::MouseButton::Middle;
	case vitreum::MouseButton::Right:  return luxel::ui::MouseButton::Right;
	default:                           return std::nullopt;
	}
}

// Solo las teclas que la UI usa por su nombre; las demás, Unknown (pero se mandan igual:
// con una caja de texto enfocada, la UI se las tiene que quedar).
inline luxel::ui::Key to_ui(vitreum::KeyCode key)
{
	using luxel::ui::Key;
	using vitreum::KeyCode;
	switch (key)
	{
	case KeyCode::Tab:        return Key::Tab;
	case KeyCode::Enter:
	case KeyCode::KP_Enter:   return Key::Enter;
	case KeyCode::Escape:     return Key::Escape;
	case KeyCode::Space:      return Key::Space;
	case KeyCode::Backspace:  return Key::Backspace;
	case KeyCode::Delete:     return Key::Delete;
	case KeyCode::LeftArrow:  return Key::Left;
	case KeyCode::RightArrow: return Key::Right;
	case KeyCode::UpArrow:    return Key::Up;
	case KeyCode::DownArrow:  return Key::Down;
	case KeyCode::Home:       return Key::Home;
	case KeyCode::End:        return Key::End;
	default:                  return Key::Unknown;
	}
}

// Y en la otra dirección: el puntero que pide la UI, para la ventana.
inline vitreum::CursorShape to_vitreum(luxel::ui::Cursor cursor)
{
	switch (cursor)
	{
	case luxel::ui::Cursor::Text:  return vitreum::CursorShape::Text;
	case luxel::ui::Cursor::Arrow: break;
	}
	return vitreum::CursorShape::Arrow;
}

inline std::optional<luxel::ui::Event> to_ui(const vitreum::WinEvent& event)
{
	using luxel::ui::EventType;
	using vitreum::WinEventType;

	switch (event.type)
	{
	case WinEventType::MouseMove:
		return luxel::ui::Event{
			.type     = EventType::MouseMove,
			.position = { static_cast<float>(event.mouse_move.xpos), static_cast<float>(event.mouse_move.ypos) }
		};
	case WinEventType::MouseButtonPress:
	case WinEventType::MouseButtonRelease:
	{
		const auto button = to_ui(event.mouse_button.button);
		if (!button) {
			return std::nullopt;
		}
		return luxel::ui::Event{
			.type     = event.type == WinEventType::MouseButtonPress ? EventType::MouseDown : EventType::MouseUp,
			.position = { static_cast<float>(event.mouse_button.xpos), static_cast<float>(event.mouse_button.ypos) },
			.button   = *button
		};
	}
	case WinEventType::MouseScroll:
		if (event.scroll.yoffset == 0.0f) {
			return std::nullopt;   // rueda horizontal
		}
		return luxel::ui::Event{ .type = EventType::MouseWheel, .wheel = event.scroll.yoffset };
	case WinEventType::MouseLeave:
		return luxel::ui::Event{ .type = EventType::MouseLeave };
	// Soltar una tecla no se manda: la UI no lo necesita y así el juego siempre se entera
	// (si no, una tecla apretada antes de empezar a escribir se le quedaría pulsada).
	case WinEventType::KeyPress:
	case WinEventType::KeyRepeat:
		return luxel::ui::Event{
			.type   = EventType::KeyDown,
			.key    = to_ui(event.key.key),
			.shift  = (event.key.modifiers & static_cast<uint32_t>(vitreum::KeyModifier::Shift)) != 0,
			.repeat = event.type == WinEventType::KeyRepeat
		};
	case WinEventType::CharInput:
		return luxel::ui::Event{ .type = EventType::Text, .codepoint = event.text.codepoint };
	// Sin foco: ni pulsaciones a medias ni nada iluminado detrás de otra ventana.
	case WinEventType::FocusLost:
		return luxel::ui::Event{ .type = EventType::MouseCancel };
	default:
		return std::nullopt;
	}
}
