// Pruebas de luxel_ttf (hitos T1 a T4 de docs/ttf-design.md), sin GPU. Caja blanca:
// ve luxel_ttf/src (reader.h, cmap.h, glyf.h, outline_builder.h y rasterizer.h). Usa las fuentes de Windows (no se versionan: tienen
// licencia); si falta alguna, el caso lo dice y se salta.

#include "cmap.h"
#include "glyf.h"
#include "outline_builder.h"
#include "rasterizer.h"
#include "reader.h"
#include <luxel/ttf/font.h>
#include <luxel/ttf/raster.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace luxel::ttf;

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what, int line)
	{
		if (!condition) {
			++g_failures;
			std::cout << "  FALLO (linea " << line << "): " << what << "\n";
		}
	}

	// Leer el fichero es cosa de quien usa la biblioteca (aquí, el test).
	std::optional<std::vector<std::byte>> load_system_font(const char* name)
	{
		const std::string path = std::string("C:/Windows/Fonts/") + name;
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file) {
			std::cout << "  (no está " << path << ": se salta)\n";
			return std::nullopt;
		}
		std::vector<std::byte> bytes(static_cast<std::size_t>(file.tellg()));
		file.seekg(0);
		file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		return bytes;
	}

	template <typename F>
	bool throws_runtime_error(F&& f)
	{
		try { f(); }
		catch (const std::runtime_error&) { return true; }
		return false;
	}

	std::vector<std::byte> bytes_of(std::initializer_list<int> values)
	{
		std::vector<std::byte> out;
		for (const int v : values) {
			out.push_back(static_cast<std::byte>(v));
		}
		return out;
	}

	// Lo que tiene que cumplir 'loca' en cualquier fuente: ningún glifo se sale de
	// 'glyf', el último acaba justo donde acaba 'glyf' y cada glifo con contornos
	// empieza con una cabecera con sentido (número de contornos >= -1 y una caja
	// con mínimo <= máximo).
	void check_loca(const Font& font, int line)
	{
		const auto glyf = font.table(tag("glyf"));
		const GlyphLocation last = font.location(static_cast<uint16_t>(font.glyph_count() - 1));
		check(last.offset + last.length == glyf.size(), "el último glifo acaba donde acaba 'glyf'", line);

		int bad = 0;
		for (uint32_t g = 0; g < font.glyph_count(); ++g)
		{
			const GlyphLocation at = font.location(static_cast<uint16_t>(g));
			if (at.length == 0) {
				continue;   // sin contornos (el espacio)
			}
			if (at.length < 10) {
				++bad;      // ni le cabe la cabecera
				continue;
			}
			Reader r(glyf.subspan(at.offset, at.length), "glifo");
			const int16_t contours = r.i16();
			const int16_t x_min = r.i16(), y_min = r.i16(), x_max = r.i16(), y_max = r.i16();
			if (contours < -1 || x_min > x_max || y_min > y_max) {
				++bad;
			}
		}
		check(bad == 0, "todos los glifos empiezan con una cabecera con sentido", line);
		if (bad != 0) {
			std::cout << "  (" << bad << " glifos con basura en la cabecera)\n";
		}
	}
}

#define CHECK(cond) check((cond), #cond, __LINE__)

namespace
{
	bool near(float a, float b, float tolerance = 0.001f) { return std::abs(a - b) <= tolerance; }
	bool near(Point a, Point b, float tolerance = 0.001f) { return near(a.x, b.x, tolerance) && near(a.y, b.y, tolerance); }

	// Que cada contorno sea una cadena cerrada: cada segmento empieza donde acaba el
	// anterior y el último vuelve al principio.
	bool contours_closed(const Outline& outline)
	{
		uint32_t first = 0;
		for (const uint32_t end : outline.contour_ends)
		{
			if (end <= first || end > outline.segments.size()) {
				return false;
			}
			for (uint32_t i = first; i < end; ++i)
			{
				const uint32_t next = (i + 1 < end) ? i + 1 : first;
				if (!near(outline.segments[i].to, outline.segments[next].from)) {
					return false;
				}
			}
			first = end;
		}
		return first == outline.segments.size();
	}
}

// 1. Reader: big-endian y sin salirse nunca.
static void test_reader()
{
	std::cout << "[1] Reader\n";
	const auto data = bytes_of({ 0x08, 0x00, 0xFF, 0xFF, 0x12, 0x34, 0x56, 0x78 });
	Reader r(data, "prueba");
	CHECK(r.u16() == 2048);              // 0x0800: el byte alto primero
	CHECK(r.i16() == -1);                // 0xFFFF con signo
	CHECK(r.u32() == 0x12345678);
	CHECK(r.tell() == 8);
	CHECK(throws_runtime_error([&] { r.u8(); }));         // no queda nada
	CHECK(throws_runtime_error([&] { r.seek(9); }));
	r.seek(8);                                             // justo al final sí se puede estar

	Reader whole(data, "prueba");
	Reader part = whole.sub(4, 4, "trozo");
	CHECK(part.u32() == 0x12345678);
	CHECK(throws_runtime_error([&] { part.u8(); }));      // el trozo no ve lo de después
	CHECK(throws_runtime_error([&] { whole.sub(6, 4, "trozo"); }));
	CHECK(throws_runtime_error([&] { whole.sub(SIZE_MAX, 2, "trozo"); }));   // sin desbordar
}

