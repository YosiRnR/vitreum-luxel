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
#include "keyboard_state.h"
#include "iria_controller.h"
#include "gdi_font_atlas.h"


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

// Qué se dibuja en cada ventana.
enum class SceneKind
{
	Stress,        // 100x100 quads girando: mide el coste en CPU de un lote grande
	SpriteSheet,   // hoja de sprites generada: recortes de UV, animación y Nearest vs Linear
	Iria           // fondo + hoja de sprites de Iria: correr, idle y saltar
};

// Medidas para el título de la ventana de estrés (se promedian y se muestran
// cada medio segundo: actualizar el título en cada frame sería ilegible).
struct FrameStats
{
	std::chrono::steady_clock::time_point window_start{ std::chrono::steady_clock::now() };
	double cpu_ms_accum{ 0.0 };   // suma del tiempo de CPU de construir y enviar el lote
	uint32_t frames{ 0 };
};

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
	SceneKind kind{ SceneKind::Stress };
	std::string base_title;             // el título sin las estadísticas
	FrameStats stats{};
	KeyboardState keyboard{};           // teclas de ESTA ventana (Windows solo manda teclas a la que tiene el foco)
};

// Recursos que se dibujan en todas las ventanas. El pipeline, los shaders y el
// sampler lineal ya no son cosa de la aplicación: los pone el Renderer2D.
struct Scene
{
	luxel::Ref<luxel::Texture> texture;         // puede quedar vacía si no se encuentra el PNG
	float texture_aspect{ 1.0f };               // ancho / alto de la imagen, para no deformarla

	luxel::Ref<luxel::Texture> atlas;           // hoja de sprites generada a mano (ver make_sprite_sheet)
	luxel::Ref<luxel::Sampler> nearest_sampler; // para pixel art: sin filtrar, píxeles nítidos

	luxel::Ref<luxel::Texture> iria_sheet;      // tests/assets/Iria-sprite-sheet.png (ver k_iria_*)
	luxel::Ref<luxel::Texture> background;      // tests/assets/forest_00001_.png
	float background_aspect{ 1.0f };

