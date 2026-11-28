#include "rasterizer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace luxel::ttf::detail
{
	// ------------------------------------------------------------------------
	// El corazón del rasterizador (escrito por el usuario en T4).
	//
	// Un trozo de recta cruza UNA fila de píxeles: entra por arriba en x_top y sale
	// por abajo en x_bottom. Su altura dentro de la fila es |height| (1 si la cruza
	// entera) y el signo dice si baja (+) o sube (-).
	//
	// Lo que hay que dejar en "row": los CAMBIOS de cobertura, de forma que la suma
	// acumulada row[0] + ... + row[i] crezca, en cada píxel i, en
	//
	//         height * (la parte del área del píxel i que queda A LA DERECHA del trozo)
	//
	// Escenarios, con height = 1 (los números son las sumas acumuladas por píxel):
	//
	//   vertical en x = 2      | 0 | 0 | 1 | 1 |    la frontera: lo de la derecha, entero
	//   vertical en x = 1,5    | 0 |0,5| 1 | 1 |    el píxel 1, partido por la mitad
	//   de x = 1,2 a x = 1,6   | 0 |0,6| 1 | 1 |    un trapecio: 2 - 1,4 (su x media)
	//   de x = 0   a x = 2     |1/4|3/4| 1 | 1 |    dos píxeles, cada uno con un triángulo
	//
	// Pistas, por orden:
	//  1. Empieza por el caso de un solo píxel (los tres primeros escenarios). ¿Qué
	//     píxel toca? ¿Cuánto le queda a la derecha? Fíjate en lo que pasa con la
	//     casilla de después: si en el píxel i la suma crece en "algo", la suma de los
	//     siguientes debe crecer en height entero. ¿Cuánto le falta a la casilla i + 1?
	//  2. Varios píxeles: recórrelos de izquierda a derecha y en cada uno calcula su
	//     "parte a la derecha" (la que acabará en la suma acumulada). Lo que dejas en
	//     row[i] es la DIFERENCIA con la del píxel anterior.
	//  3. ¿Cuánto del píxel i queda a la derecha de una recta inclinada? Dibuja el
	//     escenario de x = 0 a x = 2 en papel. Cuando lo veas, piensa en cómo hacerlo
	//     en general (por ejemplo, con la media de algo a lo largo del trozo).
	//
	// Lo prueba el caso 13 de test_ttf (escenarios sueltos y 500 trozos al azar contra
	// la cuenta a lo bruto); el 14 y el 15 rasterizan formas y letras con tu función.
	// ------------------------------------------------------------------------

	void accumulate_piece(std::span<float> row, float x_top, float x_bottom, float height)
	{
		if (row.empty() || height == 0.0f) {
			return;
		}

		const float dx = (x_bottom - x_top); // Desplazamiento horizontal de la recta
		float prev_area{ 0.0f };

		const float min_x = std::min(x_top, x_bottom);
		const float max_x = std::max(x_top, x_bottom);

		const float clamped_start = std::max(std::floor(min_x), 0.0f);
		const float clamped_end   = std::clamp(std::floor(max_x) + 1.0f, 0.0f, static_cast<float>(row.size()));

		const size_t start_x = static_cast<size_t>(clamped_start);
		const size_t end_x   = static_cast<size_t>(clamped_end);


		for (size_t i = start_x; i < end_x; ++i)
		{
			const float  left_side{ static_cast<float>(i) };
			const float right_side{ left_side + 1 };
			float right_area{ 0.0f };

			// ¿es recta vertical?
			if (std::abs(dx) <= 1e-7f)
			{
				right_area = std::clamp(right_side - x_top, 0.0f, 1.0f);
			}
			// es recta inclinada
			else
			{
				// calcular el punto de corte 'y' con el lado izquierdo y derecho usando la ecuación paramétrica de la recta
				const float t_left  = (left_side  - x_top) / dx;
				const float t_right = (right_side - x_top) / dx;

				// una recta que va hacia abajo de izquierda a derecha (dx > 0)
				if (dx > 0.0f)
				{
					// la recta pasa la izquierda quedando todo el pixel a su derecha
					if (t_left >= 1.0f) {
						right_area = 1.0f;
					}
					// la recta pasa por la derecha quedando todo el pixel a su izquierda
					else if (t_right <= 0.0f) {
						right_area = 0.0f;
					}
					// la recta pasa por x_top y x_bottom sin pasar por ningún lado vertical
					else if (t_left <= 0.0f && t_right >= 1.0f)
					{
						// El * 0.5f es porque es la mitad de 1.0f. La ecución del área del trapecio es ((B + b) / 2). También multiplica por la altura pero aquí es 1
						// que es la altura normalizada del pixel. Al final multiplicaremos por height para escalar el resultado a la altura deseada.
						right_area = 0.5f * ((right_side - x_top) + (right_side - x_bottom)); // Ecuación del trapecio que quedaría a la derecha de la recta y right_side
					}
					// la recta pasa por x_top y el punto t_right de right_side
					else if (t_left <= 0.0f && t_right > 0.0f && t_right < 1.0f)
					{
						right_area = 0.5f * (right_side - x_top) * t_right; // Trapecio aplanado a triángulo en la esquina superior derecha
					}
					// la recta entra por t_left de left_side y sale por x_bottom
					else if (t_left > 0.0f && t_left < 1.0f && t_right >= 1.0f)
					{
						right_area = 1.0f - (0.5f * (1.0f - t_left) * (x_bottom - left_side)); // El área entera del pixel - trapecio aplanado a triángulo en la esquina inferior izquierda
					}
					// la recta pasa por t_left de left_side y sale por t_right de right_side
					else
					{
						right_area = 0.5f * (t_left + t_right); // La altura de este trapecio es la longitud horizontal del pixel (1.0f)
					}
				}
				// una recta que va hacia abajo de derecha a izquierda (dx < 0)
				else
				{
					// la recta pasa la derecha quedando todo el pixel a su izquierda
					if (t_right >= 1.0f) {
						right_area = 0.0f;
					}
					// la recta pasa por la izquierda quedando todo el pixel a su derecha
					else if (t_left <= 0.0f) {
						right_area = 1.0f;
					}
					// la recta pasa por x_top y x_bottom sin pasar por ningún lado vertical
					else if (t_right <= 0.0f && t_left >= 1.0f)
					{
						right_area = 0.5f * ((right_side - x_top) + (right_side - x_bottom));
					}
					// la recta pasa por x_top y el punto t_left de left_side
					else if (t_right <= 0.0f && t_left > 0.0f && t_left < 1.0f)
					{
						right_area = 1.0f - (0.5f * t_left * (x_top - left_side)); // Todo el pixel menos el triángulo de la esquina superior izquierda
					}
					// la recta entra por t_right de right_side y sale por x_bottom
					else if (t_right > 0.0f && t_right < 1.0f && t_left >= 1.0f)
					{
						right_area = 0.5f * (1.0f - t_right) * (right_side - x_bottom); // Triángulo de la esquina inferior derecha
					}
					// la recta pasa por t_right de right_side y sale por t_left de left_side
					else
					{
						right_area = 0.5f * ((1.0f - t_right) + (1.0f - t_left)); // La altura de este trapecio es la longitud horizontal del pixel (1.0f)
					}
				}
			}

			right_area *= height;
			row[i]     += right_area - prev_area;
			prev_area   = right_area;
		}

		if (end_x < row.size()) {
			row[end_x] += height - prev_area;
		}
	}

} // namespace luxel::ttf::detail
