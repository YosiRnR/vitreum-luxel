#include "vulkan_buffer.h"
#include <luxel/types.h>
#include "vulkan_common.h"
#include "vulkan_device.h"
#include "vulkan_staging_buffer.h"
#include <luxel/buffer.h>
#include <utility>
#include <cstdint>

namespace luxel::vulkan
{
	namespace
	{
		vk::BufferUsageFlags to_vk_buffer_usage(BufferUsage usage)
		{
			vk::BufferUsageFlags flags{};

			if (has_flag(usage, BufferUsage::Storage)) {
				flags |= vk::BufferUsageFlagBits::eStorageBuffer;
			}
			if (has_flag(usage, BufferUsage::Index)) {
				flags |= vk::BufferUsageFlagBits::eIndexBuffer;
			}

			return flags;
		}

	} // namespace

	VulkanBuffer::VulkanBuffer(const Ref<VulkanDevice>& vk_device, const BufferDescriptor& buffer_desc)
		: Buffer(buffer_desc.size, buffer_desc.usage)
		, m_ref_device(vk_device)
		, m_bindless_table(&vk_device->bindless_table())
	{
		// Buffer
		vk::BufferCreateInfo buffer_info{
			.size        = buffer_desc.size,
			.usage       = to_vk_buffer_usage(buffer_desc.usage) | vk::BufferUsageFlagBits::eTransferDst,
			.sharingMode = vk::SharingMode::eExclusive
		};
		m_buffer = vk::raii::Buffer(m_ref_device->logical_device(), buffer_info);

		// Memoria del buffer
		vk::MemoryRequirements mem_requirements = m_buffer.getMemoryRequirements();
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal; // Memoria de GPU

		m_buffer_memory = m_ref_device->memory_allocator().allocate(mem_requirements, properties, ResourceKind::Buffer);
		m_buffer.bindMemory(m_buffer_memory.memory(), m_buffer_memory.offset());

		if (buffer_desc.initial_data.size() > 0)
		{
			VulkanStagingBuffer staging(*m_ref_device, buffer_desc.initial_data.size());
			staging.write_data(buffer_desc.initial_data);
			m_ref_device->submit_immediate([&staging, &buffer_desc, this](vk::raii::CommandBuffer& command) {
				command.copyBuffer(*staging.buffer(), *m_buffer, vk::BufferCopy{ .srcOffset = 0, .dstOffset = 0, .size = buffer_desc.initial_data.size()});
			});
		}

		if (buffer_desc.initial_data.size() > VulkanMemoryAllocator::k_default_block_size) {
			m_ref_device->memory_allocator().trim();
		}

		// Entrada en el SlotAllocator de Buffers
		if (has_flag(buffer_desc.usage, BufferUsage::Storage)) {
			m_slot_index = m_bindless_table->allocate_buffer_slot(m_buffer, {}, vk::WholeSize);
		}
	}

	VulkanBuffer::~VulkanBuffer()
	{
		if (m_slot_index > 0 && m_bindless_table) {
			m_bindless_table->free_buffer_slot(m_slot_index);
		}
	}

	void VulkanBuffer::on_zero_refs() const
	{
		auto* self = const_cast<VulkanBuffer*>(this);

		// Sacamos el Ref del miembro a un local ANTES de encolar: ese local debe
		// ser lo ultimo que se destruye, porque si era la ultima referencia al
		// device, su destructor vaciara la cola y hara delete de "this".
		Ref<VulkanDevice> device = std::move(self->m_ref_device);
		device->enqueue_destroy(self);
	}

} // namespace luxel::vulkan