// 2. Segoe UI negrita: las tablas básicas (loca en formato largo).
static void test_segoe()
{
	std::cout << "[2] Segoe UI negrita\n";
	const auto data = load_system_font("segoeuib.ttf");
	if (!data) {
		return;
	}
	const Font font(*data);
	CHECK(font.units_per_em() == 2048);
	CHECK(font.glyph_count() == 5321);
	CHECK(font.ascent() == 2210 && font.descent() == -514 && font.line_gap() == 0);
	CHECK(font.has_table(tag("cmap")) && !font.has_table(tag("CFF ")));
	CHECK(font.metrics(0).advance == 1322);
	CHECK(font.location(0).length > 0);   // .notdef suele ser un rectángulo
	check_loca(font, __LINE__);

	bool out_of_range = false;
	try { (void)font.metrics(5321); } catch (const std::out_of_range&) { out_of_range = true; }
	CHECK(out_of_range);
}

// 3. Lucida Console: 'loca' en formato CORTO. El reto de T1.
static void test_short_loca()
{
	std::cout << "[3] Lucida Console ('loca' corta)\n";
	const auto data = load_system_font("lucon.ttf");
	if (!data) {
		return;
	}
	const Font font(*data);
	CHECK(font.glyph_count() == 667);
	CHECK(font.table(tag("glyf")).size() == 63748);
	check_loca(font, __LINE__);
}

// 4. Consolas: 'hmtx' con la parte corta (los últimos glifos repiten el avance).
static void test_hmtx_tail()
{
	std::cout << "[4] Consolas ('hmtx' con cola)\n";
	const auto data = load_system_font("consola.ttf");
	if (!data) {
		return;
	}
	const Font font(*data);
	CHECK(font.glyph_count() == 3031);
	CHECK(font.line_gap() == 350);
	// Solo las 2638 primeras traen avance; las demás repiten el de la 2637. En una
	// monoespaciada, todos avanzan lo mismo.
	CHECK(font.metrics(0).advance == 1126);
	CHECK(font.metrics(2637).advance == 1126);
	CHECK(font.metrics(3030).advance == 1126);
	check_loca(font, __LINE__);
}

// 5. Ficheros rotos: siempre runtime_error, nunca leer fuera.
static void test_broken()
{
	std::cout << "[5] ficheros rotos\n";
	CHECK(throws_runtime_error([] { Font font(std::span<const std::byte>{}); }));
	const auto otto = bytes_of({ 'O', 'T', 'T', 'O', 0, 0 });
	CHECK(throws_runtime_error([&] { Font font(otto); }));
	const auto junk = bytes_of({ 'h', 'o', 'l', 'a', 0, 0, 0, 0, 0, 0, 0, 0 });
	CHECK(throws_runtime_error([&] { Font font(junk); }));

	// Una tabla que dice estar más allá del final del fichero.
	const auto lying = bytes_of({ 0, 1, 0, 0,   0, 1,   0, 0, 0, 0, 0, 0,
	                              'h', 'e', 'a', 'd',   0, 0, 0, 0,   0, 0, 0x10, 0,   0, 0, 0, 54 });
	CHECK(throws_runtime_error([&] { Font font(lying); }));

	const auto data = load_system_font("segoeuib.ttf");
	if (!data) {
		return;
	}
	// Cortada en muchos sitios distintos: en ninguno debe colarse.
	int escaped = 0;
	for (const std::size_t size : { std::size_t{ 10 }, std::size_t{ 100 }, std::size_t{ 1000 },
	                                data->size() / 2, data->size() - 1 })
	{
		const std::span<const std::byte> cut(data->data(), size);
		if (!throws_runtime_error([&] { Font font(cut); })) {
			++escaped;
		}
	}
	CHECK(escaped == 0);
}

// 6. 'cmap' formato 4: de carácter a glifo (tu parte, cmap_format4.cpp).
static void test_cmap_format4()
{
	std::cout << "[6] 'cmap' formato 4\n";
	if (const auto data = load_system_font("segoeuib.ttf"))
	{
		const Font font(*data);
		CHECK(font.glyph_index(U'A') == 36);
		CHECK(font.glyph_index(U'a') == 68);
		CHECK(font.glyph_index(U'?') == 34);
		CHECK(font.glyph_index(U'ñ') == 120);      // tramo con idRangeOffset != 0
		CHECK(font.glyph_index(U'€') == 189);
		CHECK(font.glyph_index(U' ') == 3);
		CHECK(font.location(font.glyph_index(U' ')).length == 0);   // el espacio no tiene contornos
		CHECK(font.glyph_index(0x1F600) == 0);    // emoji: más allá del formato 4
		CHECK(font.glyph_index(0x0378) == 0);     // sin asignar en Unicode: cae en un hueco
		CHECK(font.glyph_index(0xFFFF) == 0);     // el último tramo, el de cierre
		CHECK(font.glyph_index(0x10FFFF) == 0);
	}
	if (const auto data = load_system_font("consola.ttf"))
	{
		const Font font(*data);                   // otra numeración: la A no es la 36
		CHECK(font.glyph_index(U'A') == 4 && font.glyph_index(U'a') == 131);
		CHECK(font.glyph_index(U'ñ') == 215 && font.glyph_index(U'€') == 602);
	}
	if (const auto data = load_system_font("lucon.ttf"))
	{
		const Font font(*data);
		CHECK(font.glyph_index(U'€') == 665);
	}
}

