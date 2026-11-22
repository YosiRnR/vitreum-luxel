#pragma once

#include <luxel/types.h>
#include <luxel/buffer.h>
#include "vulkan_common.h"
#include "vulkan_device.h"
#include "vulkan_memory_allocator.h"
#include <cstdint>

namespace luxel::vulkan
{
	class VulkanBindlessTable;

	class VulkanBuffer : public Buffer
	{
	public:
		static Ref<Buffer> create(const Ref<VulkanDevice>& vk_device, const BufferDescriptor& buffer_desc) {
			return Ref<Buffer>(new VulkanBuffer(vk_device, buffer_desc));
		}

		// Destructor
		~VulkanBuffer() override;

		uint32_t bindless_index() const override { return m_slot_index; }

		// Accesores públicos
		const vk::raii::Buffer& buffer() const { return m_buffer; }

	private:
		VulkanBuffer(const Ref<VulkanDevice>& vk_device, const BufferDescriptor&);

		// Declarados en orden inverso de destrucción
		Ref<VulkanDevice> m_ref_device;
		MemoryAllocation m_buffer_memory{};
		vk::raii::Buffer m_buffer{ nullptr };
		uint32_t m_slot_index{};
		VulkanBindlessTable* m_bindless_table{ nullptr };

		void on_zero_refs() const override;
	};

} // namespace luxel::vulkan
