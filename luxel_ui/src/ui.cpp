#include <luxel/ui/ui.h>
#include <luxel/ui/painter.h>
#include "hit_test.h"
#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

namespace luxel::ui
{
	namespace
	{
		// Suma uno al entrar y lo resta al salir, también si algo lanza.
		struct DepthGuard
		{
			explicit DepthGuard(int& depth) : depth(depth) { ++depth; }
			~DepthGuard() { --depth; }
			DepthGuard(const DepthGuard&) = delete;
			DepthGuard& operator=(const DepthGuard&) = delete;
			int& depth;
		};

		// ¿Es "widget" el propio "ancestor" o cuelga de él?
		bool is_inside(const Widget* widget, const Widget* ancestor)
		{
			for (; widget != nullptr; widget = widget->parent()) {
				if (widget == ancestor) {
					return true;
				}
			}
			return false;
		}

		// ¿Se ve? Él y todos los de encima tienen que ser visibles (uno quitado con
		// remove() ya es invisible).
		bool is_shown(const Widget* widget)
		{
			for (; widget != nullptr; widget = widget->parent()) {
				if (!widget->visible()) {
					return false;
				}
			}
			return true;
		}

		// Los que pueden recibir el foco con Tab, en el orden del árbol (el de pintar:
		// padre, luego sus hijos, del primero al último). Los invisibles no cuentan.
		void collect_focusable(Widget& widget, std::vector<Widget*>& out)
		{
			if (!widget.visible()) {
				return;
			}
			if (widget.focus_mode() != FocusMode::None) {
				out.push_back(&widget);
			}
			for (const auto& child : widget.children()) {
				collect_focusable(*child, out);
			}
		}

		// Caracteres que no se escriben. Windows manda Retroceso (8), Enter (13), Tab (9),
		// Escape (27) y Ctrl+letra (1 a 26) también como texto; esas teclas ya llegan como
		// KeyDown. Y una mitad de un emoji suelta no es un carácter.
		bool is_printable(char32_t c)
		{
			if (c < 0x20 || (c >= 0x7F && c <= 0x9F)) {
				return false;   // de control
			}
			if (c >= 0xD800 && c <= 0xDFFF) {
				return false;   // media letra de UTF-16
			}
			return c <= 0x10FFFF;
		}
	}

	bool Ui::on_event(const Event& event)
	{
		bool used = false;
		{
			DepthGuard guard(m_depth);
			used = dispatch(event);
		}
		destroy_removed();   // ya no queda ningún widget a mitad de una función
		return used;
	}

	bool Ui::dispatch(const Event& event)
	{
		switch (event.type)
		{
		case EventType::MouseMove:
			m_mouse = event.position;
			refresh_hover();
			if (m_captured != nullptr) {
				m_captured->on_mouse_drag(event);
			}
			break;
		case EventType::MouseLeave:
			// La captura NO se pierde: con el botón apretado, la ventana sigue
			// recibiendo el ratón desde fuera.
			m_mouse.reset();
			refresh_hover();
			break;
		case EventType::MouseDown:   return mouse_down(event);
		case EventType::MouseUp:     return mouse_up(event);
		case EventType::MouseWheel:  return mouse_wheel(event);
		case EventType::MouseCancel: mouse_cancel(); break;
		case EventType::KeyDown:     return key_down(event);
		case EventType::Text:        return text(event);
		}
		return m_hovered != nullptr || m_captured != nullptr;
	}

	bool Ui::mouse_down(const Event& event)
	{
		m_mouse = event.position;
		refresh_hover();
		if (m_captured != nullptr) {
			return true;   // ya hay otro botón apretado: este se ignora hasta que suelte aquel
		}

		// El foco, al primero que lo acepte con un clic, subiendo desde el de debajo del
		// ratón. Si no hay ninguno (un botón, un panel, el juego), nadie tiene el foco:
		// hacer clic fuera de la caja de texto devuelve el teclado al juego.
		Widget* focus = m_hovered;
		while (focus != nullptr && focus->focus_mode() != FocusMode::All) {
			focus = focus->parent();
		}
		set_focus(focus);

		// Subida (D5): se le ofrece al de debajo del ratón y, si no lo quiere, a su
		// padre, y así hasta la raíz.
		Widget* const target = m_hovered;
		for (Widget* widget = target; widget != nullptr; widget = widget->parent())
		{
			if (widget->on_mouse_down(event))
			{
				m_captured = widget;
				m_capture_button = event.button;
				refresh_hover();   // si lo aceptó un padre, ahora es él quien tiene el ratón encima
				break;
			}
		}
		return target != nullptr;
	}

