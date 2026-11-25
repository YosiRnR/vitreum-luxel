#pragma once

#include <luxel/ui/event.h>
#include <luxel/ui/geometry.h>
#include <concepts>
#include <memory>
#include <utility>
#include <vector>

namespace luxel::ui
{
	class Painter;

	// Un nodo del árbol de la UI (D2 de docs/ui-design.md).
	//
	// Cada widget es DUEÑO de sus hijos (unique_ptr): al destruirlo mueren todos
	// los de debajo, como las ventanas hijas en Win32. El puntero al padre es solo
	// para subir por el árbol; no es dueño de nada.
	//
	//     auto& panel = ui.root().add<Panel>(style);
	//     auto& label = panel.add<Label>(u8"Salir");   // devuelve una referencia
	class Widget
	{
	public:
		Widget() = default;
		virtual ~Widget() = default;

		// Ni copiable ni movible: los hijos apuntan a su padre, y si este cambiase
		// de sitio en memoria, esos punteros quedarían apuntando a la nada.
		Widget(const Widget&) = delete;
		Widget& operator=(const Widget&) = delete;

		// Crea un hijo de tipo T y lo añade el último (se pinta encima de los demás).
		template <std::derived_from<Widget> T, typename... Args>
		T& add(Args&&... args)
		{
			auto child = std::make_unique<T>(std::forward<Args>(args)...);
			T& reference = *child;
			reference.m_parent = this;
			m_children.push_back(std::move(child));
			return reference;
		}

		// Quita el widget (y todos los de debajo). Desaparece de la pantalla y del
		// ratón al momento, pero se DESTRUYE más tarde: cuando Ui termina de repartir
		// el evento, o en el siguiente Ui::update().
		//
		// Escenario: el botón "Cerrar" de un diálogo hace dialog.remove() en su
		// on_click. Si se destruyera ya, el botón moriría a mitad de su propia
		// función. Es la misma idea que la DeletionQueue de luxel, con la CPU en el
		// papel de la GPU. No vale para la raíz.
		void remove();

		Widget* parent() const { return m_parent; }
		const std::vector<std::unique_ptr<Widget>>& children() const { return m_children; }

		// Dónde está, en píxeles de la ventana (no relativo al padre). De momento
		// lo pone la aplicación; en U3 lo hará el layout.
		const Rect& rect() const { return m_rect; }
		void set_rect(const Rect& rect) { m_rect = rect; }

		// Un widget invisible no se pinta ni recibe el ratón, y sus hijos tampoco.
		bool visible() const { return m_visible; }
		void set_visible(bool visible) { m_visible = visible; }

		// El ratón lo atraviesa: lo recibe lo que haya debajo. Sus hijos SÍ pueden
		// recibirlo. Escenario: el texto de un botón lo ignora; si no, al pasar por
		// encima de las letras el ratón "saldría" del botón.
		bool ignores_mouse() const { return m_ignores_mouse; }
		void set_ignores_mouse(bool ignores) { m_ignores_mouse = ignores; }

		// Si recorta, sus hijos no se ven (ni reciben el ratón) fuera de su rect.
		bool clips_children() const { return m_clips_children; }
		void set_clips_children(bool clips) { m_clips_children = clips; }

		// ¿Es el widget que tiene el ratón justo encima? Solo lo es uno: el más
		// profundo (si el ratón está en un botón, el panel que lo contiene no).
		// Durante una pulsación solo puede serlo el que la capturó.
		bool hovered() const { return m_hovered; }

		// Se pinta a sí mismo, sin sus hijos. Por defecto, nada.
		virtual void draw(Painter&) const {}

		// Él y todos los de debajo, en orden, con los recortes puestos.
		void draw_tree(Painter& painter) const;

	protected:
		// Avisos para que cada tipo de widget reaccione, si quiere.
		virtual void on_mouse_enter() {}
		virtual void on_mouse_leave() {}

		// Han apretado un botón del ratón encima. Devolver true = "es para mí": captura
		// el ratón y on_mouse_up (o on_mouse_cancel) le llegará a él, esté donde esté el
		// ratón. Con false se le pregunta a su padre, y así hacia arriba.
		virtual bool on_mouse_down(const Event&) { return false; }

		// Solo le llegan al que capturó, una de las dos y una sola vez por pulsación.
		// En on_mouse_up, hovered() dice si se ha soltado encima de él.
		virtual void on_mouse_up(const Event&) {}
		virtual void on_mouse_cancel() {}

	private:
		friend class Ui;   // reparte el ratón y destruye los quitados
		void set_hovered(bool hovered);

		std::vector<std::unique_ptr<Widget>> m_children;
		Widget* m_parent{ nullptr };
		Rect m_rect{};
		bool m_visible{ true };
		bool m_ignores_mouse{ false };
		bool m_clips_children{ false };
		bool m_hovered{ false };
		bool m_removed{ false };         // se ha llamado a remove(): pendiente de destruir
		bool m_removed_below{ false };   // hay alguno así más abajo. Evita recorrer el árbol
		                                 // entero en cada evento para buscarlos
	};

} // namespace luxel::ui
