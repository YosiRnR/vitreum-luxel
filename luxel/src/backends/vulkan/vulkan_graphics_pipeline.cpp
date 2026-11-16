#include "vulkan_graphics_pipeline.h"
#include "vulkan_format.h"
#include <luxel/device.h>
#include <utility>
#include <cstdint>
#include <string>
#include <array>
#include <span>

namespace luxel::vulkan
{
	namespace
	{
		vk::PrimitiveTopology to_vk_topology(PrimitiveTopology topology)
		{
			switch (topology)
			{
			case PrimitiveTopology::TriangleStrip: return vk::PrimitiveTopology::eTriangleStrip;
			case PrimitiveTopology::LineList:      return vk::PrimitiveTopology::eLineList;
			case PrimitiveTopology::LineStrip:     return vk::PrimitiveTopology::eLineStrip;
			case PrimitiveTopology::PointList:     return vk::PrimitiveTopology::ePointList;
			case PrimitiveTopology::TriangleList:
			default:                               return vk::PrimitiveTopology::eTriangleList;
			}
		}

		vk::raii::ShaderModule build_shader_module(const Ref<VulkanDevice>& vk_device, std::span<const uint32_t> byte_code)
		{
			vk::ShaderModuleCreateInfo vs_shader_info{
				.codeSize = byte_code.size() * 4, // Tamaño en bytes
				.pCode    = byte_code.data()
			};
			return vk::raii::ShaderModule(vk_device->logical_device(), vs_shader_info);
		}

	} // namespace

	VulkanGraphicsPipeline::VulkanGraphicsPipeline(const Ref<VulkanDevice>& vk_device, const GraphicsPipelineDescriptor& desc)
		: m_ref_device(vk_device)
	{
		// Shader Modules
		auto vs_module = build_shader_module(vk_device, desc.vs_byte_code);
		auto ps_module = build_shader_module(vk_device, desc.ps_byte_code);
		std::string vs_entry_name{ desc.vs_entry_name };
		std::string ps_entry_name{ desc.ps_entry_name };

		// Shader Stages
		const vk::PipelineShaderStageCreateInfo stages[] = {
			vk::PipelineShaderStageCreateInfo{
				.stage = vk::ShaderStageFlagBits::eVertex,
				.module = vs_module,
				.pName  = vs_entry_name.c_str()
			},
			vk::PipelineShaderStageCreateInfo{
				.stage = vk::ShaderStageFlagBits::eFragment,
				.module = ps_module,
				.pName  = ps_entry_name.c_str()
			}
		};

		// Vertex State
		vk::PipelineVertexInputStateCreateInfo vertex_input_info{
		};

		// Input Assembly
		vk::PipelineInputAssemblyStateCreateInfo input_assembly{
			.topology               = to_vk_topology(desc.topology),
			.primitiveRestartEnable = vk::False
		};

		// Viewport, scissor y dinámicos: no dependen del tamaño del swapchain.
		vk::PipelineViewportStateCreateInfo viewport_state{
			.viewportCount = 1,
			.scissorCount  = 1
		};
		const std::array dynamic_states{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};
		vk::PipelineDynamicStateCreateInfo dynamic_state{
			.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
			.pDynamicStates    = dynamic_states.data()
		};

		// Rasterización
		vk::PipelineRasterizationStateCreateInfo rasterization{
			.depthClampEnable        = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode             = vk::PolygonMode::eFill,
			.cullMode                = vk::CullModeFlagBits::eNone,
			.frontFace               = vk::FrontFace::eClockwise,
			.depthBiasEnable         = vk::False,
			.lineWidth               = 1.0f
		};
		
		// Multisampling
		vk::PipelineMultisampleStateCreateInfo multisampling{
			.sampleShadingEnable = vk::False
		};

		// Color blend
		vk::PipelineColorBlendAttachmentState color_blend_attachment{
			.blendEnable         = vk::False,
			//.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
			//.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
			//.colorBlendOp        = vk::BlendOp::eAdd,
			//.srcAlphaBlendFactor = vk::BlendFactor::eOne,
			//.dstAlphaBlendFactor = vk::BlendFactor::eZero,
			//.alphaBlendOp        = vk::BlendOp::eAdd,
			.colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
								   vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		};

		vk::PipelineColorBlendStateCreateInfo color_blend{
			.logicOpEnable   = vk::False,
			.attachmentCount = 1,
			.pAttachments    = &color_blend_attachment
		};

		// Pipeline Rendering
		const vk::Format color_format = to_vk_format(desc.format);
		vk::PipelineRenderingCreateInfo rendering_info{
			.colorAttachmentCount    = 1,
			.pColorAttachmentFormats = &color_format
		};

		// Graphics Pipeline
		vk::GraphicsPipelineCreateInfo pipeline_info{
			.pNext      = &rendering_info,
			.stageCount = 2,
			.pStages    = stages,
			.pVertexInputState   = &vertex_input_info,
			.pInputAssemblyState = &input_assembly,
			.pViewportState      = &viewport_state,
			.pRasterizationState = &rasterization,
			.pMultisampleState   = &multisampling,
			.pColorBlendState    = &color_blend,
			.pDynamicState       = &dynamic_state,
			.layout              = *m_ref_device->bindless_table().pipeline_layout()
		};
		m_pipeline = vk::raii::Pipeline(m_ref_device->logical_device(), nullptr, pipeline_info);
	}

	void VulkanGraphicsPipeline::on_zero_refs() const
	{
		auto* self = const_cast<VulkanGraphicsPipeline*>(this);

		// Sacamos el Ref del miembro a un local ANTES de encolar: ese local debe
		// ser lo ultimo que se destruye, porque si era la ultima referencia al
		// device, su destructor vaciara la cola y hara delete de "this".
		luxel::Ref<luxel::Device> device = std::move(self->m_ref_device);
		static_cast<luxel::vulkan::VulkanDevice*>(device.get())->enqueue_destroy(self);
	}

} // namespace luxel::vulkan
