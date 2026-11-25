#pragma once

#include <luxel/ui/widget.h>

namespace luxel::ui
{
	// El widget que recibe el ratón en el punto p, buscando en "widget" y en todo
	// lo que cuelga de él. nullptr si no hay ninguno.
	Widget* hit_test(Widget& widget, math::Vector2f p);

} // namespace luxel::ui
