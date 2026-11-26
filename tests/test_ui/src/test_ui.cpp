// Pruebas de luxel_ui (hitos U1 a U4), sin ventana ni GPU: se inventan los eventos del
// ratón y se mira qué widget lo tiene encima. Casi todo con la API pública
// (el hit test se prueba a través de Ui); de luxel_ui/src solo ve distribute.h y scroll_math.h.

#include "distribute.h"
#include "scroll_math.h"
#include <luxel/ui/scroll.h>
#include <luxel/ui/ui.h>
#include <luxel/ui/widgets.h>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace luxel::ui;

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

	// Un widget sin dibujo que cuenta cuántas veces entra y sale el ratón.
	class Box : public Widget
	{
	public:
		explicit Box(const Rect& rect) { set_rect(rect); }
		int enters{ 0 };
		int leaves{ 0 };

	protected:
		void on_mouse_enter() override { ++enters; }
		void on_mouse_leave() override { ++leaves; }
	};

	// Un widget que apunta en un diario lo que le pasa, y acepta la pulsación o no.
	class Recorder : public Widget
	{
	public:
		Recorder(const Rect& rect, char name, bool accepts, std::string& log)
			: name(name), accepts(accepts), log(log) { set_rect(rect); }
		char name;
		bool accepts;
		std::string& log;

	protected:
		bool on_mouse_down(const Event&) override { log += name; log += "d "; return accepts; }
		void on_mouse_up(const Event&) override   { log += name; log += "u "; }
		void on_mouse_cancel() override           { log += name; log += "c "; }
	};

	bool down(Ui& ui, float x, float y, MouseButton button = MouseButton::Left)
	{
		return ui.on_event({ .type = EventType::MouseDown, .position = { x, y }, .button = button });
	}
	bool up(Ui& ui, float x, float y, MouseButton button = MouseButton::Left)
	{
		return ui.on_event({ .type = EventType::MouseUp, .position = { x, y }, .button = button });
	}

	// Un sitio donde colocar widgets a mano con set_rect. Hace falta porque la raíz es
	// un Stack (colocaría ella a sus hijos); un Widget normal los deja donde estén.
	Widget& free_area(Ui& ui)
	{
		auto& area = ui.root().add<Widget>();
		area.set_ignores_mouse(true);
		return area;
	}

	// Un widget que pide un tamaño fijo, para probar el layout sin fuente.
	class Fixed : public Widget
	{
	public:
		Fixed(float width, float height, float grow = 0.0f) { set_min_size({ width, height }); set_grow(grow); }
	};

	bool wheel(Ui& ui, float notches)
	{
		return ui.on_event({ .type = EventType::MouseWheel, .wheel = notches });
	}

	bool near(float a, float b) { return std::abs(a - b) < 0.01f; }

	// Mueve el ratón y devuelve lo que queda debajo.
	Widget* at(Ui& ui, float x, float y)
	{
		ui.on_event({ .type = EventType::MouseMove, .position = { x, y } });
		return ui.hovered();
	}
}

#define CHECK(cond) check((cond), #cond, __LINE__)

// 1. Rect: los cantos de la derecha y de abajo no cuentan.
static void test_rect()
{
	std::cout << "[1] Rect::contains\n";
	const Rect left{ 0, 0, 100, 40 }, right{ 100, 0, 100, 40 };
	CHECK(left.contains({ 0, 0 }));
	CHECK(!left.contains({ 100, 0 }));    // el canto compartido...
	CHECK(right.contains({ 100, 0 }));    // ...es solo de uno
	CHECK(!left.contains({ 50, 40 }));
	CHECK(!left.contains({ -1, 10 }));
}

// 2. Árbol: padres, orden de los hijos y destrucción en cascada.
static void test_tree()
{
	std::cout << "[2] arbol\n";
	struct Counter : Widget {
		explicit Counter(int& alive) : alive(alive) { ++alive; }
		~Counter() override { --alive; }
		int& alive;
	};

	int alive = 0;
	{
		Ui ui;
		auto& parent = ui.root().add<Counter>(alive);
		auto& first  = parent.add<Counter>(alive);
		auto& second = parent.add<Counter>(alive);
		CHECK(alive == 3);
		CHECK(first.parent() == &parent);
		CHECK(parent.parent() == &ui.root());
		CHECK(ui.root().parent() == nullptr);
		CHECK(parent.children().size() == 2);
		CHECK(parent.children()[1].get() == &second);
	}
	CHECK(alive == 0);   // al morir la Ui mueren todos
}

