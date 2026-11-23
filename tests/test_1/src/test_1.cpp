#include <vitreum/vitreum.h>
#include <luxel/luxel.h>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <format>
#include <array>
#include <span>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "shader_vs_spv.h"
#include "shader_ps_spv.h"


static luxel::WindowHandle to_luxel(const vitreum::NativeWindowHandle& win_handle)
{
	luxel::WindowHandle result{};

	if (win_handle.platform == vitreum::NativePlatform::Win32)
	{
		result.platform = luxel::PlatformType::Windows;
		result.windows.hinstance = win_handle.win32.hinstance;
		result.windows.hwnd = win_handle.win32.hwnd;
	}
	else if (win_handle.platform == vitreum::NativePlatform::X11)
	{
		result.platform = luxel::PlatformType::Linux;
		result.linux.display = win_handle.x11.display;
		result.linux.window = win_handle.x11.window;
	}
	else
	{
	}

	return result;
}

static std::vector<std::byte> make_checkerboard(uint32_t width, uint32_t height, uint32_t tile_size)
{
	std::vector<std::byte> pixels(
		static_cast<size_t>(width) * height * 4);

	for (uint32_t y = 0; y < height; ++y)
	{
		for (uint32_t x = 0; x < width; ++x)
		{
			const bool light = ((x / tile_size) + (y / tile_size)) % 2 == 0;

			const std::byte color = light ? std::byte{ 0xE0 } : std::byte{ 0x20 };

			const size_t i = (static_cast<size_t>(y) * width + x) * 4;

			pixels[i + 0] = color;          // R
			pixels[i + 1] = color;          // G
			pixels[i + 2] = color;          // B
			pixels[i + 3] = std::byte{ 0xFF }; // A
		}
	}

	// Casilla amarilla en el píxel (0,0): permite comprobar la orientación de la
	// textura (debe verse arriba a la izquierda) y el orden de los canales.
	for (uint32_t y = 0; y < tile_size; ++y)
	{
		for (uint32_t x = 0; x < tile_size; ++x)
		{
			const size_t i = (static_cast<size_t>(y) * width + x) * 4;

			pixels[i + 0] = std::byte{ 0xFF }; // R
			pixels[i + 1] = std::byte{ 0xFF }; // G
			pixels[i + 2] = std::byte{ 0x00 }; // B
			pixels[i + 3] = std::byte{ 0xFF }; // A
		}
	}

	return pixels;
}

// Una ventana con su swapchain. El orden de los miembros importa: se destruyen
// en orden inverso a la declaración, así la swapchain muere antes que la ventana
// de la que depende (su surface se creó a partir del HWND).
struct RenderWindow
{
	vitreum::Window window{};
	luxel::Ref<luxel::Swapchain> swapchain{};
	luxel::ClearColor clear{ 0.0f, 0.0f, 0.0f, 1.0f };
	luxel::Camera2D camera{ 2.0f, 2.0f };   // Una por ventana: cada una tiene su propia relación de aspecto
	bool confirm_close{ false };        // Pregunta antes de cerrar (la ventana principal)
	luxel::Size2D last_extent{};        // Para avisar solo cuando cambia el tamaño de la swapchain
};

// Recursos que se dibujan en todas las ventanas.
struct Scene
{
	luxel::Ref<luxel::GraphicsPipeline> pipeline;
	luxel::Ref<luxel::Texture> texture;
	luxel::Ref<luxel::Sampler> sampler;
	luxel::Ref<luxel::Buffer> vertex_buffer;
	luxel::Ref<luxel::Buffer> index_buffer;
	uint32_t index_count{};
};

// vitreum::Window no se puede mover, por eso las ventanas viven en unique_ptr.
static std::unique_ptr<RenderWindow> make_render_window(luxel::Device& device, const char* title, vitreum::Size size,
                                                        luxel::ClearColor clear, bool confirm_close)
{
	auto render_window = std::make_unique<RenderWindow>();
	if (!render_window->window.create({ title, size })) {
		throw std::runtime_error("No se pudo crear la ventana");
	}

	render_window->swapchain = device.create_swapchain({
		.window = to_luxel(render_window->window.native_handle()),
		.size   = { static_cast<uint32_t>(render_window->window.width()), static_cast<uint32_t>(render_window->window.height()) },
		.vsync  = true
	});
	render_window->clear         = clear;
	render_window->confirm_close = confirm_close;
	return render_window;
}