// 7. Arial trae las dos: el formato 4 tuyo tiene que coincidir con el 12 en todo el
// plano básico. Y lo que pasa de U+FFFF solo lo sabe el 12.
static void test_cmap_formats_agree()
{
	std::cout << "[7] Arial: formato 4 contra formato 12\n";
	const auto data = load_system_font("arial.ttf");
	if (!data) {
		return;
	}
	const Font font(*data);
	const Reader cmap(font.table(tag("cmap")), "'cmap'");
	const auto format4  = detail::read_cmap(cmap, 4);
	const auto format12 = detail::read_cmap(cmap, 12);
	CHECK(format4 && format4->format() == 4);
	CHECK(format12 && format12->format() == 12);
	if (!format4 || !format12) {
		return;
	}
	int differences = 0;
	int found = 0;
	for (char32_t c = 0; c <= 0xFFFF; ++c)
	{
		const uint16_t g = format4->glyph_index(c, font.glyph_count());
		differences += g != format12->glyph_index(c, font.glyph_count());
		found += g != 0;
	}
	CHECK(differences == 0);
	CHECK(found > 3000);                                // que no coincidan por ser todo 0
	if (differences != 0) {
		std::cout << "  (" << differences << " caracteres distintos)\n";
	}
	CHECK(format4->glyph_index(0x1DF27, font.glyph_count()) == 0);
	CHECK(font.glyph_index(0x1DF27) == 4647);           // Font usa la 12: llega más lejos
	CHECK(font.glyph_index(U'A') == 36);
}

// 8. Subtablas hechas a mano: formato 12 roto y un formato 4 con id_delta en glyphIdArray.
static void test_cmap_broken()
{
	std::cout << "[8] 'cmap' hechas a mano\n";
	// Cabecera de 'cmap' con una subtabla (3, 10) en el offset 12, y detrás un formato 12.
	const auto make = [](std::initializer_list<int> groups_bytes, int group_count) {
		std::vector<std::byte> out = bytes_of({ 0, 0, 0, 1,   0, 3, 0, 10, 0, 0, 0, 12,
		                                        0, 12, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
		                                        0, 0, 0, group_count });
		const auto rest = bytes_of(groups_bytes);
		out.insert(out.end(), rest.begin(), rest.end());
		return out;
	};
	// Un grupo bueno: U+0041..U+0043 -> glifos 10..12.
	const auto good = make({ 0, 0, 0, 0x41,  0, 0, 0, 0x43,  0, 0, 0, 10 }, 1);
	const auto map = detail::read_cmap(Reader(good, "'cmap'"), 12);
	CHECK(map && map->glyph_index(U'B', 100) == 11 && map->glyph_index(U'D', 100) == 0);
	CHECK(map && map->glyph_index(U'C', 12) == 0);       // apunta a un glifo que no existe

	// Dice tener 200 grupos y trae uno.
	const auto lying = make({ 0, 0, 0, 0x41,  0, 0, 0, 0x43,  0, 0, 0, 10 }, 200);
	CHECK(throws_runtime_error([&] { detail::read_cmap(Reader(lying, "'cmap'"), 12); }));
	// Dos grupos que se pisan.
	const auto overlap = make({ 0, 0, 0, 0x41,  0, 0, 0, 0x43,  0, 0, 0, 10,
	                            0, 0, 0, 0x43,  0, 0, 0, 0x45,  0, 0, 0, 20 }, 2);
	CHECK(throws_runtime_error([&] { detail::read_cmap(Reader(overlap, "'cmap'"), 12); }));

	// Formato 4 hecho a mano con un tramo que lee de glyphIdArray Y tiene id_delta: lo
	// permite la especificación, pero ninguna fuente de este Windows lo hace. Dos tramos:
	//   0: 'A'..'B', id_delta 5, id_range_offset 4 -> glyphIdArray[0..1] = { 10, 0 }
	//   1: 0xFFFF (el de cierre), id_delta 1
	// id_range_offset[0] = 4 bytes: de su casilla hasta glyphIdArray hay 2 casillas.
	const auto format4 = bytes_of({ 0, 0, 0, 1,   0, 3, 0, 1, 0, 0, 0, 12,
	                                0, 4,   0, 36,   0, 0,                 // format, length, language
	                                0, 4,   0, 0, 0, 0, 0, 0,              // segCountX2 = 4, búsqueda
	                                0, 0x42,  0xFF, 0xFF,                  // endCode
	                                0, 0,                                  // reservedPad
	                                0, 0x41,  0xFF, 0xFF,                  // startCode
	                                0, 5,     0, 1,                        // idDelta
	                                0, 4,     0, 0,                        // idRangeOffset
	                                0, 10,    0, 0 });                     // glyphIdArray
	const auto map4 = detail::read_cmap(Reader(format4, "'cmap'"), 4);
	CHECK(map4 && map4->format() == 4);
	if (map4)
	{
		CHECK(map4->glyph_index(U'A', 100) == 15);   // 10 leído + 5 de id_delta
		CHECK(map4->glyph_index(U'B', 100) == 0);    // leído 0: no está (sin sumar id_delta)
		CHECK(map4->glyph_index(U'C', 100) == 0);    // en el hueco antes del tramo de cierre
		CHECK(map4->glyph_index(0xFFFF, 100) == 0);  // 0xFFFF + 1 da la vuelta a 0
	}
}

