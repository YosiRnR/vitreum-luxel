#include "distribute.h"
#include <cmath>

namespace luxel::ui
{
	// Escenario (la columna de docs/ui-design.md: título, lista que crece y botón):
	//
	//     items     = { 30, grow 0 }, { 0, grow 1 }, { 40, grow 0 }
	//     available = 264
	//
	//     entre todos piden 30 + 0 + 40 = 70, así que sobran 264 - 70 = 194
	//     resultado = 30, 194, 40        (suman 264)
	//
	// Reglas:
	//   1. Cada uno mide, por lo menos, lo que pide (desired).
	//   2. Lo que sobra se reparte entre los que crecen, según su peso, y se SUMA a lo
	//      que pedían. Con 200 de sobra y pesos 1 y 3: 50 y 150.
	//   3. Si no sobra nada (o falta), cada uno mide lo que pide. No se encoge a nadie.
	//   4. Todos los tamaños son enteros, y si alguien crece la suma es EXACTAMENTE
	//      available: ni un píxel de hueco al final ni uno de más.
	//
	// Dos trampas:
	//   - Que nadie crezca: cuidado con dividir entre la suma de los pesos.
	//   - El redondeo. 100 de sobra entre tres iguales son 33,33 cada uno. Si redondeas
	//     cada tamaño por su cuenta, 33 + 33 + 33 = 99: falta uno. Se arregla como en el
	//     algoritmo de Bresenham: lo que se le quita a uno al redondear se arrastra al
	//     siguiente (error).
	std::vector<float> distribute(std::span<const FlexItem> items, float available)
	{
		float items_size{};
		float total_grows{};
		std::vector<float> sizes;
		sizes.reserve(items.size());
		for (const FlexItem& item : items)
		{
			items_size  += item.desired;
			total_grows += item.grow;
		}

		const float free_size = available - items_size;

		// No hay espacio suficiente para el espacio deseado o nadie crece: devolvemos los
		// espacios tal cual los tienen los items (measure() ya los da enteros).
		if (free_size <= 0.0f || total_grows <= 0.0f)
		{
			for (const FlexItem& item : items) {
				sizes.push_back(item.desired);
			}
			return sizes;
		}

		// Reparto de espacio. error es lo que se les debe a los siguientes: positivo si
		// hasta ahora se ha dado de menos, negativo si se ha dado de más. Con 100 entre tres:
		//
		//     merece 33,33   da 33   error  0,33
		//     merece 33,33   da 34   error -0,33   (0,33 + 0,33 = 0,67 pasa de medio píxel)
		//     merece 33,33   da 33   error  0      (cuentas saldadas: suman 100)
		float error{ 0.0f };
		for (const FlexItem& item : items)
		{
			const float share = free_size * (item.grow / total_grows);   // lo que merece
			float given = std::floor(share);                             // lo que se le da
			error += share - given;
			if (error >= 0.5f)
			{
				given += 1.0f;
				error -= 1.0f;
			}
			sizes.push_back(item.desired + given);
		}

		return sizes;
	}

} // namespace luxel::ui
