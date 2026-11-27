#include "glyf.h"
#include <stdexcept>
#include <string>

namespace luxel::ttf::detail
{
	namespace
	{
		[[noreturn]] void invalid(const char* problem)
		{
			throw std::runtime_error(std::string("ttf: 'glyf' ") + problem);
		}

		// Bits de los flags de cada punto de un glifo simple.
		constexpr uint8_t k_on_curve      = 0x01;
		constexpr uint8_t k_x_short       = 0x02;   // x en 1 byte (sin signo; el signo va en k_x_same_or_sign)
		constexpr uint8_t k_y_short       = 0x04;
		constexpr uint8_t k_repeat        = 0x08;   // el byte siguiente dice cuántas veces más se repite este flag
		constexpr uint8_t k_x_same_or_sign = 0x10;  // corto: 1 = positivo. Largo: 1 = igual que el anterior (no hay dato)
		constexpr uint8_t k_y_same_or_sign = 0x20;

		// Flags de cada pieza de un compuesto.
		constexpr uint16_t k_args_are_words         = 0x0001;   // dx, dy (o los dos puntos) en 16 bits, si no en 8
		constexpr uint16_t k_args_are_xy_values     = 0x0002;   // dx, dy. Si no: dos números de punto que hay que hacer coincidir
		constexpr uint16_t k_we_have_a_scale        = 0x0008;   // una escala igual en x e y
		constexpr uint16_t k_more_components        = 0x0020;   // detrás de esta pieza viene otra
		constexpr uint16_t k_x_and_y_scale          = 0x0040;   // escalas distintas en x e y
		constexpr uint16_t k_two_by_two             = 0x0080;   // matriz 2x2 completa
		constexpr uint16_t k_scaled_component_offset = 0x0800;  // (dx, dy) también se transforman con la matriz

		// Las coordenadas vienen como diferencias con el punto anterior (empezando en
		// 0), en 1 o 2 bytes según los flags. Tal cual lo hacías en truetype/
		// (read_glyph_coordinates), con el Reader que no se sale.
		void read_coordinates(Reader& reader, const std::vector<uint8_t>& flags, uint8_t short_bit,
		                      uint8_t same_or_sign_bit, std::vector<ShapePoint>& points, bool is_x)
		{
			int32_t value = 0;
			for (std::size_t p = 0; p < flags.size(); ++p)
			{
				const uint8_t f = flags[p];
				if (f & short_bit) {
					const int32_t delta = reader.u8();
					value += (f & same_or_sign_bit) ? delta : -delta;
				}
				else if (!(f & same_or_sign_bit)) {
					value += reader.i16();
				}
				(is_x ? points[p].x : points[p].y) = static_cast<float>(value);
			}
		}

		// Glifo simple, justo después de la cabecera de 10 bytes:
		//
		//     endPtsOfContours[n]   uint16   el último punto de cada contorno (crecientes)
		//     instructionLength     uint16   + las instrucciones de hinting (se saltan)
		//     flags[]               uint8    uno por punto, comprimidos con k_repeat
		//     xCoordinates[]                 diferencias, de 1 o 2 bytes
		//     yCoordinates[]
		GlyphShape read_simple(Reader& reader, int16_t contour_count)
		{
			GlyphShape shape;
			shape.contour_ends.resize(static_cast<std::size_t>(contour_count));
			for (std::size_t i = 0; i < shape.contour_ends.size(); ++i)
			{
				shape.contour_ends[i] = reader.u16();
				if (i > 0 && shape.contour_ends[i] <= shape.contour_ends[i - 1]) {
					invalid("con contornos que no avanzan");
				}
			}
			if (shape.contour_ends.empty()) {
				return shape;   // 0 contornos: un glifo vacío con cabecera (pasa)
			}
			reader.skip(reader.u16());   // instrucciones

			const std::size_t point_count = static_cast<std::size_t>(shape.contour_ends.back()) + 1;
			std::vector<uint8_t> flags;
			flags.reserve(point_count);
			while (flags.size() < point_count)
			{
				const uint8_t flag = reader.u8();
				flags.push_back(flag);
				if (flag & k_repeat)
				{
					// Tu truetype/ cortaba si se pasaba; un fichero bien hecho no se pasa.
					for (uint8_t repeat = reader.u8(); repeat > 0 && flags.size() < point_count; --repeat) {
						flags.push_back(flag);
					}
				}
			}

			shape.points.resize(point_count);
			read_coordinates(reader, flags, k_x_short, k_x_same_or_sign, shape.points, true);
			read_coordinates(reader, flags, k_y_short, k_y_same_or_sign, shape.points, false);
			for (std::size_t p = 0; p < point_count; ++p) {
				shape.points[p].on_curve = (flags[p] & k_on_curve) != 0;
			}
			return shape;
		}

		GlyphShape read_any(std::span<const std::byte> glyf, const std::vector<uint32_t>& loca, uint16_t glyph, int depth);

