// Pruebas de la convencion de ejes de luxel (hito R1c).
//
// Contrato que se fija aqui:
//   - Mundo (Camera2D):  +Y hacia arriba; angulo positivo = giro antihorario.
//   - NDC de luxel:      +Y hacia arriba (como D3D12, OpenGL y Metal). El backend
//                        de Vulkan lo cumple con un viewport de altura negativa.
//   - Pixeles:           (0,0) arriba a la izquierda, +Y hacia abajo.
//
// Solo usa la API publica (luxel/cameras.h y luxel/math): no necesita GPU.
// Lo que NO puede comprobar es el viewport negativo de Vulkan; eso se ve en
// test_1 (el quad sale derecho y gira en sentido antihorario).

#include <luxel/cameras.h>
#include <luxel/math/vectors.h>
#include <luxel/math/matrix4.h>
#include <iostream>
#include <cmath>
#include <numbers>

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what, int line)
	{
		if (!condition) {
			++g_failures;
			std::cout << "  FALLO (linea " << line << "): " << what << "\n";
		}
	}

	bool near(float a, float b, float eps = 1e-3f)
	{
		return std::fabs(a - b) <= eps;
	}

	bool near(const luxel::math::Vector2f& a, const luxel::math::Vector2f& b, float eps = 1e-3f)
	{
		return near(a.x, b.x, eps) && near(a.y, b.y, eps);
	}
}

#define CHECK(cond) check((cond), #cond, __LINE__)

using luxel::math::Vector2f;
using luxel::math::Vector4f;
using luxel::math::Matrix4x4;

// Camara de 8 x 4 unidades sobre una ventana de 800 x 400 pixeles:
// 100 pixeles por unidad, mundo de x en [-4, 4] e y en [-2, 2].
constexpr float k_world_w = 8.0f;
constexpr float k_world_h = 4.0f;
const Vector2f k_viewport{ 800.0f, 400.0f };

// 1. Las esquinas del mundo caen en las esquinas de la ventana que tocan.
static void test_world_corners_to_screen()
{
	std::cout << "[1] world_to_screen: esquinas y centro\n";
	luxel::Camera2D camera{ k_world_w, k_world_h };

	CHECK(near(camera.world_to_screen({ -4.0f,  2.0f }, k_viewport), Vector2f{   0.0f,   0.0f }));  // arriba izquierda
	CHECK(near(camera.world_to_screen({  4.0f,  2.0f }, k_viewport), Vector2f{ 800.0f,   0.0f }));  // arriba derecha
	CHECK(near(camera.world_to_screen({ -4.0f, -2.0f }, k_viewport), Vector2f{   0.0f, 400.0f }));  // abajo izquierda
	CHECK(near(camera.world_to_screen({  4.0f, -2.0f }, k_viewport), Vector2f{ 800.0f, 400.0f }));  // abajo derecha
	CHECK(near(camera.world_to_screen({  0.0f,  0.0f }, k_viewport), Vector2f{ 400.0f, 200.0f }));  // centro
}

// 2. Subir en el mundo (+Y) es subir en pantalla (pixel Y mas pequeno).
static void test_world_up_is_screen_up()
{
	std::cout << "[2] +Y del mundo va hacia arriba en pantalla\n";
	luxel::Camera2D camera{ k_world_w, k_world_h };

	const Vector2f below = camera.world_to_screen({ 0.0f, -1.0f }, k_viewport);
	const Vector2f above = camera.world_to_screen({ 0.0f,  1.0f }, k_viewport);
	CHECK(above.y < below.y);
	CHECK(near(above.y, 100.0f));
	CHECK(near(below.y, 300.0f));
}

