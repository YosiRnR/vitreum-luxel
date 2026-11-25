#pragma once

#include <luxel/ui/ui.h>
#include <luxel/ui/widgets.h>
#include <array>
#include <cmath>
#include <functional>

// El menú de la ventana de Iria, hecho con widgets de luxel_ui: un panel con tres
// botones. "Jugar" lo esconde (Escape lo devuelve), "Opciones" abre un diálogo que
// se quita a sí mismo y "Salir" llama a on_quit.
//
// Los widgets se crean UNA vez, en el constructor, y persisten (modo retained).
// En cada frame solo se colocan (layout) y se pintan. El diálogo es la excepción:
// nace y muere mientras la aplicación funciona.
class IriaMenu
{
public:
	std::function<void()> on_quit;

	explicit IriaMenu(const luxel::BitmapFont* font)
		: m_ui({ .font = font, .text_size = 30.0f })
	{
		using namespace luxel::ui;

		// El panel de fondo: se aclara el borde si el ratón está en él (y no en un botón).
		BoxStyle back_hover = k_back;
		back_hover.border_color = { 1.0f, 1.0f, 1.0f, 1.0f };
		m_panel = &m_ui.root().add<Panel>(k_back, back_hover);

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
		const luxel::Color pressed_color[3] = {
			{ 0.02f, 0.15f, 0.40f, 1.0f }, { 0.04f, 0.22f, 0.08f, 1.0f }, { 0.35f, 0.02f, 0.02f, 1.0f }
		};
		const std::u8string labels[3] = { u8"Jugar", u8"Opciones", u8"Salir" };

		for (size_t i = 0; i < 3; ++i)
		{
			ButtonStyle style{ .normal = normal[i], .hover = normal[i], .pressed = normal[i] };
			style.hover.color   = hover_color[i];
			style.pressed.color = pressed_color[i];
			m_buttons[i] = &m_panel->add<Button>(labels[i]);
			m_buttons[i]->set_style(style);
		}

		// Las lambdas capturan this: vale porque los botones mueren con m_ui, que es
		// parte de este objeto (y por eso IriaMenu no se puede copiar ni mover).
		m_buttons[0]->on_click = [this] { m_panel->set_visible(false); };
		m_buttons[1]->on_click = [this] { open_options(); };
		m_buttons[2]->on_click = [this] { if (on_quit) { on_quit(); } };
	}

	IriaMenu(const IriaMenu&) = delete;
	IriaMenu& operator=(const IriaMenu&) = delete;

	bool on_event(const luxel::ui::Event& event) { return m_ui.on_event(event); }

	void show() { m_panel->set_visible(true); }

	// Coloca los widgets. Las cuentas son a mano hasta U3.
	void layout(float width, float height)
	{
		// El menú, abajo a la derecha.
		const float margin = 20.0f, panel_w = 260.0f, panel_h = 218.0f;
		const luxel::ui::Rect panel{ width - margin - panel_w, height - margin - panel_h, panel_w, panel_h };
		m_panel->set_rect(panel);
		for (size_t i = 0; i < m_buttons.size(); ++i) {
			m_buttons[i]->set_rect({ panel.x + 20.0f, panel.y + 20.0f + static_cast<float>(i) * 65.0f, 220.0f, 48.0f });
		}

		// El diálogo, si está abierto, en el centro.
		if (m_dialog != nullptr)
		{
			const luxel::ui::Rect dialog{ std::round((width - 420.0f) * 0.5f), std::round((height - 190.0f) * 0.5f), 420.0f, 190.0f };
			m_dialog->set_rect(dialog);
			m_dialog_text->set_rect({ dialog.x + 20.0f, dialog.y + 20.0f, 380.0f, 60.0f });
			m_dialog_close->set_rect({ dialog.x + 120.0f, dialog.y + 115.0f, 180.0f, 48.0f });
		}
		m_ui.update(width, height);
	}

	void draw(luxel::Renderer2D& r2d, luxel::Frame& frame) const { m_ui.draw(r2d, frame); }

	luxel::ui::Rect panel_rect() const { return m_panel->rect(); }

private:
	void open_options()
	{
		using namespace luxel::ui;
		if (m_dialog != nullptr) {
			return;   // ya está abierto
		}
		// Se añade el último a la raíz: se pinta encima de todo.
		m_dialog       = &m_ui.root().add<Panel>(k_dialog);
		m_dialog_text  = &m_dialog->add<Label>(u8"Aún no hay opciones", Align::Center);
		m_dialog_close = &m_dialog->add<Button>(u8"Cerrar");   // con el aspecto del tema

		// El escenario de Widget::remove(): este botón está DENTRO del diálogo que quita.
		// No pasa nada: se destruyen cuando Ui termina de repartir el clic. Pero nuestros
		// punteros hay que vaciarlos a mano: Ui no sabe que los tenemos.
		m_dialog_close->on_click = [this] {
			m_dialog->remove();
			m_dialog       = nullptr;
			m_dialog_text  = nullptr;
			m_dialog_close = nullptr;
		};
	}

	static constexpr luxel::ui::BoxStyle k_back{
		.color = { 0.02f, 0.05f, 0.10f, 0.75f }, .corner_radius = 24.0f,
		.border_thickness = 1.0f, .border_color = { 1.0f, 1.0f, 1.0f, 0.35f }
	};
	static constexpr luxel::ui::BoxStyle k_dialog{
		.color = { 0.05f, 0.06f, 0.09f, 0.95f }, .corner_radius = 16.0f,
		.border_thickness = 2.0f, .border_color = { 1.0f, 0.85f, 0.35f, 1.0f }
	};

	luxel::ui::Ui m_ui;

	// Punteros a widgets del árbol: no son dueños (lo es m_ui).
	luxel::ui::Panel* m_panel{ nullptr };
	std::array<luxel::ui::Button*, 3> m_buttons{};
	luxel::ui::Panel*  m_dialog{ nullptr };         // nullptr = cerrado
	luxel::ui::Label*  m_dialog_text{ nullptr };
	luxel::ui::Button* m_dialog_close{ nullptr };
};