	// Fuente para el HUD, con el atlas generado por GDI al arrancar. unique_ptr
	// porque BitmapFont no tiene constructor por defecto (y Scene sí lo necesita).
	std::unique_ptr<luxel::BitmapFont> font;
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

// UV de la celda n de una hoja de columns x rows celdas iguales:
// (columna, fila) -> rectángulo [uv_min, uv_max]. Es el "recorte" de la
// textura que se pasa en el Quad. Sirve para cualquier rejilla, cuadrada o no.
static void sprite_sheet_uv(uint32_t cell, uint32_t columns, uint32_t rows,
                            luxel::math::Vector2f& uv_min, luxel::math::Vector2f& uv_max)
{
	const float step_u = 1.0f / static_cast<float>(columns);
	const float step_v = 1.0f / static_cast<float>(rows);
	const float col    = static_cast<float>(cell % columns);
	const float row    = static_cast<float>(cell / columns);
	uv_min = { col * step_u, row * step_v };
	uv_max = { (col + 1.0f) * step_u, (row + 1.0f) * step_v };
}

// Carga un PNG de tests/assets como textura RGBA sRGB. Si no existe, avisa y
// devuelve un Ref vacío (el Quad con texture = nullptr se dibuja blanco).
// aspect (opcional) recibe ancho / alto de la imagen.
static luxel::Ref<luxel::Texture> load_png_texture(luxel::Device& device, const std::string& file_name,
                                                   float* aspect = nullptr)
{
	// LUXEL_TEST_ASSETS_DIR la define tests/test_1/CMakeLists.txt (ruta absoluta a tests/assets).
	const std::string path = std::string(LUXEL_TEST_ASSETS_DIR) + "/" + file_name;

	int img_width{};
	int img_height{};
	int img_channels{};
	stbi_set_flip_vertically_on_load(0); // No volteamos: la primera fila es la de arriba, como las UV de luxel
	stbi_uc* data = stbi_load(path.c_str(), &img_width, &img_height, &img_channels, 4);   // 4 = forzar RGBA
	if (!data) {
		std::cout << std::format("No se encontró {}: se dibujará en blanco\n", path);
		return {};
	}

	const size_t byte_count = static_cast<size_t>(img_width) * static_cast<size_t>(img_height) * 4;
	auto texture = device.create_texture({
		.width        = static_cast<uint32_t>(img_width),
		.height       = static_cast<uint32_t>(img_height),
		.format       = luxel::Format::R8G8B8A8_SRGB,
		.usage        = luxel::TextureUsage::Sampled,
		.mip_levels   = 1,
		.initial_data = { reinterpret_cast<const std::byte*>(data), byte_count }
	});
	// create_texture ya ha subido los píxeles (submit_immediate espera a la GPU):
	// la memoria de stb se puede liberar en cuanto vuelve.
	stbi_image_free(data);

	if (aspect) {
		*aspect = static_cast<float>(img_width) / static_cast<float>(img_height);
	}
	return texture;
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

	scene.texture    = load_png_texture(device, "iriaBase01.png", &scene.texture_aspect);
	scene.iria_sheet = load_png_texture(device, "Iria-sprite-sheet.png");
	scene.background = load_png_texture(device, "forest_00001_.png", &scene.background_aspect);

	// Atlas de la fuente: Segoe UI en negrita, 48 px, Latin-1 (32-255). Se
	// genera grande y se escala hacia abajo al dibujar (queda nítido a casi
	// cualquier tamaño de HUD). El atlas solo hace falta para crear la fuente:
	// create_texture lo sube a la GPU y después ya se puede tirar.
	const FontAtlasImage atlas = make_gdi_font_atlas(L"Segoe UI", 48, true);
	scene.font = std::make_unique<luxel::BitmapFont>(device, luxel::BitmapFontDescriptor{
		.atlas_width     = atlas.width,
		.atlas_height    = atlas.height,
		.atlas_pixels    = atlas.pixels,
		.cell_width      = atlas.cell_width,
		.cell_height     = atlas.cell_height,
		.columns         = atlas.columns,
		.padding         = atlas.padding,
		.first_codepoint = atlas.first_codepoint,
		.advances        = atlas.advances,
		.line_height     = atlas.line_height
	});

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

		// Teclas y foco: el KeyboardState se queda con lo que le interesa.
		render_window.keyboard.on_event(*event);

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

// Prueba de estrés: una rejilla de 150x150 = 22.500 quads que giran cada uno a
// su aire, en UN solo lote. No caben en una reserva del asignador por frame
// (máximo 16.384 quads de 64 bytes = 1 MB), así que el Renderer2D los dibuja
// en 2 draws (el título lo muestra). Uno de cada 7 lleva la textura del PNG y
// el resto la blanca: mezclar texturas no rompe el lote.
// Ocupa un cuadrado de 2x2 unidades centrado (la cámara mide 2 de alto).
constexpr int k_stress_grid = 150;   // 150 x 150 = 22.500 quads (1,4 MB: 2 bloques del asignador por frame)

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
	sprite_sheet_uv(frame_index, k_sheet_cells_per_row, k_sheet_cells_per_row, uv_min, uv_max);

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

// Medidas de Iria en la escena (unidades del mundo; la cámara mide 2 de alto).
constexpr float k_iria_sprite_size = 0.9f;    // la celda es cuadrada (256x256): el quad también
constexpr float k_iria_ground_y    = -0.72f;  // altura de los pies (a ojo, sobre el camino del bosque)

// Límites del CENTRO de Iria para que no se salga de la vista por los lados.
static float iria_max_x(float view_width)
{
	return std::max(0.0f, view_width * 0.5f - k_iria_sprite_size * 0.4f);   // 0.4: el dibujo no llena la celda
}

// Iria en el bosque, en la posición y con el fotograma que diga su controlador
// (que mueve el teclado: ver el bucle de main). La escena solo DIBUJA.
// view_width: ancho visible en unidades del mundo (2 * aspecto de la ventana).
//
// Ojo: el fondo se envía DESPUÉS de Iria a propósito. Sin capas, el fondo
// (enviado el último) la taparía; con layer = -1 el Renderer2D lo ordena y lo
// dibuja primero. Es la prueba visual de R2d.
static void draw_iria_scene(luxel::Renderer2D& r2d, const Scene& scene, const IriaController& iria, float view_width)
{
	// --- Iria --------------------------------------------------------------------
	luxel::math::Vector2f uv_min{};
	luxel::math::Vector2f uv_max{};
	sprite_sheet_uv(iria.cell(), k_iria_columns, k_iria_rows, uv_min, uv_max);

	// Mirando a la izquierda: espejo horizontal gratis intercambiando las U.
	// La textura se lee al revés en horizontal; la hoja solo tiene a Iria
	// corriendo hacia la derecha y así no hace falta dibujarla dos veces.
	if (iria.facing_left()) {
		std::swap(uv_min.x, uv_max.x);
	}

	r2d.draw_quad({
		// position es el CENTRO: para apoyar los pies en el suelo se sube medio quad.
		.position = { iria.x(), k_iria_ground_y + k_iria_sprite_size * 0.5f + iria.height() },
		.size     = { k_iria_sprite_size, k_iria_sprite_size },
		.texture  = scene.iria_sheet.get(),
		.uv_min   = uv_min,
		.uv_max   = uv_max,
		.layer    = 0
	});

	// --- Fondo (enviado el último, dibujado el primero) ------------------------
	// "Cover": que tape toda la vista sin deformarse. Alto 2 (lo que mide la
	// cámara); si la ventana es más ancha que la imagen, se agranda para cubrir
	// también el ancho (y se recorta arriba y abajo).
	const float bg_height = std::max(2.0f, view_width / scene.background_aspect);
	r2d.draw_quad({
		.position = { 0.0f, 0.0f },
		.size     = { bg_height * scene.background_aspect, bg_height },
		.texture  = scene.background.get(),
		.layer    = -1   // detrás de Iria (capa 0), aunque se envíe después
	});
}

// El HUD de Iria: un lote de UI aparte, en píxeles (begin_ui), que se dibuja
// encima del juego. Un lote más = un draw más por ventana.
//
// Diferencia con tenerlo en el mundo (como en R3a): al agrandar la ventana, el
// bosque crece con ella, pero las letras siguen midiendo los mismos píxeles.
static void draw_iria_hud(luxel::Renderer2D& r2d, const Scene& scene, const IriaController& iria, float width, float height)
{
	if (!scene.font) {
		return;
	}
	const luxel::BitmapFont& font = *scene.font;
	const float margin = 20.0f;   // píxeles

	// Texto con una sombra: el mismo texto, negro semitransparente y un poco
	// desplazado abajo a la derecha (en la UI, abajo es SUMAR Y), una capa por debajo.
	const auto shadowed_text = [&](std::u8string_view text, luxel::math::Vector2f top_left, float size, luxel::Color color) {
		const float offset = std::max(1.0f, size * 0.05f);
		r2d.draw_text(font, text, { top_left.x + offset, top_left.y + offset }, size, { 0.0f, 0.0f, 0.0f, 0.55f }, 1);
		r2d.draw_text(font, text, top_left, size, color, 2);
	};

	shadowed_text(u8"¡Hola, Iria!", { margin, margin }, 80.0f, { 1.0f, 0.85f, 0.35f, 1.0f });
	// El '\n' comprueba que la segunda línea cae debajo también con +Y abajo.
	shadowed_text(u8"Flechas o A/D: correr\nEspacio: saltar", { margin, margin + 94.0f }, 32.0f, { 1.0f, 1.0f, 1.0f, 1.0f });

	// Estado, alineado a la derecha con measure(): ancho de la fuente * escala.
	const float state_size  = 44.0f;
	const float state_width = font.measure(iria.state_label()) * (state_size / font.line_height());
	shadowed_text(iria.state_label(), { width - margin - state_width, margin }, state_size, { 0.75f, 1.0f, 0.75f, 1.0f });

	// Retrato abajo a la izquierda, un poco girado. Comprueba a ojo las dos cosas
	// que podrían salir mal con +Y abajo: que Iria no esté cabeza abajo (UV) y que
	// un giro positivo se incline hacia la IZQUIERDA (antihorario), como en el mundo.
	luxel::math::Vector2f uv_min{};
	luxel::math::Vector2f uv_max{};
	sprite_sheet_uv(k_iria_idle.first_cell, k_iria_columns, k_iria_rows, uv_min, uv_max);
	const float portrait = 110.0f;
	const luxel::math::Vector2f center{ margin + portrait * 0.6f, height - margin - portrait * 0.6f };
	r2d.draw_quad({ .position = center, .size = { portrait, portrait }, .rotation = 0.15f,
	                .color = { 0.0f, 0.0f, 0.0f, 0.5f }, .layer = 0 });
	r2d.draw_quad({ .position = center, .size = { portrait, portrait }, .rotation = 0.15f,
	                .texture = scene.iria_sheet.get(), .uv_min = uv_min, .uv_max = uv_max, .layer = 1 });

	// --- Panel con botones (R4b) ---------------------------------------------------
	// Cada rectángulo redondeado es un quad más del lote. Tres radios para ver la
	// SDF: 4 (casi recto), 14, y 100, que se limita a 24 (medio alto) y da una cápsula.
	const luxel::math::Vector2f panel_size{ 260.0f, 218.0f };
	const luxel::math::Vector2f panel{ width - margin - panel_size.x, height - margin - panel_size.y };
	r2d.draw_rounded_rect({ .top_left = panel, .size = panel_size, .corner_radius = 24.0f,
	                        .color = { 0.02f, 0.05f, 0.10f, 0.75f }, .layer = 0 });

	struct Button { std::u8string_view label; float radius; luxel::Color color; };
	const Button buttons[] = {
		{ u8"Jugar",    4.0f,   { 0.05f, 0.35f, 0.80f, 1.0f } },
		{ u8"Opciones", 14.0f,  { 0.10f, 0.50f, 0.20f, 1.0f } },
		{ u8"Salir",    100.0f, { 0.70f, 0.10f, 0.10f, 1.0f } },
	};
	const luxel::math::Vector2f button_size{ 220.0f, 48.0f };
	const float label_size = 30.0f;
	luxel::math::Vector2f at{ panel.x + 20.0f, panel.y + 20.0f };
	for (const Button& button : buttons)
	{
		r2d.draw_rounded_rect({ .top_left = at, .size = button_size, .corner_radius = button.radius,
		                        .color = button.color, .layer = 1 });
		// Texto centrado: el hueco que sobra, repartido a los dos lados.
		const float label_width = font.measure(button.label) * (label_size / font.line_height());
		r2d.draw_text(font, button.label,
		              { at.x + (button_size.x - label_width) * 0.5f, at.y + (button_size.y - label_size) * 0.5f },
		              label_size, { 1.0f, 1.0f, 1.0f, 1.0f }, 2);
		at.y += button_size.y + 17.0f;
	}
}

// Promedia el tiempo de CPU del lote y lo pone en el título cada medio segundo.
// Ojo al leerlo: con vsync los FPS están topados por el monitor; lo que mide el
// coste del Renderer2D es el tiempo de CPU (construir los registros, ordenarlos,
// copiarlos con Frame::allocate y grabar los draws), no los FPS.
static void update_stats(RenderWindow& render_window, double cpu_ms, uint32_t quad_count, uint32_t draw_count)
{
	auto& stats = render_window.stats;
	stats.cpu_ms_accum += cpu_ms;
	++stats.frames;

	const auto now = std::chrono::steady_clock::now();
	const double elapsed = std::chrono::duration<double>(now - stats.window_start).count();
	if (elapsed < 0.5) {
		return;
	}

	render_window.window.set_title(std::format("{} | {} quads, {} draw(s) | CPU {:.3f} ms/lote | {:.0f} FPS",
		render_window.base_title, quad_count, draw_count, stats.cpu_ms_accum / stats.frames, stats.frames / elapsed));
	stats = FrameStats{};
}

// Un frame completo en una ventana. Si no se puede pintar (swapchain obsoleta o
// recreándose) simplemente no hace nada: ya se volverá a intentar en la vuelta siguiente.
static void render_window_frame(luxel::Device& device, luxel::Renderer2D& r2d, RenderWindow& render_window,
                                const Scene& scene, const IriaController& iria, float time)
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

	// Todo lo que va entre begin() y end() es UN lote: un solo draw (o unos pocos).
	const auto cpu_start = std::chrono::steady_clock::now();
	r2d.begin(*frame, render_window.camera.view_proj_matrix());

	switch (render_window.kind)
	{
	case SceneKind::Stress:      draw_stress_scene(r2d, scene, time);       break;
	case SceneKind::SpriteSheet: draw_sprite_sheet_scene(r2d, scene, time); break;
	case SceneKind::Iria:        draw_iria_scene(r2d, scene, iria, 2.0f * aspect); break;
	}

	r2d.end();
	uint32_t quad_count = r2d.last_batch_quad_count();
	uint32_t draw_count = r2d.last_batch_draw_count();

	// Segundo lote: la UI, en píxeles, encima de la escena.
	if (render_window.kind == SceneKind::Iria)
	{
		const float width  = static_cast<float>(extent.width);
		const float height = static_cast<float>(extent.height);
		r2d.begin_ui(*frame, width, height);
		draw_iria_hud(r2d, scene, iria, width, height);
		r2d.end();
		quad_count += r2d.last_batch_quad_count();
		draw_count += r2d.last_batch_draw_count();
	}

	const double cpu_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpu_start).count();

