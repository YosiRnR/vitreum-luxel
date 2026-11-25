// Pruebas de luxel_ui (hitos U1 y U2), sin ventana ni GPU: se inventan los eventos del
// ratón y se mira qué widget lo tiene encima. Solo usa la API pública: el hit
// test se prueba a través de Ui.

#include <luxel/ui/ui.h>
#include <luxel/ui/widgets.h>
#include <iostream>
#include <string>

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

// 8. Subida: del más profundo hacia arriba, hasta que uno lo acepta.
static void test_bubbling()
{
	std::cout << "[8] subida y captura\n";
	std::string log;
	Ui ui;
	ui.update(800, 600);
	auto& a = ui.root().add<Recorder>(Rect{ 0, 0, 300, 300 }, 'a', true, log);
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
	ui.root().add<Recorder>(Rect{ 0, 0, 100, 100 }, 'a', true, log);

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
	auto& button = ui.root().add<Button>(u8"Salir");
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
	auto& panel  = ui.root().add<Recorder>(Rect{ 0, 0, 200, 200 }, 'p', true, log);
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
	auto& keep   = ui.root().add<Box>(Rect{ 0, 0, 400, 400 });
	auto& dialog = ui.root().add<Box>(Rect{ 0, 0, 200, 200 });
	auto& last   = ui.root().add<Box>(Rect{ 500, 0, 10, 10 });
	dialog.add<Closer>(dialog, alive).set_rect({ 0, 0, 50, 50 });
	CHECK(alive == 1);

	down(ui, 20, 20);
	up(ui, 20, 20);
	CHECK(seen == 1);   // durante su función, vivo
	CHECK(alive == 0);            // al acabar el evento, no
	CHECK(ui.captured() == nullptr);
	CHECK(ui.root().children().size() == 2);          // los demás siguen, y en orden
	CHECK(ui.root().children()[0].get() == &keep);
	CHECK(ui.root().children()[1].get() == &last);
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
	ui.root().add<Nested>(ui).set_rect({ 0, 0, 50, 50 });
	down(ui, 20, 20);
	up(ui, 20, 20);
	CHECK(alive_inside == 1);
	CHECK(alive == 0);
	CHECK(ui.root().children().empty());
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

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}
