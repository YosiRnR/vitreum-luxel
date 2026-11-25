#pragma once

// Los widgets básicos. De momento, Panel y Label.

#include <luxel/ui/painter.h>
#include <luxel/ui/widget.h>
#include <optional>
#include <string>

namespace luxel::ui
{
	// Un rectángulo con fondo y borde. Con un segundo estilo puede cambiar de
	// aspecto cuando tiene el ratón encima.
	class Panel : public Widget
	{
	public:
		explicit Panel(const BoxStyle& style) : m_style(style) {}
		Panel(const BoxStyle& style, const BoxStyle& hover_style) : m_style(style), m_hover_style(hover_style) {}

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

	private:
		std::u8string m_text;
		Align m_align;
	};

} // namespace luxel::ui
