#include "vulkan_bindless_table.h"
#include "vulkan_common.h"
#include <luxel/device.h>
#include "../../slot_allocator.h"
#include <stdexcept>
#include <utility>
#include <cstdint>
#include <array>

namespace luxel::vulkan
{
	VulkanBindlessTable::VulkanBindlessTable(const vk::raii::Device& device, const RequestedDeviceMaximums& dev_maximus)
        : m_device_ref(device)
        , m_storage_buffer_slots(dev_maximus.max_storage_buffers)
        , m_sampler_slots(dev_maximus.max_samplers)
        , m_texture_slots(dev_maximus.max_textures)
	{
        // DescriptorSet Layout
        std::array bindings{
            vk::DescriptorSetLayoutBinding{
                .binding         = k_binding_buffer_storage,
                .descriptorType  = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = dev_maximus.max_storage_buffers,
                .stageFlags      = vk::ShaderStageFlagBits::eAll
            },
            vk::DescriptorSetLayoutBinding{
                .binding         = k_binding_samplers,
                .descriptorType  = vk::DescriptorType::eSampler,
                .descriptorCount = dev_maximus.max_samplers,
                .stageFlags      = vk::ShaderStageFlagBits::eAll
            },
            vk::DescriptorSetLayoutBinding{
                .binding         = k_binding_textures,
                .descriptorType  = vk::DescriptorType::eSampledImage,
                .descriptorCount = dev_maximus.max_textures,
                .stageFlags      = vk::ShaderStageFlagBits::eAll
            }
        };

        auto flags = vk::DescriptorBindingFlagBits::ePartiallyBound  |
                     vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                     vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending;

        std::array binding_flags{ flags, flags, flags };

        vk::DescriptorSetLayoutBindingFlagsCreateInfo flags_info{
            .bindingCount  = static_cast<uint32_t>(binding_flags.size()),
            .pBindingFlags = binding_flags.data()
        };

        vk::DescriptorSetLayoutCreateInfo layout_info{
            .pNext        = &flags_info,
            .flags        = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings    = bindings.data()
        };

        m_bindless_layout = vk::raii::DescriptorSetLayout(device, layout_info);

        // Descriptor Pool
        std::array pool_sizes{
            vk::DescriptorPoolSize{ .type = vk::DescriptorType::eStorageBuffer, .descriptorCount = dev_maximus.max_storage_buffers },
            vk::DescriptorPoolSize{ .type = vk::DescriptorType::eSampler      , .descriptorCount = dev_maximus.max_samplers        },
            vk::DescriptorPoolSize{ .type = vk::DescriptorType::eSampledImage , .descriptorCount = dev_maximus.max_textures        }
        };

        vk::DescriptorPoolCreateInfo pool_info{
            .flags         = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets       = 1,
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes    = pool_sizes.data()
        };

        m_descriptor_pool = vk::raii::DescriptorPool(device, pool_info);

        // DescriptorSet
        vk::DescriptorSetAllocateInfo alloc_info{
            .descriptorPool     = *m_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &*m_bindless_layout,
        };
        auto descriptor_sets = vk::raii::DescriptorSets(device, alloc_info);
        m_descriptor_set     = std::move(descriptor_sets[0]);

        // Pipeline Layout
        vk::PipelineLayoutCreateInfo pipeline_layout_info{
            .setLayoutCount = 1,
            .pSetLayouts    = &*m_bindless_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &k_push_constant_range
        };

        m_pipeline_layout = vk::raii::PipelineLayout(device, pipeline_layout_info);
    }

    uint32_t VulkanBindlessTable::allocate_texture_slot(vk::ImageView image_view)
    {
        uint32_t slot = m_texture_slots.allocate();
        if (slot == 0) {
            throw std::runtime_error("Límite de texturas disponibles alcanzado");
        }

        vk::DescriptorImageInfo image_info{
            .imageView   = image_view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        vk::WriteDescriptorSet write_set{
            .dstSet          = *m_descriptor_set,
            .dstBinding      = k_binding_textures,
            .dstArrayElement = slot,
            .descriptorCount = 1,
            .descriptorType  = vk::DescriptorType::eSampledImage,
            .pImageInfo      = &image_info
        };

        m_device_ref.updateDescriptorSets(write_set, {});

        return slot;
    }

    uint32_t VulkanBindlessTable::allocate_sampler_slot(vk::Sampler sampler)
    {
        uint32_t slot = m_sampler_slots.allocate();
        if (slot == 0) {
            throw std::runtime_error("Límite de samplers disponibles alcanzado");
        }

        vk::DescriptorImageInfo image_info{
            .sampler = sampler
        };

        vk::WriteDescriptorSet write_set{
            .dstSet          = *m_descriptor_set,
            .dstBinding      = k_binding_samplers,
            .dstArrayElement = slot,
            .descriptorCount = 1,
            .descriptorType  = vk::DescriptorType::eSampler,
            .pImageInfo      = &image_info
        };

        m_device_ref.updateDescriptorSets(write_set, {});

        return slot;
    }

    uint32_t VulkanBindlessTable::allocate_buffer_slot(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range)
    {
        uint32_t slot = m_storage_buffer_slots.allocate();
        if (slot == 0) {
            throw std::runtime_error("Límite de buffers de almacenamiento disponibles alcanzado");
        }

        vk::DescriptorBufferInfo buffer_info{
            .buffer = buffer,
            .offset = offset,
            .range  = range
        };

        vk::WriteDescriptorSet write_set{
            .dstSet          = *m_descriptor_set,
            .dstBinding      = k_binding_buffer_storage,
            .dstArrayElement = slot,
            .descriptorCount = 1,
            .descriptorType  = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo     = &buffer_info
        };

        m_device_ref.updateDescriptorSets(write_set, {});

        return slot;
    }

    void VulkanBindlessTable::free_texture_slot(uint32_t slot) {
        m_texture_slots.free(slot);
    }

    void VulkanBindlessTable::free_sampler_slot(uint32_t slot) {
        m_sampler_slots.free(slot);
    }

    void VulkanBindlessTable::free_buffer_slot(uint32_t slot) {
        m_storage_buffer_slots.free(slot);
    }


} // namespace luxel::vulkan
