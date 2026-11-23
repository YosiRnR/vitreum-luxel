#include <luxel/renderer2d.h>
#include "batch_order.h"

#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>

// Generadas en la compilación por luxel_embed_shaders (luxel_2d/CMakeLists.txt)
// a partir de luxel_2d/shaders/*.hlsl. Cada una define un array <nombre>_spv[].
#include "quad_vs_spv.h"
#include "quad_ps_spv.h"

namespace luxel
{
	// ------------------------------------------------------------------------
	// Formato en GPU
	// ------------------------------------------------------------------------

	// Un quad tal y como lo lee quad_vs.hlsl con Load4 en 4 bloques de 16 bytes.
	// Si cambias el orden o el tamaño de un campo AQUÍ, cámbialo también en el
	// shader: no hay nada que los mantenga sincronizados salvo los static_assert
	// de abajo (que vigilan el C++, no el HLSL).
	struct Renderer2D::QuadInstance
	{
		// bloque 0 (bytes 0-15)
		float position[2];
		float size[2];
		// bloque 1 (bytes 16-31)
		float uv_min[2];
		float uv_max[2];
		// bloque 2 (bytes 32-47)
		float color[4];
		// bloque 3 (bytes 48-63)
		float rotation;
		uint32_t texture_index;
		uint32_t sampler_index;
		uint32_t padding;   // relleno hasta 64: múltiplo de 16 => cada quad empieza alineado
	};

	// Arrays de float en vez de math::Vector2f: así el formato no depende de
	// cómo esté definido Vector2f (si algún día gana un miembro o cambia su
	// alineación, el shader leería basura sin que nada avisara).
	// Los static_assert que vigilan este formato están en el constructor: como
	// QuadInstance es privado de Renderer2D, solo se puede nombrar desde dentro
	// de la clase (un static_assert suelto aquí fuera no compilaría).

	// 1 MB / 64 bytes = 16.384 quads. Se define aquí (y no en la cabecera)
	// porque necesita sizeof(QuadInstance), que solo se conoce en este .cpp.
	const uint32_t Renderer2D::k_max_quads_per_draw =
		static_cast<uint32_t>(k_frame_allocation_max_size / sizeof(Renderer2D::QuadInstance));

	namespace
	{
		// Debe coincidir con PushConstants de quad_vs.hlsl. 72 bytes de los 128
		// que permite bind_constants (ver command_list.h).
		struct PushConstants
		{
			std::array<float, 16> view_proj;   // column-major, como Matrix4x4::m
			uint32_t instances_slot;
			uint32_t instances_offset;
		};
		static_assert(sizeof(PushConstants) == 72);
		static_assert(sizeof(PushConstants) % 4 == 0, "bind_constants exige múltiplo de 4");

		// 6 vértices por quad: dos triángulos (ver la tabla de quad_vs.hlsl).
		constexpr uint32_t k_vertices_per_quad = 6;
	}

	// ------------------------------------------------------------------------
	// Construcción
	// ------------------------------------------------------------------------

	Renderer2D::Renderer2D(Device& device, Format color_format)
		: m_device(&device)   // Ref(T*) suma una referencia: el device no muere antes que nosotros
	{
		// Formato de QuadInstance (ver su definición arriba y quad_vs.hlsl).
		static_assert(sizeof(QuadInstance) == 64, "QuadInstance debe medir 64 bytes (ver quad_vs.hlsl)");
		static_assert(offsetof(QuadInstance, uv_min)   == 16);
		static_assert(offsetof(QuadInstance, color)    == 32);
		static_assert(offsetof(QuadInstance, rotation) == 48);

		// Blending Alpha por defecto: lo normal en 2D (sprites con transparencia).
		// Mismo pipeline para todos los quads => un solo draw por lote.
		m_pipeline = device.create_graphics_pipeline({
			.vs_byte_code = quad_vs_spv,
			.ps_byte_code = quad_ps_spv,
			.format       = color_format,
			.blend_mode   = BlendMode::Alpha
		});

		// Textura blanca 1x1. El pixel shader SIEMPRE muestrea una textura y
		// multiplica por el color; con esta textura el resultado es el color
		// tal cual. Así los rectángulos lisos no necesitan otro shader ni un
		// "if" en el pixel shader, y pueden ir en el mismo lote que los sprites.
		constexpr std::array<std::byte, 4> white_pixel{
			std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF }
		};
		m_white_texture = device.create_texture({
			.width        = 1,
			.height       = 1,
			.format       = Format::R8G8B8A8_UNorm,
			.usage        = TextureUsage::Sampled,
			.mip_levels   = 1,
			.initial_data = white_pixel
		});

		// Sampler por defecto: lineal y ClampToEdge. Clamp y no Repeat porque
		// en un sprite el filtrado lineal del borde mezclaría con el lado
		// opuesto de la imagen (aparece una línea fina en los bordes).
		m_default_sampler = device.create_sampler({
			.mag_filter     = SamplerFilter::Linear,
			.min_filter     = SamplerFilter::Linear,
			.address_mode_u = SamplerAddressMode::ClampToEdge,
			.address_mode_v = SamplerAddressMode::ClampToEdge,
			.address_mode_w = SamplerAddressMode::ClampToEdge
		});

