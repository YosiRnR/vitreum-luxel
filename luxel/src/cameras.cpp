#include <luxel/cameras.h>
#include <numbers>
#include <cmath>

namespace luxel
{
	Camera2D::Camera2D(float left, float right, float bottom, float top) {
		set_projection(left, right, bottom, top);
	}

	Camera2D::Camera2D(float width, float height)
		: Camera2D(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f)
	{
	}

	void Camera2D::set_projection(float left, float right, float bottom, float top)
	{
		m_projection_matrix = math::Matrix4x4::orthographic(left, right, bottom, top);
		m_is_dirty = true;
	}

	void Camera2D::set_viewport_size(float width, float height) {
		set_projection(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f);
	}

	void Camera2D::set_position(const math::Vector2f& position)
	{
		m_position = position;
		m_is_dirty = true;
	}

	void Camera2D::set_rotation(float rotation_radians)
	{
		m_rotation = rotation_radians;
		m_is_dirty = true;
	}

	void Camera2D::set_zoom(float zoom)
	{
		m_zoom = (zoom < 0.001f) ? 0.001f : zoom; // Prevenir zoom negativo o cero
		m_is_dirty = true;
	}

	void Camera2D::update_view_projection_matrix() const
	{
		if (!m_is_dirty) {
			return;
		}

		// Matriz de transformación: traslación, rotación y escalado (zoom)
		math::Matrix4x4 transform = math::Matrix4x4::translate({ -m_position.x, -m_position.y, 0.0f });
		if (m_rotation != 0.0f) {
			transform = math::Matrix4x4::rotateZ(-m_rotation) * transform; // Rotación en sentido antihorario
		}
		if (m_zoom != 1.0f) {
			transform = math::Matrix4x4::scale({ m_zoom, m_zoom, 1.0f }) * transform; // Escalado (zoom)
		}

		m_view_matrix = transform;
		m_view_projection_matrix = m_projection_matrix * m_view_matrix;

		m_is_dirty = false;
	}

	const math::Matrix4x4& Camera2D::view_matrix() const noexcept
	{
		if (m_is_dirty) {
			update_view_projection_matrix();
		}
		return m_view_matrix;
	}
	const math::Matrix4x4& Camera2D::view_proj_matrix() const noexcept
	{
		if (m_is_dirty) {
			update_view_projection_matrix();
		}
		return m_view_projection_matrix;
	}

	math::Vector2f Camera2D::screen_to_world(const math::Vector2f& screen_pos, const math::Vector2f& viewport_size) const
	{
		// 1. Convertir píxeles de pantalla (0..Width, 0..Height) a Coordenadas Normalizadas (NDC) [-1..1]
		float ndc_x = (2.0f * screen_pos.x) / viewport_size.x - 1.0f;
		float ndc_y = 1.0f - (2.0f * screen_pos.y) / viewport_size.y; // Invertir Y si origen está arriba-izquierda

		// 2. Multiplicar por la inversa de la matriz ViewProjection
		math::Matrix4x4 inv_view_proj = view_proj_matrix().Inverted();
		math::Vector4f world_pos = inv_view_proj * math::Vector4f(ndc_x, ndc_y, 0.0f, 1.0f);

		return math::Vector2f(world_pos.x, world_pos.y);
	}

	math::Vector2f Camera2D::world_to_screen(const math::Vector2f& world_pos, const math::Vector2f& viewport_size) const
	{
		// 1. Multiplicar por la matriz ViewProjection
		math::Vector4f clip_space_pos = view_proj_matrix() * math::Vector4f(world_pos.x, world_pos.y, 0.0f, 1.0f);
		// 2. Convertir de Clip Space a NDC [-1..1]
		math::Vector3f ndc_pos = clip_space_pos.toVector3(); // Dividir por w si es necesario
		// 3. Convertir de NDC a píxeles de pantalla
		float screen_x = ((ndc_pos.x + 1.0f) / 2.0f) * viewport_size.x;
		float screen_y = ((1.0f - ndc_pos.y) / 2.0f) * viewport_size.y; // Invertir Y si origen está arriba-izquierda
		return math::Vector2f(screen_x, screen_y);
	}




	Camera3D::Camera3D(float fovDegrees, float width, float height, float nearClip, float farClip)
		: m_fov(fovDegrees* std::numbers::pi_v<float> / 180.0f), m_near(nearClip), m_far(farClip)
	{
		m_aspect = width / height;
		m_proj_matrix = math::Matrix4x4::perspective(m_fov, m_aspect, m_near, m_far);
	}

	void Camera3D::set_aspect_ratio(float width, float height)
	{
		m_aspect = width / height;
		m_proj_matrix = math::Matrix4x4::perspective(m_fov, m_aspect, m_near, m_far);
		m_is_dirty = true;
	}

	void Camera3D::recalculate_matrices() const
	{
		// 1. Matriz de Vista: Invertimos la posición y rotación de la cámara para mover el mundo
		math::Matrix4x4 translation = math::Matrix4x4::translate({ -m_position.x, -m_position.y, -m_position.z });

		// Combinamos las rotaciones en 3D (Simplificado: puedes usar tus métodos rotateX/Y/Z combinados)
		math::Matrix4x4 rotation = math::Matrix4x4::rotateY(-m_rotation.y) * math::Matrix4x4::rotateX(-m_rotation.x);

		m_view_matrix = rotation * translation;

		// 2. Combinación definitiva: Proyección * Vista
		m_view_proj = m_proj_matrix * m_view_matrix;
		m_is_dirty = false;
	}

	const math::Matrix4x4& Camera3D::view_proj_matrix() const
	{
		if (m_is_dirty) {
			recalculate_matrices();
		}
		return m_view_proj;
	}

	void Camera3D::set_viewport_size(float width, float height)
	{
		// Evitamos una división por cero si la ventana se minimiza del todo
		if (height <= 0.0f) height = 1.0f;

		m_aspect = width / height;

		// 🚀 RECALCULAMOS LA PROYECCIÓN DE PERSPECTIVA CON EL NUEVO ASPECT RATIO
		m_proj_matrix = math::Matrix4x4::perspective(m_fov, m_aspect, m_near, m_far);

		// Encendemos la alerta para que la matriz combinada se actualice en el próximo frame
		m_is_dirty = true;
	}

} // namespace luxel
