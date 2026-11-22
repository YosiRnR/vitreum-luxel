#include "free_list_allocator.h"
#include <algorithm>
#include <optional>
#include <iterator>
#include <cstdint>
#include <cassert>

namespace luxel
{
	namespace
	{
		uint64_t align_up(uint64_t value, uint64_t alignment)
		{
			if (alignment <= 1) {
				return value;
			}
			assert((alignment & (alignment - 1)) == 0 && "El alineamiento debe ser potencia de 2");

			return (value + alignment - 1) & ~(alignment - 1);
		}

	} // namespace

	std::optional<MemoryRange> FreeListAllocator::allocate(uint64_t size, uint64_t alignment)
	{
		if (size == 0) {
			return std::nullopt;
		}
		// Para cubrir el caso de poner 0 y que sea equivalente a poner 1 (sin alineación)
		if (alignment == 0) {
			alignment = 1;
		}

		for (auto it = m_free_ranges.begin(); it != m_free_ranges.end(); ++it)
		{
			const uint64_t aligned  = align_up(it->offset, alignment);
			const uint64_t padding  = aligned - it->offset;

			if (padding > it->size || size > it->size - padding) {
				continue;
			}

			const uint64_t required  = padding + size;
			const uint64_t remaining = it->size - required;

			// ocupa el rango entero
			if (padding == 0 && remaining == 0) {
				m_free_ranges.erase(it);
			}
			// el tamaño solicitado es más pequeño que el rango libre, queda hueco por el final del rango
			else if (padding == 0)
			{
				it->offset = aligned + size;
				it->size   = remaining;
			}
			// la alineación provoca que quede hueco por el principio del rango
			else if (remaining == 0) {
				it->size = padding;
			}
			// la alineación provoca que quede hueco por el principio del rango y el size no completa el rango.
			// queda hueco por delante y por detrás
			else
			{
				it->size = padding;
				m_free_ranges.insert(std::next(it), MemoryRange{ .offset = aligned + size, .size = remaining });
			}

			return MemoryRange{ .offset = aligned, .size = size };
		}

		return std::nullopt;
	}

	void FreeListAllocator::free(uint64_t offset, uint64_t size)
	{
		if (size == 0) {
			return;
		}

		assert(offset <= m_size && size <= m_size - offset && "El rango a liberar excede los límites del bloque");

		auto next_it = std::lower_bound(
			m_free_ranges.begin(),
			m_free_ranges.end(),
			offset,
			[](const MemoryRange& range, uint64_t target_offset) {
				return range.offset < target_offset;
		});

		// comprobar solapamiento accidental
		assert((next_it == m_free_ranges.end() || offset + size <= next_it->offset) && "Detectado doble borrado o borrado solapado");

		bool merged_with_prev = false;

		// ¿tenemos vecino a la izquierda?
		if (next_it != m_free_ranges.begin())
		{
			auto prev_it = std::prev(next_it);
			assert(prev_it->offset + prev_it->size <= offset && "Detectado doble borrado o borrado solapado");

			// ¿lo tenemos pegado al offset que queremos eliminar?
			if (prev_it->offset + prev_it->size == offset)
			{
				prev_it->size += size; // fusionamos con el vecino izquierdo
				merged_with_prev = true;

				// ¿al fusionar con la izquierda alcanzamos a tocar al vecino derecho? (solo tenemos vecino derecho si next_it no está fuera del vector)
				if (next_it != m_free_ranges.end() && prev_it->offset + prev_it->size == next_it->offset)
				{
					prev_it->size += next_it->size; // fusionamos con el vecino derecho
					m_free_ranges.erase(next_it);   // eliminamos el vecino derecho pues ya lo hemos fusionado
				}
			}
		}

		// si no nos hemos fusionado con el vecino izquierdo, comprobamos si tocamos solo al vecino derecho
		if (!merged_with_prev)
		{
			// si existe el vecino derecho y lo tocamos con el rango que queremos liberar
			if (next_it != m_free_ranges.end() && offset + size == next_it->offset)
			{
				next_it->offset = offset;  // nos fusionamos con la derecha
				next_it->size  += size;
			}
			else
			{
				// estamos aislados, insertamos hueco nuevo
				m_free_ranges.insert(next_it, MemoryRange{ .offset = offset, .size = size });
			}
		}
	}

	uint64_t FreeListAllocator::free_size() const noexcept
	{
		uint64_t total = 0;
		for (const auto& range : m_free_ranges) {
			total += range.size;
		}
		return total;
	}

	bool FreeListAllocator::is_empty() const noexcept
	{
		return m_free_ranges.size()    == 1 &&
			   m_free_ranges[0].offset == 0	&&
			   m_free_ranges[0].size   == m_size;
	}

} // namespace luxel
