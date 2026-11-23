#pragma once

#include <luxel/cameras.h>
#include <luxel/math/vectors.h>
#include <cmath>

// Controlador de cámara libre: traduce órdenes de movimiento (teclado u otra
// entrada) en cambios de posición de una Camera3D. Es lógica de juego, no de
// render, así que vive en la aplicación y solo usa la API pública de la cámara
// (position(), rotation() y set_position()). Cuando exista una capa de motor
// por encima de luxel, este es el tipo de cosa que se muda allí.
//
// Guarda una referencia a la cámara: la cámara debe vivir más que el controlador.
class FlyCameraController
{
public:
	explicit FlyCameraController(luxel::Camera3D& camera, float speed = 10.0f)
		: m_camera(camera)
		, m_speed(speed)
	{
	}

	void set_speed(float speed) { m_speed = speed; }

	// Se mueven sobre el plano horizontal según el Yaw de la cámara; subir y bajar usan el eje Y del mundo.
	void move_forward(float dt)  { translate(flat_forward() * (m_speed * dt)); }
	void move_backward(float dt) { translate(flat_forward() * (-m_speed * dt)); }
	void move_right(float dt)    { translate(flat_right() * (m_speed * dt)); }
	void move_left(float dt)     { translate(flat_right() * (-m_speed * dt)); }
	void move_up(float dt)       { translate({ 0.0f, m_speed * dt, 0.0f }); }
	void move_down(float dt)     { translate({ 0.0f, -m_speed * dt, 0.0f }); }

private:
	luxel::math::Vector3f flat_forward() const
	{
		const float yaw = m_camera.rotation().y;
		return { std::sin(yaw), 0.0f, -std::cos(yaw) };
	}

	luxel::math::Vector3f flat_right() const
	{
		const float yaw = m_camera.rotation().y;
		return { std::cos(yaw), 0.0f, std::sin(yaw) };
	}

	void translate(const luxel::math::Vector3f& delta) {
		m_camera.set_position(m_camera.position() + delta);
	}

	luxel::Camera3D& m_camera;
	float m_speed;
};