// 3. El escenario del comentario de hit_test.cpp.
static void test_hit_test()
{
	std::cout << "[3] hit test: hijos, solapes y textos\n";
	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	auto& panel = area.add<Box>(Rect{ 0, 0, 200, 200 });
	auto& a     = panel.add<Box>(Rect{ 10, 10, 100, 40 });
	auto& b     = panel.add<Box>(Rect{ 80, 10, 100, 40 });
	auto& label = panel.add<Label>(u8"hola");
	label.set_rect({ 10, 150, 100, 20 });

	CHECK(at(ui, 20, 20) == &a);
	CHECK(at(ui, 90, 20) == &b);          // se pisan: gana el que se pinta encima
	CHECK(at(ui, 20, 155) == &panel);     // el texto lo deja pasar
	CHECK(at(ui, 190, 190) == &panel);
	CHECK(at(ui, 300, 300) == nullptr);   // la raíz no cuenta
}

// 4. Invisibles, y padres que ignoran el ratón.
static void test_visibility()
{
	std::cout << "[4] hit test: invisibles y contenedores que ignoran el raton\n";
	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	auto& below     = area.add<Box>(Rect{ 0, 0, 100, 100 });
	auto& container = area.add<Box>(Rect{ 0, 0, 100, 100 });
	auto& child     = container.add<Box>(Rect{ 10, 10, 20, 20 });

	CHECK(at(ui, 15, 15) == &child);
	CHECK(at(ui, 60, 60) == &container);

	container.set_ignores_mouse(true);    // él no, pero su hijo sí
	CHECK(at(ui, 15, 15) == &child);
	CHECK(at(ui, 60, 60) == &below);

	container.set_ignores_mouse(false);
	container.set_visible(false);         // ni él ni su hijo
	CHECK(at(ui, 15, 15) == &below);
	CHECK(at(ui, 60, 60) == &below);
}

// 5. Hijos que se salen de su padre, con y sin recorte.
static void test_clipping()
{
	std::cout << "[5] hit test: hijos que se salen\n";
	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	auto& list = area.add<Box>(Rect{ 0, 0, 100, 100 });
	auto& row  = list.add<Box>(Rect{ 0, 80, 100, 40 });   // asoma 20 px por debajo

	CHECK(at(ui, 50, 90) == &row);
	CHECK(at(ui, 50, 110) == &row);       // sin recorte, se ve y recibe el ratón

	list.set_clips_children(true);
	CHECK(at(ui, 50, 90) == &row);
	CHECK(at(ui, 50, 110) == nullptr);    // con recorte, esa parte no existe
}

// 6. Entrar y salir: una vez cada uno, aunque el ratón se mueva por dentro.
static void test_enter_leave()
{
	std::cout << "[6] entrar y salir\n";
	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	auto& panel  = area.add<Box>(Rect{ 0, 0, 200, 200 });
	auto& button = panel.add<Box>(Rect{ 50, 50, 50, 50 });

	CHECK(ui.on_event({ .type = EventType::MouseMove, .position = { 10, 10 } }));   // sobre la UI
	at(ui, 20, 20);
	at(ui, 30, 30);
	CHECK(panel.hovered() && panel.enters == 1 && panel.leaves == 0);

	at(ui, 60, 60);   // del panel al botón: solo uno a la vez
	CHECK(!panel.hovered() && panel.leaves == 1);
	CHECK(button.hovered() && button.enters == 1);

	// Sale de la ventana: nadie lo tiene.
	CHECK(!ui.on_event({ .type = EventType::MouseLeave }));
	CHECK(ui.hovered() == nullptr);
	CHECK(!button.hovered() && button.leaves == 1);
}

// 7. El ratón quieto y el widget que se mueve: update() se da cuenta.
static void test_widget_moves()
{
	std::cout << "[7] el widget se mueve bajo el raton\n";
	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	auto& box = area.add<Box>(Rect{ 0, 0, 100, 100 });
	CHECK(at(ui, 50, 50) == &box);

	box.set_rect({ 300, 300, 100, 100 });
	ui.update(800, 600);
	CHECK(ui.hovered() == nullptr && box.leaves == 1);

	box.set_rect({ 0, 0, 100, 100 });
	ui.update(800, 600);
	CHECK(ui.hovered() == &box && box.enters == 2);
}

