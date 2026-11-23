#pragma once
#include <luxel/command_list.h>
#include <cstdint>
#include <cstddef>
#include <span>

namespace luxel
{
	struct BufferAllocation
	{
		uint32_t slot_index{};   // hueco bindless del buffer donde quedaron los datos
		uint32_t offset{};       // byte donde empiezan dentro de ese buffer (múltiplo de 16)
	};

	// Tamaño máximo de UNA llamada a Frame::allocate. El total por frame no
	// tiene límite fijo: el asignador encadena bloques de este tamaño según
	// hagan falta (cada uno con su propio hueco bindless, por eso
	// BufferAllocation lleva slot_index). Quien necesite subir más que esto
	// debe partir sus datos (como hace Renderer2D con los lotes grandes).
	inline constexpr size_t k_frame_allocation_max_size = 1024 * 1024;   // 1 MB

	class Frame
	{
	public:
		virtual ~Frame() = default;

		virtual CommandList& commands() = 0;

		// Copia datos a memoria visible por la GPU, válida hasta que termine este
		// frame. Lanza std::runtime_error si buffer_data supera
		// k_frame_allocation_max_size. Datos vacíos => BufferAllocation{} (slot 0).
		virtual BufferAllocation allocate(std::span<const std::byte> buffer_data) = 0;
	};

} // namespace luxel
