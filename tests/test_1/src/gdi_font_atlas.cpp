#include "gdi_font_atlas.h"

#define WIN32_LEAN_AND_MEAN   // sin las partes raras de windows.h (sockets, COM...)
#define NOMINMAX              // que windows.h no defina las macros min/max (rompen std::min)
#include <windows.h>

#include <algorithm>
#include <stdexcept>

// Cómo funciona, en cuatro pasos:
//   1) Crear una "imagen en memoria" de GDI (DIB section: un bitmap cuyos
//      píxeles podemos leer directamente) y un DC que dibuje en ella.
//   2) Elegir la fuente (CreateFontW) y medirla (GetTextMetricsW) para decidir
//      el tamaño de las celdas.
//   3) Pintar cada carácter en su celda, en blanco sobre negro, con suavizado en
//      escala de grises (ANTIALIASED_QUALITY: sin ClearType, que mete colores).
//   4) Leer los píxeles: como es blanco sobre negro, el rojo de cada píxel ES
//      cuánto lo cubre la letra (0-255). Eso pasa a ser el alfa del atlas, con
//      RGB blanco: al dibujar, color del texto * blanco = color del texto.

namespace
{
	// Libera objetos de GDI al salir del ámbito (también si algo lanza).
	struct DcGuard     { HDC dc;        ~DcGuard()     { if (dc) DeleteDC(dc); } };
	struct ObjectGuard { HGDIOBJ obj;   ~ObjectGuard() { if (obj) DeleteObject(obj); } };
}

FontAtlasImage make_gdi_font_atlas(const std::wstring& face_name, int pixel_height, bool bold,
                                   char32_t first, char32_t last)
{
	if (last < first || last > 0xFFFF) {
		throw std::runtime_error("make_gdi_font_atlas: rango de caracteres no válido");
	}

	DcGuard dc{ CreateCompatibleDC(nullptr) };
	if (!dc.dc) {
		throw std::runtime_error("make_gdi_font_atlas: CreateCompatibleDC falló");
	}

	// Alto negativo = alto del carácter en píxeles (sin el espacio interlineal).
	ObjectGuard font{ CreateFontW(-pixel_height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
	                              DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
	                              DEFAULT_PITCH | FF_DONTCARE, face_name.c_str()) };
	if (!font.obj) {
		throw std::runtime_error("make_gdi_font_atlas: CreateFontW falló");
	}
	SelectObject(dc.dc, font.obj);

	// --- Medidas de la fuente y de las celdas ---
	TEXTMETRICW metrics{};
	GetTextMetricsW(dc.dc, &metrics);

	const uint32_t glyph_count = static_cast<uint32_t>(last - first + 1);
	std::vector<float> advances(glyph_count);
	int widest = 0;
	for (uint32_t g = 0; g < glyph_count; ++g)
	{
		const wchar_t ch = static_cast<wchar_t>(first + g);
		SIZE extent{};
		GetTextExtentPoint32W(dc.dc, &ch, 1, &extent);   // cuánto avanza el lápiz con esta letra
		advances[g] = static_cast<float>(extent.cx);
		widest = std::max<int>(widest, extent.cx);
	}

	// Margen alrededor de cada glifo: algunas letras se salen un poco de su
	// avance (la cola de la 'j', la 'f' en cursiva...) y el filtrado lineal mira
	// el píxel de al lado. 2 px de margen evitan que se cuele la celda vecina.
	const uint32_t padding = 2;
	const uint32_t cell_w  = static_cast<uint32_t>(widest) + 2 * padding + 2;   // +2 por los glifos que sobresalen
	const uint32_t cell_h  = static_cast<uint32_t>(metrics.tmHeight) + 2 * padding;
	const uint32_t columns = 16;
	const uint32_t rows    = (glyph_count + columns - 1) / columns;
	const uint32_t width   = columns * cell_w;
	const uint32_t height  = rows * cell_h;

	// --- Imagen en memoria: 32 bits por píxel, filas de arriba abajo
	//     (biHeight negativo = "top-down", como las UV de luxel) ---
	BITMAPINFO info{};
	info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth       = static_cast<LONG>(width);
	info.bmiHeader.biHeight      = -static_cast<LONG>(height);
	info.bmiHeader.biPlanes      = 1;
	info.bmiHeader.biBitCount    = 32;
	info.bmiHeader.biCompression = BI_RGB;

	void* bits = nullptr;
	ObjectGuard bitmap{ CreateDIBSection(dc.dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0) };
	if (!bitmap.obj || !bits) {
		throw std::runtime_error("make_gdi_font_atlas: CreateDIBSection falló");
	}
	SelectObject(dc.dc, bitmap.obj);
	std::fill_n(static_cast<uint32_t*>(bits), static_cast<size_t>(width) * height, 0u);   // fondo negro

	// --- Pintar cada carácter en su celda ---
	SetBkMode(dc.dc, TRANSPARENT);
	SetTextColor(dc.dc, RGB(255, 255, 255));
	for (uint32_t g = 0; g < glyph_count; ++g)
	{
		const wchar_t ch = static_cast<wchar_t>(first + g);
		const int x = static_cast<int>((g % columns) * cell_w + padding);
		const int y = static_cast<int>((g / columns) * cell_h + padding);
		TextOutW(dc.dc, x, y, &ch, 1);   // (x, y) = arriba a la izquierda del carácter
	}
	GdiFlush();   // GDI puede ir con retraso: asegurar que ha terminado antes de leer

	// --- Pasar a RGBA: blanco con alfa = rojo (la cobertura) ---
	FontAtlasImage atlas;
	atlas.width           = width;
	atlas.height          = height;
	atlas.cell_width      = cell_w;
	atlas.cell_height     = cell_h;
	atlas.columns         = columns;
	atlas.padding         = padding;
	atlas.first_codepoint = first;
	atlas.advances        = std::move(advances);
	atlas.line_height     = static_cast<float>(metrics.tmHeight + metrics.tmExternalLeading);
	atlas.pixels.resize(static_cast<size_t>(width) * height * 4);

	const auto* src = static_cast<const uint8_t*>(bits);   // BGRA (el alfa de GDI no vale: lo ignora)
	for (size_t p = 0; p < static_cast<size_t>(width) * height; ++p)
	{
		const uint8_t coverage = src[p * 4 + 2];   // canal rojo
		atlas.pixels[p * 4 + 0] = std::byte{ 0xFF };
		atlas.pixels[p * 4 + 1] = std::byte{ 0xFF };
		atlas.pixels[p * 4 + 2] = std::byte{ 0xFF };
		atlas.pixels[p * 4 + 3] = std::byte{ coverage };
	}
	return atlas;
}
