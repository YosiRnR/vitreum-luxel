#include <luxel/types.h>
#include <luxel/texture.h>
#include "vulkan_texture.h"
#include "vulkan_common.h"
#include "vulkan_device.h"
#include "vulkan_format.h"
#include "vulkan_staging_buffer.h"
#include <span>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <stdexcept>

namespace luxel::vulkan
{
	VulkanTexture::VulkanTexture(const Ref<VulkanDevice>& vk_device, const TextureDescriptor& texture_desc)
		: Texture(texture_desc.width, texture_desc.height, texture_desc.format, texture_desc.mip_levels, texture_desc.usage)
		, m_ref_device(vk_device)
		, m_bindless_table(&vk_device->bindless_table())
	{
		// Image
		vk::ImageCreateInfo image_info{
			.imageType     = vk::ImageType::e2D,
			.format        = to_vk_format(texture_desc.format),
			.extent        = { texture_desc.width, texture_desc.height, 1 },
			.mipLevels     = texture_desc.mip_levels,
			.arrayLayers   = 1,
			.samples       = vk::SampleCountFlagBits::e1,
			.tiling        = vk::ImageTiling::eOptimal,
			.usage         = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			.sharingMode   = vk::SharingMode::eExclusive,
			.initialLayout = vk::ImageLayout::eUndefined
		};
		m_image = vk::raii::Image(m_ref_device->logical_device(), image_info);

		// Memoria de la imagen
		vk::MemoryRequirements mem_requirements = m_image.getMemoryRequirements();
		vk::MemoryPropertyFlags properties      = vk::MemoryPropertyFlagBits::eDeviceLocal; // Memoria de GPU

		m_image_memory = m_ref_device->memory_allocator().allocate(mem_requirements, properties, ResourceKind::Image);
		m_image.bindMemory(m_image_memory.memory(), m_image_memory.offset());

		// ImageView
		vk::ImageViewCreateInfo view_info{
			.image    = *m_image,
			.viewType = vk::ImageViewType::e2D,
			.format   = to_vk_format(texture_desc.format),
			.subresourceRange = {
				.aspectMask     = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel   = 0,
				.levelCount     = texture_desc.mip_levels,
				.baseArrayLayer = 0,
				.layerCount     = 1
			}
		};
		m_view = vk::raii::ImageView(m_ref_device->logical_device(), view_info);

		upload(texture_desc.initial_data);

		// Entrada en el SlotAllocator de Texturas
		m_slot_index = m_bindless_table->allocate_texture_slot(*m_view);
	}

	void VulkanTexture::upload(std::span<const std::byte> data)
	{
		VulkanStagingBuffer staging(*m_ref_device, data.size());
		staging.write_data(data);

		m_ref_device->submit_immediate([this, &staging](vk::raii::CommandBuffer& command)
		{
			vk::ImageMemoryBarrier2 to_transfer{
				.srcStageMask  = vk::PipelineStageFlagBits2::eNone,
				.srcAccessMask = vk::AccessFlags2{},
				.dstStageMask  = vk::PipelineStageFlagBits2::eTransfer,
				.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
				.oldLayout     = vk::ImageLayout::eUndefined,
				.newLayout     = vk::ImageLayout::eTransferDstOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image            = *m_image,
				.subresourceRange = {
					.aspectMask     = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel   = 0,
					.levelCount     = 1,
					.baseArrayLayer = 0,
					.layerCount     = 1
				}
			};

			command.pipelineBarrier2(vk::DependencyInfo{
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers    = &to_transfer
			});

			command.copyBufferToImage(
				*staging.buffer(),
				*m_image,
				vk::ImageLayout::eTransferDstOptimal,
				vk::BufferImageCopy{
					.bufferOffset      = 0,
					.bufferRowLength   = 0,
					.bufferImageHeight = 0,
					.imageSubresource{
						.aspectMask     = vk::ImageAspectFlagBits::eColor,
						.mipLevel       = 0,
						.baseArrayLayer = 0,
						.layerCount     = 1
					},
					.imageOffset = { 0, 0, 0 },
					.imageExtent = { width(), height(), 1 }
				}
			);

			vk::ImageMemoryBarrier2 to_final{
				.srcStageMask  = vk::PipelineStageFlagBits2::eTransfer,
				.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
				.dstStageMask  = vk::PipelineStageFlagBits2::eFragmentShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
				.oldLayout     = vk::ImageLayout::eTransferDstOptimal,
				.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image            = *m_image,
				.subresourceRange = {
					.aspectMask     = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel   = 0,
					.levelCount     = 1,
					.baseArrayLayer = 0,
					.layerCount     = 1
				}
			};
			command.pipelineBarrier2(vk::DependencyInfo{
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers    = &to_final
			});
		});
	}

	VulkanTexture::~VulkanTexture()
	{
		if (m_slot_index > 0 && m_bindless_table) {
			m_bindless_table->free_texture_slot(m_slot_index);
		}
	}

	void VulkanTexture::on_zero_refs() const
	{
		auto* self = const_cast<VulkanTexture*>(this);

		// Sacamos el Ref del miembro a un local ANTES de encolar: ese local debe
		// ser lo ultimo que se destruye, porque si era la ultima referencia al
		// device, su destructor vaciara la cola y hara delete de "this".
		Ref<VulkanDevice> device = std::move(self->m_ref_device);
		device->enqueue_destroy(self);
	}

} // namespace luxel::vulkan