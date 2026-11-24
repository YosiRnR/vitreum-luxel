#pragma once

#include <vitreum/vitreum.h>
#include <array>
#include <cstddef>

// Estado del teclado de UNA ventana, construido a partir de sus eventos.
//
// vitreum entrega eventos ("se ha pulsado la A", "se ha soltado la A"), pero a
// un juego casi siempre le interesa otra pregunta: "¿está la A pulsada AHORA?".
// Esta clase hace esa traducción:
//
//   - is_down(tecla):  la tecla está pulsada (mantener la flecha = correr).
//   - was_pressed(tecla): se pulsó durante ESTE frame (saltar una vez por
//     pulsación, aunque se mantenga la tecla). Se limpia con end_frame().
//
// Escenario: el jugador mantiene la flecha derecha y pulsa espacio una vez.
//   frame 1: KeyPress(Right), KeyPress(Space) -> is_down(Right) y was_pressed(Space)
//   frame 2: KeyRepeat(Right) (autorrepetición de Windows: se ignora)
//            -> is_down(Right); was_pressed(Space) ya es false (end_frame)
//   frame 3: KeyRelease(Right) -> nada pulsado
//
// Al perder el foco se sueltan todas: si la ventana pierde el foco con una
// tecla pulsada, nunca llegará su KeyRelease y se quedaría "atascada".
//
// Es lógica de aplicación (vive en test_1); cuando haya capa de motor, algo así
// iría allí o en vitreum como ayuda opcional encima de los eventos.
class KeyboardState
{
public:
	// Llamar con cada evento de la ventana (process_events).
	void on_event(const vitreum::WinEvent& event)
	{
		switch (event.type)
		{
		case vitreum::WinEventType::KeyPress:
			set(m_down, event.key.key, true);
			set(m_pressed, event.key.key, true);
			break;
		case vitreum::WinEventType::KeyRelease:
			set(m_down, event.key.key, false);
			break;
		case vitreum::WinEventType::FocusLost:
			m_down.fill(false);
			break;
		default:
			// KeyRepeat no cambia nada: la tecla ya estaba pulsada.
			break;
		}
	}

	bool is_down(vitreum::KeyCode key) const { return get(m_down, key); }
	bool was_pressed(vitreum::KeyCode key) const { return get(m_pressed, key); }

	// Llamar una vez por frame, DESPUÉS de que el juego haya leído el estado.
	void end_frame() { m_pressed.fill(false); }

private:
	// KeyCode tiene ~110 valores; 256 casillas sobran. index() comprueba el
	// rango por si algún día crece el enum.
	static constexpr std::size_t k_max_keys = 256;
	using KeyArray = std::array<bool, k_max_keys>;

	static std::size_t index(vitreum::KeyCode key) { return static_cast<std::size_t>(key); }

	static void set(KeyArray& keys, vitreum::KeyCode key, bool value)
	{
		if (index(key) < k_max_keys) {
			keys[index(key)] = value;
		}
	}

	static bool get(const KeyArray& keys, vitreum::KeyCode key)
	{
		return index(key) < k_max_keys && keys[index(key)];
	}

	KeyArray m_down{};
	KeyArray m_pressed{};
};
