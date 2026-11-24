// Pruebas de la lógica de CPU del lote del Renderer2D (hito R2d), sin GPU:
//   - stable_order_by_layer: orden por capa, estable, y sin trabajo si ya
//     venían en orden.
//   - draw_count / draw_range: troceo de un lote grande en draws.
//   - ui_space.h: proyección en píxeles y sitio de cada letra (hito R4a).
//   - style_table.h: tabla de estilos (hito R4b).
//
// La parte con GPU (que los draws salen bien y sin avisos de validación) está
// en test_deferred (caso 25) y se ve en test_1.

#include "batch_order.h"
#include "style_table.h"
#include "ui_space.h"
#include "utf8.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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
}

#define CHECK(cond) check((cond), #cond, __LINE__)

using luxel::detail::stable_order_by_layer;
using luxel::detail::draw_count;
using luxel::detail::draw_range;

// 1. Ya en orden (todo en la misma capa, o crecientes): no hay que reordenar
//    y "order" no se toca.
static void test_already_sorted()
{
	std::cout << "[1] capas ya en orden: no se reordena\n";
	std::vector<uint32_t> order{ 99 };   // centinela: no debe cambiar

	const std::vector<int32_t> same{ 0, 0, 0, 0 };
	CHECK(!stable_order_by_layer(same, order));

	const std::vector<int32_t> growing{ -3, -1, -1, 0, 5 };
	CHECK(!stable_order_by_layer(growing, order));

	const std::vector<int32_t> empty{};
	CHECK(!stable_order_by_layer(empty, order));

	CHECK(order.size() == 1 && order[0] == 99);
}

// 2. El caso de la escena de Iria: se envía el personaje antes que el fondo.
//    El fondo (capa -1) debe dibujarse primero.
static void test_background_sent_late()
{
	std::cout << "[2] fondo enviado despues del personaje\n";
	const std::vector<int32_t> layers{ 0, -1 };   // envío: Iria, fondo
	std::vector<uint32_t> order;

	CHECK(stable_order_by_layer(layers, order));
	CHECK((order == std::vector<uint32_t>{ 1, 0 }));   // fondo, Iria
}

// 3. Estabilidad: a igual capa se respeta el orden de envío.
static void test_stable()
{
	std::cout << "[3] estable: a igual capa, orden de envio\n";
	//                      envío:  0   1  2   3  4  5
	const std::vector<int32_t> layers{ 2, 0, 1, 0, 2, 1 };
	std::vector<uint32_t> order;

	CHECK(stable_order_by_layer(layers, order));
	// capa 0: envíos 1, 3 | capa 1: envíos 2, 5 | capa 2: envíos 0, 4
	CHECK((order == std::vector<uint32_t>{ 1, 3, 2, 5, 0, 4 }));
}

// 4. Capas negativas, extremos de int32 y un vector reutilizado con basura de
//    una llamada anterior (como hace el Renderer2D entre frames).
static void test_extremes_and_reuse()
{
	std::cout << "[4] extremos de int32 y vector reutilizado\n";
	std::vector<uint32_t> order{ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };   // más largo que el lote

	const std::vector<int32_t> layers{ INT32_MAX, 0, INT32_MIN, -1 };
	CHECK(stable_order_by_layer(layers, order));
	CHECK((order == std::vector<uint32_t>{ 2, 3, 1, 0 }));
}

// 5. Muchos quads: el resultado es una permutación ordenada por capa y estable.
static void test_many()
{
	std::cout << "[5] 40.000 quads en 7 capas\n";
	constexpr uint32_t n = 40000;
	std::vector<int32_t> layers(n);
	for (uint32_t i = 0; i < n; ++i) {
		layers[i] = static_cast<int32_t>((i * 37) % 7) - 3;   // capas de -3 a 3, mezcladas
	}

	std::vector<uint32_t> order;
	CHECK(stable_order_by_layer(layers, order));
	CHECK(order.size() == n);

	bool sorted_and_stable = true;
	for (uint32_t i = 1; i < n; ++i)
	{
		const int32_t prev = layers[order[i - 1]];
		const int32_t curr = layers[order[i]];
		// Capa creciente; a igual capa, índice de envío creciente (estable).
		if (prev > curr || (prev == curr && order[i - 1] > order[i])) {
			sorted_and_stable = false;
			break;
		}
	}
	CHECK(sorted_and_stable);
}

