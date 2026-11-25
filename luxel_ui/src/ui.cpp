#include <luxel/ui/ui.h>
#include <luxel/ui/painter.h>
#include "hit_test.h"
#include <utility>

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
			break;
		case EventType::MouseLeave:
			// La captura NO se pierde: con el botón apretado, la ventana sigue
			// recibiendo el ratón desde fuera.
			m_mouse.reset();
			refresh_hover();
			break;
		case EventType::MouseDown:   return mouse_down(event);
		case EventType::MouseUp:     return mouse_up(event);
		case EventType::MouseCancel: mouse_cancel(); break;
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
		m_root.set_rect({ 0.0f, 0.0f, width, height });
		refresh_hover();
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
