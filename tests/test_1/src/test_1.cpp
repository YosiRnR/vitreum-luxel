#include <vitreum/vitreum.h>
#include <luxel/luxel.h>
#include <luxel/renderer2d.h>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <cmath>
#include <string>
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
// Qué se dibuja en cada ventana.
enum class SceneKind
{
	Stress,        // 100x100 quads girando: mide el coste en CPU de un lote grande
	SpriteSheet    // hoja de sprites: recortes de UV, animación y Nearest vs Linear
};

// Medidas para el título de la ventana de estrés (se promedian y se muestran
// cada medio segundo: actualizar el título en cada frame sería ilegible).
struct FrameStats
{
	std::chrono::steady_clock::time_point window_start{ std::chrono::steady_clock::now() };
	double cpu_ms_accum{ 0.0 };   // suma del tiempo de CPU de construir y enviar el lote
	uint32_t frames{ 0 };
};

struct RenderWindow
{
	vitreum::Window window{};
	luxel::Ref<luxel::Swapchain> swapchain{};
	luxel::ClearColor clear{ 0.0f, 0.0f, 0.0f, 1.0f };
	luxel::Camera2D camera{ 2.0f, 2.0f };   // Una por ventana: cada una tiene su propia relación de aspecto
	bool confirm_close{ false };        // Pregunta antes de cerrar (la ventana principal)
	luxel::Size2D last_extent{};        // Para avisar solo cuando cambia el tamaño de la swapchain
	SceneKind kind{ SceneKind::Stress };
	std::string base_title;             // el título sin las estadísticas
	FrameStats stats{};
};

// Recursos que se dibujan en todas las ventanas. El pipeline, los shaders y el
// sampler lineal ya no son cosa de la aplicación: los pone el Renderer2D.
struct Scene
{
	luxel::Ref<luxel::Texture> texture;         // puede quedar vacía si no se encuentra el PNG
	float texture_aspect{ 1.0f };               // ancho / alto de la imagen, para no deformarla

	luxel::Ref<luxel::Texture> atlas;           // hoja de sprites generada a mano (ver make_sprite_sheet)
	luxel::Ref<luxel::Sampler> nearest_sampler; // para pixel art: sin filtrar, píxeles nítidos
};

// Hoja de sprites de prueba: 4x4 celdas de 8x8 píxeles (textura de 32x32).
// Cada celda es un "fotograma" de una animación: un fondo de color propio con
// borde oscuro y una barra blanca vertical que avanza una columna por celda.
// Al recorrer las celdas en orden, la barra barre la celda de izquierda a
// derecha (y vuelve). Las celdas se numeran por filas desde arriba a la
// izquierda, igual que las UV de luxel.
constexpr uint32_t k_sheet_cells_per_row = 4;
constexpr uint32_t k_sheet_cell_size     = 8;
constexpr uint32_t k_sheet_frames        = k_sheet_cells_per_row * k_sheet_cells_per_row;

static std::vector<std::byte> make_sprite_sheet()
{
	constexpr uint32_t size = k_sheet_cells_per_row * k_sheet_cell_size;   // 32
	std::vector<std::byte> pixels(static_cast<size_t>(size) * size * 4);

	for (uint32_t y = 0; y < size; ++y)
	{
		for (uint32_t x = 0; x < size; ++x)
		{
			const uint32_t cell = (y / k_sheet_cell_size) * k_sheet_cells_per_row + (x / k_sheet_cell_size);
			const uint32_t cx = x % k_sheet_cell_size;   // coordenada dentro de la celda
			const uint32_t cy = y % k_sheet_cell_size;

			// Color de fondo distinto por celda (varía R y B con el número de celda).
			uint8_t r = static_cast<uint8_t>(60 + cell * 12);
			uint8_t g = 90;
			uint8_t b = static_cast<uint8_t>(240 - cell * 12);

			const bool border = cx == 0 || cy == 0 || cx == k_sheet_cell_size - 1 || cy == k_sheet_cell_size - 1;
			// La barra recorre las columnas interiores 1..6 de ida y de vuelta.
			const uint32_t phase = cell % 12;
			const uint32_t bar_x = 1 + (phase < 6 ? phase : 11 - phase);

			if (border)          { r = 20;  g = 20;  b = 30; }
			else if (cx == bar_x) { r = 255; g = 255; b = 255; }

			const size_t i = (static_cast<size_t>(y) * size + x) * 4;
			pixels[i + 0] = std::byte{ r };
			pixels[i + 1] = std::byte{ g };
			pixels[i + 2] = std::byte{ b };
			pixels[i + 3] = std::byte{ 0xFF };
		}
	}
	return pixels;
}

