#include <vitreum/vitreum.h>
#include <luxel/luxel.h>
#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <format>
#include <array>
#include <span>

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

static std::vector<uint32_t> read_binary_file(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file) {
		throw std::runtime_error("No se pudo abrir el archivo: " + path.string());
	}

	const auto size = static_cast<size_t>(file.tellg());
	if (size % 4 != 0) {
		throw std::runtime_error("El tamaño del fichero SPV de shaders debe ser múltiplo de 4 para ser correcto");
	}
	std::vector<uint32_t> data(size / 4);
	file.seekg(0);
	file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
	return data;
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

	for (uint32_t y = 0; y < tile_size; ++y)
	{
		for (uint32_t x = 0; x < tile_size; ++x)
		{
			//const bool light = ((x / tile_size) + (y / tile_size)) % 2 == 0;

			//const std::byte color = light ? std::byte{ 0xE0 } : std::byte{ 0x20 };

			const size_t i = (static_cast<size_t>(y) * width + x) * 4;

			pixels[i + 0] = std::byte{ 0xFF }; // R
			pixels[i + 1] = std::byte{ 0xFF }; // G
			pixels[i + 2] = std::byte{ 0x00 }; // B
			pixels[i + 3] = std::byte{ 0xFF }; // A
		}
	}

	return pixels;
}


int main(int argc, char** argv)
{
	vitreum::Window main_wnd{};
	main_wnd.create({ "Vitreum Window",{ 1920, 1080 } });

	auto render_device = luxel::create_device(luxel::DeviceDescriptor{
		.backend = luxel::Backend::Vulkan,
		.debug   = true
	});

	auto swapchain = render_device->create_swapchain({
		.window = to_luxel(main_wnd.native_handle()),
		.size   = { static_cast<uint32_t>(main_wnd.width()), static_cast<uint32_t>(main_wnd.height()) },
		.vsync  = true
	});

	auto vs_code  = read_binary_file(std::filesystem::path(LUXEL_SHADER_DIR) / "shader_vs.spv");
	auto ps_code  = read_binary_file(std::filesystem::path(LUXEL_SHADER_DIR) / "shader_ps.spv");
	auto pipeline = render_device->create_graphics_pipeline({
		.vs_byte_code = std::span<uint32_t>(vs_code),
		.ps_byte_code = std::span<uint32_t>(ps_code),
		.format       = swapchain->color_format()
	});

	constexpr uint32_t width  = 256;
	constexpr uint32_t height = 256;
	auto pixels = make_checkerboard(width, height, 32);
	luxel::TextureDescriptor texture_desc{
		.width        = width,
		.height       = height,
		.format       = luxel::Format::R8G8B8A8_SRGB,
		.usage        = luxel::TextureUsage::Sampled,
		.mip_levels   = 1,
		.initial_data = pixels
	};
	auto texture = render_device->create_texture(texture_desc);
	auto texture_slot = texture->bindless_index();

	luxel::SamplerDescriptor sampler_desc{
	};
	auto sampler = render_device->create_sampler(sampler_desc);
	auto sampler_slot = sampler->bindless_index();

	//const std::array<float, 24> vertices{
	//	// posición      // UV
	//	-0.5f, -0.5f,    0.0f, 1.0f, // abajo izquierda
	//	 0.5f, -0.5f,    1.0f, 1.0f, // abajo derecha
	//	 0.5f,  0.5f,    1.0f, 0.0f, // arriba derecha

	//	-0.5f, -0.5f,    0.0f, 1.0f, // abajo izquierda
	//	 0.5f,  0.5f,    1.0f, 0.0f, // arriba derecha
	//	-0.5f,  0.5f,    0.0f, 0.0f  // arriba izquierda
	//};
	const std::array<float, 16> vertices{
		// posición      // UV
		-0.5f, -0.5f,    0.0f, 0.0f, // abajo izquierda
		 0.5f, -0.5f,    1.0f, 0.0f, // abajo derecha
		 0.5f,  0.5f,    1.0f, 1.0f, // arriba derecha
		-0.5f,  0.5f,    0.0f, 1.0f  // arriba izquierda
	};
	auto vertex_buffer = render_device->create_buffer(luxel::BufferDescriptor::from_data(vertices, luxel::BufferUsage::Storage));
	auto vb_slot_index = vertex_buffer->bindless_index();

	const std::array<uint16_t, 6> indices{
		0, 1, 2, 0, 2, 3
	};
	auto index_buffer  = render_device->create_buffer(luxel::BufferDescriptor::from_data(indices, luxel::BufferUsage::Index));

	luxel::math::Matrix4x4 xform_matrix = luxel::math::Matrix4x4::Identity();

	auto extent       = swapchain->extent();
	auto color_format = swapchain->color_format();

	std::cout << std::format("      Extent: {} x {}\n", extent.width, extent.height);
	std::cout << std::format("Color Format: {}\n", static_cast<uint8_t>(color_format));

	main_wnd.set_size({ 800, 600 });

	main_wnd.show();

	float angle{ 0.0f };

	while (!main_wnd.should_close())
	{
		main_wnd.poll_events();

		while (main_wnd.has_pending_events())
		{
			auto event = main_wnd.pop_event();

			if (event && event->type == vitreum::WinEventType::Close) {
				main_wnd.cancel_close(
					vitreum::message_box_okcancel(main_wnd.native_handle(), "¿Desea cerrar la ventana?", "WARNING")
				);
			}
			else if (event && event->type == vitreum::WinEventType::Resize) {
				swapchain->resize({ static_cast<uint32_t>(event->size.width), static_cast<uint32_t>(event->size.height) });
			}
		}

		auto* frame = render_device->begin_frame(*swapchain);
		if (frame == nullptr) {
			continue;
		}

		xform_matrix = luxel::math::Matrix4x4::rotateZ(angle);
		angle += 0.025f;
		auto buf_alloc = frame->allocate(std::as_bytes(std::span(xform_matrix.m)));
		std::array<uint32_t, 5> push_constants{ vb_slot_index, buf_alloc.slot_index, buf_alloc.offset, texture_slot, sampler_slot };

		frame->commands().begin_rendering({ .clear = { 1.0f, 0.0f, 0.0f, 1.0f } });
		frame->commands().bind_constants(std::as_bytes(std::span(push_constants)));
		frame->commands().bind_index_buffer(*index_buffer, luxel::IndexType::Uint16);
		frame->commands().bind_pipeline(*pipeline);
		//frame->commands().draw(6);
		frame->commands().draw_indexed(static_cast<uint32_t>(indices.size()));
		frame->commands().end_rendering();

		render_device->end_frame(*frame);

		static luxel::Size2D last{};
		auto now = swapchain->extent();
		if (now.width != last.width || now.height != last.height) {
			std::cout << std::format("Swapchain: {} x {}\n", now.width, now.height);
			last = now;
		}
	}

	render_device->wait_idle();
}