// 8. Subida: del más profundo hacia arriba, hasta que uno lo acepta.
static void test_bubbling()
{
	std::cout << "[8] subida y captura\n";
	std::string log;
	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	auto& a = area.add<Recorder>(Rect{ 0, 0, 300, 300 }, 'a', true, log);
	auto& b = a.add<Recorder>(Rect{ 0, 0, 200, 200 }, 'b', true, log);
	auto& c = b.add<Recorder>(Rect{ 0, 0, 100, 100 }, 'c', false, log);

	CHECK(down(ui, 50, 50));
	CHECK(log == "cd bd ");              // c no lo quiere, b sí; a ni se entera
	CHECK(ui.captured() == &b);
	CHECK(b.hovered() && !c.hovered());  // durante la pulsación, solo el que capturó

	CHECK(at(ui, 250, 250) == nullptr);  // está sobre a, pero el ratón es de b
	CHECK(up(ui, 250, 250));             // se suelta fuera y aun así le llega a b
	CHECK(log == "cd bd bu ");
	CHECK(ui.captured() == nullptr);
	CHECK(ui.hovered() == &a);           // terminada la pulsación, vuelve lo normal

	// Nadie lo acepta: sigue siendo de la UI, pero no hay captura.
	log.clear();
	a.accepts = b.accepts = false;
	CHECK(down(ui, 50, 50));
	CHECK(log == "cd bd ad " && ui.captured() == nullptr);
	CHECK(!up(ui, 50, 50));
}

// 9. Dos botones del ratón a la vez, anulación y pulsaciones de fuera de la UI.
static void test_capture_rules()
{
	std::cout << "[9] reglas de la captura\n";
	std::string log;
	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	area.add<Recorder>(Rect{ 0, 0, 100, 100 }, 'a', true, log);

	// Con el izquierdo apretado, el derecho se ignora.
	down(ui, 50, 50);
	CHECK(down(ui, 50, 50, MouseButton::Right));
	CHECK(up(ui, 50, 50, MouseButton::Right));
	CHECK(log == "ad " && ui.captured() != nullptr);
	up(ui, 50, 50);
	CHECK(log == "ad au ");

	// Salir de la ventana NO anula; perder el foco (MouseCancel), sí.
	log.clear();
	down(ui, 50, 50);
	ui.on_event({ .type = EventType::MouseLeave });
	CHECK(ui.captured() != nullptr && ui.hovered() == nullptr);
	ui.on_event({ .type = EventType::MouseCancel });
	CHECK(ui.captured() == nullptr);
	CHECK(!up(ui, 50, 50));       // el que llega tarde ya no es de nadie
	CHECK(log == "ad ac ");

	// Apretar en el juego y soltar sobre la UI: los dos son del juego.
	log.clear();
	CHECK(!down(ui, 400, 400));
	CHECK(!up(ui, 50, 50));
	CHECK(log.empty());
}

// 10. El escenario de button.cpp: los cuatro estados y los clics.
static void test_button()
{
	std::cout << "[10] boton: estados y clics\n";
	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	auto& button = area.add<Button>(u8"Salir");
	button.set_rect({ 0, 0, 100, 40 });
	int clicks = 0;
	ButtonState during = ButtonState::Pressed;
	button.on_click = [&] { ++clicks; during = button.state(); };

	CHECK(button.state() == ButtonState::Normal);
	at(ui, 50, 20);
	CHECK(button.state() == ButtonState::Hovered);
	down(ui, 50, 20);
	CHECK(button.state() == ButtonState::Pressed && clicks == 0);   // aún no: hay que soltar
	at(ui, 300, 300);
	CHECK(button.state() == ButtonState::PressedOutside);
	at(ui, 50, 20);
	CHECK(button.state() == ButtonState::Pressed);
	up(ui, 50, 20);
	CHECK(button.state() == ButtonState::Hovered && clicks == 1);
	CHECK(during == ButtonState::Hovered);   // dentro de on_click ya no está apretado

	// Se arrepiente: suelta fuera.
	down(ui, 50, 20);
	up(ui, 300, 300);
	CHECK(button.state() == ButtonState::Normal && clicks == 1);

	// Aprieta fuera y suelta encima: tampoco.
	down(ui, 300, 300);
	up(ui, 50, 20);
	CHECK(button.state() == ButtonState::Hovered && clicks == 1);

	// Temblor de la mano: se mueve un poco, pero sigue encima.
	down(ui, 60, 20);
	up(ui, 70, 20);
	CHECK(clicks == 2);
}