	commands.end_rendering();

	device.end_frame(*frame);

	update_stats(render_window, cpu_ms, quad_count, draw_count);

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
	// (Títulos solo en ASCII: los acentos en títulos están aparcados en vitreum.)
	windows.push_back(make_render_window(*render_device, "Iria - flechas o A/D: mover, espacio o W: saltar",
	                                     { 1280,  720 }, { 0.0f,  0.0f,  0.0f,  1.0f }, false, SceneKind::Iria));

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
	// El teclado va a la ventana con el foco: se lo damos a la de Iria para
	// poder jugar nada más arrancar.
	windows.back()->window.request_focus();

	// Lógica de juego de Iria. Vive fuera de las ventanas: el estado del juego
	// no es de ninguna ventana (si se cerrase la de Iria, el resto sigue igual).
	IriaController iria;

	// Tiempo real desde el arranque: las animaciones van a la misma velocidad
	// sea cual sea la tasa de refresco del monitor (antes, angle += 0.025 por
	// vuelta iba más rápido en un monitor de 144 Hz que en uno de 60).
	const auto start_time = std::chrono::steady_clock::now();
	auto last_time = start_time;

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

		const auto now = std::chrono::steady_clock::now();
		const float time = std::chrono::duration<float>(now - start_time).count();
		// dt: segundos desde la vuelta anterior, para la física. Se limita a
		// 50 ms: tras estar minimizada (wait_events) o parada en un breakpoint,
		// un dt enorme haría que Iria atravesase el suelo o saliera disparada.
		const float dt = std::min(std::chrono::duration<float>(now - last_time).count(), 0.05f);
		last_time = now;

