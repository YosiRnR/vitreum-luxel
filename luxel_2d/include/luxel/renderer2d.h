#pragma once

// Renderer2D: dibuja quads (rectángulos con color y/o textura) agrupados en
// lotes. Diseño y decisiones en docs/renderers-design.md ("Decisiones de R2").
//
// Solo usa la API pública de luxel: es una biblioteca aparte (luxel_2d).

#include <luxel/device.h>
#include <luxel/bitmap_font.h>
#include <luxel/math/vectors.h>
#include <luxel/math/matrix4.h>
#include <cstdint>
#include <string_view>
#include <vector>

namespace luxel
{
	namespace detail { struct ClipRect; }   // privado: se define en src/clip_rect.h

	// Color RGBA en coma flotante, de 0 a 1. Ojo: si la swapchain es sRGB
	// (lo habitual), estos valores son LINEALES; la GPU los convierte a sRGB al
	// escribir. Por eso un gris 0.5 se verá más claro que el "50 %" de un
	// programa de dibujo (allí los valores ya están en sRGB).
	struct Color
	{
		float r{ 1.0f };
		float g{ 1.0f };
		float b{ 1.0f };
		float a{ 1.0f };
	};

	// Lo que la aplicación pide dibujar. Todos los campos tienen valor por
	// defecto, así se pueden usar designated initializers y escribir solo lo que
	// cambia:
	//     r2d.draw_quad({ .position = { 1, 0 }, .color = { 1, 0, 0, 1 } });
	// Ojo: con designated initializers los campos deben ir EN EL ORDEN de la struct.
	struct Quad
	{
		math::Vector2f position{ 0.0f, 0.0f };  // CENTRO del quad, en unidades del lote (mundo, o píxeles con begin_ui)
		math::Vector2f size{ 1.0f, 1.0f };      // ancho y alto, en esas mismas unidades
		float rotation{ 0.0f };                 // radianes, positivo = antihorario (tal como se ve)
		Color color{};                          // se multiplica por el color de la textura
		Texture* texture{ nullptr };            // nullptr = textura blanca 1x1 => color liso
		Sampler* sampler{ nullptr };            // nullptr = sampler lineal por defecto del renderer
		math::Vector2f uv_min{ 0.0f, 0.0f };    // UV de la esquina de ARRIBA a la izquierda
		math::Vector2f uv_max{ 1.0f, 1.0f };    // UV de la esquina de ABAJO a la derecha
		int32_t layer{ 0 };                     // capa: las bajas se dibujan antes (detrás); a igual capa, orden de envío
	};

	// Un rectángulo con las esquinas redondeadas (botones, paneles...). Se coloca
	// por su esquina de ARRIBA a la izquierda, como el texto, y no gira.
	struct RoundedRect
	{
		math::Vector2f top_left{ 0.0f, 0.0f };  // en unidades del lote
		math::Vector2f size{ 1.0f, 1.0f };
		float corner_radius{ 0.0f };            // se limita a la mitad del lado corto (ahí ya es una cápsula)
		Color color{};                          // se multiplica por la textura, si la hay
		float border_thickness{ 0.0f };         // 0 = sin borde. Crece hacia DENTRO: size es el tamaño total
		Color border_color{ 0.0f, 0.0f, 0.0f, 1.0f };   // se pinta encima del relleno (que puede ser transparente)
		Texture* texture{ nullptr };            // nullptr = color liso
		Sampler* sampler{ nullptr };
		math::Vector2f uv_min{ 0.0f, 0.0f };
		math::Vector2f uv_max{ 1.0f, 1.0f };
		int32_t layer{ 0 };
	};

	// Uso típico, dentro de un frame:
	//
	//     frame->commands().begin_rendering(...);
	//     r2d.begin(*frame, camera.view_proj_matrix());
	//     r2d.draw_quad({ ... });   // tantos como se quiera: no dibujan, acumulan
	//     r2d.end();                // UN solo draw para todo el lote
	//     frame->commands().end_rendering();
	//
	// begin_rendering/end_rendering siguen siendo de la aplicación: así puede
	// mezclar el Renderer2D con sus propios draws o con otros renderers.
	class Renderer2D
	{
	public:
		// color_format: el de la swapchain (o la textura) donde se va a dibujar.
		// El pipeline se crea para ese formato concreto.
		Renderer2D(Device& device, Format color_format);
		~Renderer2D();

		// No copiable: tiene recursos de GPU y un lote a medias. (Tampoco movible
		// por ahora: no hace falta y así no hay que pensar en estados "movidos".)
		Renderer2D(const Renderer2D&) = delete;
		Renderer2D& operator=(const Renderer2D&) = delete;

		// Empieza un lote. view_proj es la matriz que lleva del mundo al NDC; se
		// recibe una matriz y no una Camera2D para que la UI pueda pasar su propia
		// proyección en píxeles. Lanza si ya había un lote abierto.
		void begin(Frame& frame, const math::Matrix4x4& view_proj);

		// Empieza un lote de UI, en píxeles: (0,0) es la esquina de ARRIBA a la
		// izquierda y la Y crece hacia ABAJO. width y height son el tamaño de donde se
		// dibuja (el extent de la swapchain). No recibe una matriz a propósito: la
		// proyección y el sentido de la Y van siempre juntos y no se pueden descuadrar.
		// Lo demás no cambia: las imágenes y las letras salen derechas, y un giro
		// positivo se sigue VIENDO antihorario. Se cierra igual, con end().
		// Lanza logic_error si ya había un lote abierto e invalid_argument si el
		// tamaño no es positivo.
		//
		//     r2d.begin(*frame, camera.view_proj_matrix());   // el juego
		//     r2d.end();
		//     r2d.begin_ui(*frame, 1280, 720);                // encima, la UI
		//     r2d.end();
		void begin_ui(Frame& frame, float width, float height);