// 9. Contornos de letras de verdad (Segoe UI negrita a 2048 px por em: 1 unidad = 1 px).
static void test_real_outlines()
{
	std::cout << "[9] contornos de Segoe UI negrita\n";
	const auto data = load_system_font("segoeuib.ttf");
	if (!data) {
		return;
	}
	const Font font(*data);
	const float em = 2048.0f;   // a este tamaño los píxeles coinciden con las unidades

	const Outline l = font.outline(font.glyph_index(U'l'), em);
	CHECK(l.contour_ends.size() == 1 && contours_closed(l));
	CHECK(near(l.min, { 133, -1516 }) && near(l.max, { 449, 0 }));   // +Y abajo: la parte de arriba es negativa

	const Outline small = font.outline(font.glyph_index(U'l'), 48.0f);   // escala 48 / 2048
	CHECK(near(small.min, { 133 * 48.0f / em, -1516 * 48.0f / em }));

	CHECK(font.outline(font.glyph_index(U'o'), em).contour_ends.size() == 2);   // el de fuera y el agujero
	CHECK(font.outline(font.glyph_index(U'B'), em).contour_ends.size() == 3);
	CHECK(contours_closed(font.outline(font.glyph_index(U'B'), em)));
	CHECK(font.outline(font.glyph_index(U' '), em).empty());
	CHECK(!font.outline(0, em).empty());   // .notdef

	// La "ñ" es compuesta: la 'n' tal cual + la virgulilla (glifo 216 en esta fuente)
	// movida 228 unidades a la derecha. Tu append_component.
	const Outline n     = font.outline(font.glyph_index(U'n'), em);
	const Outline tilde = font.outline(216, em);
	const Outline enye  = font.outline(font.glyph_index(U'ñ'), em);
	CHECK(enye.contour_ends.size() == 2 && contours_closed(enye));
	CHECK(enye.segments.size() == n.segments.size() + tilde.segments.size());
	if (enye.segments.size() == n.segments.size() + tilde.segments.size() && !n.empty() && !tilde.empty())
	{
		CHECK(near(enye.segments.front().from, n.segments.front().from));
		const OutlineSegment& moved = enye.segments[n.segments.size()];
		CHECK(near(moved.from, { tilde.segments.front().from.x + 228, tilde.segments.front().from.y }));
	}
	CHECK(near(enye.max.y, 0) && near(enye.min.y, -1497));   // de la base a la punta de la virgulilla

	// Recorrer todos los glifos: ninguno se rompe y todos quedan cerrados.
	int broken = 0;
	for (uint32_t g = 0; g < font.glyph_count(); ++g) {
		try {
			if (!contours_closed(font.outline(static_cast<uint16_t>(g), 64.0f))) {
				++broken;
			}
		}
		catch (const std::exception&) {
			++broken;
		}
	}
	CHECK(broken == 0);
	if (broken != 0) {
		std::cout << "  (" << broken << " glifos rotos o sin cerrar)\n";
	}
}

// 10. Tu parte, por separado: f2dot14 y append_component.
static void test_components()
{
	std::cout << "[10] f2dot14 y append_component\n";
	CHECK(detail::f2dot14(0x4000) == 1.0f);
	CHECK(detail::f2dot14(0x2000) == 0.5f);
	CHECK(detail::f2dot14(static_cast<int16_t>(0xC000)) == -1.0f);
	CHECK(detail::f2dot14(static_cast<int16_t>(0x8000)) == -2.0f);
	CHECK(detail::f2dot14(0) == 0.0f);
	CHECK(near(detail::f2dot14(0x7FFF), 1.99993896f, 1e-6f));

	// Un cuadrado de 100 con la esquina en (0, 0), un contorno de 4 puntos.
	const detail::GlyphShape square{
		.points = { { 0, 0, true }, { 100, 0, true }, { 100, 100, false }, { 0, 100, true } },
		.contour_ends = { 3 }
	};
	detail::GlyphShape shape;
	detail::append_component(shape, square, { .dx = 10, .dy = 20 });
	CHECK(shape.points.size() == 4 && shape.contour_ends == std::vector<uint32_t>{ 3 });
	CHECK(shape.points.size() == 4 && shape.points[1].x == 110 && shape.points[1].y == 20);
	CHECK(shape.points.size() == 4 && !shape.points[2].on_curve && shape.points[3].on_curve);

	detail::append_component(shape, square, { .a = 0.5f, .d = 0.5f, .dx = -5, .dy = 5 });
	CHECK(shape.contour_ends == (std::vector<uint32_t>{ 3, 7 }));   // el segundo cuadrado son los puntos 4..7
	CHECK(shape.points.size() == 8 && shape.points[6].x == 45 && shape.points[6].y == 55);

	// Matriz completa: girar 90 grados a la izquierda, (x, y) -> (-y, x).
	detail::GlyphShape turned;
	detail::append_component(turned, square, { .a = 0, .b = 1, .c = -1, .d = 0 });
	CHECK(turned.points.size() == 4 && turned.points[1].x == 0 && turned.points[1].y == 100);
	CHECK(turned.points.size() == 4 && turned.points[2].x == -100 && turned.points[2].y == 100);
}