// 11. Botón: lo que no es un clic, y lo que no debe romperse.
static void test_button_edges()
{
	std::cout << "[11] boton: derecho, anulado y sin on_click\n";
	std::string log;
	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	auto& panel  = area.add<Recorder>(Rect{ 0, 0, 200, 200 }, 'p', true, log);
	auto& button = panel.add<Button>(u8"X");
	auto& other  = panel.add<Button>(u8"Y");
	button.set_rect({ 0, 0, 100, 40 });
	other.set_rect({ 0, 50, 100, 40 });
	int clicks = 0, other_clicks = 0;
	button.on_click = [&] { ++clicks; };
	other.on_click  = [&] { ++other_clicks; };

	// El derecho no es para él: sube al panel.
	down(ui, 50, 20, MouseButton::Right);
	CHECK(log == "pd " && button.state() != ButtonState::Pressed);
	up(ui, 50, 20, MouseButton::Right);
	CHECK(clicks == 0);

	// El izquierdo sí lo es: el panel no se entera.
	log.clear();
	down(ui, 50, 20);
	CHECK(log.empty() && ui.captured() == &button);

	// Arrastrar hasta otro botón: no se ilumina, y soltar ahí no hace clic en ninguno.
	at(ui, 50, 70);
	CHECK(other.state() == ButtonState::Normal);
	up(ui, 50, 70);
	CHECK(clicks == 0 && other_clicks == 0);
	CHECK(other.state() == ButtonState::Hovered);   // ahora sí

	// Alt+Tab con el botón apretado.
	down(ui, 50, 20);
	ui.on_event({ .type = EventType::MouseCancel });
	CHECK(button.state() == ButtonState::Normal);
	up(ui, 50, 20);
	CHECK(clicks == 0 && button.state() == ButtonState::Hovered);

	// Sin on_click no pasa nada (una std::function vacía lanza si se la llama).
	// Pero sigue siendo un botón: se hunde, y el clic no lo atraviesa hasta el panel.
	button.on_click = nullptr;
	log.clear();
	down(ui, 50, 20);
	CHECK(button.state() == ButtonState::Pressed);
	CHECK(log.empty());
	up(ui, 50, 20);
	CHECK(button.state() == ButtonState::Hovered);
}

// 12. Quitar un widget desde su propio evento, y desde fuera.
static void test_remove()
{
	std::cout << "[12] borrado aplazado\n";
	static int seen = -1;   // cuántos vivos había DURANTE el evento
	struct Closer : Widget {
		Closer(Widget& target, int& alive) : target(target), alive(alive) { ++alive; }
		~Closer() override { --alive; }
		Widget& target;
		int& alive;
		bool on_mouse_down(const Event&) override { return true; }
		void on_mouse_up(const Event&) override
		{
			target.remove();
			target.remove();        // dos veces no es un problema
			seen = alive;   // seguimos vivos: "alive" se lee a través de nosotros mismos
		}
	};

	int alive = 0;
	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	auto& keep   = area.add<Box>(Rect{ 0, 0, 400, 400 });
	auto& dialog = area.add<Box>(Rect{ 0, 0, 200, 200 });
	auto& last   = area.add<Box>(Rect{ 500, 0, 10, 10 });
	dialog.add<Closer>(dialog, alive).set_rect({ 0, 0, 50, 50 });
	CHECK(alive == 1);

	down(ui, 20, 20);
	up(ui, 20, 20);
	CHECK(seen == 1);   // durante su función, vivo
	CHECK(alive == 0);            // al acabar el evento, no
	CHECK(ui.captured() == nullptr);
	CHECK(area.children().size() == 2);          // los demás siguen, y en orden
	CHECK(area.children()[0].get() == &keep);
	CHECK(area.children()[1].get() == &last);
	CHECK(at(ui, 20, 20) == &keep);

	// Desde fuera, con el ratón encima y APRETADO: se oculta ya, se destruye en update.
	auto& timed = keep.add<Closer>(keep, alive);
	timed.set_rect({ 0, 0, 50, 50 });
	down(ui, 20, 20);
	CHECK(ui.captured() == &timed && ui.hovered() == &timed);
	timed.remove();
	CHECK(alive == 1 && !timed.visible());
	ui.update(800, 600);
	CHECK(alive == 0);
	CHECK(ui.captured() == nullptr && ui.hovered() == &keep);
	CHECK(!up(ui, 20, 20));       // ya no hay nadie esperando que se suelte
	CHECK(keep.children().empty());
}

// 13. on_event dentro de on_event: solo el de más afuera destruye.
static void test_reentrancy()
{
	std::cout << "[13] eventos dentro de eventos\n";
	static int alive = 0;
	static int alive_inside = -1;
	struct Nested : Widget {
		explicit Nested(Ui& ui) : ui(ui) { ++alive; }
		~Nested() override { --alive; }
		Ui& ui;
		bool on_mouse_down(const Event&) override { return true; }
		void on_mouse_up(const Event&) override
		{
			remove();
			ui.on_event({ .type = EventType::MouseMove, .position = { 5, 5 } });
			ui.update(800, 600);
			alive_inside = alive;   // si alguno de los dos hubiera destruido, 0 (y memoria liberada)
		}
	};

	Ui ui;
	ui.update(800, 600);
	auto& area = free_area(ui);
	area.add<Nested>(ui).set_rect({ 0, 0, 50, 50 });
	down(ui, 20, 20);
	up(ui, 20, 20);
	CHECK(alive_inside == 1);
	CHECK(alive == 0);
	CHECK(area.children().empty());
}

