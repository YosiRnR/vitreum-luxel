// Pruebas de luxel_ttf (hitos T1 y T2 de docs/ttf-design.md), sin GPU. Caja blanca:
// ve luxel_ttf/src (reader.h y cmap.h). Usa las fuentes de Windows (no se versionan: tienen
// licencia); si falta alguna, el caso lo dice y se salta.

#include "cmap.h"
#include "reader.h"
#include <luxel/ttf/font.h>
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
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

int main()
{
	std::cout << std::unitbuf;   // sin búfer: si algo se cae, se ve hasta dónde llegó
	test_reader();
	test_segoe();
	test_short_loca();
	test_hmtx_tail();
	test_broken();
	test_cmap_format4();
	test_cmap_formats_agree();
	test_cmap_broken();

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}
