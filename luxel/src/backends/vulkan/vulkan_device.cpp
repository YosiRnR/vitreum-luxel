#include <luxel/types.h>
#include <luxel/device.h>
#include <luxel/swapchain.h>
#include <luxel/graphics_pipeline.h>
#include <luxel/buffer.h>
#include <luxel/texture.h>
#include <luxel/sampler.h>
#include <luxel/frame.h>
#include "vulkan_common.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_graphics_pipeline.h"
#include "vulkan_buffer.h"
#include "vulkan_sampler.h"
#include "vulkan_texture.h"
#include "vulkan_platform.h"
#include "vulkan_frame.h"
#include "vulkan_physical_device.h"
#include "vulkan_bindless_table.h"
#include "vulkan_frame_allocator.h"
#include <set>
#include <ranges>
#include <vector>
#include <format>
#include <string>
#include <memory>
#include <cstring>
#include <cstdint>
#include <utility>
#include <optional>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <functional>
#include <string_view>
#include <type_traits>

namespace luxel::vulkan
{
	namespace
	{
		template<typename Properties>
		std::vector<const char*> filter_supported(std::vector<const char*> const& requested, std::vector<Properties> const& available)
		{
			auto filtered = requested | std::views::filter([&available](const char* req)
				{
					return std::ranges::any_of(available, [req](const Properties& avail)
						{
							if constexpr (std::is_same_v<Properties, vk::LayerProperties>) {
								return std::strcmp(req, avail.layerName) == 0;
							}
							else if constexpr (std::is_same_v<Properties, vk::ExtensionProperties>) {
								return std::strcmp(req, avail.extensionName) == 0;
							}
							else {
								static_assert(std::is_same_v<Properties, vk::LayerProperties> ||
									          std::is_same_v<Properties, vk::ExtensionProperties>,
									"Unsupported Properties type");
							}
						});
				});

			return std::vector<const char*>(filtered.begin(), filtered.end());
		}

	} // namespace


	VulkanDevice::VulkanDevice(const DeviceDescriptor& device_desc)
	{
		// Creamos el objeto con el constructor por defecto para que ponga los valores por defecto
		// para app_name, app_version, engine_name, engine_version y api_version
		// aunque algunos de estos campos deberían ser seleccionables
		InstanceConfig config{};

		config.enable_validation_layers = device_desc.debug;

		m_deletion_queue.init();
		m_dev_maximus = device_desc.device_maximus;

		create_instance(config);
		setup_debug_messenger();
		pick_physical_device();
		create_logical_device();

		// Command Pool tipo eTransient para crear CommandBuffers de corto tiempo de vida, del tipo eOneTimeSubmit
		vk::CommandPoolCreateInfo transient_pool_info{
			.flags            = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = m_graphics_queue_family_index
		};
		m_transient_command_pool = vk::raii::CommandPool(m_logical_device, transient_pool_info);
		
		// Podría ir en una función pero por simplificar ahora mismo lo dejo así
		m_bindless_table = std::make_unique<VulkanBindlessTable>(m_logical_device, m_dev_maximus);

		create_frames();
	}

	VulkanDevice::~VulkanDevice()
	{
		m_logical_device.waitIdle();
		m_deletion_queue.flush_all();
	}

	Ref<Swapchain> VulkanDevice::create_swapchain(const SwapchainDescriptor& swapchain_desc) {
		return VulkanSwapchain::create_ref(Ref<VulkanDevice>(this), swapchain_desc);
	}

	Ref<GraphicsPipeline> VulkanDevice::create_graphics_pipeline(const GraphicsPipelineDescriptor& pipeline_desc)
	{
		if (pipeline_desc.format == Format::Undefined) {
			throw std::runtime_error("Es necesario establecer un formato de color para poder crear el pipeline. Usa el mismo que para el swapchain");
		}
		if (pipeline_desc.vs_byte_code.empty() || pipeline_desc.ps_byte_code.empty()) {
			throw std::runtime_error("Es necesario tener un Vertex Shader y un Pixel/Fragment Shader para poder crear el pipeline");
		}
		return VulkanGraphicsPipeline::create(Ref<VulkanDevice>(this), pipeline_desc);
	}

