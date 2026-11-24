#pragma once

// Rectángulos de recorte del Renderer2D (hito R4d). Aparte y sin GPU para poder
// probarlos en tests/test_renderer2d.

#include <luxel/math/vectors.h>
#include <algorithm>
#include <cmath>

namespace luxel::detail
{
	// Un rectángulo como esquina mínima y máxima, en unidades del lote. "min" es
	// la de X e Y más pequeñas: arriba a la izquierda en la UI (+Y abajo) y abajo
	// a la izquierda en el mundo (+Y arriba). Así las cuentas son las mismas.
	struct ClipRect
	{
		math::Vector2f min{};
		math::Vector2f max{};

		bool is_empty() const { return !(min.x < max.x && min.y < max.y); }
	};

	// A partir de la esquina de arriba a la izquierda. Un tamaño negativo cuenta
	// como 0: no deja ver nada.
	inline ClipRect make_clip(math::Vector2f top_left, math::Vector2f size, float y_sign)
	{
		const float w = std::max(0.0f, size.x);
		const float h = std::max(0.0f, size.y);
		// La otra punta de Y está "h" más abajo: en la UI sumando, en el mundo restando.
		const float bottom = top_left.y - y_sign * h;
		return { { top_left.x, std::min(top_left.y, bottom) }, { top_left.x + w, std::max(top_left.y, bottom) } };
	}

	// Lo que se ve con los dos recortes a la vez. Escenario: un panel que va de
	// x = 0 a 200 y dentro una columna de x = 150 a 300 -> se ve de 150 a 200.
	// Si no se tocan, sale un rectángulo vacío (is_empty).
	inline ClipRect intersect(const ClipRect& a, const ClipRect& b)
	{
		ClipRect r{ { std::max(a.min.x, b.min.x), std::max(a.min.y, b.min.y) },
		            { std::min(a.max.x, b.max.x), std::min(a.max.y, b.max.y) } };
		// Sin solape, max queda por debajo de min; se deja en tamaño 0.
		r.max.x = std::max(r.max.x, r.min.x);
		r.max.y = std::max(r.max.y, r.min.y);
		return r;
	}

	// Cómo queda un quad respecto al recorte.
	enum class ClipResult
	{
		Outside,   // no se ve nada: ni se envía
		Inside,    // se ve entero: no necesita recorte
		Partial    // el borde lo corta: lo recorta el pixel shader
	};

	// center y size, como en Quad. Si está girado, se usa el círculo que lo
	// contiene (radio = media diagonal): puede decir Partial de más, pero nunca
	// se equivoca al decir Outside o Inside.
	inline ClipResult classify(const ClipRect& clip, math::Vector2f center, math::Vector2f size, float rotation)
	{
		if (clip.is_empty()) {
			return ClipResult::Outside;
		}

		float half_w = std::fabs(size.x) * 0.5f;   // fabs: un tamaño negativo es un quad en espejo
		float half_h = std::fabs(size.y) * 0.5f;
		if (rotation != 0.0f) {
			half_w = half_h = std::sqrt(half_w * half_w + half_h * half_h);
		}

		const float left = center.x - half_w, right = center.x + half_w;
		const float low  = center.y - half_h, high  = center.y + half_h;

		// Tocarse solo por el canto (right == clip.min.x) es estar fuera.
		if (right <= clip.min.x || left >= clip.max.x || high <= clip.min.y || low >= clip.max.y) {
			return ClipResult::Outside;
		}
		if (left >= clip.min.x && right <= clip.max.x && low >= clip.min.y && high <= clip.max.y) {
			return ClipResult::Inside;
		}
		return ClipResult::Partial;
	}

} // namespace luxel::detail
