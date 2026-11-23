#include "vulkan_frame_allocator.h"
#include "vulkan_device.h"
#include <luxel/frame.h>
#include <stdexcept>
#include <format>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <span>

namespace luxel::vulkan
{
	VulkanFrameAllocator::VulkanFrameAllocator(VulkanDevice& vk_device)
		: m_device(&vk_device)
		, m_bindless_table(&vk_device.bindless_table())
	{
		// El primer bloque se crea ya: casi todos los frames suben algo, y así
		// un device recién creado tiene la misma memoria que antes de que el
		// asignador pudiera crecer (los tests cuentan con esa línea base).
		m_chunks.push_back(create_chunk());
	}

	VulkanFrameAllocator::~VulkanFrameAllocator()
	{
		// Huecos en orden INVERSO al de creación (como C++ con los miembros). La
		// tabla los reutiliza en orden LIFO, así el hueco del bloque 0 es el
		// primero que se vuelve a dar: un asignador nuevo recibe el mismo hueco
		// que tenía el anterior en su primer bloque.
		for (auto it = m_chunks.rbegin(); it != m_chunks.rend(); ++it) {
			if (it->slot_index > 0 && m_bindless_table) {
				m_bindless_table->free_buffer_slot(it->slot_index);
			}
		}
		// Después, el vector destruye cada Chunk: buffer y luego memoria.
	}

	VulkanFrameAllocator::Chunk VulkanFrameAllocator::create_chunk() const
	{
		Chunk chunk;

		vk::BufferCreateInfo buffer_info{
			.size        = k_frame_allocator_capacity,
			.usage       = vk::BufferUsageFlagBits::eStorageBuffer,
			.sharingMode = vk::SharingMode::eExclusive
		};
		chunk.buffer = vk::raii::Buffer(m_device->logical_device(), buffer_info);

		// Memoria visible y coherente: el bloque del asignador de memoria ya está
		// mapeado, así que el puntero mapeado sale gratis de la MemoryAllocation.
		const vk::MemoryRequirements mem_requirements = chunk.buffer.getMemoryRequirements();
		const vk::MemoryPropertyFlags properties =
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

		chunk.memory = m_device->memory_allocator().allocate(mem_requirements, properties, ResourceKind::Buffer);
		chunk.buffer.bindMemory(chunk.memory.memory(), chunk.memory.offset());

		// El hueco al final: si algo de arriba lanza, no queda un hueco perdido.
		chunk.slot_index = m_bindless_table->allocate_buffer_slot(chunk.buffer, {}, vk::WholeSize);
		return chunk;
	}

	void VulkanFrameAllocator::reset_cursor()
	{
		m_current_chunk = 0;
		m_cursor = 0;
	}

	BufferAllocation VulkanFrameAllocator::allocate(std::span<const std::byte> buffer_data)
	{
		const size_t size = buffer_data.size();
		if (size > k_frame_allocator_capacity) {
			throw std::runtime_error(std::format(
				"Frame::allocate: {} bytes superan el máximo de una reserva ({} bytes); hay que partir los datos",
				size, k_frame_allocator_capacity));
		}
		if (size == 0) {
			return {};
		}

		// Alineado a 16: cualquier struct del shader (float4, matrices) puede
		// empezar en el offset devuelto.
		size_t chunk_index = m_current_chunk;
		uint32_t offset = (m_cursor + 15u) & ~15u;

		// ¿No cabe en el bloque actual? Al siguiente, desde el principio. (El
		// hueco que queda al final del bloque se pierde hasta el próximo reset.)
		if (offset + size > k_frame_allocator_capacity) {
			chunk_index = m_current_chunk + 1;
			offset = 0;
		}

		// Si ese bloque aún no existe, se crea. Se trabaja con variables locales
		// hasta aquí: si create_chunk lanza, el asignador queda como estaba.
		if (chunk_index == m_chunks.size()) {
			m_chunks.push_back(create_chunk());
		}

		Chunk& chunk = m_chunks[chunk_index];
		std::byte* mapped = chunk.memory.mapped();
		if (mapped == nullptr) {
			throw std::logic_error("VulkanFrameAllocator: el bloque no está mapeado");
		}

		std::memcpy(mapped + offset, buffer_data.data(), size);

		m_current_chunk = chunk_index;
		m_cursor = offset + static_cast<uint32_t>(size);

		return {
			.slot_index = chunk.slot_index,
			.offset     = offset
		};
	}

} // namespace luxel::vulkan
