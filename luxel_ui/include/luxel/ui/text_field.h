#pragma once

#include <luxel/ui/widget.h>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace luxel::ui
{
	// Una caja de texto de una línea.
	//
	//     auto& name = panel.add<TextField>(u8"Iria");
	//     name.set_max_length(16);
	//     name.on_change = [&](const std::u8string& text) { player = text; };
	//
	// Se le da el foco con un clic (el cursor va a la letra más cercana al ratón) o con
	// Tab (el cursor va al final). Con el foco se queda TODAS las teclas menos Tab y
	// Escape: escribir "ada" no debe mover a Iria. Escape (o un clic fuera) le quita el
	// foco. Entiende flechas izquierda y derecha, Inicio, Fin, Retroceso y Supr.
	//
	// No pide ancho (no crecería al escribir): se le da con set_min_size o set_grow. Si
	// el texto no cabe, se desplaza para que el cursor se vea siempre.
	//
	// Pendiente: seleccionar, copiar y pegar, saltar palabras con Ctrl y el parpadeo del
	// cursor.
	class TextField : public Widget
	{
	public:
		explicit TextField(std::u8string text = {});

		const std::u8string& text() const { return m_text; }
		void set_text(std::u8string text);   // el cursor va al final; no avisa a on_change

		// El cursor, en bytes desde el principio (ver utf8_edit.h).
		std::size_t caret() const { return m_caret; }

		// Cuántos caracteres admite como mucho (0 = sin límite). No recorta lo que ya hay.
		void set_max_length(std::size_t characters) { m_max_length = characters; }

		std::function<void(const std::u8string&)> on_change;   // cada vez que cambia el texto
		std::function<void(const std::u8string&)> on_submit;   // al pulsar Enter

		Cursor cursor() const override { return Cursor::Text; }
		void draw(Painter& painter) const override;

	protected:
		math::Vector2f measure_content(const Theme& theme) override;
		void arrange_children() override;
		bool on_mouse_down(const Event& event) override;
		void on_focus_gained() override;
		bool on_key_down(const Event& event) override;
		bool on_text(const Event& event) override;

	private:
		// Una frontera entre letras: en qué byte está y a cuántos píxeles del principio
		// del texto. Se calculan al medir (hace falta la fuente) y sirven para pintar el
		// cursor y para saber en qué letra se ha hecho clic.
		struct Stop
		{
			std::size_t byte;
			float x;
		};

		float caret_x() const;
		void changed();

		std::u8string m_text;
		std::size_t m_caret{ 0 };
		std::size_t m_max_length{ 0 };

		std::vector<Stop> m_stops;
		float m_text_width{ 0.0f };
		float m_scroll{ 0.0f };          // cuántos píxeles del texto quedan escondidos a la izquierda
		math::Vector2f m_padding{};      // los del tema, guardados al medir
		float m_caret_width{ 0.0f };
	};

} // namespace luxel::ui