// 14. El reparto de lo que sobra, a solas.
static void test_distribute()
{
	std::cout << "[14] distribute\n";
	using V = std::vector<float>;
	const auto run = [](std::vector<FlexItem> items, float available) { return distribute(items, available); };
	const auto sum = [](const V& sizes) { float total = 0.0f; for (float s : sizes) { total += s; } return total; };

	CHECK((run({ { 30, 0 }, { 0, 1 }, { 40, 0 } }, 264) == V{ 30, 194, 40 }));   // el de los docs
	CHECK((run({ { 0, 1 }, { 0, 3 } }, 200) == V{ 50, 150 }));
	CHECK((run({ { 50, 1 }, { 10, 1 } }, 100) == V{ 70, 30 }));      // se SUMA a lo que pedían
	CHECK((run({ { 0, 2 }, { 0, 6 } }, 200) == V{ 50, 150 }));       // solo importa la proporción

	// Nadie crece, o no sobra nada.
	CHECK((run({ { 30, 0 }, { 40, 0 } }, 100) == V{ 30, 40 }));
	CHECK((run({ { 30, 1 }, { 40, 1 } }, 70) == V{ 30, 40 }));
	CHECK((run({ { 30, 1 }, { 40, 0 } }, 50) == V{ 30, 40 }));
	CHECK((run({ { 30, 1 } }, -20) == V{ 30 }));                     // ventana diminuta
	CHECK(run({}, 100).empty());

	// Redondeo: enteros, y que sumen justo lo que hay.
	for (const float available : { 100.0f, 101.0f, 1000.0f, 7.0f, 1.0f })
	{
		for (const int count : { 2, 3, 6, 7, 13 })
		{
			const V sizes = run(std::vector<FlexItem>(static_cast<size_t>(count), FlexItem{ 0.0f, 0.1f }), available);
			bool whole = sizes.size() == static_cast<size_t>(count);
			for (const float size : sizes) {
				// Iguales salvo un píxel, y enteros.
				whole = whole && size == std::floor(size) && std::abs(size - available / static_cast<float>(count)) <= 1.0f;
			}
			CHECK(whole);
			CHECK(sum(sizes) == available);
		}
	}

	// El píxel suelto del redondeo no le cae a quien no crece.
	const V mixed = run({ { 10, 0 }, { 0, 1 }, { 10, 0 }, { 0, 1 }, { 10, 0 }, { 0, 1 }, { 10, 0 } }, 140);
	CHECK(mixed.size() == 7 && mixed[0] == 10 && mixed[2] == 10 && mixed[4] == 10 && mixed[6] == 10);
	CHECK(sum(mixed) == 140);
}

// 15. Column y Row: el ejemplo de docs/ui-design.md, completo.
static void test_linear()
{
	std::cout << "[15] Row y Column\n";
	Ui ui;
	auto& column = ui.root().add<Column>();
	column.set_padding(Insets::all(10));
	column.set_gap(8);
	auto& title  = column.add<Fixed>(50.0f, 30.0f);
	auto& list   = column.add<Fixed>(80.0f, 0.0f, 1.0f);
	auto& button = column.add<Fixed>(60.0f, 40.0f);

	ui.update(200, 300);
	CHECK(column.desired_size().x == 100 && column.desired_size().y == 106);   // 80 + márgenes; 30 + 0 + 40 + 16 + 20
	CHECK((column.rect() == Rect{ 0, 0, 200, 300 }));
	CHECK((title.rect()  == Rect{ 10, 10, 180, 30 }));      // Fill: todo el ancho
	CHECK((list.rect()   == Rect{ 10, 48, 180, 194 }));
	CHECK((button.rect() == Rect{ 10, 250, 180, 40 }));

	// Al agrandar la ventana, el único que crece es la lista.
	ui.update(200, 400);
	CHECK((list.rect()   == Rect{ 10, 48, 180, 294 }));
	CHECK((button.rect() == Rect{ 10, 350, 180, 40 }));

	// Uno escondido no ocupa, ni deja su separación.
	title.set_visible(false);
	ui.update(200, 300);
	CHECK((list.rect()   == Rect{ 10, 10, 180, 232 }));
	CHECK((button.rect() == Rect{ 10, 250, 180, 40 }));

	// Colocación a lo ancho.
	button.set_align(Placement::End, Placement::Fill);
	ui.update(200, 300);
	CHECK((button.rect() == Rect{ 130, 250, 60, 40 }));
	button.set_align(Placement::Center, Placement::Fill);
	ui.update(200, 300);
	CHECK((button.rect() == Rect{ 70, 250, 60, 40 }));

	// Row, con un Spacer que empuja al último contra el borde.
	Ui other;
	auto& row = other.root().add<Row>();
	row.set_gap(5);
	row.set_align(Placement::Fill, Placement::Start);
	auto& first = row.add<Fixed>(40.0f, 20.0f);
	row.add<Spacer>();
	auto& last = row.add<Fixed>(60.0f, 30.0f);
	other.update(300, 100);
	CHECK((first.rect() == Rect{ 0, 0, 40, 30 }));          // tan alta como la mayor
	CHECK((last.rect()  == Rect{ 240, 0, 60, 30 }));
	CHECK(at(other, 150, 10) == nullptr);                   // el Spacer no coge el ratón
	CHECK(at(other, 10, 10) == &first);
}

