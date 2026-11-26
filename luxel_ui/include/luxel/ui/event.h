#pragma once

#include <luxel/math/vectors.h>
#include <cstdint>

namespace luxel::ui
{
	// Eventos propios de la UI. La aplicación convierte los de su ventana (por
	// ejemplo, los WinEvent de vitreum) con una función suya, to_ui().
	enum class EventType : uint8_t
	{
		MouseMove,    // el ratón está en "position"
		MouseLeave,   // el ratón ha salido de la ventana
		MouseDown,    // se aprieta "button" en "position"
		MouseUp,      // se suelta "button" en "position"
		MouseWheel,   // se gira la rueda "wheel" muescas. Sin posición: vale la última
		MouseCancel,  // la pulsación en curso queda anulada. Escenario: Alt+Tab con el
		              // botón apretado; se soltará en otra ventana y no nos enteraremos
		KeyDown,      // se aprieta "key" (o se repite por tenerla apretada: "repeat")
		Text          // se escribe "codepoint". Llega APARTE de KeyDown: con Mayúsculas+A
		              // llegan un KeyDown de la A y un Text con 'A'; con la tecla muerta
		              // del acento y luego la e, dos KeyDown y un solo Text, 'é'
	};

	// Las teclas que la UI usa por su nombre. Las demás llegan como Unknown, y
	// también hay que mandarlas: un TextField con el foco se las queda para que el
	// juego no las vea (escribir "ada" no debe mover a Iria a la izquierda).
	enum class Key : uint8_t
	{
		Unknown,
		Tab, Enter, Escape, Space, Backspace, Delete,
		Left, Right, Up, Down, Home, End
	};

	enum class MouseButton : uint8_t { Left, Middle, Right };

	struct Event
	{
		EventType type{ EventType::MouseMove };
		math::Vector2f position{};   // píxeles, (0,0) arriba a la izquierda (MouseMove, MouseDown y MouseUp)
		MouseButton button{ MouseButton::Left };   // MouseDown y MouseUp
		float wheel{ 0.0f };                       // MouseWheel: muescas, positivo = hacia arriba (puede traer decimales)
		Key key{ Key::Unknown };                   // KeyDown
		bool shift{ false };                       // KeyDown: con Mayúsculas apretada
		bool repeat{ false };                      // KeyDown: no es la primera, es la repetición automática
		char32_t codepoint{ 0 };                   // Text: un carácter Unicode entero (no media letra)
	};

} // namespace luxel::ui
