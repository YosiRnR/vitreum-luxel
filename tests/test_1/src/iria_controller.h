#pragma once

#include <algorithm>
#include <cstdint>

// Controlador de Iria: lógica de juego (movimiento, gravedad, salto y qué
// fotograma de la hoja toca). No sabe nada de render ni de teclado: recibe
// "quiero ir a la izquierda / derecha / saltar" y expone posición, hacia dónde
// mira y la celda de la hoja. La escena lo dibuja con el Renderer2D.
// (Mismo criterio que FlyCameraController: cuando haya capa de motor, se muda.)

// ---------------------------------------------------------------------------
// Hoja de sprites
// ---------------------------------------------------------------------------

// Iria-sprite-sheet.png: 1536x2048, rejilla de 6 columnas x 8 filas de celdas
// de 256x256. Los fotogramas se numeran por filas desde arriba a la izquierda
// (la celda n está en la columna n % 6 y la fila n / 6). Cada animación es un
// rango de celdas consecutivas; las 2 últimas (46 y 47) están vacías. Iria
// corre mirando a la DERECHA y tiene los pies casi en el borde de abajo de la
// celda: el quad se apoya en el suelo por su lado de abajo.
constexpr uint32_t k_iria_columns = 6;
constexpr uint32_t k_iria_rows    = 8;

struct SpriteAnimation
{
	uint32_t first_cell;   // primera celda de la hoja
	uint32_t frame_count;  // cuántas celdas seguidas
	float fps;             // fotogramas por segundo
};

// Celda que toca mostrar de una animación tras "seconds" segundos reproduciéndola.
// loop = false: se queda en el último fotograma al terminar.
inline uint32_t animation_cell(const SpriteAnimation& anim, float seconds, bool loop = true)
{
	uint32_t frame = static_cast<uint32_t>(std::max(seconds, 0.0f) * anim.fps);
	frame = loop ? frame % anim.frame_count : std::min(frame, anim.frame_count - 1);
	return anim.first_cell + frame;
}

constexpr SpriteAnimation k_iria_run { .first_cell =  0, .frame_count = 21, .fps = 24.0f };
constexpr SpriteAnimation k_iria_idle{ .first_cell = 21, .frame_count = 13, .fps = 10.0f };

// El salto (celdas 34-45) no se reproduce de corrido: se trocea por fases,
// porque cuánto dura en el aire lo decide la física, no la animación.
//   34-35 agacharse para coger impulso      -> k_iria_jump_crouch
//   36-39 en el aire (subiendo ... bajando) -> se elige según la velocidad vertical
//   40-41 aterrizar (agachada)              -> k_iria_jump_land
//   42-45 erguirse de frente (no se usa: rompería la carrera, que es de perfil)
constexpr SpriteAnimation k_iria_jump_crouch{ .first_cell = 34, .frame_count = 2, .fps = 14.0f };
constexpr uint32_t        k_iria_jump_air_first = 36;
constexpr SpriteAnimation k_iria_jump_land  { .first_cell = 40, .frame_count = 2, .fps = 14.0f };

// ---------------------------------------------------------------------------
// Controlador
// ---------------------------------------------------------------------------

struct IriaInput
{
	bool left{ false };
	bool right{ false };
	bool jump{ false };   // pulsado ESTE frame (no "mantenido")
};

class IriaController
{
public:
	// Parámetros de juego, en unidades del mundo y segundos.
	static constexpr float k_run_speed     = 1.3f;    // unidades por segundo
	static constexpr float k_jump_height   = 0.55f;   // altura máxima del salto
	static constexpr float k_time_to_apex  = 0.35f;   // segundos hasta lo más alto
	// Con altura h y tiempo t hasta el punto más alto (movimiento con
	// aceleración constante): gravedad g = 2h / t² y velocidad inicial v0 = 2h / t.
	// Elegir h y t y deducir g y v0 es más fácil de ajustar "a ojo" que tocar g
	// directamente: "salta así de alto y tarda esto en subir".
	static constexpr float k_gravity       = 2.0f * k_jump_height / (k_time_to_apex * k_time_to_apex);
	static constexpr float k_jump_velocity = 2.0f * k_jump_height / k_time_to_apex;