// 16. Stack y Panel: esquinas, centro y márgenes.
static void test_stack()
{
	std::cout << "[16] Stack y Panel\n";
	Ui ui;
	auto& screen = ui.root().add<Stack>();
	screen.set_padding({ 20, 90, 20, 20 });
	auto& full   = screen.add<Fixed>(10.0f, 10.0f);
	auto& corner = screen.add<Fixed>(100.0f, 50.0f);
	auto& middle = screen.add<Panel>(BoxStyle{});
	corner.set_align(Placement::End, Placement::End);
	middle.set_align(Placement::Center, Placement::Center);
	middle.set_padding(Insets::all(15));
	auto& content = middle.add<Fixed>(101.0f, 40.0f);

	ui.update(800, 600);
	CHECK((full.rect()    == Rect{ 20, 90, 760, 490 }));
	CHECK((corner.rect()  == Rect{ 680, 530, 100, 50 }));
	CHECK((middle.rect()  == Rect{ 334, 300, 131, 70 }));   // 131 x 70; a píxel entero (334,5 -> 334)
	CHECK((content.rect() == Rect{ 349, 315, 101, 40 }));
	CHECK(at(ui, 340, 305) == &middle);                     // un Panel sí coge el ratón

	// Ventana más pequeña que lo que se pide: nada negativo.
	ui.update(30, 50);
	CHECK(full.rect().width == 0 && full.rect().height == 0);
	CHECK(corner.rect().width == 0 && content.rect().width == 0);
}

// 17. Lo que se añade o se quita recoloca a los demás.
static void test_relayout()
{
	std::cout << "[17] anadir y quitar recoloca\n";
	Ui ui;
	auto& panel = ui.root().add<Panel>(BoxStyle{});
	panel.set_align(Placement::Center, Placement::Center);
	auto& column = panel.add<Column>();
	column.set_gap(10);
	auto& a = column.add<Fixed>(100.0f, 40.0f);
	auto& b = column.add<Fixed>(100.0f, 40.0f);
	ui.update(300, 300);
	CHECK((panel.rect() == Rect{ 100, 105, 100, 90 }));

	column.add<Fixed>(100.0f, 40.0f);
	ui.update(300, 300);
	CHECK((panel.rect() == Rect{ 100, 80, 100, 140 }));

	a.remove();
	ui.update(300, 300);
	CHECK((panel.rect() == Rect{ 100, 105, 100, 90 }));
	CHECK(b.rect().y == 105);
	CHECK(column.children().size() == 2);

	// Botón sin fuente: el alto del texto más su margen.
	auto& button = column.add<Button>(u8"X");
	ui.update(300, 300);
	CHECK(button.rect().height == ui.theme().text_size + 2 * ui.theme().button_padding.y);
}

