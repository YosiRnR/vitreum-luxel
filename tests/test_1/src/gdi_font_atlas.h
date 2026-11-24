#pragma once

// Genera el atlas de una BitmapFont pintando una fuente del sistema con GDI
// (Win32). Es código de plataforma de la APLICACIÓN: luxel_2d solo recibe el
// resultado (píxeles + avances), sin saber de dónde salen.
//
// Más adelante esto se puede sustituir por un rasterizador de TTF propio (hito
// de aprendizaje) sin tocar BitmapFont ni el Renderer2D.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct FontAtlasImage
{
	std::vector<std::byte> pixels;   // RGBA8: blanco con alfa = cobertura
	uint32_t width{};
	uint32_t height{};
	uint32_t cell_width{};
	uint32_t cell_height{};
	uint32_t columns{};
	uint32_t padding{};
	char32_t first_codepoint{};
	std::vector<float> advances;     // uno por glifo, en píxeles
	float line_height{};
};

// Caracteres de first a last (incluidos). Por defecto Latin-1 imprimible
// (32-255): ASCII más ¡ ¿ á é í ó ú ñ ü ç... Para esos caracteres el número
// Unicode coincide con el de Latin-1, así que basta con un wchar_t por número.
// pixel_height: alto de la fuente en píxeles. Lanza std::runtime_error si GDI falla.
FontAtlasImage make_gdi_font_atlas(const std::wstring& face_name, int pixel_height, bool bold = false,
                                   char32_t first = 32, char32_t last = 255);