		// Glifo compuesto, justo después de la cabecera. Una pieza detrás de otra:
		//
		//     flags          uint16
		//     glyphIndex     uint16
		//     arg1, arg2     int8 o int16 (k_args_are_words): dx y dy, o dos números de punto
		//     escala         nada, 1, 2 o 4 números F2Dot14 según los flags
		//
		// mientras los flags lleven k_more_components.
		GlyphShape read_composite(Reader& reader, std::span<const std::byte> glyf, const std::vector<uint32_t>& loca, int depth)
		{
			GlyphShape shape;
			uint16_t flags = 0;
			do
			{
				flags = reader.u16();
				const uint16_t child_glyph = reader.u16();

				int32_t arg1 = 0;
				int32_t arg2 = 0;
				if (flags & k_args_are_words) {
					// Con signo si son desplazamientos; sin signo si son números de punto.
					arg1 = (flags & k_args_are_xy_values) ? reader.i16() : reader.u16();
					arg2 = (flags & k_args_are_xy_values) ? reader.i16() : reader.u16();
				}
				else {
					arg1 = (flags & k_args_are_xy_values) ? static_cast<int8_t>(reader.u8()) : reader.u8();
					arg2 = (flags & k_args_are_xy_values) ? static_cast<int8_t>(reader.u8()) : reader.u8();
				}

				ComponentTransform t;
				if (flags & k_we_have_a_scale) {
					t.a = t.d = f2dot14(reader.i16());
				}
				else if (flags & k_x_and_y_scale) {
					t.a = f2dot14(reader.i16());
					t.d = f2dot14(reader.i16());
				}
				else if (flags & k_two_by_two) {
					t.a = f2dot14(reader.i16());
					t.b = f2dot14(reader.i16());
					t.c = f2dot14(reader.i16());
					t.d = f2dot14(reader.i16());
				}

				const GlyphShape child = read_any(glyf, loca, child_glyph, depth + 1);

				if (flags & k_args_are_xy_values)
				{
					t.dx = static_cast<float>(arg1);
					t.dy = static_cast<float>(arg2);
					if (flags & k_scaled_component_offset) {
						// El desplazamiento también pasa por la matriz (raro: fuentes de Apple).
						t.dx = t.a * static_cast<float>(arg1) + t.c * static_cast<float>(arg2);
						t.dy = t.b * static_cast<float>(arg1) + t.d * static_cast<float>(arg2);
					}
					append_component(shape, child, t);
				}
				else
				{
					// "Que el punto arg2 de la pieza caiga encima del punto arg1 de lo que ya
					// hay". Se coloca la pieza sin mover, se mira cuánto falta y se mueve.
					// (Así no hace falta repetir aquí las cuentas de append_component.)
					GlyphShape placed;
					append_component(placed, child, t);
					if (static_cast<std::size_t>(arg1) >= shape.points.size() ||
					    static_cast<std::size_t>(arg2) >= placed.points.size()) {
						invalid("con una pieza que se engancha a un punto que no existe");
					}
					const ShapePoint& anchor = shape.points[static_cast<std::size_t>(arg1)];
					const ShapePoint& hook   = placed.points[static_cast<std::size_t>(arg2)];
					append_component(shape, placed, ComponentTransform{ .dx = anchor.x - hook.x, .dy = anchor.y - hook.y });
				}
			} while (flags & k_more_components);

			// Si las piezas traen instrucciones (flag 0x0100), van aquí detrás: se ignoran.
			return shape;
		}

		GlyphShape read_any(std::span<const std::byte> glyf, const std::vector<uint32_t>& loca, uint16_t glyph, int depth)
		{
			if (depth > k_max_component_depth) {
				invalid("con compuestos anidados demasiado hondo (¿un glifo que se incluye a sí mismo?)");
			}
			if (static_cast<std::size_t>(glyph) + 1 >= loca.size()) {
				invalid("con una pieza que es un glifo que no existe");
			}
			const uint32_t start = loca[glyph];
			const uint32_t end   = loca[glyph + 1u];
			if (start == end) {
				return {};   // sin contornos: el espacio
			}

			Reader reader = Reader(glyf, "'glyf'").sub(start, end - start, "'glyf'");
			const int16_t contour_count = reader.i16();
			reader.skip(8);   // la caja (xMin, yMin, xMax, yMax): ya la calculará quien la quiera
			if (contour_count >= 0) {
				return read_simple(reader, contour_count);
			}
			if (contour_count == -1) {
				return read_composite(reader, glyf, loca, depth);
			}
			invalid("con un número de contornos negativo distinto de -1");
		}
	}

	GlyphShape read_glyph_shape(std::span<const std::byte> glyf, const std::vector<uint32_t>& loca, uint16_t glyph)
	{
		return read_any(glyf, loca, glyph, 0);
	}

} // namespace luxel::ttf::detail