static Scene create_scene(luxel::Device& device, luxel::Format color_format)
{
	Scene scene;

	// El pipeline se crea para un formato de color: sirve para las ventanas cuya
	// swapchain use ese mismo formato (aquí, todas).
	scene.pipeline = device.create_graphics_pipeline({
		.vs_byte_code = shader_vs_spv,
		.ps_byte_code = shader_ps_spv,
		.format       = color_format,
		.blend_mode   = luxel::BlendMode::Alpha
	});

	//constexpr uint32_t width  = 256;
	//constexpr uint32_t height = 256;
	//auto pixels = make_checkerboard(width, height, 32);
	int img_width{};
	int img_height{};
	int img_channels{};

	stbi_set_flip_vertically_on_load(0); // No volteamos verticalmente la imagen
	stbi_uc* data = stbi_load("D:/IriaGameResources/iriaBase01.png", &img_width, &img_height, &img_channels, 4);
	//stbi_uc* data = stbi_load("D:/IriaGameResources/IriaGame/assets/sprites/Iria-idle-trimmed.png", &img_width, &img_height, &img_channels, 4);
	if (!data){
		return{};
	}
	const size_t byte_count = static_cast<size_t>(img_width) * static_cast<size_t>(img_height) * 4;
	std::span<const std::byte> pixels{ reinterpret_cast<const std::byte*>(data), byte_count };

	const uint32_t width  = img_width;
	const uint32_t height = img_height;

	scene.texture = device.create_texture({
		.width        = width,
		.height       = height,
		.format       = luxel::Format::R8G8B8A8_SRGB,
		.usage        = luxel::TextureUsage::Sampled,
		.mip_levels   = 1,
		.initial_data = pixels
	});

	stbi_image_free(data);

	scene.sampler = device.create_sampler({});

	const std::array<float, 16> vertices{
		// posición      // UV
		-0.5f, -0.5f,    0.0f, 1.0f, // abajo izquierda
		 0.5f, -0.5f,    1.0f, 1.0f, // abajo derecha
		 0.5f,  0.5f,    1.0f, 0.0f, // arriba derecha
		-0.5f,  0.5f,    0.0f, 0.0f  // arriba izquierda
	};
	scene.vertex_buffer = device.create_buffer(luxel::BufferDescriptor::from_data(vertices, luxel::BufferUsage::Storage));

	const std::array<uint16_t, 6> indices{ 0, 1, 2, 0, 2, 3 };
	scene.index_buffer = device.create_buffer(luxel::BufferDescriptor::from_data(indices, luxel::BufferUsage::Index));
	scene.index_count  = static_cast<uint32_t>(indices.size());

	return scene;
}

// Consume los eventos de una ventana. Cada ventana trae su propia cola.
static void process_events(RenderWindow& render_window)
{
	auto& window = render_window.window;

	while (window.has_pending_events())
	{
		auto event = window.pop_event();
		if (!event) {
			continue;
		}

		if (event->type == vitreum::WinEventType::Close)
		{
			// WM_CLOSE ya dejó should_close a true. Si la ventana pide confirmación,
			// la respuesta del usuario decide si sigue o no.
			if (render_window.confirm_close) {
				window.cancel_close(
					vitreum::message_box_okcancel(window.native_handle(), "¿Desea cerrar la ventana?", "WARNING")
				);
			}
		}
		else if (event->type == vitreum::WinEventType::Resize)
		{
			render_window.swapchain->resize({ static_cast<uint32_t>(event->size.width), static_cast<uint32_t>(event->size.height) });
		}
	}
}

