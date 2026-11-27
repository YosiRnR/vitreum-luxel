#pragma once

#include "glyf.h"
#include <luxel/ttf/outline.h>

namespace luxel::ttf::detail
{
	// De puntos on/off (unidades de la fuente, +Y arriba) a rectas y cuadráticas en
	// píxeles (+Y abajo): multiplica por "scale" (píxeles por unidad) y da la vuelta a Y.
	Outline build_outline(const GlyphShape& shape, float scale);

} // namespace luxel::ttf::detail