// 11. 'glyf' hecho a mano: simple, compuesto con escala, enganchado por puntos y uno
// que se incluye a sí mismo.
static void test_handmade_glyf()
{
	std::cout << "[11] 'glyf' hecho a mano\n";
	const auto glyf = bytes_of({
		// glifo 0 (32 bytes): un cuadrado de 100, 4 puntos on, coordenadas de 16 bits
		0, 1,   0, 0, 0, 0, 0, 100, 0, 100,           // 1 contorno, caja
		0, 3,   0, 0,                                 // endPts = { 3 }, sin instrucciones
		0x09, 3,                                      // flag "on" + repetir 3 veces más
		0, 0,   0, 100,   0, 0,   0xFF, 0x9C,         // x: 0, +100, 0, -100
		0, 0,   0, 0,   0, 100,   0, 0,               // y: 0, 0, +100, 0
		// glifo 1 (26 bytes): compuesto = glifo 0 movido (10, 20) + glifo 0 a la mitad movido (-5, 5)
		0xFF, 0xFF,   0, 0, 0, 0, 0, 0, 0, 0,
		0x00, 0x23,   0, 0,   0, 10,   0, 20,         // palabras, xy, hay más; glifo 0; (10, 20)
		0x00, 0x0A,   0, 0,   0xFB, 5,   0x20, 0x00,  // bytes, xy, escala; glifo 0; (-5, 5); 0,5
		// glifo 2 (16 bytes): se incluye a sí mismo
		0xFF, 0xFF,   0, 0, 0, 0, 0, 0, 0, 0,
		0x00, 0x02,   0, 2,   0, 0,
		// glifo 3 (22 bytes): glifo 0 + otro glifo 0 con su punto 0 enganchado al punto 2 del primero
		0xFF, 0xFF,   0, 0, 0, 0, 0, 0, 0, 0,
		0x00, 0x22,   0, 0,   0, 0,                   // xy (0, 0), hay más
		0x00, 0x00,   0, 0,   2, 0,                   // sin xy: arg1 = punto 2 de lo que hay, arg2 = punto 0 de la pieza
	});
	const std::vector<uint32_t> loca{ 0, 32, 58, 74, 96 };

	const detail::GlyphShape square = detail::read_glyph_shape(glyf, loca, 0);
	CHECK(square.points.size() == 4 && square.contour_ends == std::vector<uint32_t>{ 3 });
	CHECK(square.points.size() == 4 && square.points[2].x == 100 && square.points[2].y == 100 && square.points[2].on_curve);

	const detail::GlyphShape two = detail::read_glyph_shape(glyf, loca, 1);
	CHECK(two.points.size() == 8 && two.contour_ends == (std::vector<uint32_t>{ 3, 7 }));
	CHECK(two.points.size() == 8 && two.points[0].x == 10 && two.points[0].y == 20);
	CHECK(two.points.size() == 8 && two.points[6].x == 45 && two.points[6].y == 55);

	CHECK(throws_runtime_error([&] { detail::read_glyph_shape(glyf, loca, 2); }));

	const detail::GlyphShape hooked = detail::read_glyph_shape(glyf, loca, 3);
	CHECK(hooked.points.size() == 8 && hooked.points[4].x == 100 && hooked.points[4].y == 100);
	CHECK(hooked.points.size() == 8 && hooked.points[6].x == 200 && hooked.points[6].y == 200);

	// A contorno, a escala 1: 4 rectas, +Y abajo.
	const Outline outline = detail::build_outline(square, 1.0f);
	CHECK(outline.segments.size() == 4 && contours_closed(outline));
	CHECK(near(outline.min, { 0, -100 }) && near(outline.max, { 100, 0 }));
	CHECK(std::ranges::none_of(outline.segments, &OutlineSegment::is_curve));
}