// Un frame completo en una ventana. Si no se puede pintar (swapchain obsoleta o
// recreándose) simplemente no hace nada: ya se volverá a intentar en la vuelta siguiente.
static void render_window_frame(luxel::Device& device, RenderWindow& render_window, const Scene& scene, float angle)
{
	auto* frame = device.begin_frame(*render_window.swapchain);
	if (frame == nullptr) {
		return;
	}

	// La cámara mide 2 unidades de alto y 2 * aspecto de ancho: así 1 unidad del
	// mundo mide lo mismo en horizontal que en vertical y el quad se ve cuadrado
	// sea cual sea el tamaño de la ventana. Se lee el tamaño después de begin_frame
	// porque la swapchain pudo recrearse al empezar el frame.
	const auto extent = render_window.swapchain->extent();
	const float aspect = extent.height > 0 ? static_cast<float>(extent.width) / static_cast<float>(extent.height) : 1.0f;
	render_window.camera.set_viewport_size(2.0f * aspect, 2.0f);

	// proyección * vista * modelo: el giro ocurre en unidades del mundo y la proyección lo ajusta a la ventana.
	const auto xform_matrix = render_window.camera.view_proj_matrix() * luxel::math::Matrix4x4::rotateZ(angle);
	auto buf_alloc = frame->allocate(std::as_bytes(std::span(xform_matrix.m)));
	const std::array<uint32_t, 5> push_constants{
		scene.vertex_buffer->bindless_index(), buf_alloc.slot_index, buf_alloc.offset,
		scene.texture->bindless_index(), scene.sampler->bindless_index()
	};

	auto& commands = frame->commands();
	commands.begin_rendering({ .clear = render_window.clear });
	commands.bind_constants(std::as_bytes(std::span(push_constants)));
	commands.bind_index_buffer(*scene.index_buffer, luxel::IndexType::Uint16);
	commands.bind_pipeline(*scene.pipeline);
	commands.draw_indexed(scene.index_count);
	commands.end_rendering();

	device.end_frame(*frame);

	const auto now = render_window.swapchain->extent();
	if (now.width != render_window.last_extent.width || now.height != render_window.last_extent.height) {
		std::cout << std::format("Swapchain: {} x {}\n", now.width, now.height);
		render_window.last_extent = now;
	}
}

int main(int argc, char** argv)
{
	auto render_device = luxel::create_device(luxel::DeviceDescriptor{
		.backend = luxel::Backend::Vulkan,
		.debug   = true
	});

	// La primera ventana es la principal (pide confirmación al cerrar).
	std::vector<std::unique_ptr<RenderWindow>> windows;
	windows.push_back(make_render_window(*render_device, "Vitreum Window",       { 1920, 1080 }, { 1.0f, 0.0f, 0.0f, 1.0f }, true));
	windows.push_back(make_render_window(*render_device, "Second Vitreum Window", {  800,  600 }, { 0.0f, 0.0f, 1.0f, 1.0f }, false));

	const auto color_format = windows.front()->swapchain->color_format();
	const auto extent       = windows.front()->swapchain->extent();
	std::cout << std::format("      Extent: {} x {}\n", extent.width, extent.height);
	std::cout << std::format("Color Format: {}\n", static_cast<uint8_t>(color_format));

	Scene scene = create_scene(*render_device, color_format);

	for (auto& render_window : windows) {
		render_window->window.show();
	}

	float angle{ 0.0f };

	// El programa termina cuando se cierra la última ventana.
	while (!windows.empty())
	{
		// Todas las ventanas comparten el hilo de mensajes; cada una entrega solo los suyos.
		for (auto& render_window : windows) {
			render_window->window.poll_events();
			process_events(*render_window);
		}

		// Ventanas cerradas: se destruyen aquí (swapchain primero, luego la ventana).
		std::erase_if(windows, [](const auto& render_window) { return render_window->window.should_close(); });
		if (windows.empty()) {
			break;
		}

		// Si todas están minimizadas no hay nada que pintar: dormir hasta el siguiente mensaje.
		const bool all_minimized = std::ranges::all_of(windows, [](const auto& render_window) {
			return render_window->window.is_minimized();
		});
		if (all_minimized) {
			windows.front()->window.wait_events();
			continue;
		}

		angle += 0.025f;
		for (auto& render_window : windows)
		{
			if (!render_window->window.is_minimized()) {
				render_window_frame(*render_device, *render_window, scene, angle);
			}
		}
	}

	render_device->wait_idle();
}
