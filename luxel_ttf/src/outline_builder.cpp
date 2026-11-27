#include "outline_builder.h"
#include <algorithm>
#include <optional>

namespace luxel::ttf::detail
{
	namespace
	{
		Point mid(Point a, Point b) { return { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f }; }

		// Recorre UN contorno y lo convierte en segmentos. Las reglas de TrueType:
		//
		//     on  on           una recta
		//     on  off on       una cuadrática (el off es su control)
		//     on  off off on   dos cuadráticas: entre dos off seguidos hay un on
		//                      implícito justo en su punto medio
		//
		// Es lo que hacías en truetype/ (render_glyph_to_bitmap), reorganizado: se
		// empieza en un punto on (o, si todos los del principio y el final son off, en
		// el punto medio de ambos), se va guardando el control pendiente y se cierra
		// volviendo al principio.
		class ContourWalker
		{
		public:
			explicit ContourWalker(Outline& outline) : m_outline(outline) {}

			void walk(const Point* points, const uint8_t* on, std::size_t n)
			{
				if (n < 2) {
					return;   // un punto solo no encierra nada
				}
				// Dónde empezar: en el primer on; si el primero es off pero el último es
				// on, en el último; si los dos son off, en su punto medio (que es on).
				std::size_t first = 0;
				Point start{};
				if (on[0]) {
					start = points[0];
					first = 1;
				}
				else if (on[n - 1]) {
					start = points[n - 1];
					first = 0;
					--n;   // el último ya se ha usado como principio
				}
				else {
					start = mid(points[n - 1], points[0]);
					first = 0;
				}

				m_current = start;
				m_control.reset();
				for (std::size_t i = first; i < n; ++i)
				{
					if (on[i]) {
						go_to(points[i]);
					}
					else {
						if (m_control) {
							go_to(mid(*m_control, points[i]));   // el on implícito
						}
						m_control = points[i];
					}
				}
				go_to(start);   // cerrar
			}

		private:
			// Del punto actual a "to": cuadrática si hay un control pendiente, recta si no.
			void go_to(Point to)
			{
				if (m_control) {
					m_outline.segments.push_back({ m_current, *m_control, to, true });
					m_control.reset();
				}
				else if (!(to == m_current)) {   // una recta de longitud 0 no aporta nada
					m_outline.segments.push_back({ m_current, {}, to, false });
				}
				m_current = to;
			}

			Outline& m_outline;
			Point m_current{};
			std::optional<Point> m_control;
		};
	}

	Outline build_outline(const GlyphShape& shape, float scale)
	{
		Outline outline;
		ContourWalker walker(outline);

		std::vector<Point> points;
		std::vector<uint8_t> on;   // 1 = en la curva (vector<bool> no da punteros a sus elementos)
		uint32_t first = 0;
		for (const uint32_t last : shape.contour_ends)
		{
			points.clear();
			on.clear();
			for (uint32_t p = first; p <= last && p < shape.points.size(); ++p)
			{
				const ShapePoint& sp = shape.points[p];
				points.push_back({ sp.x * scale, -sp.y * scale });   // a píxeles y +Y abajo
				on.push_back(sp.on_curve ? 1 : 0);
			}
			walker.walk(points.data(), on.data(), points.size());
			// Un contorno que no ha dado ningún segmento (un punto suelto) no cuenta.
			const uint32_t done = outline.contour_ends.empty() ? 0 : outline.contour_ends.back();
			if (outline.segments.size() > done) {
				outline.contour_ends.push_back(static_cast<uint32_t>(outline.segments.size()));
			}
			first = last + 1;
		}

		// La caja: todos los puntos, también los de control (la curva nunca se sale de
		// su triángulo, así que la caja la contiene seguro).
		bool any = false;
		const auto include = [&](Point p) {
			if (!any) {
				outline.min = outline.max = p;
				any = true;
			}
			outline.min = { std::min(outline.min.x, p.x), std::min(outline.min.y, p.y) };
			outline.max = { std::max(outline.max.x, p.x), std::max(outline.max.y, p.y) };
		};
		for (const OutlineSegment& s : outline.segments)
		{
			include(s.from);
			include(s.to);
			if (s.is_curve) {
				include(s.control);
			}
		}
		return outline;
	}

} // namespace luxel::ttf::detail