// UV de la celda n de la hoja: (columna, fila) -> rectángulo [uv_min, uv_max].
// Es el "recorte" de la textura que se pasa en el Quad.
static void sprite_sheet_uv(uint32_t cell, luxel::math::Vector2f& uv_min, luxel::math::Vector2f& uv_max)
{
	const float step = 1.0f / static_cast<float>(k_sheet_cells_per_row);
	const float col  = static_cast<float>(cell % k_sheet_cells_per_row);
	const float row  = static_cast<float>(cell / k_sheet_cells_per_row);
	uv_min = { col * step, row * step };
	uv_max = { (col + 1.0f) * step, (row + 1.0f) * step };
}

// vitreum::Window no se puede mover, por eso las ventanas viven en unique_ptr.
static std::unique_ptr<RenderWindow> make_render_window(luxel::Device& device, const char* title, vitreum::Size size,
                                                        luxel::ClearColor clear, bool confirm_close, SceneKind kind)
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
	render_window->kind          = kind;
	render_window->base_title    = title;
	return render_window;
}

static Scene create_scene(luxel::Device& device)
{
	Scene scene;

	// Hoja de sprites (generada, no sale de disco) y su sampler Nearest. Van
	// antes que el PNG para que existan aunque el PNG no se encuentre.
	const auto sheet_pixels = make_sprite_sheet();
	constexpr uint32_t sheet_size = k_sheet_cells_per_row * k_sheet_cell_size;
	scene.atlas = device.create_texture({
		.width        = sheet_size,
		.height       = sheet_size,
		.format       = luxel::Format::R8G8B8A8_SRGB,
		.usage        = luxel::TextureUsage::Sampled,
		.mip_levels   = 1,
		.initial_data = sheet_pixels
	});
	scene.nearest_sampler = device.create_sampler({
		.mag_filter     = luxel::SamplerFilter::Nearest,
		.min_filter     = luxel::SamplerFilter::Nearest,
		.address_mode_u = luxel::SamplerAddressMode::ClampToEdge,
		.address_mode_v = luxel::SamplerAddressMode::ClampToEdge,
		.address_mode_w = luxel::SamplerAddressMode::ClampToEdge
	});

	//constexpr uint32_t width  = 256;
	//constexpr uint32_t height = 256;
	//auto pixels = make_checkerboard(width, height, 32);
	int img_width{};
	int img_height{};
	int img_channels{};

	stbi_set_flip_vertically_on_load(0); // No volteamos verticalmente la imagen
	// LUXEL_TEST_ASSETS_DIR la define tests/test_1/CMakeLists.txt (ruta absoluta a tests/assets).
	stbi_uc* data = stbi_load(LUXEL_TEST_ASSETS_DIR "/iriaBase01.png", &img_width, &img_height, &img_channels, 4);
	if (!data){
		std::cout << "No se encontró tests/assets/iriaBase01.png: el sprite se dibujará como un cuadrado blanco\n";
		return scene;
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

	scene.texture_aspect = static_cast<float>(width) / static_cast<float>(height);

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

// Prueba de estrés: una rejilla de 100x100 = 10.000 quads que giran cada uno a
// su aire, en UN solo lote (un solo draw). Uno de cada 7 lleva la textura del
// PNG y el resto la blanca: comprueba que mezclar texturas no rompe el lote.
// Ocupa un cuadrado de 2x2 unidades centrado (la cámara mide 2 de alto).
constexpr int k_stress_grid = 100;   // 100 x 100 = 10.000 quads (640 KB de los 1 MB del asignador por frame)

static void draw_stress_scene(luxel::Renderer2D& r2d, const Scene& scene, float time)
{
	constexpr float cell = 2.0f / k_stress_grid;   // lado de cada celda en unidades del mundo

	for (int j = 0; j < k_stress_grid; ++j)
	{
		for (int i = 0; i < k_stress_grid; ++i)
		{
			const float u = static_cast<float>(i) / (k_stress_grid - 1);   // 0..1 de izquierda a derecha
			const float v = static_cast<float>(j) / (k_stress_grid - 1);   // 0..1 de abajo arriba
			const bool textured = (i + j) % 7 == 0;

			// Una "ola" que recorre la rejilla en diagonal: escala y brillo.
			const float wave = 0.5f + 0.5f * std::sin(time * 3.0f - (u + v) * 6.0f);

			r2d.draw_quad({
				.position = { -1.0f + (static_cast<float>(i) + 0.5f) * cell,
				              -1.0f + (static_cast<float>(j) + 0.5f) * cell },
				.size     = { cell * (0.4f + 0.5f * wave), cell * (0.4f + 0.5f * wave) },
				.rotation = time * 2.0f + (u - v) * 4.0f,
				.color    = textured ? luxel::Color{} : luxel::Color{ u, v, 0.3f + 0.7f * wave, 1.0f },
				.texture  = textured ? scene.texture.get() : nullptr
			});
		}
	}
}

// Hoja de sprites: tres quads en fila, todos de la misma textura (la hoja), y
// el sprite PNG girando encima.
//   - izquierda: la hoja ENTERA (UV 0..1) con Nearest: se ven las 16 celdas;
//   - centro:    UNA celda animada (UV recortadas) con Nearest: píxeles nítidos;
//   - derecha:   la misma celda con el sampler Linear por defecto: borrosa. En
//                el borde, el filtro lineal mezcla con el texel de al lado, que
//                ya es de OTRA celda ("bleeding"). Aquí no se nota porque todas
//                las celdas tienen un borde oscuro de 1 píxel: ese margen es
//                justo la solución habitual (en las hojas de sprites de verdad,
//                un "padding" o "extrude" entre fotogramas).
static void draw_sprite_sheet_scene(luxel::Renderer2D& r2d, const Scene& scene, float time)
{
	constexpr float fps = 8.0f;   // fotogramas de la animación por segundo
	const uint32_t frame_index = static_cast<uint32_t>(time * fps) % k_sheet_frames;

	luxel::math::Vector2f uv_min{};
	luxel::math::Vector2f uv_max{};
	sprite_sheet_uv(frame_index, uv_min, uv_max);

	// La ventana de 800x600 mide 2 * 4/3 = 2.67 unidades de ancho (x de -1.33 a 1.33).
	const luxel::math::Vector2f size{ 0.7f, 0.7f };

	r2d.draw_quad({
		.position = { -0.9f, -0.4f },
		.size     = size,
		.texture  = scene.atlas.get(),
		.sampler  = scene.nearest_sampler.get()
	});
	r2d.draw_quad({
		.position = { 0.0f, -0.4f },
		.size     = size,
		.texture  = scene.atlas.get(),
		.sampler  = scene.nearest_sampler.get(),
		.uv_min   = uv_min,
		.uv_max   = uv_max
	});
	r2d.draw_quad({
		.position = { 0.9f, -0.4f },
		.size     = size,
		.texture  = scene.atlas.get(),   // sin sampler => Linear del renderer
		.uv_min   = uv_min,
		.uv_max   = uv_max
	});

	// El sprite PNG girando arriba, con el ancho ajustado a la imagen.
	r2d.draw_quad({
		.position = { 0.0f, 0.5f },
		.size     = { 0.8f * scene.texture_aspect, 0.8f },
		.rotation = time,
		.texture  = scene.texture.get()   // nullptr si no se cargó el PNG => cuadrado blanco
	});
}

// Promedia el tiempo de CPU del lote y lo pone en el título cada medio segundo.
// Ojo al leerlo: con vsync los FPS están topados por el monitor; lo que mide el
// coste del Renderer2D es el tiempo de CPU (construir 10.000 registros,
// copiarlos con Frame::allocate y grabar el draw), no los FPS.
static void update_stats(RenderWindow& render_window, double cpu_ms, uint32_t quad_count)
{
	auto& stats = render_window.stats;
	stats.cpu_ms_accum += cpu_ms;
	++stats.frames;

	const auto now = std::chrono::steady_clock::now();
	const double elapsed = std::chrono::duration<double>(now - stats.window_start).count();
	if (elapsed < 0.5) {
		return;
	}

	render_window.window.set_title(std::format("{} | {} quads, 1 draw | CPU {:.3f} ms/lote | {:.0f} FPS",
		render_window.base_title, quad_count, stats.cpu_ms_accum / stats.frames, stats.frames / elapsed));
	stats = FrameStats{};
}

// Un frame completo en una ventana. Si no se puede pintar (swapchain obsoleta o
// recreándose) simplemente no hace nada: ya se volverá a intentar en la vuelta siguiente.
static void render_window_frame(luxel::Device& device, luxel::Renderer2D& r2d, RenderWindow& render_window,
                                const Scene& scene, float time)
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

	auto& commands = frame->commands();
	commands.begin_rendering({ .clear = render_window.clear });

	// Todo lo que va entre begin() y end() es UN lote: un solo draw.
	const auto cpu_start = std::chrono::steady_clock::now();
	r2d.begin(*frame, render_window.camera.view_proj_matrix());

	switch (render_window.kind)
	{
	case SceneKind::Stress:      draw_stress_scene(r2d, scene, time);       break;
	case SceneKind::SpriteSheet: draw_sprite_sheet_scene(r2d, scene, time); break;
	}

	r2d.end();
	const double cpu_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpu_start).count();

	commands.end_rendering();

	device.end_frame(*frame);

	update_stats(render_window, cpu_ms, r2d.last_batch_quad_count());

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
	windows.push_back(make_render_window(*render_device, "Vitreum Window",       { 1920, 1080 }, { 0.05f, 0.05f, 0.08f, 1.0f }, true,  SceneKind::Stress));
	windows.push_back(make_render_window(*render_device, "Second Vitreum Window", {  800,  600 }, { 0.15f, 0.15f, 0.20f, 1.0f }, false, SceneKind::SpriteSheet));

	const auto color_format = windows.front()->swapchain->color_format();
	const auto extent       = windows.front()->swapchain->extent();
	std::cout << std::format("      Extent: {} x {}\n", extent.width, extent.height);
	std::cout << std::format("Color Format: {}\n", static_cast<uint8_t>(color_format));

	Scene scene = create_scene(*render_device);

	// Un Renderer2D para todas las ventanas: su pipeline sirve para cualquier
	// swapchain con este formato de color (aquí, todas). Cada ventana abre y
	// cierra su propio lote en su frame.
	luxel::Renderer2D r2d(*render_device, color_format);

	for (auto& render_window : windows) {
		render_window->window.show();
	}

	// Tiempo real desde el arranque: las animaciones van a la misma velocidad
	// sea cual sea la tasa de refresco del monitor (antes, angle += 0.025 por
	// vuelta iba más rápido en un monitor de 144 Hz que en uno de 60).
	const auto start_time = std::chrono::steady_clock::now();

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

		const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - start_time).count();
		for (auto& render_window : windows)
		{
			if (!render_window->window.is_minimized()) {
				render_window_frame(*render_device, r2d, *render_window, scene, time);
			}
		}
	}

	render_device->wait_idle();
}