// 6. Troceo en draws.
static void test_draw_split()
{
	std::cout << "[6] troceo en draws\n";
	constexpr uint32_t max = 16384;   // lo que usa el Renderer2D (1 MB / 64 bytes)

	CHECK(draw_count(0, max) == 0);
	CHECK(draw_count(1, max) == 1);
	CHECK(draw_count(max, max) == 1);       // justo lleno: un draw
	CHECK(draw_count(max + 1, max) == 2);   // uno más: dos
	CHECK(draw_count(40000, max) == 3);     // 16384 + 16384 + 7232
	CHECK(draw_count(10, 0) == 0);          // max 0: no divide por cero

	const auto r0 = draw_range(40000, max, 0);
	const auto r1 = draw_range(40000, max, 1);
	const auto r2 = draw_range(40000, max, 2);
	const auto r3 = draw_range(40000, max, 3);   // fuera: tramo vacío
	CHECK(r0.first == 0     && r0.count == max);
	CHECK(r1.first == max   && r1.count == max);
	CHECK(r2.first == 2 * max && r2.count == 40000 - 2 * max);
	CHECK(r3.count == 0);

	// Los tramos cubren el lote entero, sin huecos ni solapes.
	uint32_t covered = 0;
	for (uint32_t d = 0; d < draw_count(40000, max); ++d) {
		const auto r = draw_range(40000, max, d);
		CHECK(r.first == covered);
		covered += r.count;
	}
	CHECK(covered == 40000);
}

// 7. Decodificador UTF-8 de draw_text (R3).
static std::u32string decode(std::u8string_view text)
{
	std::u32string out;
	for (std::size_t i = 0; i < text.size();) {
		out.push_back(luxel::detail::next_codepoint(text, i));
	}
	return out;
}

static void test_utf8()
{
	std::cout << "[7] UTF-8: ASCII, 2, 3 y 4 bytes, y secuencias rotas\n";
	using luxel::detail::k_replacement_char;

	CHECK(decode(u8"Iria") == U"Iria");
	CHECK(decode(u8"¡Hola, Iria!") == U"¡Hola, Iria!");     // ¡ = U+00A1 (2 bytes)
	CHECK(decode(u8"ñÑüç") == U"ñÑüç");
	CHECK(decode(u8"€") == U"€");                            // U+20AC (3 bytes)
	CHECK(decode(u8"\U0001F642") == U"\U0001F642");          // emoji (4 bytes)

	// Secuencias mal formadas: U+FFFD y se sigue con lo de detrás.
	const char8_t lone_continuation[] = { 0x80, u8'a', 0 };          // 10xxxxxx suelto
	const char8_t cut_at_end[]        = { u8'a', 0xC3, 0 };          // empieza 'ñ' pero se corta
	const char8_t overlong[]          = { 0xC0, 0x80, u8'b', 0 };    // '\0' en 2 bytes (prohibido)
	const char8_t bad_continuation[]  = { 0xE2, u8'x', u8'y', 0 };   // 3 bytes, el 2º no es 10xxxxxx
	CHECK(decode(lone_continuation) == (std::u32string{ k_replacement_char, U'a' }));
	CHECK(decode(cut_at_end) == (std::u32string{ U'a', k_replacement_char }));
	CHECK(decode(overlong) == (std::u32string{ k_replacement_char, k_replacement_char, U'b' }));
	CHECK(decode(bad_continuation) == (std::u32string{ k_replacement_char, U'x', U'y' }));
}

// 8. Proyección de la UI (R4a): (0,0) arriba a la izquierda y +Y abajo. En el
//    NDC de luxel (+Y arriba), arriba a la izquierda es (-1, +1).
static void test_ui_projection()
{
	std::cout << "[8] UI: proyeccion en pixeles\n";
	const auto m = luxel::detail::ui_projection(1280.0f, 720.0f);
	const auto ndc = [&](float x, float y) { return m.TransformPoint({ x, y }); };

	CHECK(ndc(0.0f, 0.0f).nearlyEquals({ -1.0f, 1.0f }));        // arriba izquierda
	CHECK(ndc(1280.0f, 0.0f).nearlyEquals({ 1.0f, 1.0f }));      // arriba derecha
	CHECK(ndc(0.0f, 720.0f).nearlyEquals({ -1.0f, -1.0f }));     // abajo izquierda
	CHECK(ndc(1280.0f, 720.0f).nearlyEquals({ 1.0f, -1.0f }));   // abajo derecha
	CHECK(ndc(640.0f, 360.0f).nearlyEquals({ 0.0f, 0.0f }));     // centro

	// Un píxel a la derecha son 2/1280 de NDC; uno hacia abajo, -2/720.
	CHECK(ndc(1.0f, 1.0f).nearlyEquals({ -1.0f + 2.0f / 1280.0f, 1.0f - 2.0f / 720.0f }));
}

