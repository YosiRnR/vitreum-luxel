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
	struct Theme;

	// Dónde se pone un widget en el hueco que le da su contenedor, en una dirección.
	enum class Placement { Fill, Start, Center, End };   // Fill = lo ocupa entero

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

		// Dónde está, en píxeles de la ventana (no relativo al padre).
		//
		// Quién lo pone depende del PADRE. Si es un contenedor (Row, Column, Stack,
		// Panel o la raíz), lo pone él en cada Ui::update() y set_rect no sirve de nada.
		// Si es un Widget normal, deja a sus hijos donde estén: lo pone la aplicación
		// con set_rect. (Es la diferencia entre Control y Container en Godot.)
		const Rect& rect() const { return m_rect; }
		void set_rect(const Rect& rect) { m_rect = rect; }

		// --- Layout: lo que el widget le pide a su contenedor ---

		// Como poco, este tamaño (el contenido puede pedir más).
		void set_min_size(math::Vector2f size) { m_min_size = size; }

		// Peso para repartirse lo que sobre a lo largo de un Row o un Column.
		// 0 = no crece. Con 1 y 3, al segundo le toca el triple que al primero.
		float grow() const { return m_grow; }
		void set_grow(float grow) { m_grow = grow; }

		// Dónde ponerse en su hueco. Un Stack mira los dos; un Column, solo x; un Row,
		// solo y (en la otra dirección manda el orden de los hijos).
		Placement align_x() const { return m_align_x; }
		Placement align_y() const { return m_align_y; }
		void set_align(Placement x, Placement y) { m_align_x = x; m_align_y = y; }

		// Primera pasada, de abajo arriba: calcula desired_size(), lo que quiere medir.
		void measure(const Theme& theme);
		math::Vector2f desired_size() const { return m_desired; }

		// Segunda pasada, de arriba abajo: este es tu sitio; coloca a tus hijos en él.
		void arrange(const Rect& rect);

		// Un widget invisible no se pinta ni recibe el ratón, y sus hijos tampoco. En un
		// contenedor tampoco ocupa sitio: los demás se corren.
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
		// Lo que pide el contenido (texto, hijos...), sin contar min_size. Un contenedor
		// tiene que llamar a measure() de sus hijos visibles.
		virtual math::Vector2f measure_content(const Theme& theme);

		// Coloca a los hijos dentro de rect(), llamando a su arrange(). Por defecto los
		// deja donde están.
		virtual void arrange_children();

		// Avisos para que cada tipo de widget reaccione, si quiere.
		virtual void on_mouse_enter() {}
		virtual void on_mouse_leave() {}

		// Han apretado un botón del ratón encima. Devolver true = "es para mí": captura
		// el ratón y on_mouse_up (o on_mouse_cancel) le llegará a él, esté donde esté el
		// ratón. Con false se le pregunta a su padre, y así hacia arriba.
		virtual bool on_mouse_down(const Event&) { return false; }

		// El ratón se mueve con el botón apretado. Solo le llega al que capturó, y aunque
		// el ratón esté fuera de él.
		virtual void on_mouse_drag(const Event&) {}

		// Han girado la rueda encima. Sube hacia arriba como on_mouse_down: devolver true =
		// "es para mí". Escenario: la rueda sobre un botón de una lista; el botón no la
		// quiere y acaba llegando a la lista.
		virtual bool on_mouse_wheel(const Event&) { return false; }

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
		math::Vector2f m_min_size{ 0.0f, 0.0f };
		math::Vector2f m_desired{ 0.0f, 0.0f };
		float m_grow{ 0.0f };
		Placement m_align_x{ Placement::Fill };
		Placement m_align_y{ Placement::Fill };
		bool m_visible{ true };
		bool m_ignores_mouse{ false };
		bool m_clips_children{ false };
		bool m_hovered{ false };
		bool m_removed{ false };         // se ha llamado a remove(): pendiente de destruir
		bool m_removed_below{ false };   // hay alguno así más abajo. Evita recorrer el árbol
		                                 // entero en cada evento para buscarlos
	};

} // namespace luxel::ui
