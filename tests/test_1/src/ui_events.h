#pragma once

#include <luxel/ui/event.h>
#include <vitreum/vitreum.h>
#include <optional>

// El puente entre la ventana y la UI: convierte los eventos de vitreum en los de
// luxel_ui. Vive en la aplicación porque es la única que conoce a las dos (igual
// que to_luxel() con la ventana). Vacío = a la UI no le interesa ese evento.
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
	// Al perder el foco también: no hay que dejar nada iluminado detrás de otra ventana.
	case WinEventType::MouseLeave:
	case WinEventType::FocusLost:
		return luxel::ui::Event{ .type = EventType::MouseLeave };
	default:
		return std::nullopt;
	}
}
