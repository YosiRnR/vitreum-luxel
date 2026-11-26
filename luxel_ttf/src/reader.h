#pragma once

// Reader: lee enteros big-endian de un trozo de bytes, sin salirse nunca de él.
//
// Es tu SpanReader de truetype/ con una diferencia: cada lectura comprueba que hay
// bytes suficientes y, si no, lanza std::runtime_error diciendo qué se estaba
// leyendo. Escenario: un .ttf cortado a la mitad dice que 'glyf' empieza en el byte
// 90.000 y el fichero solo tiene 50.000. SpanReader leería memoria de fuera (con
// suerte, se cae; sin suerte, pinta basura). Reader dice "ttf: 'glyf' cortada".
//
// Big-endian = el byte MÁS significativo va primero (al revés que en x86):
//     bytes 0x08 0x00  ->  0x0800 = 2048 (units_per_em de Segoe UI)

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace luxel::ttf
{
	class Reader
	{
	public:
		// "what" es solo para los mensajes de error (p. ej. "'head'").
		Reader(std::span<const std::byte> data, std::string_view what)
			: m_data(data), m_what(what) {}

		template <std::unsigned_integral T>
		T read()
		{
			need(sizeof(T));
			T value = 0;
			for (std::size_t i = 0; i < sizeof(T); ++i) {
				value = static_cast<T>((value << 8) | std::to_integer<uint8_t>(m_data[m_pos + i]));
			}
			m_pos += sizeof(T);
			return value;
		}

		uint8_t  u8()  { return read<uint8_t>(); }
		uint16_t u16() { return read<uint16_t>(); }
		uint32_t u32() { return read<uint32_t>(); }
		uint64_t u64() { return read<uint64_t>(); }
		// Con signo: se lee sin signo y se reinterpreta (0xFFFF -> -1).
		int16_t  i16() { return static_cast<int16_t>(read<uint16_t>()); }

		void skip(std::size_t bytes) { need(bytes); m_pos += bytes; }

		void seek(std::size_t offset)
		{
			if (offset > m_data.size()) {
				fail("se intenta ir más allá del final");
			}
			m_pos = offset;
		}

		std::size_t tell() const { return m_pos; }
		std::size_t size() const { return m_data.size(); }

		// Un Reader nuevo sobre un trozo de este, empezando en su byte 0. Así una
		// subtabla no puede salirse de su tabla aunque sus offsets estén mal.
		Reader sub(std::size_t offset, std::size_t length, std::string_view what) const
		{
			// Cuidado con el desbordamiento: "offset + length > size" podría dar la
			// vuelta con números enormes. Así no.
			if (offset > m_data.size() || length > m_data.size() - offset) {
				fail("un trozo se sale de la tabla");
			}
			return Reader(m_data.subspan(offset, length), what);
		}

		std::span<const std::byte> bytes() const { return m_data; }

	private:
		void need(std::size_t bytes) const
		{
			// m_pos nunca pasa de size(), así que la resta no da la vuelta.
			if (bytes > m_data.size() - m_pos) {
				fail("cortada");
			}
		}

		[[noreturn]] void fail(std::string_view problem) const
		{
			throw std::runtime_error("ttf: " + m_what + " " + std::string(problem));
		}

		std::span<const std::byte> m_data;
		std::string m_what;
		std::size_t m_pos{ 0 };
	};

} // namespace luxel::ttf
