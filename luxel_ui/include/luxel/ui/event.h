#pragma once

#include <luxel/math/vectors.h>
#include <cstdint>

namespace luxel::ui
{
	// Eventos propios de la UI. La aplicación convierte los de su ventana (por
	// ejemplo, los WinEvent de vitreum) con una función suya, to_ui().
	// De momento, solo el ratón.
	enum class EventType : uint8_t
	{
		MouseMove,    // el ratón está en "position"
		MouseLeave,   // el ratón ha salido de la ventana
		MouseDown,    // se aprieta "button" en "position"
		MouseUp,      // se suelta "button" en "position"
		MouseWheel,   // se gira la rueda "wheel" muescas. Sin posición: vale la última
		MouseCancel   // la pulsación en curso queda anulada. Escenario: Alt+Tab con el
		              // botón apretado; se soltará en otra ventana y no nos enteraremos
	};

	enum class MouseButton : uint8_t { Left, Middle, Right };

	struct Event
	{
		EventType type{ EventType::MouseMove };
		math::Vector2f position{};   // píxeles, (0,0) arriba a la izquierda (MouseMove, MouseDown y MouseUp)
		MouseButton button{ MouseButton::Left };   // MouseDown y MouseUp
		float wheel{ 0.0f };                       // MouseWheel: muescas, positivo = hacia arriba (puede traer decimales)
	};

} // namespace luxel::ui