// 9. Sitio de cada letra (R4a): misma fuente (celda de 20x30 con 2 de margen) y
//    lápiz en (100, 100), en los dos espacios. En X igual; en Y, la celda empieza
//    2 por ENCIMA del lápiz y su centro está 13 por DEBAJO de él.
static void test_glyph_center()
{
	std::cout << "[9] UI: centro de una letra con +Y arriba y +Y abajo\n";
	using luxel::detail::glyph_center;

	const auto up = glyph_center({ 100.0f, 100.0f }, 2.0f, 20.0f, 30.0f, luxel::detail::k_y_up);
	CHECK(up.nearlyEquals({ 108.0f, 87.0f }));      // abajo = restar

	const auto down = glyph_center({ 100.0f, 100.0f }, 2.0f, 20.0f, 30.0f, luxel::detail::k_y_down);
	CHECK(down.nearlyEquals({ 108.0f, 113.0f }));   // abajo = sumar
}

// 10. Tabla de estilos (R4b): el 0 es "sin estilo" y los números empiezan en 1;
//     si el estilo es igual que el último, se reutiliza.
static void test_add_style()
{
	std::cout << "[10] estilos: anadir y reutilizar el ultimo\n";
	std::vector<int> styles;   // un "estilo" puede ser cualquier cosa con ==

	CHECK(luxel::detail::add_style(styles, 8) == 1);
	CHECK(luxel::detail::add_style(styles, 8) == 1);    // igual que el último: el mismo
	CHECK(luxel::detail::add_style(styles, 16) == 2);
	CHECK(luxel::detail::add_style(styles, 8) == 3);    // solo se mira el último
	CHECK((styles == std::vector<int>{ 8, 16, 8 }));
}

// 11. compact_styles (R4b): cada draw lleva solo los estilos que usan sus quads,
//     renumerados. Escenario del comentario de style_table.h: quads con los
//     estilos 7, 0, 9, 7 -> tabla { 7, 9 } y quads 1, 0, 2, 1.
static void test_compact_styles()
{
	std::cout << "[11] estilos: tabla de cada draw\n";
	struct FakeQuad { uint32_t style_index; };

	std::vector<int> styles;
	for (int i = 1; i <= 10; ++i) { styles.push_back(i * 100); }   // el estilo n vale n * 100

	std::vector<FakeQuad> quads{ { 7 }, { 0 }, { 9 }, { 7 }, /* otro draw: */ { 9 }, { 10 } };
	std::vector<uint32_t> remap{ 5, 5, 5 };   // basura de una llamada anterior
	std::vector<int> table{ 1, 2, 3 };

	luxel::detail::compact_styles(std::span(quads).subspan(0, 4), styles, remap, table);
	CHECK((table == std::vector<int>{ 700, 900 }));
	CHECK(quads[0].style_index == 1 && quads[1].style_index == 0 && quads[2].style_index == 2 && quads[3].style_index == 1);
	CHECK(quads[4].style_index == 9 && quads[5].style_index == 10);   // los del otro draw, intactos

	// El segundo draw empieza de cero: no se acuerda de la numeración del primero.
	luxel::detail::compact_styles(std::span(quads).subspan(4, 2), styles, remap, table);
	CHECK((table == std::vector<int>{ 900, 1000 }));
	CHECK(quads[4].style_index == 1 && quads[5].style_index == 2);

	// Un draw sin ningún estilo: tabla vacía.
	std::vector<FakeQuad> plain{ { 0 }, { 0 } };
	luxel::detail::compact_styles(std::span(plain), styles, remap, table);
	CHECK(table.empty());
}

int main()
{
	test_already_sorted();
	test_background_sent_late();
	test_stable();
	test_extremes_and_reuse();
	test_many();
	test_draw_split();
	test_utf8();
	test_ui_projection();
	test_glyph_center();
	test_add_style();
	test_compact_styles();

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}
