#include <luxel/ui/layout.h>
#include "distribute.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace luxel::ui
{
	namespace
	{
		struct Span1D
		{
			float start{ 0.0f };
			float size{ 0.0f };
		};

		// Coloca algo que quiere medir "desired" en el hueco [start, start + extent).
		Span1D place(Placement placement, float start, float extent, float desired)
		{
			if (placement == Placement::Fill) {
				return { start, extent };
			}
			const float size  = std::min(desired, extent);   // si no cabe, hasta donde llegue
			const float spare = extent - size;
			switch (placement)
			{
			case Placement::Center: return { start + std::floor(spare * 0.5f), size };   // a píxel entero
			case Placement::End:    return { start + spare, size };
			default:                return { start, size };
			}
		}

		// Lo que queda dentro de rect quitando los márgenes (nunca negativo).
		Rect inside(const Rect& rect, const Insets& padding)
		{
			return { rect.x + padding.left, rect.y + padding.top,
			         std::max(0.0f, rect.width  - padding.left - padding.right),
			         std::max(0.0f, rect.height - padding.top  - padding.bottom) };
		}
	}

	// ---------------------------------------------------------------- Stack

	math::Vector2f Stack::measure_content(const Theme& theme)
	{
		// Como se pisan, basta con que quepa el mayor.
		math::Vector2f largest{ 0.0f, 0.0f };
		for (const auto& child : children())
		{
			if (!child->visible()) {
				continue;
			}
			child->measure(theme);
			largest.x = std::max(largest.x, child->desired_size().x);
			largest.y = std::max(largest.y, child->desired_size().y);
		}
		return { largest.x + m_padding.left + m_padding.right, largest.y + m_padding.top + m_padding.bottom };
	}

	void Stack::arrange_children()
	{
		const Rect content = inside(rect(), m_padding);
		for (const auto& child : children())
		{
			if (!child->visible()) {
				continue;
			}
			const Span1D x = place(child->align_x(), content.x, content.width,  child->desired_size().x);
			const Span1D y = place(child->align_y(), content.y, content.height, child->desired_size().y);
			child->arrange({ x.start, y.start, x.size, y.size });
		}
	}

	// --------------------------------------------------------------- Linear
	// "main" es a lo largo (en Column, la altura) y "cross", a lo ancho.

	math::Vector2f Linear::measure_content(const Theme& theme)
	{
		float main = 0.0f, cross = 0.0f;
		int count = 0;
		for (const auto& child : children())
		{
			if (!child->visible()) {
				continue;   // no ocupa, y tampoco deja su separación
			}
			child->measure(theme);
			const math::Vector2f desired = child->desired_size();
			main += m_vertical ? desired.y : desired.x;
			cross = std::max(cross, m_vertical ? desired.x : desired.y);
			++count;
		}
		if (count > 1) {
			main += m_gap * static_cast<float>(count - 1);
		}
		return { (m_vertical ? cross : main) + m_padding.left + m_padding.right,
		         (m_vertical ? main : cross) + m_padding.top  + m_padding.bottom };
	}

	void Linear::arrange_children()
	{
		const Rect content = inside(rect(), m_padding);

		// 1. Apuntar lo que pide cada uno.
		std::vector<FlexItem> items;
		for (const auto& child : children())
		{
			if (child->visible()) {
				const math::Vector2f desired = child->desired_size();
				items.push_back({ m_vertical ? desired.y : desired.x, child->grow() });
			}
		}
		if (items.empty()) {
			return;
		}

		// 2. Repartir. Las separaciones no se reparten: se descuentan antes.
		const float gaps = m_gap * static_cast<float>(items.size() - 1);
		const std::vector<float> sizes = distribute(items, (m_vertical ? content.height : content.width) - gaps);

		// 3. Colocarlos uno detrás de otro.
		float cursor = m_vertical ? content.y : content.x;
		size_t i = 0;
		for (const auto& child : children())
		{
			if (!child->visible()) {
				continue;
			}
			const float size = sizes[i++];
			if (m_vertical) {
				const Span1D x = place(child->align_x(), content.x, content.width, child->desired_size().x);
				child->arrange({ x.start, cursor, x.size, size });
			}
			else {
				const Span1D y = place(child->align_y(), content.y, content.height, child->desired_size().y);
				child->arrange({ cursor, y.start, size, y.size });
			}
			cursor += size + m_gap;
		}
	}

} // namespace luxel::ui
