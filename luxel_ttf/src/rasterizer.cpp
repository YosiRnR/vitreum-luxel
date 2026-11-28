#include "rasterizer.h"
#include <luxel/ttf/raster.h>
#include <algorithm>
#include <cmath>

namespace luxel::ttf::detail
{
	Rasterizer::Rasterizer(uint32_t width, uint32_t height)
		: m_width(width)
		, m_height(height)
		, m_stride(width + 2)
		, m_changes(static_cast<std::size_t>(m_stride) * height, 0.0f)
	{
	}

	std::span<float> Rasterizer::row(uint32_t y)
	{
		return std::span<float>(m_changes).subspan(static_cast<std::size_t>(y) * m_stride, m_stride);
	}

	// Parte la recta en trozos, uno por fila que cruza, y le pasa cada trozo a
	// accumulate_piece. Escenario: de (2, 0,5) a (4, 2,5) cruza tres filas:
	//
	//     fila 0: de y = 0,5 a 1    -> altura 0,5, x de 2   a 2,5
	//     fila 1: de y = 1   a 2    -> altura 1,   x de 2,5 a 3,5
	//     fila 2: de y = 2   a 2,5  -> altura 0,5, x de 3,5 a 4
	void Rasterizer::draw_line(Point from, Point to)
	{
		if (from.y == to.y) {
			return;   // horizontal: no cambia la cobertura de nadie
		}

		// Recorrer siempre hacia abajo; el sentido real queda en el signo.
		float sign = 1.0f;
		if (from.y > to.y) {
			std::swap(from, to);
			sign = -1.0f;
		}

		const float dx_per_y = (to.x - from.x) / (to.y - from.y);
		const float max_x = static_cast<float>(m_width);
		const int first_row = std::max(0, static_cast<int>(std::floor(from.y)));
		const int end_row = std::min(static_cast<int>(m_height), static_cast<int>(std::ceil(to.y)));

		for (int y = first_row; y < end_row; ++y) {
			const float top = std::max(from.y, static_cast<float>(y));
			const float bottom = std::min(to.y, static_cast<float>(y + 1));
			if (bottom <= top) {
				continue;
			}
			// La caja de la imagen contiene la letra, así que x ya está dentro; el tope
			// solo protege de los redondeos de float (y de llamadas a mano).
			const float x_top = std::clamp(from.x + (top - from.y) * dx_per_y, 0.0f, max_x);
			const float x_bottom = std::clamp(from.x + (bottom - from.y) * dx_per_y, 0.0f, max_x);
			accumulate_piece(row(static_cast<uint32_t>(y)), x_top, x_bottom, (bottom - top) * sign);
		}
	}

	// La curva se aproxima con rectas. ¿Cuántas? Lo que más se separa una cuadrática de
	// la recta entre sus extremos es |from - 2 * control + to| / 4 (la "flecha"), y con n
	// rectas la separación baja como 1 / n^2. Se eligen las justas para que ninguna
	// recta quede a más de k_tolerance píxeles de la curva. Escenario: una curva con
	// flecha de 2 px necesita sqrt(2 / 0,02) = 10 rectas; una casi recta, 1.
	void Rasterizer::draw_quadratic(Point from, Point control, Point to)
	{
		constexpr float k_tolerance = 0.02f;
		const float fx = from.x - 2.0f * control.x + to.x;
		const float fy = from.y - 2.0f * control.y + to.y;
		const float deviation = 0.25f * std::sqrt(fx * fx + fy * fy);
		const int steps = std::clamp(static_cast<int>(std::ceil(std::sqrt(deviation / k_tolerance))), 1, 256);

		Point previous = from;
		for (int i = 1; i <= steps; ++i) {
			const float t = static_cast<float>(i) / static_cast<float>(steps);
			const float u = 1.0f - t;
			// La última, exacta: que el contorno cierre sin un hueco de redondeo.
			const Point p = i == steps
				? to
				: Point{ u * u * from.x + 2.0f * u * t * control.x + t * t * to.x,
						 u * u * from.y + 2.0f * u * t * control.y + t * t * to.y };
			draw_line(previous, p);
			previous = p;
		}
	}

	std::vector<uint8_t> Rasterizer::coverage() const
	{
		std::vector<uint8_t> out(static_cast<std::size_t>(m_width) * m_height);
		for (uint32_t y = 0; y < m_height; ++y) {
			const float* changes = m_changes.data() + static_cast<std::size_t>(y) * m_stride;
			float sum = 0.0f;
			for (uint32_t x = 0; x < m_width; ++x) {
				sum += changes[x];
				// Valor absoluto: da igual en qué sentido vaya el contorno. Tope en 1:
				// dos contornos del mismo sentido que se pisan no hacen "2 veces lleno".
				const float c = std::min(1.0f, std::abs(sum));
				out[static_cast<std::size_t>(y) * m_width + x] = static_cast<uint8_t>(c * 255.0f + 0.5f);
			}
		}
		return out;
	}

} // namespace luxel::ttf::detail

namespace luxel::ttf
{
	GlyphBitmap rasterize(const Outline& outline)
	{
		GlyphBitmap bitmap;
		if (outline.empty()) {
			return bitmap;
		}

		// La caja en píxeles enteros que contiene la letra.
		const float left = std::floor(outline.min.x);
		const float top = std::floor(outline.min.y);
		const float right = std::ceil(outline.max.x);
		const float bottom = std::ceil(outline.max.y);
		bitmap.left = static_cast<int32_t>(left);
		bitmap.top = static_cast<int32_t>(top);
		bitmap.width = static_cast<uint32_t>(right - left);
		bitmap.height = static_cast<uint32_t>(bottom - top);
		if (bitmap.width == 0 || bitmap.height == 0) {
			return bitmap;   // sin área (una raya de grosor 0): nada que pintar
		}

		// Del espacio del lápiz al de la imagen: restar la esquina.
		const auto local = [&](Point p) { return Point{ p.x - left, p.y - top }; };

		detail::Rasterizer rasterizer(bitmap.width, bitmap.height);
		for (const OutlineSegment& s : outline.segments) {
			if (s.is_curve) {
				rasterizer.draw_quadratic(local(s.from), local(s.control), local(s.to));
			}
			else {
				rasterizer.draw_line(local(s.from), local(s.to));
			}
		}
		bitmap.coverage = rasterizer.coverage();
		return bitmap;
	}

} // namespace luxel::ttf