	Frame* VulkanDevice::begin_frame(Swapchain& swapchain)
	{
		VulkanSwapchain& vk_swapchain = static_cast<VulkanSwapchain&>(swapchain);

		auto result = m_logical_device.waitForFences(*m_frames_resources[m_frame_index].in_flight, vk::True, UINT64_MAX);
		if (result != vk::Result::eSuccess) {
			throw std::runtime_error("Fatal error esperando la valla del frame en vuelo");
		}

		if (vk_swapchain.needs_recreate() && !vk_swapchain.recreate()) {
			return nullptr;
		}

		m_active_frame_index = m_frame_index;
		m_deletion_queue.flush(m_frame_index);

		auto index = vk_swapchain.acquire(m_frames_resources[m_frame_index].image_available);
		if (index == std::nullopt) {
			return nullptr;
		}

		m_logical_device.resetFences(*m_frames_resources[m_frame_index].in_flight);

		auto& frame_res = m_frames_resources[m_frame_index];
		frame_res.frame_allocator->reset_cursor();
		frame_res.command_pool.reset();
		frame_res.command_buffer.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		vk::ImageMemoryBarrier2 barrier{
			.srcStageMask     = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask    = {},
			.dstStageMask     = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask    = vk::AccessFlagBits2::eColorAttachmentWrite,
			.oldLayout        = vk::ImageLayout::eUndefined,
			.newLayout        = vk::ImageLayout::eColorAttachmentOptimal,
			.image            = vk_swapchain.image(index.value()),
			.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
		};

		vk::DependencyInfo dep{ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
		frame_res.command_buffer.pipelineBarrier2(dep);

		// Enlace del Set
		frame_res.command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			*m_bindless_table->pipeline_layout(),
			0,
			*m_bindless_table->descriptor_set(),
			nullptr
		);

		m_frame.reset(
			*frame_res.frame_allocator,
			frame_res.command_buffer,
			vk_swapchain.image(index.value()),
			vk_swapchain.image_view(index.value()),
			vk_swapchain.vk_extent(),
			vk_swapchain,
			index.value(),
			*m_bindless_table->pipeline_layout()
		);

		return &m_frame;
	}

