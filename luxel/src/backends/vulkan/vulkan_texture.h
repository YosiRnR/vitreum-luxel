#pragma once

#include <luxel/types.h>
#include <luxel/texture.h>
#include "vulkan_common.h"
#include "vulkan_device.h"
#include <cstdint>
#include <cstddef>
#include <span>

namespace luxel::vulkan
{
	class VulkanBindlessTable;

	class VulkanTexture : public Texture
	{
	public:
		static Ref<Texture> create(const Ref<VulkanDevice>& vk_device, const TextureDescriptor& texture_desc) {
			return Ref<Texture>(new VulkanTexture(vk_device, texture_desc));
		}

		// Destructor
		~VulkanTexture() override;

		uint32_t bindless_index() const override { return m_slot_index; }

	private:
		VulkanTexture(const Ref<VulkanDevice>& vk_device, const TextureDescriptor& texture_desc);

		// Declarados en orden inverso de destrucción
		Ref<VulkanDevice> m_ref_device;
		vk::raii::DeviceMemory m_image_memory{ nullptr };
		vk::raii::Image m_image{ nullptr };
		vk::raii::ImageView m_view{ nullptr };
		uint32_t m_slot_index{};
		VulkanBindlessTable* m_bindless_table{ nullptr };

		void upload(std::span<const std::byte> data);
		void on_zero_refs() const override;
	};

} // namespace luxel::vulkan