		// --- Juego: leer la entrada de la ventana de Iria y avanzar la simulación ---
		// Primero se actualiza el juego y después se dibuja: así el frame
		// muestra el estado de ESTA vuelta, no el de la anterior.
		for (auto& render_window : windows)
		{
			if (render_window->kind != SceneKind::Iria) {
				continue;
			}
			using vitreum::KeyCode;
			const auto& keys = render_window->keyboard;
			const IriaInput input{
				.left  = keys.is_down(KeyCode::LeftArrow)  || keys.is_down(KeyCode::A),
				.right = keys.is_down(KeyCode::RightArrow) || keys.is_down(KeyCode::D),
				.jump  = keys.was_pressed(KeyCode::Space)  || keys.was_pressed(KeyCode::W) || keys.was_pressed(KeyCode::UpArrow)
			};
			const float view_width = render_window->window.height() > 0
				? 2.0f * static_cast<float>(render_window->window.width()) / static_cast<float>(render_window->window.height())
				: 2.0f;
			iria.update(dt, input, -iria_max_x(view_width), iria_max_x(view_width));
		}

		for (auto& render_window : windows)
		{
			if (!render_window->window.is_minimized()) {
				render_window_frame(*render_device, r2d, *render_window, scene, iria, time);
			}
			// Lo "pulsado en este frame" ya se ha leído: se limpia para la vuelta siguiente.
			render_window->keyboard.end_frame();
		}
	}

	render_device->wait_idle();
}
