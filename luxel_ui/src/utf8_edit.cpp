#include "utf8_edit.h"
#include <algorithm>
#include <cstdint>

namespace luxel::ui
{
	// Cómo es un carácter en UTF-8. El PRIMER byte dice cuántos le siguen; los demás
	// (los de continuación) empiezan todos por los bits 10:
	//
	//     bytes   primer byte   los demás     ejemplos
	//       1     0xxxxxxx      -             a = 0x61
	//       2     110xxxxx      10xxxxxx      ñ = 0xC3 0xB1
	//       3     1110xxxx      10xxxxxx x2   € = 0xE2 0x82 0xAC
	//       4     11110xxx      10xxxxxx x3   😀 = 0xF0 0x9F 0x98 0x80
	//
	// encode_utf8 hace la tabla de izquierda a derecha: reparte los bits del número
	// entre los huecos "x".

	std::u8string encode_utf8(char32_t c)
	{
		std::u8string out;
		if (c < 0x80) {                       // 7 bits: tal cual
			out += static_cast<char8_t>(c);
		}
		else if (c < 0x800) {                 // 11 bits: 5 + 6
			out += static_cast<char8_t>(0xC0 | (c >> 6));
			out += static_cast<char8_t>(0x80 | (c & 0x3F));
		}
		else if (c < 0x10000) {               // 16 bits: 4 + 6 + 6
			out += static_cast<char8_t>(0xE0 | (c >> 12));
			out += static_cast<char8_t>(0x80 | ((c >> 6) & 0x3F));
			out += static_cast<char8_t>(0x80 | (c & 0x3F));
		}
		else {                                // 21 bits: 3 + 6 + 6 + 6
			out += static_cast<char8_t>(0xF0 | (c >> 18));
			out += static_cast<char8_t>(0x80 | ((c >> 12) & 0x3F));
			out += static_cast<char8_t>(0x80 | ((c >> 6) & 0x3F));
			out += static_cast<char8_t>(0x80 | (c & 0x3F));
		}
		return out;
	}

	std::size_t count_codepoints(std::u8string_view text)
	{
		std::size_t count = 0;
		for (std::size_t i = 0; i < text.size(); i = next_boundary(text, i)) {
			++count;
		}
		return count;
	}

	// ------------------------------------------------------------------------
	// TODO (U5): tu parte. Las dos funciones de abajo mueven el cursor un carácter.
	//
	// Pista: mira la tabla de arriba. No hace falta saber cuántos bytes mide un
	// carácter para encontrar dónde empieza o acaba: basta con saber reconocer un
	// byte de continuación (con una máscara de bits y una comparación).
	//
	// Escenarios (están en test_ui, caso 24):
	//     "aña", cursor en 3  -> previous 1, next 4
	//     "aña", cursor en 1  -> previous 0, next 3
	//     "a€",  cursor en 4  -> previous 1          (el € son 3 bytes)
	//     "😀",  cursor en 0  -> next 4
	//     en los extremos no se mueve: previous(.., 0) = 0, next(.., size) = size
	//
	// Cuidado con salirte del texto: el bucle tiene que parar en 0 y en size().
	// ------------------------------------------------------------------------

	std::size_t previous_boundary(std::u8string_view text, std::size_t caret)
	{
		if (caret == 0) {
			return 0;
		}

		// Prevenir posiciones de caret erróneas más allá del límite del texto y posicionar el caret en el previo
		caret = std::min(caret, text.size()) - 1;

		// Si el byte es <= 127 (ASCII) no entramos al bucle y devolvemos la posición previa calculada en el std::min
		// Si el byte, una vez puestos a cero los 6 bits más bajos es 128 (0x80) (0b10xxxxxx), entonces es un byte de continuación
		// y seguimos retrocediendo hasta que no lo sea
		while (caret > 0 && (static_cast<uint8_t>(text[caret]) & 0xC0u) == 0x80u) {
			caret--;
		}

		return caret;
	}

	std::size_t next_boundary(std::u8string_view text, std::size_t caret)
	{
		if (caret >= text.size()) {
			return text.size();
		}

		caret++;

		while (caret < text.size() && (static_cast<uint8_t>(text[caret]) & 0xC0u) == 0x80u) {
			caret++;
		}

		return caret;
	}

} // namespace luxel::ui
