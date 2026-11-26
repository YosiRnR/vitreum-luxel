#include "scroll_math.h"
#include <algorithm>

namespace luxel::ui
{
	// Escenario: una lista de 1000 px de la que se ven 200, con un carril de 200.
	//
	//     la lista (1000)                        el carril (200)
	//     ┌───────────────┐                      ┌──┐
	//     │ se ve:    200 │                      │██│ pulgar: 40
	//     ├───────────────┤                      ├──┤
	//     │               │                      │  │
	//     │ escondido:    │                      │  │ le queda por recorrer:
	//     │          800  │                      │  │                   160
	//     │               │                      │  │
	//     └───────────────┘                      └──┘
	//
	// Largo del pulgar: se ve 1/5 de la lista, así que ocupa 1/5 del carril = 40.
	//
	// Posición: los dos recorridos empiezan y acaban A LA VEZ. Mientras la lista recorre
	// sus 800 escondidos, el pulgar recorre los 160 que le quedan:
	//
	//     lista bajada   0  ->  pulgar en   0
	//     lista bajada 400  ->  pulgar en  80     (la mitad, la mitad)
	//     lista bajada 800  ->  pulgar en 160     (160 + 40 = 200: pegado al final)
	//
	// Y al revés: si arrastro el pulgar 10 px, la lista baja 50.
	//
	// Casos especiales:
	//   1. Si cabe todo (content <= view): pulgar del largo del carril, en 0; y scroll 0.
	//   2. El pulgar nunca es más corto que min_thumb. Con 10.000 px de lista le tocarían
	//      4: no habría quien lo agarrase.
	//   3. ...ni más largo que el carril. Si lo llena, no le queda por dónde moverse:
	//      posición 0 y scroll 0. (Cuidado: ¿entre qué estarías dividiendo?)
	//
	// Trampa: con el pulgar mínimo, el recorrido ya no es el que saldría de la proporción.
	// Con la lista de 10.000, el pulgar mide 24 (no 4), así que recorre 176 (no 196). Si
	// no se tiene en cuenta, al llegar al final el pulgar se sale del carril.
	//
	// No hace falta recortar start ni scroll a sus límites: ya lo hace ScrollView.

	Thumb thumb_for_scroll(const ScrollRange& range, float scroll)
	{
		if (range.track <= 0.0f) {
			return { .start = 0.0f, .length = 0.0f };
		}
		const float min_length = std::min(range.min_thumb, range.track);
		const float max_scroll = range.content - range.view;

		if (max_scroll <= 0.0f) {
			return { .start = 0.0f, .length = range.track };
		}

		const float raw_length   = range.track * (range.view / range.content);
		const float thumb_length = std::clamp(raw_length, min_length, range.track);
		const float max_thumb    = range.track - thumb_length;

		if (max_thumb <= 0.0f) {
			return { .start = 0.0f, .length = thumb_length };
		}

		const float factor      = scroll / max_scroll;
		const float thumb_start = factor * max_thumb;

		return { .start = thumb_start, .length = thumb_length };
	}

	float scroll_for_thumb(const ScrollRange& range, float start)
	{
		if (range.track <= 0.0f) {
			return 0.0f;
		}

		const float min_length = std::min(range.min_thumb, range.track);
		const float max_scroll = range.content - range.view;

		if (max_scroll <= 0.0f) {
			return 0.0f;
		}

		const float raw_length   = range.track * (range.view / range.content);
		const float thumb_length = std::clamp(raw_length, min_length, range.track);
		const float max_thumb    = range.track - thumb_length;

		if (max_thumb <= 0.0f) {
			return 0.0f;
		}

		const float factor = start / max_thumb;

		return factor * max_scroll;
	}

} // namespace luxel::ui
