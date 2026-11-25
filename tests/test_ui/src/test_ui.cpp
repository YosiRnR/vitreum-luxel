// Pruebas de luxel_ui (hito U1), sin ventana ni GPU: se inventan los eventos del
// ratón y se mira qué widget lo tiene encima. Solo usa la API pública: el hit
// test se prueba a través de Ui.

#include <luxel/ui/ui.h>
#include <luxel/ui/widgets.h>
#include <iostream>

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
	auto& panel = ui.root().add<Box>(Rect{ 0, 0, 200, 200 });
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
	auto& below     = ui.root().add<Box>(Rect{ 0, 0, 100, 100 });
	auto& container = ui.root().add<Box>(Rect{ 0, 0, 100, 100 });
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
	auto& list = ui.root().add<Box>(Rect{ 0, 0, 100, 100 });
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
	auto& panel  = ui.root().add<Box>(Rect{ 0, 0, 200, 200 });
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
	auto& box = ui.root().add<Box>(Rect{ 0, 0, 100, 100 });
	CHECK(at(ui, 50, 50) == &box);

	box.set_rect({ 300, 300, 100, 100 });
	ui.update(800, 600);
	CHECK(ui.hovered() == nullptr && box.leaves == 1);

	box.set_rect({ 0, 0, 100, 100 });
	ui.update(800, 600);
	CHECK(ui.hovered() == &box && box.enters == 2);
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

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}