// 18. Las cuentas de la barra, a solas.
static void test_scroll_math()
{
	std::cout << "[18] cuentas de la barra\n";

	// El escenario de scroll_math.cpp: se ven 200 de 1000 px, con un carril de 200.
	const ScrollRange list{ .view = 200, .content = 1000, .track = 200, .min_thumb = 24 };
	CHECK(near(thumb_for_scroll(list, 0).length, 40));
	CHECK(near(thumb_for_scroll(list, 0).start, 0));
	CHECK(near(thumb_for_scroll(list, 400).start, 80));
	CHECK(near(thumb_for_scroll(list, 800).start, 160));     // al final: 160 + 40 = 200, pegado abajo
	CHECK(near(scroll_for_thumb(list, 10), 50));
	CHECK(near(scroll_for_thumb(list, 160), 800));

	// Un carril más corto que la ventana: el pulgar es proporcional al CARRIL.
	const ScrollRange shorter{ .view = 200, .content = 1000, .track = 150, .min_thumb = 24 };
	CHECK(near(thumb_for_scroll(shorter, 0).length, 30));
	CHECK(near(thumb_for_scroll(shorter, 800).start, 120));
	CHECK(near(scroll_for_thumb(shorter, 60), 400));

	// Lista larguísima: le tocarían 4 px, se queda en 24. Y aun así llega al final justo.
	const ScrollRange huge{ .view = 200, .content = 10000, .track = 200, .min_thumb = 24 };
	CHECK(near(thumb_for_scroll(huge, 0).length, 24));
	CHECK(near(thumb_for_scroll(huge, 9800).start, 176));
	CHECK(near(scroll_for_thumb(huge, 176), 9800));
	CHECK(near(scroll_for_thumb(huge, 88), 4900));           // a la mitad, la mitad

	// Cabe todo, o justo: pulgar entero, y nada de dividir entre cero.
	for (const float content : { 0.0f, 150.0f, 200.0f })
	{
		const ScrollRange fits{ .view = 200, .content = content, .track = 200, .min_thumb = 24 };
		CHECK(near(thumb_for_scroll(fits, 0).start, 0) && near(thumb_for_scroll(fits, 0).length, 200));
		CHECK(near(scroll_for_thumb(fits, 0), 0));
		CHECK(near(scroll_for_thumb(fits, 50), 0));
	}

	// Carril más corto que el pulgar mínimo: no se sale, y no puede moverse.
	const ScrollRange tiny{ .view = 10, .content = 1000, .track = 10, .min_thumb = 24 };
	CHECK(near(thumb_for_scroll(tiny, 0).length, 10));
	CHECK(near(thumb_for_scroll(tiny, 990).start, 0));
	CHECK(near(scroll_for_thumb(tiny, 0), 0));
	CHECK(near(scroll_for_thumb(tiny, 5), 0));

	// Ida y vuelta: una función deshace la otra.
	bool round_trip = true;
	for (const ScrollRange& range : { list, shorter, huge }) {
		for (float scroll = 0.0f; scroll <= range.content - range.view; scroll += 37.0f) {
			round_trip = round_trip && std::abs(scroll_for_thumb(range, thumb_for_scroll(range, scroll).start) - scroll) < 0.1f;
		}
	}
	CHECK(round_trip);
}

// Una lista de 10 filas de 40 px (400) en una ventana de 200x100: sobran 300.
static std::vector<Widget*> fill(ScrollView& list, int count = 10)
{
	std::vector<Widget*> rows;
	for (int i = 0; i < count; ++i) {
		rows.push_back(&list.content().add<Fixed>(50.0f, 40.0f));
	}
	return rows;
}

// 19. ScrollView: layout, rueda y límites.
static void test_scroll_view()
{
	std::cout << "[19] ScrollView y la rueda\n";
	Ui ui;
	auto& list = ui.root().add<ScrollView>();
	const auto rows = fill(list);
	ui.update(200, 100);
	CHECK(list.desired_size().x == 62 && list.desired_size().y == 0);   // 50 + la barra; a lo alto no pide
	CHECK(list.bar().visible() && (list.bar().rect() == Rect{ 188, 0, 12, 100 }));
	CHECK((rows[0]->rect() == Rect{ 0, 0, 188, 40 }));
	CHECK(list.max_scroll() == 300);

	// La rueda hacia abajo (negativa) sube las filas. Con el ratón quieto, lo de debajo cambia.
	CHECK(at(ui, 10, 10) == rows[0]);
	CHECK(wheel(ui, -1));                    // la fila no la quiere y sube hasta la lista
	CHECK(list.scroll() == 60);
	ui.update(200, 100);
	CHECK(rows[0]->rect().y == -60);
	CHECK(ui.hovered() == rows[1]);

	// Los topes.
	wheel(ui, -100);
	CHECK(list.scroll() == 300);
	ui.update(200, 100);
	CHECK((rows[9]->rect() == Rect{ 0, 60, 188, 40 }));   // la última, pegada abajo
	wheel(ui, 100);
	CHECK(list.scroll() == 0);

	// Media muesca (trackpad): se acumula, y las filas siempre en píxel entero.
	ui.theme().scroll_step = 25.0f;
	ui.update(200, 100);
	wheel(ui, -0.5f);
	CHECK(list.scroll() == 12.5f);
	ui.update(200, 100);
	CHECK(rows[0]->rect().y == std::round(rows[0]->rect().y));
	wheel(ui, -0.5f);
	CHECK(list.scroll() == 25.0f);

	// Fuera de la ventana no hay a quién dársela.
	ui.on_event({ .type = EventType::MouseLeave });
	CHECK(!wheel(ui, -1));
	CHECK(list.scroll() == 25.0f);

	// Estaba al final y la ventana crece: sin hueco debajo.
	list.set_scroll(1000);
	ui.update(200, 250);
	CHECK(list.scroll() == 150 && rows[9]->rect().y == 210);

	// Se quitan filas: lo mismo. Y si cabe todo, sin barra y a todo el ancho.
	for (int i = 4; i < 10; ++i) { rows[static_cast<size_t>(i)]->remove(); }
	ui.update(200, 250);
	CHECK(list.scroll() == 0 && list.max_scroll() == 0 && !list.bar().visible());
	CHECK((rows[0]->rect() == Rect{ 0, 0, 200, 40 }));
	CHECK(at(ui, 10, 10) == rows[0]);
	CHECK(wheel(ui, -1) && list.scroll() == 0);
}