	bool Ui::mouse_up(const Event& event)
	{
		m_mouse = event.position;
		refresh_hover();   // antes de avisar: hovered() tiene que valer para ESTE punto
		if (m_captured == nullptr) {
			return false;   // se apretó fuera de la UI
		}
		if (event.button != m_capture_button) {
			return true;
		}

		// La captura se suelta ANTES de avisar: si el widget hace algo en su on_mouse_up
		// que reparta otro evento, la pulsación ya ha terminado.
		Widget* const widget = m_captured;
		m_captured = nullptr;
		widget->on_mouse_up(event);
		refresh_hover();   // sin captura, los demás vuelven a iluminarse
		return true;
	}

	bool Ui::mouse_wheel(const Event& event)
	{
		// Con un botón apretado no se hace caso a la rueda: movería la lista debajo de lo
		// que se está arrastrando.
		if (m_captured != nullptr) {
			return true;
		}
		Widget* const target = m_hovered;
		for (Widget* widget = target; widget != nullptr; widget = widget->parent())
		{
			if (widget->on_mouse_wheel(event)) {
				break;
			}
		}
		// Lo que haya ahora bajo el ratón lo mirará update(), cuando se coloque de nuevo.
		return target != nullptr;
	}

	bool Ui::key_down(const Event& event)
	{
		drop_hidden_focus();

		// Tab y Mayúsculas+Tab pasan el foco: no son de ningún widget.
		if (event.key == Key::Tab) {
			return move_focus(event.shift);
		}

		// Sube desde el que tiene el foco. Escenario: las flechas con el foco en un botón
		// de una lista; el botón no las quiere y la lista podría usarlas.
		for (Widget* widget = m_focused; widget != nullptr; widget = widget->parent())
		{
			if (widget->on_key_down(event)) {
				return true;
			}
		}

		// Escape suelta el foco si nadie lo ha querido: así siempre hay forma de devolverle
		// el teclado al juego.
		if (m_focused != nullptr && event.key == Key::Escape)
		{
			set_focus(nullptr);
			return true;
		}
		return false;
	}

	bool Ui::text(const Event& event)
	{
		drop_hidden_focus();
		if (m_focused == nullptr) {
			return false;
		}
		if (!is_printable(event.codepoint)) {
			return true;   // con foco, tampoco es del juego
		}
		for (Widget* widget = m_focused; widget != nullptr; widget = widget->parent())
		{
			if (widget->on_text(event)) {
				return true;
			}
		}
		return false;
	}

	bool Ui::move_focus(bool backwards)
	{
		std::vector<Widget*> order;
		collect_focusable(m_root, order);
		if (order.empty()) {
			return false;   // no hay nada que enfocar: el Tab es del juego
		}

		// Desde el que lo tiene, uno más (o uno menos), dando la vuelta al llegar al final.
		// Si no lo tiene nadie, el primero (o el último).
		const auto it = std::find(order.begin(), order.end(), m_focused);
		const size_t n = order.size();
		size_t next = 0;
		if (it == order.end()) {
			next = backwards ? n - 1 : 0;
		}
		else {
			const size_t index = static_cast<size_t>(it - order.begin());
			next = backwards ? (index + n - 1) % n : (index + 1) % n;
		}
		set_focus(order[next]);
		return true;
	}

	void Ui::set_focus(Widget* widget)
	{
		assert((widget == nullptr || widget->focus_mode() != FocusMode::None) && "ese widget no admite el foco");
		if (widget == m_focused) {
			return;
		}
		// Primero lo pierde uno y luego lo gana el otro, como con el ratón.
		Widget* const old = m_focused;
		m_focused = widget;
		if (old != nullptr) {
			old->set_focused(false);
		}
		if (widget != nullptr) {
			widget->set_focused(true);
		}
	}

