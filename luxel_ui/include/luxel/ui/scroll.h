#pragma once

// Una lista más larga que su hueco: ScrollView y su barra, ScrollBar. Solo en
// vertical.

#include <luxel/ui/layout.h>
#include <luxel/ui/painter.h>
#include <luxel/ui/widget.h>
#include <functional>

namespace luxel::ui
{
	// Una barra vertical: el carril y el "pulgar" que se arrastra. No sabe qué mueve:
	// le dicen los tamaños y ella avisa con on_change. Todo en píxeles.
	class ScrollBar : public Widget
	{
	public:
		// Se llama mientras se arrastra, con el nuevo desplazamiento. Puede salirse de
		// los límites (se arrastra más allá del final): lo recorta quien lo recibe.
		std::function<void(float)> on_change;

		// view: lo que se ve. content: lo que mide todo. scroll: cuánto se ha bajado.
		void set_values(float view, float content, float scroll);

		// Dónde está el pulgar, en píxeles de la ventana.
		Rect thumb_rect() const;

		void draw(Painter& painter) const override;

	protected:
		math::Vector2f measure_content(const Theme& theme) override;
		bool on_mouse_down(const Event& event) override;
		void on_mouse_drag(const Event& event) override;
		void on_mouse_up(const Event&) override { m_dragging = false; }
		void on_mouse_cancel() override { m_dragging = false; }

	private:
		void drag_to(float mouse_y);

		float m_view{ 0.0f };
		float m_content{ 0.0f };
		float m_scroll{ 0.0f };
		float m_min_thumb{ 0.0f };   // del tema (se apunta al medir)
		bool m_dragging{ false };

		// Por dónde se agarró el pulgar: distancia de su canto de arriba al ratón. Sin
		// esto, al apretar saltaría hasta poner su canto justo debajo del ratón.
		float m_grab{ 0.0f };
	};

	// Enseña un trozo de una columna y deja moverla con la rueda o con la barra.
	//
	//     auto& list = side.add<ScrollView>();
	//     list.set_grow(1.0f);                         // el alto que sobre
	//     list.content().add<Label>(u8"Manzana");      // ¡en content(), no en list!
	//
	// Por dentro:
	//
	//     ScrollView
	//       ├─ ventana (recorta)
	//       │    └─ content(): una Column, más alta que la ventana y subida "scroll" px
	//       └─ ScrollBar (escondida si cabe todo)
	//
	// A lo alto no pide nada (para eso está): hay que darle set_grow o set_min_size.
	class ScrollView : public Widget
	{
	public:
		ScrollView();

		Column& content() { return m_content; }
		const ScrollBar& bar() const { return m_bar; }

		// Cuántos píxeles se ha bajado: entre 0 y max_scroll(). Se recorta solo.
		float scroll() const { return m_scroll; }
		void set_scroll(float scroll);

		// Lo que no cabe, según el último layout.
		float max_scroll() const { return m_max_scroll; }

	protected:
		math::Vector2f measure_content(const Theme& theme) override;
		void arrange_children() override;
		bool on_mouse_wheel(const Event& event) override;

	private:
		// Son hijos suyos (los crea el constructor y mueren con él). El orden importa:
		// m_content se crea dentro de m_viewport.
		Widget& m_viewport;
		Column& m_content;
		ScrollBar& m_bar;

		float m_scroll{ 0.0f };
		float m_max_scroll{ 0.0f };
		float m_step{ 0.0f };   // del tema (se apunta al medir)
	};

} // namespace luxel::ui
