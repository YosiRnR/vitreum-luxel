#pragma once

// Decodificador UTF-8 mínimo (solo std) para draw_text. Privado de luxel_2d;
// probado sin GPU en tests/test_renderer2d.
//
// UTF-8 en dos líneas: los caracteres ASCII ocupan 1 byte (0xxxxxxx) y el
// resto 2, 3 o 4 bytes. El primer byte dice cuántos son (110xxxxx = 2,
// 1110xxxx = 3, 11110xxx = 4) y los siguientes empiezan por 10xxxxxx. Los bits
// "x" juntos forman el número del carácter (su "codepoint").
//
//   'A' = U+0041 -> 0x41                    (1 byte)
//   'ñ' = U+00F1 -> 0xC3 0xB1               (2 bytes: 110 00011 + 10 110001)
//   '€' = U+20AC -> 0xE2 0x82 0xAC          (3 bytes)

#include <cstddef>
#include <string_view>

namespace luxel::detail
{
	inline constexpr char32_t k_replacement_char = U'�';   // "carácter no válido"

	// Lee el carácter que empieza en text[i] y deja i en el siguiente.
	// Una secuencia mal formada (byte de continuación suelto, secuencia cortada,
	// codificación demasiado larga, surrogates) devuelve U+FFFD y avanza 1 byte,
	// para no quedarse atascado y seguir con lo que venga detrás.
	// Precondición: i < text.size().
	constexpr char32_t next_codepoint(std::u8string_view text, std::size_t& i)
	{
		const auto byte = [&](std::size_t k) { return static_cast<unsigned char>(text[k]); };
		const unsigned char first = byte(i);

		// Cuántos bytes ocupa, el valor de los bits útiles del primero y el
		// codepoint mínimo con esa longitud (para rechazar codificaciones largas).
		std::size_t length = 0;
		char32_t cp = 0;
		char32_t min_cp = 0;
		if      (first < 0x80)           { ++i; return first; }                     // 0xxxxxxx
		else if ((first & 0xE0) == 0xC0) { length = 2; cp = first & 0x1F; min_cp = 0x80; }
		else if ((first & 0xF0) == 0xE0) { length = 3; cp = first & 0x0F; min_cp = 0x800; }
		else if ((first & 0xF8) == 0xF0) { length = 4; cp = first & 0x07; min_cp = 0x10000; }
		else                             { ++i; return k_replacement_char; }        // 10xxxxxx suelto o 11111xxx

		if (i + length > text.size()) {   // secuencia cortada al final del texto
			++i;
			return k_replacement_char;
		}
		for (std::size_t k = 1; k < length; ++k)
		{
			const unsigned char next = byte(i + k);
			if ((next & 0xC0) != 0x80) {   // tenía que ser 10xxxxxx
				++i;
				return k_replacement_char;
			}
			cp = (cp << 6) | (next & 0x3F);
		}

		if (cp < min_cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
			++i;
			return k_replacement_char;
		}

		i += length;
		return cp;
	}

} // namespace luxel::detail
