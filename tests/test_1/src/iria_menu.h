#pragma once

#include <luxel/ui/scroll.h>
#include <luxel/ui/text_field.h>
#include <luxel/ui/ui.h>
#include <luxel/ui/widgets.h>
#include <array>
#include <functional>

// La UI de la ventana de Iria, hecha con widgets de luxel_ui. A la derecha, una
// columna con la lista de objetos (que se estira con la ventana y se mueve con la rueda
// o con su barra) y el menú, con una caja para el nombre (el HUD saluda con él); "Jugar"
// esconde el menú (Escape lo devuelve), "Opciones" abre un diálogo que se quita a sí
// mismo y "Salir" llama a on_quit. Tab recorre la caja y los botones.
//
// Los widgets se crean UNA vez, en el constructor, y persisten (modo retained). En
// cada frame se colocan (update) y se pintan. El diálogo es la excepción: nace y
// muere mientras la aplicación funciona. Aquí no hay ni una cuenta de posiciones:
// solo se dice cómo se reparte el espacio.
class IriaMenu
{
public:
	std::function<void()> on_quit;

	explicit IriaMenu(const luxel::BitmapFont* font)
		: m_ui({ .font = font, .text_size = 30.0f })
	{
		using namespace luxel::ui;

		// El margen de toda la ventana (arriba, más: ahí está el estado de Iria).
		m_screen = &m_ui.root().add<Stack>();
		m_screen->set_padding({ 20.0f, 90.0f, 20.0f, 20.0f });

		// Columna pegada a la derecha y tan alta como la ventana.
		auto& side = m_screen->add<Column>();
		side.set_align(Placement::End, Placement::Fill);
		side.set_gap(20.0f);

		// La lista de objetos. Se queda con todo el alto que sobre.
		auto& list_panel = side.add<Panel>(k_list);
		list_panel.set_padding(Insets::all(12.0f));
		list_panel.set_min_size({ 260.0f, 110.0f });
		list_panel.set_grow(1.0f);
		fill_list(list_panel.add<ScrollView>().content());

		// El menú: se aclara el borde si el ratón está en él (y no en un botón).
		BoxStyle back_hover = k_back;
		back_hover.border_color = { 1.0f, 1.0f, 1.0f, 1.0f };
		m_panel = &side.add<Panel>(k_back, back_hover);
		m_panel->set_padding(Insets::all(20.0f));
		auto& buttons = m_panel->add<Column>();
		buttons.set_gap(17.0f);

		// El nombre: al escribir cambia el saludo; Enter devuelve el teclado al juego.
		buttons.add<Label>(u8"Tu nombre:");
		auto& name = buttons.add<TextField>(m_name);
		name.set_max_length(12);
		name.on_change = [this](const std::u8string& text) { m_name = text; };
		name.on_submit = [this](const std::u8string&) { m_ui.set_focus(nullptr); };

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

		std::array<Button*, 3> items{};
		for (size_t i = 0; i < items.size(); ++i)
		{
			ButtonStyle style{ .normal = normal[i], .hover = normal[i], .pressed = normal[i] };
			style.hover.color   = hover_color[i];
			style.pressed.color = pressed_color[i];
			items[i] = &buttons.add<Button>(labels[i]);
			items[i]->set_style(style);
		}

		// Las lambdas capturan this: vale porque los botones mueren con m_ui, que es
		// parte de este objeto (y por eso IriaMenu no se puede copiar ni mover).
		items[0]->on_click = [this] { m_panel->set_visible(false); };
		items[1]->on_click = [this] { open_options(); };
		items[2]->on_click = [this] { if (on_quit) { on_quit(); } };
	}

	IriaMenu(const IriaMenu&) = delete;
	IriaMenu& operator=(const IriaMenu&) = delete;

	bool on_event(const luxel::ui::Event& event) { return m_ui.on_event(event); }

	void show() { m_panel->set_visible(true); }

	const std::u8string& player_name() const { return m_name; }

	// El puntero que pide la UI (la "I" encima de la caja del nombre).
	luxel::ui::Cursor cursor() const { return m_ui.cursor(); }

	// Una vez por frame: coloca todo para este tamaño de ventana.
	void update(float width, float height) { m_ui.update(width, height); }

