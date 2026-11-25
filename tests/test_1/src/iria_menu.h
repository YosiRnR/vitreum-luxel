#pragma once

#include <luxel/ui/ui.h>
#include <luxel/ui/widgets.h>
#include <array>

// El menú de la ventana de Iria, hecho con widgets de luxel_ui: un panel con tres
// paneles dentro, cada uno con su texto. Se iluminan con el ratón; aún no se
// pueden pulsar (los botones de verdad llegan en U2).
//
// Los widgets se crean UNA vez, en el constructor, y persisten (modo retained).
// En cada frame solo se colocan (layout) y se pintan.
class IriaMenu
{
public:
	explicit IriaMenu(const luxel::BitmapFont* font)
		: m_ui({ .font = font, .text_size = 30.0f })
	{
		using namespace luxel::ui;

		// El panel de fondo: se aclara el borde si el ratón está en él (y no en un botón).
		BoxStyle back{ .color = { 0.02f, 0.05f, 0.10f, 0.75f }, .corner_radius = 24.0f,
		               .border_thickness = 1.0f, .border_color = { 1.0f, 1.0f, 1.0f, 0.35f } };
		BoxStyle back_hover = back;
		back_hover.border_color = { 1.0f, 1.0f, 1.0f, 1.0f };
		m_panel = &m_ui.root().add<Panel>(back, back_hover);

		const BoxStyle normal[3] = {
			{ .color = { 0.05f, 0.35f, 0.80f, 1.0f }, .corner_radius = 4.0f },
			{ .color = { 0.10f, 0.50f, 0.20f, 1.0f }, .corner_radius = 14.0f,
			  .border_thickness = 4.0f, .border_color = { 0.60f, 1.00f, 0.60f, 1.0f } },
			{ .color = { 0.00f, 0.00f, 0.00f, 0.0f }, .corner_radius = 100.0f,
			  .border_thickness = 2.0f, .border_color = { 1.00f, 1.00f, 1.00f, 1.0f } },
		};
		const luxel::Color hover_color[3] = {
			{ 0.20f, 0.60f, 1.00f, 1.0f }, { 0.20f, 0.80f, 0.35f, 1.0f }, { 0.80f, 0.10f, 0.10f, 1.0f }
		};
		const std::u8string labels[3] = { u8"Jugar", u8"Opciones", u8"Salir" };

		for (size_t i = 0; i < 3; ++i)
		{
			BoxStyle hover = normal[i];
			hover.color = hover_color[i];
			m_items[i]  = &m_panel->add<Panel>(normal[i], hover);
			m_labels[i] = &m_items[i]->add<Label>(labels[i], Align::Center);
		}
	}

	bool on_event(const luxel::ui::Event& event) { return m_ui.on_event(event); }

	// Coloca los widgets abajo a la derecha. Las cuentas son a mano hasta U3.
	void layout(float width, float height)
	{
		const float margin = 20.0f, panel_w = 260.0f, panel_h = 218.0f;
		const luxel::ui::Rect panel{ width - margin - panel_w, height - margin - panel_h, panel_w, panel_h };
		m_panel->set_rect(panel);

		for (size_t i = 0; i < m_items.size(); ++i)
		{
			const luxel::ui::Rect item{ panel.x + 20.0f, panel.y + 20.0f + static_cast<float>(i) * 65.0f, 220.0f, 48.0f };
			m_items[i]->set_rect(item);
			m_labels[i]->set_rect(item);
		}
		m_ui.update(width, height);
	}

	void draw(luxel::Renderer2D& r2d, luxel::Frame& frame) const { m_ui.draw(r2d, frame); }

	luxel::ui::Rect panel_rect() const { return m_panel->rect(); }

private:
	luxel::ui::Ui m_ui;

	// Punteros a widgets del árbol: no son dueños (lo es m_ui).
	luxel::ui::Panel* m_panel{ nullptr };
	std::array<luxel::ui::Panel*, 3> m_items{};
	std::array<luxel::ui::Label*, 3> m_labels{};
};
