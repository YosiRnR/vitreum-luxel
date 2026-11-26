#pragma once

namespace luxel::ui
{
	// Los tamaños de una lista con barra, en píxeles.
	struct ScrollRange
	{
		float view;        // lo que se ve
		float content;     // lo que mide la lista entera
		float track;       // lo que mide el carril de la barra
		float min_thumb;   // el pulgar no puede ser más corto que esto
	};

	// El pulgar, contando desde el principio del carril.
	struct Thumb
	{
		float start;
		float length;
	};

	// De contenido a barra: con la lista bajada "scroll" píxeles, ¿dónde está el pulgar?
	Thumb thumb_for_scroll(const ScrollRange& range, float scroll);

	// De barra a contenido: si el pulgar empieza en "start", ¿cuánto se ha bajado?
	float scroll_for_thumb(const ScrollRange& range, float start);

} // namespace luxel::ui
