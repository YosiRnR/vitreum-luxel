#pragma once

#include <luxel/ui/geometry.h>
#include <luxel/ui/theme.h>
#include <luxel/renderer2d.h>
#include <string_view>

namespace luxel::ui
{
	// Lo que un widget usa para pintarse (D6 de docs/ui-design.md). Envuelve al
	// Renderer2D para que los widgets hablen en Rect y en el tema, y para que no
	// puedan llamar a begin/end (el lote es cosa de Ui).
	//
	// Todo va en la misma capa: lo que se pinta después queda encima, así que el
	// orden del árbol (padre, luego hijos) ya es el correcto.
	class Painter
	{
	public:
		Painter(Renderer2D& renderer, const Theme& theme) : m_renderer(renderer), m_theme(theme) {}

		const Theme& theme() const { return m_theme; }

		void box(const Rect& rect, const BoxStyle& style);

		// Una línea de texto, con su esquina de arriba a la izquierda en top_left.
		void text(std::u8string_view text, math::Vector2f top_left, float size, Color color);

		// Lo que ocupa un texto de una línea, en píxeles.
		math::Vector2f measure(std::u8string_view text, float size) const;

		void push_clip(const Rect& rect);
		void pop_clip();

	private:
		Renderer2D& m_renderer;
		const Theme& m_theme;
	};

} // namespace luxel::ui