// 3. Un clic en una esquina de la ventana da la esquina del mundo que se ve ahi.
static void test_screen_corners_to_world()
{
	std::cout << "[3] screen_to_world: esquinas y centro\n";
	luxel::Camera2D camera{ k_world_w, k_world_h };

	CHECK(near(camera.screen_to_world({   0.0f,   0.0f }, k_viewport), Vector2f{ -4.0f,  2.0f }));
	CHECK(near(camera.screen_to_world({ 800.0f, 400.0f }, k_viewport), Vector2f{  4.0f, -2.0f }));
	CHECK(near(camera.screen_to_world({ 400.0f, 200.0f }, k_viewport), Vector2f{  0.0f,  0.0f }));
	CHECK(near(camera.screen_to_world({ 400.0f, 100.0f }, k_viewport), Vector2f{  0.0f,  1.0f }));
}

// 4. La camara movida: el punto al que mira queda en el centro de la ventana.
static void test_camera_position()
{
	std::cout << "[4] la posicion de la camara queda en el centro\n";
	luxel::Camera2D camera{ k_world_w, k_world_h };
	camera.set_position({ 10.0f, 5.0f });

	CHECK(near(camera.world_to_screen({ 10.0f, 5.0f }, k_viewport), Vector2f{ 400.0f, 200.0f }));
	CHECK(near(camera.world_to_screen({ 10.0f, 6.0f }, k_viewport), Vector2f{ 400.0f, 100.0f }));  // 1 unidad mas arriba
	CHECK(near(camera.screen_to_world({   0.0f, 0.0f }, k_viewport), Vector2f{  6.0f, 7.0f }));
}

// 5. Zoom 2: cada unidad del mundo ocupa el doble de pixeles.
static void test_camera_zoom()
{
	std::cout << "[5] zoom 2 duplica los pixeles por unidad\n";
	luxel::Camera2D camera{ k_world_w, k_world_h };
	camera.set_zoom(2.0f);

	CHECK(near(camera.world_to_screen({ 1.0f, 0.0f }, k_viewport), Vector2f{ 600.0f, 200.0f }));
	CHECK(near(camera.world_to_screen({ 0.0f, 1.0f }, k_viewport), Vector2f{ 400.0f,   0.0f }));
}

// 6. Ida y vuelta: screen_to_world(world_to_screen(p)) == p con la camara
//    movida, girada y con zoom.
static void test_round_trip()
{
	std::cout << "[6] ida y vuelta mundo -> pantalla -> mundo\n";
	luxel::Camera2D camera{ k_world_w, k_world_h };
	camera.set_position({ -3.0f, 1.5f });
	camera.set_rotation(0.7f);
	camera.set_zoom(1.5f);

	const Vector2f points[] = { { 0.0f, 0.0f }, { -3.0f, 1.5f }, { 2.5f, -1.25f }, { -7.0f, 4.0f } };
	for (const auto& p : points) {
		const Vector2f screen = camera.world_to_screen(p, k_viewport);
		CHECK(near(camera.screen_to_world(screen, k_viewport), p));
	}
}

// 7. rotateZ con angulo positivo gira en sentido antihorario:
//    el eje +X pasa a +Y y el eje +Y pasa a -X.
static void test_rotate_z_counter_clockwise()
{
	std::cout << "[7] rotateZ positivo gira en sentido antihorario\n";
	const Matrix4x4 r = Matrix4x4::rotateZ(std::numbers::pi_v<float> * 0.5f);

	const Vector4f x = r * Vector4f{ 1.0f, 0.0f, 0.0f, 1.0f };
	const Vector4f y = r * Vector4f{ 0.0f, 1.0f, 0.0f, 1.0f };
	CHECK(near(x.x, 0.0f) && near(x.y,  1.0f));
	CHECK(near(y.x, -1.0f) && near(y.y, 0.0f));
}

int main()
{
	test_world_corners_to_screen();
	test_world_up_is_screen_up();
	test_screen_corners_to_world();
	test_camera_position();
	test_camera_zoom();
	test_round_trip();
	test_rotate_z_counter_clockwise();

	std::cout << (g_failures == 0 ? "\nTODO OK\n" : "\nHAY FALLOS\n");
	return g_failures == 0 ? 0 : 1;
}
