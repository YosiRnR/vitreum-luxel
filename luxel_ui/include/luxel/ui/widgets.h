#pragma once

// Los widgets básicos: Panel, Label y Button.

#include <luxel/ui/layout.h>
#include <luxel/ui/painter.h>
#include <luxel/ui/widget.h>
#include <functional>
#include <optional>
#include <string>

namespace luxel::ui
{
	// Un rectángulo con fondo y borde. Con un segundo estilo puede cambiar de
	// aspecto cuando tiene el ratón encima. Coloca a sus hijos como un Stack (con
	// uno solo, lo normal: el contenido, dentro de su set_padding).
	class Panel : public Stack
	{
	public:
		explicit Panel(const BoxStyle& style) : m_style(style) { set_ignores_mouse(false); }
		Panel(const BoxStyle& style, const BoxStyle& hover_style)
			: m_style(style), m_hover_style(hover_style) { set_ignores_mouse(false); }

		void draw(Painter& painter) const override;

	private:
		BoxStyle m_style;
		std::optional<BoxStyle> m_hover_style;   // vacío = siempre el mismo aspecto
	};

	enum class Align { Start, Center, End };

	// Una línea de texto, centrada en vertical. Ignora el ratón: es un adorno de
	// lo que tenga debajo.
	class Label : public Widget
	{
	public:
		explicit Label(std::u8string text, Align align = Align::Start);

		const std::u8string& text() const { return m_text; }
		void set_text(std::u8string text) { m_text = std::move(text); }

		void draw(Painter& painter) const override;

	protected:
		math::Vector2f measure_content(const Theme& theme) override;

	private:
		std::u8string m_text;
		Align m_align;
	};

	enum class ButtonState
	{
		Normal,
		Hovered,         // el ratón está encima, sin apretar
		Pressed,         // apretado y con el ratón encima: si se suelta, es un clic
		PressedOutside   // apretado, pero el ratón se ha ido: si se suelta, no pasa nada
	};

	// Un botón con texto. Clic = apretar Y soltar el botón izquierdo encima de él.
	//
	//     auto& salir = panel.add<Button>(u8"Salir");
	//     salir.on_click = [&] { quit = true; };   // "quit" debe vivir más que el botón
	class Button : public Widget
	{
	public:
		explicit Button(std::u8string text) : m_text(std::move(text)) {}

		std::function<void()> on_click;   // puede estar vacía

		ButtonState state() const;

		// Aspecto propio. Sin él, el del tema.
		void set_style(const ButtonStyle& style) { m_style = style; }

		void draw(Painter& painter) const override;

	protected:
		math::Vector2f measure_content(const Theme& theme) override;
		bool on_mouse_down(const Event& event) override;
		void on_mouse_up(const Event& event) override;
		void on_mouse_cancel() override;

	private:
		std::u8string m_text;
		std::optional<ButtonStyle> m_style;
		bool m_pressed{ false };
	};

} // namespace luxel::ui
