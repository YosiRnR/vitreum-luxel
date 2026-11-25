#pragma once

#include <luxel/ui/event.h>
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
	//     ...colocar los widgets (en U3, ui.layout)...
	//     ui.update(width, height);          // una vez por frame
	//     ui.draw(renderer, frame);          // dentro de begin_rendering/end_rendering
	class Ui
	{
	public:
		explicit Ui(const Theme& theme = {}) : m_theme(theme) { m_root.set_ignores_mouse(true); }

		// No copiable ni movible: guarda punteros a sus propios widgets.
		Ui(const Ui&) = delete;
		Ui& operator=(const Ui&) = delete;

		// La raíz ocupa toda la ventana y es invisible para el ratón: donde no
		// hay ningún widget, el ratón es del juego.
		Widget& root() { return m_root; }

		Theme& theme() { return m_theme; }

		// Devuelve true si el evento es "de la UI" (el ratón está sobre un widget)
		// y el juego no debería hacerle caso.
		bool on_event(const Event& event);

		// Una vez por frame, tras colocar los widgets: ajusta la raíz al tamaño de
		// la ventana y vuelve a mirar qué hay bajo el ratón. Aunque el ratón no se
		// mueva, los widgets pueden moverse debajo de él.
		void update(float width, float height);

		// Abre un lote de UI (begin_ui), pinta el árbol entero y lo cierra.
		void draw(Renderer2D& renderer, Frame& frame) const;

		// El widget con el ratón encima, o nullptr.
		Widget* hovered() const { return m_hovered; }

	private:
		void refresh_hover();

		Theme m_theme;
		Widget m_root;
		std::optional<math::Vector2f> m_mouse;   // vacío = el ratón está fuera de la ventana
		Widget* m_hovered{ nullptr };
	};

} // namespace luxel::ui