// 12. Puntos implícitos: entre dos de control seguidos hay uno "en la curva" en medio.
static void test_implicit_points()
{
	std::cout << "[12] puntos implicitos\n";
	// Cuatro puntos de control, ninguno en la curva: un "círculo" de 4 cuadráticas.
	const detail::GlyphShape round{
		.points = { { 0, 0, false }, { 100, 0, false }, { 100, 100, false }, { 0, 100, false } },
		.contour_ends = { 3 }
	};
	const Outline circle = detail::build_outline(round, 1.0f);
	CHECK(circle.segments.size() == 4 && contours_closed(circle));
	CHECK(std::ranges::all_of(circle.segments, &OutlineSegment::is_curve));
	if (circle.segments.size() == 4)
	{
		// Empieza en el punto medio del último y el primero: (0, 50) -> (0, -50) con +Y abajo.
		CHECK(near(circle.segments[0].from, { 0, -50 }));
		CHECK(near(circle.segments[0].control, { 0, 0 }) && near(circle.segments[0].to, { 50, 0 }));
	}

	// on, off, on, on: una cuadrática y dos rectas (la última cierra).
	const detail::GlyphShape mixed{
		.points = { { 0, 0, true }, { 50, 100, false }, { 100, 0, true }, { 50, -50, true } },
		.contour_ends = { 3 }
	};
	const Outline m = detail::build_outline(mixed, 1.0f);
	CHECK(m.segments.size() == 3 && contours_closed(m));
	CHECK(m.segments.size() == 3 && m.segments[0].is_curve && !m.segments[1].is_curve && !m.segments[2].is_curve);
	CHECK(near(m.min, { 0, -100 }) && near(m.max, { 100, 50 }));   // la caja incluye el control
}

namespace
{
	// La suma acumulada de las primeras "count" casillas: la cobertura de cada píxel.
	std::vector<float> running_sum(const std::vector<float>& row, std::size_t count)
	{
		std::vector<float> out(count);
		float sum = 0.0f;
		for (std::size_t i = 0; i < count; ++i) {
			sum += row[i];
			out[i] = sum;
		}
		return out;
	}

	// La misma cuenta a lo bruto: el trozo partido en muchas rayitas verticales, y cada
	// una tapa de cada píxel lo que le queda a su derecha.
	std::vector<float> brute_force(std::size_t count, float x_top, float x_bottom, float height)
	{
		constexpr int k_slices = 2000;
		std::vector<float> out(count, 0.0f);
		for (int k = 0; k < k_slices; ++k) {
			const float x = x_top + (x_bottom - x_top) * (static_cast<float>(k) + 0.5f) / k_slices;
			for (std::size_t i = 0; i < count; ++i) {
				out[i] += height / k_slices * std::clamp(static_cast<float>(i) + 1.0f - x, 0.0f, 1.0f);
			}
		}
		return out;
	}

	bool all_near(const std::vector<float>& a, const std::vector<float>& b, float tolerance)
	{
		if (a.size() != b.size()) {
			return false;
		}
		for (std::size_t i = 0; i < a.size(); ++i) {
			if (std::abs(a[i] - b[i]) > tolerance) {
				return false;
			}
		}
		return true;
	}

	// Llama a accumulate_piece sobre una fila nueva de "width" píxeles (+2 casillas)
	// y devuelve la cobertura de cada píxel.
	std::vector<float> piece(std::size_t width, float x_top, float x_bottom, float height,
		float* total = nullptr)
	{
		std::vector<float> row(width + 2, 0.0f);
		detail::accumulate_piece(row, x_top, x_bottom, height);
		if (total) {
			*total = 0.0f;
			for (const float v : row) {
				*total += v;
			}
		}
		return running_sum(row, width);
	}

	// Un polígono cerrado de rectas.
	Outline polygon(std::initializer_list<std::initializer_list<Point>> contours)
	{
		Outline out;
		out.min = { 1e30f, 1e30f };
		out.max = { -1e30f, -1e30f };
		for (const auto& contour : contours) {
			const std::vector<Point> p(contour);
			for (std::size_t i = 0; i < p.size(); ++i) {
				out.segments.push_back({ p[i], {}, p[(i + 1) % p.size()], false });
				out.min = { std::min(out.min.x, p[i].x), std::min(out.min.y, p[i].y) };
				out.max = { std::max(out.max.x, p[i].x), std::max(out.max.y, p[i].y) };
			}
			out.contour_ends.push_back(static_cast<uint32_t>(out.segments.size()));
		}
		return out;
	}

	float coverage_sum(const GlyphBitmap& b)
	{
		float sum = 0.0f;
		for (const uint8_t c : b.coverage) {
			sum += c / 255.0f;
		}
		return sum;
	}

	// El área exacta de un contorno: la fórmula del cordón de zapato (shoelace) por cada
	// segmento, y en las curvas, además, 2/3 del triángulo que forman con su control.
	float outline_area(const Outline& o)
	{
		float area = 0.0f;
		for (const OutlineSegment& s : o.segments) {
			area += 0.5f * (s.from.x * s.to.y - s.to.x * s.from.y);
			if (s.is_curve) {
				const float triangle = 0.5f * ((s.control.x - s.from.x) * (s.to.y - s.from.y)
					- (s.to.x - s.from.x) * (s.control.y - s.from.y));
				area += 2.0f / 3.0f * triangle;
			}
		}
		return std::abs(area);
	}

	void print_bitmap(const GlyphBitmap& b)
	{
		const char* ramp = " .:-=+*#%@";
		for (uint32_t y = 0; y < b.height; ++y) {
			std::cout << "    ";
			for (uint32_t x = 0; x < b.width; ++x) {
				std::cout << ramp[b.at(x, y) * 9 / 255];
			}
			std::cout << "\n";
		}
	}
}