		// Reserva inicial: evita realojar los vectores en los primeros frames.
		m_instances.reserve(1024);
		m_layers.reserve(1024);
	}

	// Definido aquí (y no "= default" en la cabecera) porque destruir
	// m_instances necesita QuadInstance completo, y solo lo está en este .cpp.
	Renderer2D::~Renderer2D() = default;

	// ------------------------------------------------------------------------
	// Lote
	// ------------------------------------------------------------------------

	void Renderer2D::begin(Frame& frame, const math::Matrix4x4& view_proj)
	{
		if (m_frame != nullptr) {
			throw std::logic_error("Renderer2D::begin: ya hay un lote abierto (falta end())");
		}

		m_frame = &frame;
		m_view_proj = view_proj;
		// clear() no libera la memoria del vector: el frame siguiente reutiliza
		// la misma capacidad sin volver a reservar.
		m_instances.clear();
		m_layers.clear();
	}

	void Renderer2D::draw_quad(const Quad& quad)
	{
		if (m_frame == nullptr) {
			throw std::logic_error("Renderer2D::draw_quad: fuera de begin()/end()");
		}

		// Si no hay textura o sampler, los del renderer. bindless_index() es el
		// hueco de la tabla bindless: lo único que el shader necesita saber.
		const Texture& texture = quad.texture ? *quad.texture : *m_white_texture;
		const Sampler& sampler = quad.sampler ? *quad.sampler : *m_default_sampler;

		// Nota sobre la vida de la textura: aquí solo copiamos su índice. Si la
		// aplicación suelta su último Ref antes de end(), no pasa nada: luxel
		// difiere la destrucción hasta que la GPU termina este frame.
		m_instances.push_back(QuadInstance{
			.position      = { quad.position.x, quad.position.y },
			.size          = { quad.size.x, quad.size.y },
			.uv_min        = { quad.uv_min.x, quad.uv_min.y },
			.uv_max        = { quad.uv_max.x, quad.uv_max.y },
			.color         = { quad.color.r, quad.color.g, quad.color.b, quad.color.a },
			.rotation      = quad.rotation,
			.texture_index = texture.bindless_index(),
			.sampler_index = sampler.bindless_index(),
			.padding       = 0
		});
		// La capa no va a la GPU: solo sirve para ordenar en end().
		m_layers.push_back(quad.layer);
	}

	void Renderer2D::end()
	{
		if (m_frame == nullptr) {
			throw std::logic_error("Renderer2D::end: no hay lote abierto (falta begin())");
		}

		// Cerramos el lote pase lo que pase (también si algo de abajo lanza),
		// para que el siguiente begin() no se queje.
		Frame& frame = *m_frame;
		m_frame = nullptr;
		const auto quad_count = static_cast<uint32_t>(m_instances.size());
		m_last_batch_quad_count = quad_count;
		m_last_batch_draw_count = 0;

		if (quad_count == 0) {
			return;   // nada que dibujar: ni siquiera enlazamos el pipeline
		}

		// 1) Orden por capa. Si ya venían en orden (lo normal: todo en la misma
		//    capa) no se copia nada y se dibuja directamente m_instances. Si no,
		//    se copia cada quad a su sitio en m_sorted siguiendo m_order.
		std::span<const QuadInstance> quads = m_instances;
		if (detail::stable_order_by_layer(m_layers, m_order))
		{
			m_sorted.resize(quad_count);
			for (uint32_t i = 0; i < quad_count; ++i) {
				m_sorted[i] = m_instances[m_order[i]];
			}
			quads = m_sorted;
		}

		CommandList& commands = frame.commands();
		commands.bind_pipeline(*m_pipeline);

		// 2) Un draw por tramo de como mucho k_max_quads_per_draw quads (lo que
		//    cabe en una reserva del asignador por frame). Con menos de 16.384
		//    quads, que es lo normal, el bucle da una sola vuelta: un draw.
		//    El orden entre tramos se conserva: el tramo 1 se pinta encima del 0.
		const uint32_t draws = detail::draw_count(quad_count, k_max_quads_per_draw);
		for (uint32_t d = 0; d < draws; ++d)
		{
			const detail::DrawRange range = detail::draw_range(quad_count, k_max_quads_per_draw, d);

			// a) Copiar el tramo al asignador por frame: memoria visible por la
			//    CPU y la GPU, válida hasta que este frame termine. Cada tramo
			//    puede acabar en un bloque distinto (otro slot bindless).
			const BufferAllocation allocation =
				frame.allocate(std::as_bytes(quads.subspan(range.first, range.count)));

			// b) Push constants: la matriz y dónde están los quads de ESTE tramo.
			//    Para el shader, cada tramo empieza en el quad 0 de su offset.
			const PushConstants push_constants{
				.view_proj        = m_view_proj.m,
				.instances_slot   = allocation.slot_index,
				.instances_offset = allocation.offset
			};
			commands.bind_constants(std::as_bytes(std::span(&push_constants, 1)));

			// c) 6 vértices por quad. El vertex shader convierte cada
			//    SV_VertexID en (quad, esquina).
			commands.draw(range.count * k_vertices_per_quad);
			++m_last_batch_draw_count;
		}
	}

} // namespace luxel
