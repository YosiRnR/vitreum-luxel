#include <luxel/ui/ui.h>
#include <luxel/ui/painter.h>
#include "hit_test.h"

namespace luxel::ui
{
	bool Ui::on_event(const Event& event)
	{
		switch (event.type)
		{
		case EventType::MouseMove:  m_mouse = event.position; break;
		case EventType::MouseLeave: m_mouse.reset();          break;
		}
		refresh_hover();
		return m_hovered != nullptr;
	}

	void Ui::update(float width, float height)
	{
		m_root.set_rect({ 0.0f, 0.0f, width, height });
		refresh_hover();
	}

	void Ui::refresh_hover()
	{
		Widget* now = m_mouse ? hit_test(m_root, *m_mouse) : nullptr;
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