		// Añade un quad al lote. No toca la GPU: solo escribe en un vector en CPU.
		void draw_quad(const Quad& quad);

		// Añade un rectángulo redondeado. Va en el mismo lote que los quads y las
		// letras: es un quad más, con un "estilo" que el pixel shader usa para
		// recortar las esquinas y pintar el borde (ver style_table.h y quad_ps.hlsl).
		void draw_rounded_rect(const RoundedRect& rect);

		// Recorte: lo que se añada entre push y pop (quads, letras, rectángulos) solo
		// se ve dentro de este rectángulo. Se pueden anidar: un recorte dentro de
		// otro deja ver solo la parte común. No corta el lote y es compatible con
		// layer, porque cada quad se lleva su recorte.
		//
		//     r2d.push_clip_rect({ 20, 20 }, { 200, 300 });   // el hueco de la lista
		//     ...filas y textos, aunque se salgan...
		//     r2d.pop_clip_rect();
		//
		// Los quads que quedan fuera del todo ni se envían (no cuentan en
		// last_batch_quad_count). Lanzan logic_error fuera de begin/end; pop, además,
		// si no hay ningún recorte; y end(), si queda alguno sin cerrar.
		void push_clip_rect(math::Vector2f top_left, math::Vector2f size);
		void pop_clip_rect();

		// Añade un texto (UTF-8) al lote: un quad por letra, con la textura del
		// atlas de la fuente. Las letras van en el MISMO lote que el resto
		// (bindless), así que un HUD no cuesta draws extra.
		//   top_left: esquina de arriba a la izquierda de la primera línea, en
		//             unidades del lote (mundo con begin, píxeles con begin_ui).
		//   size:     alto de una línea, en esas mismas unidades (la escala de la fuente).
		// '\n' empieza una línea nueva debajo.
		// Ojo: guarda el índice bindless del atlas, así que la fuente debe vivir
		// hasta que se dibuje el frame (como cualquier textura).
		void draw_text(const BitmapFont& font, std::u8string_view text, math::Vector2f top_left, float size,
		               Color color = {}, int32_t layer = 0);

		// Cierra el lote: ordena los quads por capa (estable), los copia a memoria
		// de GPU (Frame::allocate) y los dibuja. Un draw por cada
		// k_max_quads_per_draw quads (casi siempre, uno solo). Debe llamarse
		// entre begin_rendering y end_rendering. Lanza si no había un lote abierto.
		void end();

		// Máximo de quads por draw: los que caben en una reserva del asignador
		// por frame (k_frame_allocation_max_size / 64 bytes = 16.384).
		static const uint32_t k_max_quads_per_draw;

		// Estadísticas del último lote cerrado (para el título de test_1 y tests).
		uint32_t last_batch_quad_count() const noexcept { return m_last_batch_quad_count; }
		uint32_t last_batch_draw_count() const noexcept { return m_last_batch_draw_count; }

	private:
		// Lo común de begin y begin_ui.
		void open_batch(Frame& frame, const math::Matrix4x4& view_proj, float y_sign);

		// El registro que llega a la GPU (64 bytes). Se define en el .cpp: es un
		// detalle de implementación que tiene que coincidir con el shader, y la
		// aplicación no necesita verlo. Un std::vector de un tipo incompleto
		// es válido como miembro desde C++17 mientras el tipo esté completo donde
		// se USA el vector (constructor, destructor, push_back...), es decir, en
		// el .cpp. Por eso el destructor se define allí y no aquí con "= default".
		struct QuadInstance;

		// Lo que un sprite no necesita (radio...), en una tabla aparte. También
		// llega a la GPU y también se define en el .cpp.
		struct StyleRecord;

		// Lo común de draw_quad y draw_rounded_rect: aplica el recorte y añade el quad.
		void push_quad(const Quad& quad, StyleRecord style);

		Ref<Device> m_device;                    // mantiene vivo el device mientras exista el renderer
		Ref<GraphicsPipeline> m_pipeline;
		Ref<Texture> m_white_texture;            // 1x1 blanca: color * blanco = color
		Ref<Sampler> m_default_sampler;

		// El lote en CPU. Todos se reutilizan de un frame a otro (clear() no
		// libera memoria), así que tras los primeros frames no se reserva nada.
		std::vector<QuadInstance> m_instances;   // en orden de envío
		std::vector<int32_t> m_layers;           // capa de cada quad (paralelo a m_instances)
		std::vector<uint32_t> m_order;           // orden de dibujo, si hubo que reordenar
		std::vector<QuadInstance> m_sorted;      // los quads ya reordenados, si hubo que reordenar
		std::vector<StyleRecord> m_styles;       // tabla de estilos del lote entero
		std::vector<StyleRecord> m_draw_styles;  // los que usa el draw que se está preparando
		std::vector<uint32_t> m_style_remap;     // memoria de trabajo de compact_styles
		std::vector<detail::ClipRect> m_clip_stack;   // el de arriba es el recorte activo (ya intersecado)

		Frame* m_frame{ nullptr };               // != nullptr <=> hay un lote abierto
		math::Matrix4x4 m_view_proj{};
		float m_y_sign{ 1.0f };                  // +1: la Y crece hacia arriba (begin); -1: hacia abajo (begin_ui)
		uint32_t m_last_batch_quad_count{ 0 };
		uint32_t m_last_batch_draw_count{ 0 };
	};

} // namespace luxel