	// Avanza la simulación dt segundos. min_x / max_x: límites del centro de
	// Iria (la escena los calcula con el ancho visible para que no se salga).
	void update(float dt, const IriaInput& input, float min_x, float max_x)
	{
		m_state_time += dt;

		// Dirección pedida: -1 izquierda, +1 derecha, 0 nada (o las dos a la vez).
		const float direction = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f);
		if (direction != 0.0f) {
			m_facing_left = direction < 0.0f;
		}

		switch (m_state)
		{
		case State::Ground:
		{
			const bool moving = direction != 0.0f;
			if (moving != m_moving) {
				m_moving = moving;
				m_state_time = 0.0f;   // al pasar de quieta a correr (o al revés), la animación empieza en su fotograma 0
			}
			m_x += direction * k_run_speed * dt;

			if (input.jump) {
				enter(State::Crouch);
			}
			break;
		}
		case State::Crouch:
			// Se agacha sin moverse y, al acabar los 2 fotogramas, despega.
			if (m_state_time >= crouch_duration()) {
				m_velocity_y = k_jump_velocity;
				enter(State::Air);
			}
			break;

		case State::Air:
			// En el aire se puede corregir la dirección (control aéreo, típico
			// de los plataformas). Gravedad: la velocidad baja, la altura sigue
			// a la velocidad (integración de Euler semiimplícita: primero v, luego y).
			m_x += direction * k_run_speed * dt;
			m_velocity_y -= k_gravity * dt;
			m_height += m_velocity_y * dt;

			if (m_height <= 0.0f) {
				m_height = 0.0f;
				m_velocity_y = 0.0f;
				enter(State::Land);
			}
			break;

		case State::Land:
			if (m_state_time >= land_duration()) {
				m_moving = false;
				enter(State::Ground);
			}
			break;
		}

		m_x = std::clamp(m_x, min_x, max_x);
	}

	float x() const { return m_x; }
	float height() const { return m_height; }        // sobre el suelo (0 = de pie)
	bool facing_left() const { return m_facing_left; }

	// Celda de la hoja que toca dibujar ahora.
	uint32_t cell() const
	{
		switch (m_state)
		{
		case State::Ground:
			return animation_cell(m_moving ? k_iria_run : k_iria_idle, m_state_time);
		case State::Crouch:
			return animation_cell(k_iria_jump_crouch, m_state_time, false);
		case State::Air:
		{
			// 4 fotogramas para el vuelo, elegidos por la velocidad vertical:
			// subiendo rápido, subiendo, bajando, bajando rápido.
			const float v = m_velocity_y / k_jump_velocity;   // de +1 (despegue) a -1 (caída)
			const uint32_t frame = v > 0.5f ? 0 : v > 0.0f ? 1 : v > -0.5f ? 2 : 3;
			return k_iria_jump_air_first + frame;
		}
		case State::Land:
			return animation_cell(k_iria_jump_land, m_state_time, false);
		}
		return k_iria_idle.first_cell;
	}

private:
	enum class State
	{
		Ground,   // de pie o corriendo
		Crouch,   // cogiendo impulso para saltar
		Air,      // en el aire
		Land      // aterrizando
	};

	static constexpr float crouch_duration() { return k_iria_jump_crouch.frame_count / k_iria_jump_crouch.fps; }
	static constexpr float land_duration()   { return k_iria_jump_land.frame_count / k_iria_jump_land.fps; }

	void enter(State state)
	{
		m_state = state;
		m_state_time = 0.0f;
	}

	State m_state{ State::Ground };
	float m_state_time{ 0.0f };   // segundos en el estado actual (sirve de reloj de la animación)
	bool m_moving{ false };       // en Ground: corriendo o quieta

	float m_x{ 0.0f };
	float m_height{ 0.0f };
	float m_velocity_y{ 0.0f };
	bool m_facing_left{ false };
};