	// Escenario: Tab hasta "Jugar" y Enter. El botón esconde el menú, pero el foco
	// seguiría en él: la siguiente barra espaciadora haría clic en un botón invisible.
	void Ui::drop_hidden_focus()
	{
		if (m_focused != nullptr && !is_shown(m_focused)) {
			set_focus(nullptr);
		}
	}

	Cursor Ui::cursor() const
	{
		const Widget* widget = m_captured != nullptr ? m_captured : m_hovered;
		return widget != nullptr ? widget->cursor() : Cursor::Arrow;
	}

	void Ui::mouse_cancel()
	{
		m_mouse.reset();
		Widget* const widget = m_captured;
		m_captured = nullptr;
		if (widget != nullptr) {
			widget->on_mouse_cancel();
		}
		refresh_hover();
	}

	void Ui::update(float width, float height)
	{
		// Primero destruir: si el que tenía la captura se ha quitado, al mirar qué hay
		// bajo el ratón ya no debe contar.
		destroy_removed();

		DepthGuard guard(m_depth);
		m_root.measure(m_theme);                           // de abajo arriba: ¿cuánto queréis?
		m_root.arrange({ 0.0f, 0.0f, width, height });     // de arriba abajo: esto es lo que hay
		refresh_hover();
		drop_hidden_focus();
	}

	void Ui::refresh_hover()
	{
		Widget* now = m_mouse ? hit_test(m_root, *m_mouse) : nullptr;

		// Con captura, o el ratón está en el que capturó (o en algo suyo) o no está en
		// nadie: al arrastrar, los demás botones no se iluminan al pasar.
		if (m_captured != nullptr) {
			now = is_inside(now, m_captured) ? m_captured : nullptr;
		}

		if (now == m_hovered) {
			return;
		}
		// Primero sale uno y luego entra el otro: nunca hay dos a la vez.
		if (m_hovered != nullptr) {
			m_hovered->set_hovered(false);
		}
		m_hovered = now;
		if (m_hovered != nullptr) {
			m_hovered->set_hovered(true);
		}
	}

	void Ui::destroy_removed()
	{
		if (m_depth > 0) {
			return;   // hay algún widget a medias más arriba: ya lo hará quien empezó
		}
		// Es un bucle porque el destructor de un widget puede quitar otros.
		while (m_root.m_removed_below)
		{
			// Primero se sacan todos del árbol y después se destruyen, así nadie ve el
			// árbol a medio cambiar.
			std::vector<std::unique_ptr<Widget>> dead;
			collect_removed(m_root, dead);
		}
	}

	void Ui::collect_removed(Widget& widget, std::vector<std::unique_ptr<Widget>>& dead)
	{
		if (!widget.m_removed_below) {
			return;   // por esta rama no hay ninguno
		}
		widget.m_removed_below = false;

		for (auto& child : widget.m_children)
		{
			if (!child->m_removed) {
				collect_removed(*child, dead);
				continue;
			}
			// Se van él y todo lo suyo: que Ui no se quede apuntando a un muerto.
			if (is_inside(m_hovered, child.get()))  { m_hovered  = nullptr; }
			if (is_inside(m_captured, child.get())) { m_captured = nullptr; }
			if (is_inside(m_focused, child.get()))  { m_focused  = nullptr; }
			dead.push_back(std::move(child));   // deja un unique_ptr vacío en el sitio
		}
		std::erase(widget.m_children, nullptr);
	}

	void Ui::draw(Renderer2D& renderer, Frame& frame) const
	{
		const Rect& screen = m_root.rect();
		if (screen.width <= 0.0f || screen.height <= 0.0f) {
			return;   // aún no se ha llamado a update()
		}
		Painter painter(renderer, m_theme);
		renderer.begin_ui(frame, screen.width, screen.height);
		m_root.draw_tree(painter);
		renderer.end();
	}

} // namespace luxel::ui