	void draw(luxel::Renderer2D& r2d, luxel::Frame& frame) const { m_ui.draw(r2d, frame); }

private:
	// Una fila por objeto: [punto] [nombre]. Se aclara con el ratón encima.
	static void fill_list(luxel::ui::Column& list)
	{
		using namespace luxel::ui;
		const std::u8string names[] = {
			u8"Manzana", u8"Seta del bosque encantado", u8"Piña", u8"Llave", u8"Cuerda larguísima de 20 metros",
			u8"Flor", u8"Mapa", u8"Poción de salto", u8"Galleta", u8"Linterna", u8"Piedra", u8"Pluma"
		};
		list.set_gap(4.0f);
		list.set_padding({ 0.0f, 0.0f, 6.0f, 0.0f });   // aire entre las filas y la barra
		for (const std::u8string& name : names)
		{
			auto& row = list.add<Panel>(k_row, k_row_hover);
			row.set_padding({ 10.0f, 3.0f, 10.0f, 3.0f });
			auto& cells = row.add<Row>();
			cells.set_gap(12.0f);

			auto& dot = cells.add<Panel>(BoxStyle{ .color = { 1.0f, 0.75f, 0.30f, 1.0f }, .corner_radius = 9.0f });
			dot.set_min_size({ 18.0f, 18.0f });
			dot.set_align(Placement::Fill, Placement::Center);
			dot.set_ignores_mouse(true);   // el ratón es de la fila entera

			auto& label = cells.add<Label>(name);
			label.set_clip_text(true);     // los nombres largos se cortan
			label.set_grow(1.0f);
		}
	}

	void open_options()
	{
		using namespace luxel::ui;
		if (m_dialog != nullptr) {
			return;   // ya está abierto
		}
		// Centrado y tan grande como pida su contenido. Va el último en la pantalla, así
		// que se pinta encima de todo.
		m_dialog = &m_screen->add<Panel>(k_dialog);
		m_dialog->set_align(Placement::Center, Placement::Center);
		m_dialog->set_padding(Insets::all(24.0f));

		auto& column = m_dialog->add<Column>();
		column.set_gap(24.0f);
		m_dialog_lines = &column.add<Column>();   // aquí se van añadiendo las líneas
		m_dialog_lines->set_gap(4.0f);
		m_dialog_lines->add<Label>(u8"Aún no hay opciones", Align::Center);

		// Los dos botones se reparten el ancho que sobre: 1 parte y 2 partes.
		auto& row = column.add<Row>();
		row.set_gap(16.0f);
		row.set_min_size({ 480.0f, 0.0f });
		auto& close = row.add<Button>(u8"Cerrar");
		auto& more  = row.add<Button>(u8"Otra línea");
		close.set_grow(1.0f);
		more.set_grow(2.0f);

		more.on_click = [this] { m_dialog_lines->add<Label>(u8"...pero el diálogo crece solo", Align::Center); };

		// El escenario de Widget::remove(): este botón está DENTRO del diálogo que quita.
		// No pasa nada: se destruyen cuando Ui termina de repartir el clic. Pero nuestros
		// punteros hay que vaciarlos a mano: Ui no sabe que los tenemos.
		close.on_click = [this] {
			m_dialog->remove();
			m_dialog       = nullptr;
			m_dialog_lines = nullptr;
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

	static constexpr luxel::ui::BoxStyle k_list{
		.color = { 0.02f, 0.05f, 0.10f, 0.75f }, .corner_radius = 16.0f,
		.border_thickness = 2.0f, .border_color = { 1.0f, 0.75f, 0.30f, 1.0f }
	};
	static constexpr luxel::ui::BoxStyle k_row{ .color = { 0.10f, 0.20f, 0.35f, 1.0f }, .corner_radius = 8.0f };
	static constexpr luxel::ui::BoxStyle k_row_hover{ .color = { 0.20f, 0.38f, 0.62f, 1.0f }, .corner_radius = 8.0f };

	std::u8string m_name{ u8"Iria" };   // antes que m_ui: la caja se crea con él
	luxel::ui::Ui m_ui;

	// Punteros a widgets del árbol: no son dueños (lo es m_ui).
	luxel::ui::Stack*  m_screen{ nullptr };
	luxel::ui::Panel*  m_panel{ nullptr };
	luxel::ui::Panel*  m_dialog{ nullptr };         // nullptr = cerrado
	luxel::ui::Column* m_dialog_lines{ nullptr };
};
