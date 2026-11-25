#pragma once

#include <luxel/math/vectors.h>

namespace luxel::ui
{
	// Rectángulo en píxeles: (0,0) arriba a la izquierda de la ventana, +Y abajo.
	struct Rect
	{
		float x{ 0.0f };
		float y{ 0.0f };
		float width{ 0.0f };
		float height{ 0.0f };

		math::Vector2f top_left() const { return { x, y }; }
		math::Vector2f size() const { return { width, height }; }

		// ¿Está el punto dentro? Los cantos de la derecha y de abajo NO cuentan.
		// Escenario: dos botones pegados, de x = 0 a 100 y de x = 100 a 200. El
		// punto x = 100 tiene que ser de uno solo: es del segundo.
		bool contains(math::Vector2f p) const
		{
			return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height;
		}

		bool operator==(const Rect&) const = default;
	};

} // namespace luxel::ui