// 13. Tu parte, por separado: accumulate_piece.
static void test_accumulate_piece()
{
	std::cout << "[13] accumulate_piece\n";
	using V = std::vector<float>;

	// Vertical, justo en la frontera entre dos píxeles: tapa enteros los de la derecha.
	CHECK(all_near(piece(4, 2.0f, 2.0f, 1.0f), V{ 0, 0, 1, 1 }, 1e-5f));
	// Vertical por la mitad del píxel 1: lo tapa a medias.
	CHECK(all_near(piece(4, 1.5f, 1.5f, 1.0f), V{ 0, 0.5f, 1, 1 }, 1e-5f));
	// Solo cruza media fila: todo vale la mitad.
	CHECK(all_near(piece(4, 1.5f, 1.5f, 0.5f), V{ 0, 0.25f, 0.5f, 0.5f }, 1e-5f));
	// Si la recta sube, el signo cambia.
	CHECK(all_near(piece(4, 1.5f, 1.5f, -1.0f), V{ 0, -0.5f, -1, -1 }, 1e-5f));
	// Inclinada pero dentro de un solo píxel (de x = 1,2 a 1,6): a su derecha, en el
	// píxel 1, queda un trapecio de 2 - 1,4 = 0,6 (1,4 es la x de su punto medio).
	CHECK(all_near(piece(4, 1.2f, 1.6f, 1.0f), V{ 0, 0.6f, 1, 1 }, 1e-5f));
	// Inclinada de x = 0 a x = 2, cruzando la fila entera: del píxel 0 queda a la
	// derecha del trozo un triángulo (1/4), y del 1, todo menos otro triángulo (3/4).
	CHECK(all_near(piece(4, 0.0f, 2.0f, 1.0f), V{ 0.25f, 0.75f, 1, 1 }, 1e-5f));
	// Da igual por dónde entre: de arriba a la izquierda o a la derecha, el área es la misma.
	CHECK(all_near(piece(6, 0.5f, 4.25f, 1.0f), piece(6, 4.25f, 0.5f, 1.0f), 1e-5f));
	// En el borde derecho de la fila (x = ancho): usa la casilla de más y no se sale.
	float total = 0.0f;
	CHECK(all_near(piece(4, 4.0f, 4.0f, 1.0f, &total), V{ 0, 0, 0, 0 }, 1e-5f) && std::abs(total - 1.0f) < 1e-5f);
	CHECK(all_near(piece(4, 0.0f, 0.0f, 1.0f), V{ 1, 1, 1, 1 }, 1e-5f));

	// Muchos trozos al azar contra la cuenta a lo bruto. Y lo que se deja en la fila
	// suma siempre "height" (lo que entra por la izquierda acaba saliendo).
	std::mt19937 random(1234);
	std::uniform_real_distribution<float> any_x(0.0f, 8.0f);
	std::uniform_real_distribution<float> any_height(-1.0f, 1.0f);
	int wrong = 0;
	int bad_total = 0;
	for (int k = 0; k < 500; ++k) {
		const float a = any_x(random);
		const float b = k % 5 == 0 ? a : any_x(random);   // algunos verticales
		const float h = any_height(random);
		if (!all_near(piece(8, a, b, h, &total), brute_force(8, a, b, h), 2e-3f)) {
			++wrong;
		}
		if (std::abs(total - h) > 1e-4f) {
			++bad_total;
		}
	}
	CHECK(wrong == 0);
	CHECK(bad_total == 0);
	if (wrong != 0 || bad_total != 0) {
		std::cout << "  (" << wrong << " de 500 distintos de la cuenta a lo bruto, " << bad_total
				  << " que no suman height)\n";
	}
}

