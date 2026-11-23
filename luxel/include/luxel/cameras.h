#pragma once

#include <luxel/math/vectors.h>
#include <luxel/math/matrix4.h>

namespace luxel
{
	class Camera2D
	{
	public:
		Camera2D(float left, float right, float bottom, float top);
		Camera2D(float width, float height);
		~Camera2D() = default;

		void set_projection(float left, float right, float bottom, float top);
		void set_viewport_size(float width, float height);
		void set_position(const math::Vector2f& position);
		void set_rotation(float rotation_radians);
		void set_zoom(float zoom);

		const math::Matrix4x4& view_matrix() const noexcept;
		const math::Matrix4x4& view_proj_matrix() const noexcept;

		math::Vector2f screen_to_world(const math::Vector2f& screen_pos, const math::Vector2f& viewport_size) const;
		math::Vector2f world_to_screen(const math::Vector2f& world_pos, const math::Vector2f& viewport_size) const;

	private:
		void update_view_projection_matrix() const;

		math::Matrix4x4   m_projection_matrix{ math::Matrix4x4::Identity() };
		mutable math::Matrix4x4 m_view_matrix{ math::Matrix4x4::Identity() };
		mutable math::Matrix4x4 m_view_projection_matrix{ math::Matrix4x4::Identity() };

		math::Vector2f m_position{ 0.f, 0.f };
		float m_rotation{ 0.f };
		float     m_zoom{ 1.f };
		mutable bool m_is_dirty{ true };
	};

	class Camera3D
	{
	public:
		Camera3D(float fovDegrees, float width, float height, float nearClip = 0.1f, float farClip = 1000.0f);
		~Camera3D() = default;

		void set_position(const math::Vector3f& pos) { m_position = pos; m_is_dirty = true; }
		void set_rotation(const math::Vector3f& rot) { m_rotation = rot; m_is_dirty = true; } // Pitch (x), Yaw (y), Roll (z), en radianes
		void set_aspect_ratio(float width, float height);
		void set_viewport_size(float width, float height);

		const math::Matrix4x4& view_proj_matrix() const;

		// El movimiento con teclado u otra entrada no es de la cámara: lo hace un
		// controlador de la aplicación leyendo position()/rotation() y llamando a set_position().
		const math::Vector3f& position() const { return m_position; }
		const math::Vector3f& rotation() const { return m_rotation; } // radianes

	private:
		void recalculate_matrices() const;

		math::Vector3f m_position{ 0.0f, 0.0f, 5.0f }; // Empezamos a 5 unidades de distancia en el eje Z
		math::Vector3f m_rotation{ 0.0f, 0.0f, 0.0f };

		float m_fov;
		float m_aspect;
		float m_near;
		float m_far;

		mutable math::Matrix4x4 m_view_matrix{ math::Matrix4x4::Identity() };
		mutable math::Matrix4x4 m_proj_matrix{ math::Matrix4x4::Identity() };
		mutable math::Matrix4x4   m_view_proj{ math::Matrix4x4::Identity() };
		mutable bool               m_is_dirty{ true };
	};

} // namespace luxel
