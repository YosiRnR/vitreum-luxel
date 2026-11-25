#pragma once

#include <span>
#include <vector>

namespace luxel::ui
{
	// Un hijo de una fila o columna, visto solo a lo largo.
	struct FlexItem
	{
		float desired{ 0.0f };   // lo que pide, en píxeles (entero)
		float grow{ 0.0f };      // su peso al repartir lo que sobra; 0 = no crece
	};

	// Reparte "available" píxeles (entero) entre los hijos y devuelve cuánto mide
	// cada uno, en el mismo orden. Las reglas, en distribute.cpp.
	std::vector<float> distribute(std::span<const FlexItem> items, float available);

} // namespace luxel::ui
