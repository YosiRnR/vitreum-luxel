#include "vulkan_buffer.h"
#include <luxel/types.h>
#include "vulkan_common.h"
#include "vulkan_device.h"
#include <luxel/buffer.h>
#include <stdexcept>
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
		vk::BufferCreateInfo buffer_info{
			.size        = buffer_desc.size,
			.usage       = to_vk_buffer_usage(buffer_desc.usage) | vk::BufferUsageFlagBits::eTransferDst,
			.sharingMode = vk::SharingMode::eExclusive
		};
		m_buffer = vk::raii::Buffer(m_ref_device->logical_device(), buffer_info);

		vk::MemoryRequirements mem_requirements = m_buffer.getMemoryRequirements();
		vk::PhysicalDeviceMemoryProperties mem_properties = m_ref_device->physical_device().getMemoryProperties();
		vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal; // Memoria de GPU
		uint32_t memory_type_index{ UINT32_MAX };
		for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
		{
			// vamos comprobando todos los bits a 1 de memoryTypeBits
			if ((mem_requirements.memoryTypeBits & (1u << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
				memory_type_index = i;
				break;
			}
		}
		if (memory_type_index == UINT32_MAX) {
			throw std::runtime_error("No se pudo encontrar memoria para el buffer");
		}
		vk::MemoryAllocateInfo alloc_info{
			.allocationSize  = mem_requirements.size,
			.memoryTypeIndex = memory_type_index
		};
		m_buffer_memory = vk::raii::DeviceMemory(m_ref_device->logical_device(), alloc_info);

		m_buffer.bindMemory(*m_buffer_memory, 0);

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
