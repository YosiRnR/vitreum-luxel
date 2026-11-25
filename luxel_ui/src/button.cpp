#include <luxel/ui/widgets.h>
#include <luxel/ui/event.h>

namespace luxel::ui
{
	// El botón solo recuerda UNA cosa: m_pressed (me han apretado y aún no han
	// soltado). Si el ratón está encima ya lo sabe el Widget: hovered(). Con esos dos
	// booleanos salen los cuatro estados:
	//
	//                        hovered()         !hovered()
	//        !m_pressed      Hovered           Normal
	//         m_pressed      Pressed           PressedOutside
	//
	// Escenario ("Salir", y el usuario duda):
	//
	//     entra en el botón     ->  Hovered
	//     aprieta               ->  Pressed         (Ui le da la captura)
	//     arrastra fuera        ->  PressedOutside  (hovered() cambia solo: lo hace Ui)
	//     vuelve a entrar       ->  Pressed
	//     suelta                ->  Hovered, y clic
	//
	//     ...y si hubiera soltado estando fuera: Normal, sin clic.

	ButtonState Button::state() const
	{
		if (m_pressed) {
			return hovered() ? ButtonState::Pressed : ButtonState::PressedOutside;
		}
		else {
			return hovered() ? ButtonState::Hovered : ButtonState::Normal;
		}
	}

	// Han apretado un botón del ratón encima. true = "es para mí" (y Ui nos captura).
	// Solo nos interesa el izquierdo (event.button).
	bool Button::on_mouse_down(const Event& event)
	{
		if (event.button == MouseButton::Left) // ¿es para mi?
		{
			m_pressed = true;
			return true;
		}
		return false;
	}

	// Han soltado. Ui ya ha puesto al día hovered() para el punto donde se soltó.
	//
	// Dos trampas:
	//   - on_click puede estar vacía, y llamar a una std::function vacía lanza.
	//   - Dentro de on_click la pulsación ya tiene que haber terminado. Escenario:
	//     on_click abre un cuadro de mensaje; mientras está abierto, el botón de
	//     detrás no debe seguir hundido. El ORDEN de las líneas importa.
	void Button::on_mouse_up(const Event& event)
	{
		(void)event;

		m_pressed = false;
		if (state() == ButtonState::Hovered && on_click) {
			on_click();
		}
	}

	// Pulsación anulada (Alt+Tab): ni clic ni seguir hundido.
	void Button::on_mouse_cancel()
	{
		m_pressed = false;
	}

} // namespace luxel::ui
