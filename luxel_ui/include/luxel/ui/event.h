#pragma once

#include <luxel/math/vectors.h>
#include <cstdint>

namespace luxel::ui
{
	// Eventos propios de la UI. La aplicación convierte los de su ventana (por
	// ejemplo, los WinEvent de vitreum) con una función suya, to_ui().
	// De momento solo los que usa U1; los botones llegan en U2.
	enum class EventType : uint8_t
	{
		MouseMove,    // el ratón está en "position"
		MouseLeave    // el ratón ha salido de la ventana (o la ventana ha perdido el foco)
	};

	struct Event
	{
		EventType type{ EventType::MouseMove };
		math::Vector2f position{};   // píxeles, (0,0) arriba a la izquierda (MouseMove)
	};

} // namespace luxel::ui
