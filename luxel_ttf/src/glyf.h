#pragma once

// 'glyf': los puntos de cada glifo, en unidades de la fuente (+Y hacia ARRIBA, como
// en la fuente). Dos clases de glifo:
//
//   simple     contornos de puntos "en la curva" (on) y "de control" (off)
//   compuesto  una lista de piezas: "pinta el glifo X, movido y quizá escalado".
//              En Segoe UI y Arial, la "ñ" es así:
//
//                  pieza 1: glifo 81 ('n') tal cual
//                  pieza 2: glifo 216 (la virgulilla) movido 228 unidades a la derecha
//
// Los dos acaban en lo mismo, un GlyphShape: los compuestos se "aplanan" copiando los
// puntos de sus piezas ya transformados.

#include "reader.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace luxel::ttf::detail
{
	struct ShapePoint
	{
		float x{};
		float y{};
		bool on_curve{};
	};

	struct GlyphShape
	{
		std::vector<ShapePoint> points;
		// El último punto de cada contorno: el contorno i son points[ends[i-1] + 1 .. ends[i]].
		// (Así viene en el fichero: con 'n' = 18 puntos en un contorno, ends = { 17 }.)
		std::vector<uint32_t> contour_ends;
	};

	// La transformación de una pieza de un compuesto:
	//
	//     x' = a·x + c·y + dx
	//     y' = b·x + d·y + dy
	//
	// Sin escala, a = d = 1 y b = c = 0: solo se mueve (dx, dy). Con una escala de la
	// mitad, a = d = 0,5. Con b y c se puede inclinar o girar (raro en letras).
	struct ComponentTransform
	{
		float a{ 1.0f };
		float b{ 0.0f };
		float c{ 0.0f };
		float d{ 1.0f };
		float dx{ 0.0f };
		float dy{ 0.0f };
	};

	// Un número F2Dot14 (2 bits de parte entera con signo + 14 de decimales) a float.
	// TU PARTE (glyf_component.cpp).
	float f2dot14(int16_t raw);

	// Añade al final de "parent" los puntos de "child" transformados con "t", y sus
	// contornos. TU PARTE (glyf_component.cpp).
	void append_component(GlyphShape& parent, const GlyphShape& child, const ComponentTransform& t);

	// Los puntos del glifo "glyph". loca son los offsets de 'glyf' (glyph_count + 1).
	// Lanza runtime_error si los datos están rotos o si los compuestos se anidan más
	// de k_max_component_depth (un fichero malicioso podría hacer que un glifo se
	// incluya a sí mismo y no acabar nunca).
	inline constexpr int k_max_component_depth = 8;
	GlyphShape read_glyph_shape(std::span<const std::byte> glyf, const std::vector<uint32_t>& loca, uint16_t glyph);

} // namespace luxel::ttf::detail
