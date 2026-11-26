#pragma once

#include <luxel/ui/event.h>
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
	// Sin foco: ni pulsaciones a medias ni nada iluminado detrás de otra ventana.
	case WinEventType::FocusLost:
		return luxel::ui::Event{ .type = EventType::MouseCancel };
	default:
		return std::nullopt;
	}
}
