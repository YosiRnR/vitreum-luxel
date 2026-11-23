#pragma once

// Lógica de CPU del lote del Renderer2D que no necesita GPU: el orden por
// capa y el troceo en draws. Está aparte (y solo con std) para poder probarla
// sin device ni ventana: tests/test_renderer2d.

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace luxel::detail
{
	// Orden de dibujo por capa, ESTABLE: a igual capa se respeta el orden de
	// envío (importante con transparencias: lo que se envió después se pinta
	// encima).
	//
	// Devuelve false si los quads ya están en orden (el caso habitual: todo en
	// la misma capa, o enviado ya de atrás hacia delante): entonces no hace
	// falta reordenar ni copiar nada y "order" no se toca.
	// Si devuelve true, order[i] es el índice (en el orden de envío) del quad
	// que hay que dibujar en la posición i.
	//
	// Ejemplo: capas { 0, -1, 0, -1 } (envío: Iria, fondo, Iria2, fondo2)
	//          => order = { 1, 3, 0, 2 } (fondo, fondo2, Iria, Iria2).
	//
	// Se ordenan índices (4 bytes) y no los QuadInstance (64 bytes): mover
	// índices es mucho más barato; luego se copia cada quad una sola vez.
	inline bool stable_order_by_layer(std::span<const int32_t> layers, std::vector<uint32_t>& order)
	{
		if (std::ranges::is_sorted(layers)) {
			return false;
		}

		order.resize(layers.size());
		std::iota(order.begin(), order.end(), 0u);   // 0, 1, 2, ... (orden de envío)

		// La "proyección" (tercer argumento) dice por qué se compara: por la capa
		// de cada índice. stable_sort no cambia el orden relativo de los iguales.
		std::ranges::stable_sort(order, {}, [layers](uint32_t i) { return layers[i]; });
		return true;
	}

	// Un tramo del lote que cabe en un draw.
	struct DrawRange
	{
		uint32_t first{ 0 };   // primer quad del tramo
		uint32_t count{ 0 };   // cuántos quads
	};

	// Cuántos draws hacen falta para quad_count quads con max_per_draw por draw.
	// (División redondeando hacia arriba: 16.385 quads con 16.384 por draw => 2.)
	constexpr uint32_t draw_count(uint32_t quad_count, uint32_t max_per_draw)
	{
		return max_per_draw == 0 ? 0 : (quad_count + max_per_draw - 1) / max_per_draw;
	}

	// El tramo número draw_index (0, 1, ...). Todos llenos salvo el último.
	constexpr DrawRange draw_range(uint32_t quad_count, uint32_t max_per_draw, uint32_t draw_index)
	{
		const uint32_t first = draw_index * max_per_draw;
		if (first >= quad_count) {
			return { quad_count, 0 };
		}
		return { first, std::min(max_per_draw, quad_count - first) };
	}

} // namespace luxel::detail
