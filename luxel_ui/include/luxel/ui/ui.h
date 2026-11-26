#pragma once

#include <luxel/ui/event.h>
#include <luxel/ui/layout.h>
#include <luxel/ui/theme.h>
#include <luxel/ui/widget.h>
#include <luxel/renderer2d.h>
#include <optional>

namespace luxel::ui
{
	// Una UI completa: el árbol de widgets y el reparto de eventos. Normalmente
	// una por ventana.
	//
	//     ui.on_event(to_ui(event));         // por cada evento de la ventana
	//     ui.update(width, height);          // una vez por frame
	//     ui.draw(renderer, frame);          // dentro de begin_rendering/end_rendering
	class Ui
	{
	public:
		explicit Ui(const Theme& theme = {}) : m_theme(theme) {}

		// No copiable ni movible: guarda punteros a sus propios widgets.
		Ui(const Ui&) = delete;
		Ui& operator=(const Ui&) = delete;

		// La raíz ocupa toda la ventana y es un Stack: sus hijos se pisan, y cada uno
		// se coloca según su set_align(). Como todo Stack, es invisible para el ratón:
		// donde no hay ningún widget, el ratón es del juego.
		Stack& root() { return m_root; }

		Theme& theme() { return m_theme; }

		// Reparte un evento. Devuelve true si es "de la UI" y el juego no debería
		// hacerle caso: el ratón está sobre un widget, o hay uno que lo tiene capturado.
		// Soltar un botón que se apretó fuera de la UI es del juego, aunque se suelte
		// encima de un widget (si no, el juego creería que sigue apretado).
		bool on_event(const Event& event);

		// Una vez por frame: mide y coloca todo el árbol para este tamaño de ventana y
		// vuelve a mirar qué hay bajo el ratón. Aunque el ratón no se
		// mueva, los widgets pueden moverse debajo de él. También destruye los
		// quitados con Widget::remove() desde fuera de un evento.
		void update(float width, float height);

		// Abre un lote de UI (begin_ui), pinta el árbol entero y lo cierra.
		void draw(Renderer2D& renderer, Frame& frame) const;

		// El widget con el ratón encima, o nullptr.
		Widget* hovered() const { return m_hovered; }

		// El que tiene capturado el ratón (hay un botón apretado sobre él), o nullptr.
		Widget* captured() const { return m_captured; }

	private:
		bool dispatch(const Event& event);
		bool mouse_down(const Event& event);
		bool mouse_up(const Event& event);
		void mouse_cancel();
		bool mouse_wheel(const Event& event);
		void refresh_hover();

		void destroy_removed();
		void collect_removed(Widget& widget, std::vector<std::unique_ptr<Widget>>& dead);

		Theme m_theme;
		Stack m_root;
		std::optional<math::Vector2f> m_mouse;   // vacío = el ratón está fuera de la ventana
		Widget* m_hovered{ nullptr };
		Widget* m_captured{ nullptr };
		MouseButton m_capture_button{ MouseButton::Left };   // con cuál se capturó

		// Cuántas funciones de Ui hay a medias. Un on_click puede acabar llamando otra
		// vez a on_event o update; solo la de más afuera puede destruir widgets.
		int m_depth{ 0 };
	};

} // namespace luxel::ui
