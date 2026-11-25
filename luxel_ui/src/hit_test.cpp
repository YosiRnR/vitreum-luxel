#include "hit_test.h"
#include <ranges>

namespace luxel::ui
{
	// Escenario (ventana con un panel, dos botones que se pisan y un texto):
	//
	//     panel (0,0) 200x200
	//       ├─ botón A (10,10) 100x40
	//       ├─ botón B (80,10) 100x40     <- añadido después: se pinta encima de A
	//       └─ texto (10,150) 100x20      <- ignora el ratón
	//
	//     p = ( 20,  20) -> A          solo está A
	//     p = ( 90,  20) -> B          están los dos, pero B se ve encima
	//     p = ( 20, 155) -> panel      el texto lo deja pasar
	//     p = (190, 190) -> panel      ahí no hay ningún hijo
	//     p = (300, 300) -> nullptr
	//
	// Reglas:
	//   1. Invisible: ni él ni sus hijos.
	//   2. Si recorta a sus hijos y p está fuera de su rect: nada (una fila
	//      escondida de una lista no recibe clics).
	//   3. Primero los hijos, y del ÚLTIMO al PRIMERO: el último se pinta encima.
	//      El primero que conteste que sí, gana. OJO: un hijo puede salirse del
	//      rect de su padre (si este no recorta), así que hay que mirarlos aunque
	//      p esté fuera del padre.
	//   4. Si ningún hijo lo quiere, él mismo, si p está en su rect y no ignora el
	//      ratón.
	Widget* hit_test(Widget& widget, math::Vector2f p)
	{
		if (!widget.visible()) {
			return nullptr;
		}

		const bool inside = widget.rect().contains(p);

		if (widget.clips_children() && !inside) {
			return nullptr;
		}

		for (const auto& child : std::views::reverse(widget.children()))
		{
			if (Widget* hit = hit_test(*child, p)) {
				return hit;
			}
		}

		if (inside && !widget.ignores_mouse()) {
			return &widget;
		}

		return nullptr;
	}

} // namespace luxel::ui