// 20. La barra: agarrar, arrastrar, pasarse y volver.
static void test_scroll_bar()
{
	std::cout << "[20] arrastrar la barra\n";
	Ui ui;
	auto& list = ui.root().add<ScrollView>();
	const auto rows = fill(list);
	ui.update(200, 100);
	// Carril de 100; pulgar de 100 * 100/400 = 25; recorrido 75 para 300: 4 px por píxel.
	CHECK((list.bar().thumb_rect() == Rect{ 188, 0, 12, 25 }));

	down(ui, 194, 10);
	CHECK(ui.captured() == &list.bar());
	CHECK(list.scroll() == 0);                      // al agarrarlo no salta
	at(ui, 194, 20);
	CHECK(near(list.scroll(), 40));
	at(ui, 20, 30);                                 // el ratón se sale de la barra: se sigue arrastrando
	CHECK(near(list.scroll(), 80));
	CHECK(!rows[0]->hovered());
	CHECK(wheel(ui, -1) && near(list.scroll(), 80));   // arrastrando, la rueda no cuenta

	// Se pasa de largo y vuelve: el pulgar sigue agarrado por el mismo sitio.
	at(ui, 194, 5000);
	CHECK(list.scroll() == 300);
	ui.update(200, 100);
	CHECK(near(list.bar().thumb_rect().y, 75));
	at(ui, 194, -5000);
	CHECK(list.scroll() == 0);
	at(ui, 194, 20);
	CHECK(near(list.scroll(), 40));
	up(ui, 194, 20);
	at(ui, 194, 60);                                // suelto: ya no se mueve
	CHECK(near(list.scroll(), 40) && ui.captured() == nullptr);

	// Clic en el carril: el pulgar salta y queda centrado en el ratón (80 - 12,5 = 67,5).
	ui.update(200, 100);
	down(ui, 194, 80);
	CHECK(near(list.scroll(), 270));
	up(ui, 194, 80);

	// El derecho no arrastra, y un Alt+Tab a medias lo suelta.
	ui.update(200, 100);
	down(ui, 194, 80, MouseButton::Right);
	at(ui, 194, 40);
	CHECK(near(list.scroll(), 270));
	up(ui, 194, 40, MouseButton::Right);
	down(ui, 194, 80);
	ui.on_event({ .type = EventType::MouseCancel });
	at(ui, 194, 40);
	CHECK(near(list.scroll(), 270));
}

// 21. Una lista dentro de otra: la de fuera solo se mueve cuando la de dentro ya no puede.
static void test_nested_scroll()
{
	std::cout << "[21] una lista dentro de otra\n";
	Ui ui;
	auto& outer = ui.root().add<ScrollView>();
	auto& inner = outer.content().add<ScrollView>();
	inner.set_min_size({ 0.0f, 80.0f });
	fill(inner, 4);                                    // 160 en 80: sobran 80
	outer.content().add<Fixed>(50.0f, 200.0f);         // 280 en 100: sobran 180
	ui.update(200, 100);
	CHECK(inner.max_scroll() == 80 && outer.max_scroll() == 180);

	at(ui, 10, 10);
	wheel(ui, -1);
	CHECK(inner.scroll() == 60 && outer.scroll() == 0);
	wheel(ui, -1);
	CHECK(inner.scroll() == 80 && outer.scroll() == 0);    // se gasta en la de dentro
	wheel(ui, -1);
	CHECK(inner.scroll() == 80 && outer.scroll() == 60);   // esa ya no puede más
}

int main()
{
	test_rect();
	test_tree();
	test_hit_test();
	test_visibility();
	test_clipping();
	test_enter_leave();
	test_widget_moves();
	test_bubbling();
	test_capture_rules();
	test_button();
	test_button_edges();
	test_remove();
	test_reentrancy();
	test_distribute();
	test_linear();
	test_stack();
	test_relayout();
	test_scroll_math();
	test_scroll_view();
	test_scroll_bar();
	test_nested_scroll();

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}
