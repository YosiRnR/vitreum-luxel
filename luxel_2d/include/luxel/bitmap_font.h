#pragma once

// BitmapFont: una fuente hecha de una imagen (atlas) con los caracteres en una
// rejilla de celdas iguales, más el avance de cada carácter. Renderer2D::draw_text
// la usa para convertir texto en quads (uno por letra) dentro del mismo lote.
//
// Igual que el núcleo de luxel no lee ficheros, BitmapFont tampoco: recibe los
// píxeles y las medidas ya preparados. Quién los genere (una imagen de disco,
// GDI de Windows como hace test_1, o un rasterizador de TTF propio algún día)
// no es asunto suyo.
//
// Escenario (con una rejilla de 16 columnas que empieza en el carácter 32):
//   'A' = 65 -> glifo 65 - 32 = 33 -> columna 33 % 16 = 1, fila 33 / 16 = 2.
//   Se dibuja la celda entera como un quad (lo vacío es transparente) y el
//   "lápiz" avanza advance('A') píxeles para la letra siguiente. Así las letras
//   estrechas ('i') y anchas ('M') quedan bien juntas aunque las celdas sean iguales.
//
//   celda (cell_width x cell_height):
//   +-----------------+
//   |  padding        |   padding: margen en píxeles alrededor del glifo, para
//   |   +---------    |   que el filtrado lineal no mezcle con la celda vecina
//   |   | A  <- el lápiz empieza aquí (padding, padding): arriba a la
//   |   |               izquierda de la línea de texto
//   +-----------------+

#include <luxel/device.h>
#include <luxel/math/vectors.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace luxel
{
	struct BitmapFontDescriptor
	{
		// Atlas RGBA8, filas de arriba abajo. Lo normal: RGB blanco y alfa = cuánto
		// cubre la letra ese píxel (así el color del texto se elige al dibujar).
		uint32_t atlas_width{};
		uint32_t atlas_height{};
		std::span<const std::byte> atlas_pixels;

		uint32_t cell_width{};      // tamaño de cada celda, en píxeles
		uint32_t cell_height{};
		uint32_t columns{};         // celdas por fila
		uint32_t padding{};         // margen dentro de cada celda (ver dibujo)

		char32_t first_codepoint{ U' ' };   // carácter de la celda 0 (las siguientes van seguidas)
		std::span<const float> advances;    // avance de cada glifo, en píxeles (uno por celda usada)
		float line_height{};                // distancia entre líneas, en píxeles

		char32_t fallback{ U'?' };          // lo que se dibuja si falta un carácter
	};

	class BitmapFont
	{
	public:
		struct Glyph
		{
			math::Vector2f uv_min;   // celda en la textura
			math::Vector2f uv_max;
			float advance{};         // píxeles que avanza el lápiz tras esta letra
		};

		// Crea la textura del atlas y la tabla de glifos. Lanza
		// std::invalid_argument si el descriptor no cuadra (tamaños, número de
		// avances, fallback fuera de la fuente...).
		BitmapFont(Device& device, const BitmapFontDescriptor& desc);

		// Glifo de un carácter, o el de fallback si la fuente no lo tiene.
		const Glyph& glyph(char32_t codepoint) const;
		bool has_glyph(char32_t codepoint) const;

		Texture& texture() const { return *m_texture; }
		float cell_width() const { return m_cell_width; }      // píxeles
		float cell_height() const { return m_cell_height; }
		float padding() const { return m_padding; }
		float line_height() const { return m_line_height; }

		// Ancho en píxeles de la línea más larga de un texto (sin escalar). Para
		// centrar o alinear a la derecha: ancho_en_mundo = measure(texto) * escala.
		float measure(std::u8string_view text) const;

	private:
		Ref<Texture> m_texture;
		std::vector<Glyph> m_glyphs;
		char32_t m_first_codepoint{};
		uint32_t m_fallback_index{};
		float m_cell_width{};
		float m_cell_height{};
		float m_padding{};
		float m_line_height{};
	};

} // namespace luxel
