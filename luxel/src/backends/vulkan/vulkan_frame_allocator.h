#pragma once

#include "vulkan_common.h"
#include "vulkan_memory_allocator.h"
#include <luxel/frame.h>
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

namespace luxel::vulkan
{
	class VulkanDevice;
	class VulkanBindlessTable;

	// Capacidad de cada bloque = máximo de una reserva (contrato público de Frame).
	constexpr size_t k_frame_allocator_capacity = k_frame_allocation_max_size;

	// Asignador lineal ("bump allocator") de un frame en vuelo.
	//
	// Es una lista de bloques de 1 MB: cada bloque es un VkBuffer de storage en
	// memoria visible y coherente, mapeado para siempre, con su propio hueco
	// bindless. allocate() escribe en el bloque actual avanzando un cursor; si
	// no cabe, salta al siguiente bloque (y lo crea si no existe).
	// reset_cursor() vuelve al primer bloque SIN destruir ninguno: el frame
	// siguiente que use este asignador reutiliza los bloques ya creados.
	//
	// Escenario: un frame sube 2,5 MB en trozos de 512 KB. Los dos primeros
	// trozos van al bloque 0, los dos siguientes al bloque 1 y el quinto al
	// bloque 2 (que se crea en ese momento). El siguiente frame con este
	// asignador empieza otra vez en el bloque 0 y ya tiene los 3 bloques.
	//
	// Los bloques no se liberan hasta que muere el asignador: un pico puntual
	// deja la memoria reservada (pendiente: recortarla si sobra durante muchos
	// frames, como trim() del asignador de memoria).
	class VulkanFrameAllocator
	{
		// Un bloque. El orden de los miembros importa: se destruyen al revés, así
		// el VkBuffer muere antes de devolver su memoria. El hueco bindless lo
		// devuelve ~VulkanFrameAllocator (necesita la tabla).
		struct Chunk
		{
			MemoryAllocation memory{};
			vk::raii::Buffer buffer{ nullptr };
			uint32_t slot_index{};
		};

		VulkanDevice* m_device{ nullptr };
		VulkanBindlessTable* m_bindless_table{ nullptr };

		std::vector<Chunk> m_chunks;
		size_t m_current_chunk{ 0 };   // bloque donde se escribe ahora
		uint32_t m_cursor{ 0 };        // siguiente byte libre dentro de ese bloque

		Chunk create_chunk() const;

	public:
		VulkanFrameAllocator(VulkanDevice& device);
		~VulkanFrameAllocator();

		VulkanFrameAllocator(const VulkanFrameAllocator&) = delete;
		VulkanFrameAllocator& operator=(const VulkanFrameAllocator&) = delete;

		BufferAllocation allocate(std::span<const std::byte> buffer_data);
		void reset_cursor();

		// Bloques creados hasta ahora (para tests y estadísticas).
		size_t chunk_count() const noexcept { return m_chunks.size(); }
	};

} // namespace luxel::vulkan
