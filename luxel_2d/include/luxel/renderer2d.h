#pragma once

// Renderer2D: dibuja quads (rectángulos con color y/o textura) agrupados en
// lotes. Diseño y decisiones en docs/renderers-design.md ("Decisiones de R2").
//
// Solo usa la API pública de luxel: es una biblioteca aparte (luxel_2d).

#include <luxel/device.h>
#include <luxel/math/vectors.h>
#include <luxel/math/matrix4.h>
#include <cstdint>
#include <vector>

namespace luxel
{
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
		math::Vector2f position{ 0.0f, 0.0f };  // CENTRO del quad, en unidades del mundo
		math::Vector2f size{ 1.0f, 1.0f };      // ancho y alto, en unidades del mundo
		float rotation{ 0.0f };                 // radianes, positivo = antihorario (+Y arriba)
		Color color{};                          // se multiplica por el color de la textura
		Texture* texture{ nullptr };            // nullptr = textura blanca 1x1 => color liso
		Sampler* sampler{ nullptr };            // nullptr = sampler lineal por defecto del renderer
		math::Vector2f uv_min{ 0.0f, 0.0f };    // UV de la esquina de ARRIBA a la izquierda
		math::Vector2f uv_max{ 1.0f, 1.0f };    // UV de la esquina de ABAJO a la derecha
		int32_t layer{ 0 };                     // orden de dibujo (de momento sin usar: R2d)
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

		// Añade un quad al lote. No toca la GPU: solo escribe en un vector en CPU.
		void draw_quad(const Quad& quad);

		// Cierra el lote: copia los quads a memoria de GPU (Frame::allocate) y
		// los dibuja con un único draw. Debe llamarse entre begin_rendering y
		// end_rendering. Lanza si no había un lote abierto.
		void end();

		// Cuántos quads tenía el último lote cerrado (para estadísticas/tests).
		uint32_t last_batch_quad_count() const noexcept { return m_last_batch_quad_count; }

	private:
		// El registro que llega a la GPU (64 bytes). Se define en el .cpp: es un
		// detalle de implementación que tiene que coincidir con el shader, y la
		// aplicación no necesita verlo. Un std::vector de un tipo incompleto
		// es válido como miembro desde C++17 mientras el tipo esté completo donde
		// se USA el vector (constructor, destructor, push_back...), es decir, en
		// el .cpp. Por eso el destructor se define allí y no aquí con "= default".
		struct QuadInstance;

		Ref<Device> m_device;                    // mantiene vivo el device mientras exista el renderer
		Ref<GraphicsPipeline> m_pipeline;
		Ref<Texture> m_white_texture;            // 1x1 blanca: color * blanco = color
		Ref<Sampler> m_default_sampler;

		std::vector<QuadInstance> m_instances;   // el lote en CPU; se reutiliza (no libera memoria entre frames)

		Frame* m_frame{ nullptr };               // != nullptr <=> hay un lote abierto
		math::Matrix4x4 m_view_proj{};
		uint32_t m_last_batch_quad_count{ 0 };
	};

} // namespace luxel
