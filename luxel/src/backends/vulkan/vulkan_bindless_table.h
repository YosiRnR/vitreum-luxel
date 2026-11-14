#pragma once

#include "vulkan_common.h"
#include <luxel/device.h>
#include "../../slot_allocator.h"
#include <cstdint>

namespace luxel::vulkan
{
	constexpr uint32_t k_binding_buffer_storage = 0;
	constexpr uint32_t k_binding_samplers = 1;
	constexpr uint32_t k_binding_textures = 2;

	class VulkanBindlessTable
	{
	public:
		static constexpr vk::PushConstantRange k_push_constant_range{
			.stageFlags = vk::ShaderStageFlagBits::eAll,
			.offset = 0,
			.size = 128 // 128 bytes garantizados por Vulkan
		};

		VulkanBindlessTable(const vk::raii::Device& device, const RequestedDeviceMaximums& dev_maximus);

		uint32_t allocate_texture_slot(vk::ImageView image);
		uint32_t allocate_sampler_slot(vk::Sampler sampler);
		uint32_t allocate_buffer_slot(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range);

		void free_texture_slot(uint32_t slot);
		void free_sampler_slot(uint32_t slot);
		void free_buffer_slot(uint32_t slot);

		// Accesores públicos
		const vk::raii::DescriptorSetLayout& descriptor_layout() const { return m_bindless_layout; }
		const vk::raii::DescriptorSet& descriptor_set() const { return m_descriptor_set; }
		const vk::raii::PipelineLayout& pipeline_layout() const { return m_pipeline_layout; }

	private:
		vk::raii::DescriptorSetLayout m_bindless_layout{ nullptr };
		vk::raii::DescriptorPool m_descriptor_pool{ nullptr };
		vk::raii::DescriptorSet m_descriptor_set{ nullptr };

		const vk::raii::Device& m_device_ref;

		luxel::SlotAllocator m_storage_buffer_slots;
		luxel::SlotAllocator m_sampler_slots;
		luxel::SlotAllocator m_texture_slots;

		vk::raii::PipelineLayout m_pipeline_layout{ nullptr };
	};

} // namespace luxel::vulkan