// 14. Rasterizar formas conocidas.
static void test_rasterize_shapes()
{
	std::cout << "[14] rasterizar formas conocidas\n";

	// Un cuadrado de 2x2 alineado con los píxeles: 4 píxeles llenos.
	const GlyphBitmap square = rasterize(polygon({ { { 1, 1 }, { 3, 1 }, { 3, 3 }, { 1, 3 } } }));
	CHECK(square.left == 1 && square.top == 1 && square.width == 2 && square.height == 2);
	CHECK(square.coverage == (std::vector<uint8_t>{ 255, 255, 255, 255 }));

	// Desplazado medio píxel: los bordes quedan a medias (128).
	const GlyphBitmap half = rasterize(polygon({ { { 0.5f, 0 }, { 2.5f, 0 }, { 2.5f, 1 }, { 0.5f, 1 } } }));
	CHECK(half.width == 3 && half.height == 1);
	CHECK(half.coverage == (std::vector<uint8_t>{ 128, 255, 128 }));
	// El mismo, recorrido al revés: da igual el sentido.
	const GlyphBitmap reversed = rasterize(polygon({ { { 0.5f, 1 }, { 2.5f, 1 }, { 2.5f, 0 }, { 0.5f, 0 } } }));
	CHECK(reversed.coverage == half.coverage);

	// Un triángulo rectángulo de catetos 4: área 8, y la diagonal parte en dos mitades
	// los píxeles que cruza.
	const GlyphBitmap triangle = rasterize(polygon({ { { 0, 0 }, { 4, 0 }, { 0, 4 } } }));
	CHECK(std::abs(coverage_sum(triangle) - 8.0f) < 0.05f);
	CHECK(triangle.at(0, 0) == 255 && triangle.at(3, 0) == 128 && triangle.at(0, 3) == 128 && triangle.at(3, 3) == 0);

	// Dos cuadrados del mismo sentido que se pisan: non-zero, la zona común no "se
	// pasa" de 255 ni se vacía. Área de la unión: 4 + 4 - 1 = 7.
	const GlyphBitmap overlap = rasterize(polygon({ { { 0, 0 }, { 2, 0 }, { 2, 2 }, { 0, 2 } },
												  { { 1, 1 }, { 3, 1 }, { 3, 3 }, { 1, 3 } } }));
	CHECK(overlap.at(1, 1) == 255 && overlap.at(2, 0) == 0);
	CHECK(std::abs(coverage_sum(overlap) - 7.0f) < 0.01f);

	// Un marco: el cuadrado de dentro va al revés, así que es un agujero (como la "o").
	const GlyphBitmap frame = rasterize(polygon({ { { 0, 0 }, { 4, 0 }, { 4, 4 }, { 0, 4 } },
												{ { 1, 1 }, { 1, 3 }, { 3, 3 }, { 3, 1 } } }));
	CHECK(frame.at(0, 0) == 255 && frame.at(1, 1) == 0 && frame.at(2, 2) == 0 && frame.at(3, 3) == 255);

	CHECK(rasterize(Outline{}).empty());
}

// 15. Letras de verdad.
static void test_rasterize_glyphs()
{
	std::cout << "[15] rasterizar Segoe UI negrita\n";
	const auto data = load_system_font("segoeuib.ttf");
	if (!data) {
		return;
	}
	const Font font(*data);

	// La "l" a 48 px va de x = 3,117 a 10,523 y de y = -35,53 a 0.
	const GlyphBitmap l = rasterize(font.outline(font.glyph_index(U'l'), 48.0f));
	CHECK(l.left == 3 && l.top == -36 && l.width == 8 && l.height == 36);
	if (l.width == 8 && l.height == 36) {
		CHECK(std::abs(l.at(0, 20) - 225) <= 1);   // el píxel x = 3, tapado al 88 %
		CHECK(l.at(3, 20) == 255 && l.at(3, 35) == 255);
		CHECK(std::abs(l.at(7, 20) - 133) <= 1);   // el píxel x = 10, al 52 %
	}

	// La "o" tiene un agujero en medio.
	const GlyphBitmap o = rasterize(font.outline(font.glyph_index(U'o'), 48.0f));
	CHECK(!o.empty() && o.at(o.width / 2, o.height / 2) == 0);

	// Lo que suma la cobertura es el área de la letra (con las curvas exactas).
	for (const char32_t c : std::u32string(U"loBgñae8")) {
		const Outline outline = font.outline(font.glyph_index(c), 48.0f);
		const float expected = outline_area(outline);
		const float got = coverage_sum(rasterize(outline));
		if (std::abs(got - expected) > 0.005f * expected) {
			CHECK(std::abs(got - expected) <= 0.005f * expected);
			std::cout << "  (U+" << std::hex << static_cast<uint32_t>(c) << std::dec << ": área " << expected
					  << ", cobertura " << got << ")\n";
		}
	}

	// Todos los glifos a 24 px: ninguno se rompe y cada imagen mide lo que su caja.
	int broken = 0;
	const auto start = std::chrono::steady_clock::now();
	for (uint32_t g = 0; g < font.glyph_count(); ++g) {
		try {
			const GlyphBitmap b = rasterize(font.outline(static_cast<uint16_t>(g), 24.0f));
			if (b.coverage.size() != static_cast<std::size_t>(b.width) * b.height) {
				++broken;
			}
		}
		catch (const std::exception&) {
			++broken;
		}
	}
	const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	CHECK(broken == 0);
	std::cout << "  (" << font.glyph_count() << " glifos a 24 px en " << ms << " ms)\n";

	// Y para verla: la "g" a 20 px.
	print_bitmap(rasterize(font.outline(font.glyph_index(U'g'), 20.0f)));
}

// Cada caso por separado: si uno lanza una excepción que no esperaba, cuenta como
// fallo y siguen los demás (sin esto, el programa se cae con abort()).
static void run(void (*test)())
{
	try {
		test();
	}
	catch (const std::exception& e) {
		++g_failures;
		std::cout << "  FALLO: excepcion inesperada: " << e.what() << "\n";
	}
}

int main()
{
	std::cout << std::unitbuf;   // sin búfer: si algo se cae, se ve hasta dónde llegó
	run(test_reader);
	run(test_segoe);
	run(test_short_loca);
	run(test_hmtx_tail);
	run(test_broken);
	run(test_cmap_format4);
	run(test_cmap_formats_agree);
	run(test_cmap_broken);
	run(test_real_outlines);
	run(test_components);
	run(test_handmade_glyf);
	run(test_implicit_points);
	run(test_accumulate_piece);
	run(test_rasterize_shapes);
	run(test_rasterize_glyphs);

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}