	void VulkanDevice::end_frame(Frame& frame)
	{
		auto& vk_frame = static_cast<VulkanFrame&>(frame);

		auto& frame_res = m_frames_resources[m_frame_index];

		vk::ImageMemoryBarrier2 barrier{
			.srcStageMask     = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask    = vk::AccessFlagBits2::eColorAttachmentWrite,
			.dstStageMask     = {},
			.dstAccessMask    = {},
			.oldLayout        = vk::ImageLayout::eColorAttachmentOptimal,
			.newLayout        = vk::ImageLayout::ePresentSrcKHR,
			.image            = vk_frame.swapchain().image(vk_frame.image_index()),
			.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
		};

		vk::DependencyInfo dep{ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
		frame_res.command_buffer.pipelineBarrier2(dep);

		frame_res.command_buffer.end();

		vk::SemaphoreSubmitInfo wait_info{
			.semaphore = *frame_res.image_available,
			.stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
		};

		vk::SemaphoreSubmitInfo signal_info{
			.semaphore = *vk_frame.swapchain().render_finished(vk_frame.image_index()),
			.stageMask = vk::PipelineStageFlagBits2::eAllCommands
		};

		vk::CommandBufferSubmitInfo cmd_info{
			.commandBuffer = *frame_res.command_buffer
		};

		vk::SubmitInfo2 submit_info{
			.waitSemaphoreInfoCount   = 1, .pWaitSemaphoreInfos   = &wait_info,
			.commandBufferInfoCount   = 1, .pCommandBufferInfos   = &cmd_info,
			.signalSemaphoreInfoCount = 1, .pSignalSemaphoreInfos = &signal_info
		};
		m_graphics_queue.submit2(submit_info, *frame_res.in_flight);

		vk_frame.swapchain().present(m_present_queue, vk_frame.image_index());

		m_frame_index = (m_frame_index + 1) % k_frames_in_flight;
	}

	void VulkanDevice::wait_idle() {
		m_logical_device.waitIdle();
	}

	Ref<Buffer> VulkanDevice::create_buffer(const BufferDescriptor& buffer_desc)
	{
		if (buffer_desc.size <= 0 || buffer_desc.usage == BufferUsage::None || buffer_desc.initial_data.size() > buffer_desc.size) {
			throw std::runtime_error("Parámetros de creación del buffer incorrectos"); // Podría dividir los IFs para dar un mensaje más preciso
		}
		return VulkanBuffer::create(Ref<VulkanDevice>(this), buffer_desc);
	}

	Ref<Sampler> VulkanDevice::create_sampler(const SamplerDescriptor& sampler_desc)
	{
		return VulkanSampler::create(Ref<VulkanDevice>(this), sampler_desc);
	}

	Ref<Texture> VulkanDevice::create_texture(const TextureDescriptor& texture_desc)
	{
		uint8_t bpp = format_bytes_per_pixel(texture_desc.format);

		if (texture_desc.width == 0 || texture_desc.height == 0 || bpp == 0) {
			throw std::runtime_error("El tamaño de la textura no puede ser cero");
		}
		if (texture_desc.usage != TextureUsage::Sampled) {
			throw std::runtime_error("Solo soportado el modo de creación de texturas 'Sampled' por ahora");
		}
		if (texture_desc.mip_levels != 1) {
			throw std::runtime_error("Solo soportado número de mip levels a 1 por ahora");
		}
		if (texture_desc.initial_data.size() <= 0 || texture_desc.initial_data.size() != static_cast<size_t>(texture_desc.width) * texture_desc.height * bpp) {
			throw std::runtime_error("El tamaño de la imagen de la textura no coincide con los datos informados");
		}
		return VulkanTexture::create(Ref<VulkanDevice>(this), texture_desc);
	}

	uint32_t VulkanDevice::find_memory_type(uint32_t type_bits, vk::MemoryPropertyFlags properties) const
	{
		uint32_t mem_type_index{ UINT32_MAX };
		vk::PhysicalDeviceMemoryProperties mem_properties = m_physical_device.getMemoryProperties();

		for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
		{
			// Vamos comprobando todos los bits a 1 de memoryTypeBits
			if ((type_bits & (1u << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
				mem_type_index = i;
				break;
			}
		}

		if (mem_type_index == UINT32_MAX) {
			throw std::runtime_error("No se pudo encontrar memoria para el buffer");
		}

		return mem_type_index;
	}

	void VulkanDevice::submit_immediate(std::function<void(vk::raii::CommandBuffer&)> recorder)
	{
		vk::CommandBufferAllocateInfo alloc_info{
			.commandPool        = *m_transient_command_pool,
			.level              = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1
		};
		vk::raii::CommandBuffer command_buffer = std::move(vk::raii::CommandBuffers(m_logical_device, alloc_info).front());

		command_buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		recorder(command_buffer);
		command_buffer.end();

		vk::SubmitInfo submit_info{
			.commandBufferCount = 1,
			.pCommandBuffers    = &*command_buffer
		};
		m_graphics_queue.submit(submit_info, nullptr);
		m_graphics_queue.waitIdle();
	}

	vk::DebugUtilsMessengerCreateInfoEXT VulkanDevice::debug_utils_create_info()
	{
		using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
		using enum vk::DebugUtilsMessageTypeFlagBitsEXT;

		vk::DebugUtilsMessengerCreateInfoEXT debug_create_info{
			.messageSeverity = /*eVerbose |*/ eWarning | eError,
			.messageType     = eGeneral | eValidation | ePerformance,
			.pfnUserCallback = debug_callback,
			.pUserData       = this
		};
		return debug_create_info;
	}

	void VulkanDevice::create_instance(const InstanceConfig& config)
	{
		if (config.api_version < vk::ApiVersion13) {
			throw std::runtime_error(std::format("Required minimum API 1.3 for dynamic rendering, selected is: {}", config.api_version));
		}

		std::vector<char const*> enabled_layers{};
		std::vector<char const*> enabled_extensions{};

		auto layer_properties = m_context.enumerateInstanceLayerProperties();
		auto extension_properties = m_context.enumerateInstanceExtensionProperties();

		// Filtrar las capas de validación requeridas según las capas disponibles
		if (config.enable_validation_layers) {
			enabled_layers = filter_supported<vk::LayerProperties>({ "VK_LAYER_KHRONOS_validation" }, layer_properties);
		}
		m_enable_validation_layers = !enabled_layers.empty();

		// Filtrar las extensiones requeridas según las extensiones disponibles
		std::vector<char const*> required_extensions = platform::get_platform_instance_extensions();
		if (m_enable_validation_layers) {
			required_extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		enabled_extensions = filter_supported<vk::ExtensionProperties>(required_extensions, extension_properties);
		if (enabled_extensions.size() != required_extensions.size())
		{
			std::vector<const char*> missing_extensions;
			for (const char* req_ext : required_extensions)
			{
				if (std::find(enabled_extensions.begin(), enabled_extensions.end(), req_ext) == enabled_extensions.end()) {
					missing_extensions.push_back(req_ext);
				}
			}

			auto join = [](const std::vector<const char*>& names, std::string_view delimiter)
				{
					std::string result;
					for (const auto* name : names)
					{
						if (!result.empty()) {
							result += delimiter;
						}
						result += name;
					}
					return result;
				};
			throw std::runtime_error(std::format("Missing required Vulkan extensions: {}", join(missing_extensions, ", ")));
		}

		// Crear la instancia de Vulkan con las capas y extensiones habilitadas
		vk::ApplicationInfo app_info{
			.pApplicationName   = config.app_name.c_str(),
			.applicationVersion = config.app_version,
			.pEngineName        = config.engine_name.c_str(),
			.engineVersion      = config.engine_version,
			.apiVersion         = config.api_version
		};

		auto debug_messenger_create_info = debug_utils_create_info();

		vk::InstanceCreateInfo create_info{
			.pNext                   = m_enable_validation_layers ? &debug_messenger_create_info : nullptr,
			.pApplicationInfo        = &app_info,
			.enabledLayerCount       = static_cast<uint32_t>(enabled_layers.size()),
			.ppEnabledLayerNames     = enabled_layers.data(),
			.enabledExtensionCount   = static_cast<uint32_t>(enabled_extensions.size()),
			.ppEnabledExtensionNames = enabled_extensions.data()
		};
		m_instance = vk::raii::Instance(m_context, create_info);
	}

	void VulkanDevice::setup_debug_messenger()
	{
		if (!m_enable_validation_layers) {
			return;
		}

		auto debug_messenger_create_info = debug_utils_create_info();
		m_debug_messenger = vk::raii::DebugUtilsMessengerEXT(m_instance, debug_messenger_create_info);
	}

	void VulkanDevice::pick_physical_device()
	{
		auto best_device = select_physical_device(m_instance, m_dev_maximus);

		// Guardamos los índices de las familias de colas del dispositivo elegido.
		// Son necesarios para crear el dispositivo lógico y el command pool.
		m_graphics_queue_family_index = best_device.indices.graphics_family.value();
		m_present_queue_family_index  = best_device.indices.present_family.value();

		m_physical_device = std::move(best_device.device);

		// Opcional, mostrar info por consola sobre el dispositivo físico seleccionado
		print_physical_device_info(m_physical_device);
	}

	void VulkanDevice::create_logical_device()
	{
		std::set<uint32_t> unique_queue_families = { m_graphics_queue_family_index, m_present_queue_family_index };
		std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
		float queue_priority = 1.0f;

		for (uint32_t queue_family : unique_queue_families)
		{
			queue_create_infos.push_back(vk::DeviceQueueCreateInfo{
				.queueFamilyIndex = queue_family,
				.queueCount       = 1,
				.pQueuePriorities = &queue_priority
			});
		}

		auto device_features_chain = make_required_features();

		vk::DeviceCreateInfo create_info{
			.pNext                   = &device_features_chain.get<vk::PhysicalDeviceFeatures2>(),
			.queueCreateInfoCount    = static_cast<uint32_t>(queue_create_infos.size()),
			.pQueueCreateInfos       = queue_create_infos.data(),
			.enabledExtensionCount   = static_cast<uint32_t>(k_required_device_extensions.size()),
			.ppEnabledExtensionNames = k_required_device_extensions.data(),
			.pEnabledFeatures        = nullptr // Usamos features extendidas vía pNext
		};

		m_logical_device = vk::raii::Device(m_physical_device, create_info);

		// Crear las colas de gráficos y presentación
		m_graphics_queue = vk::raii::Queue(m_logical_device, m_graphics_queue_family_index, 0);

		if (m_graphics_queue_family_index == m_present_queue_family_index) {
			m_present_queue = m_graphics_queue;
		}
		else {
			m_present_queue = vk::raii::Queue(m_logical_device, m_present_queue_family_index, 0);
		}
	}

	void VulkanDevice::create_frames()
	{
		for (auto& frame : m_frames_resources)
		{
			vk::FenceCreateInfo fence_create_info{
				.flags = vk::FenceCreateFlagBits::eSignaled
			};
			
			vk::SemaphoreCreateInfo semaphore_create_info{};

			vk::CommandPoolCreateInfo cmd_pool_create_info{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = m_graphics_queue_family_index
			};

			frame.command_pool = vk::raii::CommandPool(m_logical_device, cmd_pool_create_info);

			vk::CommandBufferAllocateInfo alloc_info{
				.commandPool        = *frame.command_pool,
				.level              = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = 1
			};
			vk::raii::CommandBuffers temp_buffers(m_logical_device, alloc_info);
			frame.command_buffer = std::move(temp_buffers[0]);

			frame.image_available = vk::raii::Semaphore(m_logical_device, semaphore_create_info);
			frame.in_flight       = vk::raii::Fence(m_logical_device, fence_create_info);

			frame.frame_allocator = std::make_unique<VulkanFrameAllocator>(*this);
		}
	}

	VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDevice::debug_callback
	(
		vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
		vk::DebugUtilsMessageTypeFlagsEXT message_type,
		const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
		void* user_data // NOSONAR - void* user_data es requerido así por la API de Vulkan, aunque no lo usaremos aquí.
	)
	{
		std::string severity;
		if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
			severity = "VERBOSE";
		}
		else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
			severity = "INFO";
		}
		else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
			severity = "WARNING";
		}
		else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
			severity = "ERROR";
		}
		std::string type;
		if (message_type & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral) {
			type = "GENERAL";
		}
		else if (message_type & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation) {
			type = "VALIDATION";
		}
		else if (message_type & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance) {
			type = "PERFORMANCE";
		}
		std::string message(callback_data->pMessage);
		std::cerr << std::format("[Vulkan][{}][{}] {}\n", severity, type, message);

		return vk::False; // Indica que no se debe interrumpir la ejecución del programa
	}


} // namespace luxel::vulkan
