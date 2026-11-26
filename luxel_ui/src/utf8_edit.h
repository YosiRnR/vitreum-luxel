#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// Cuentas de UTF-8 para editar texto, sin fuente ni GPU (las prueba test_ui).
//
// El texto se guarda en UTF-8 y el cursor es una posición en BYTES: el número de bytes
// que hay delante de él. Siempre está en una frontera entre caracteres, nunca a mitad de
// uno. Escenario, "aña" (la ñ ocupa 2 bytes):
//
//     bytes:     a    ñ    ñ    a
//              0x61 0xC3 0xB1 0x61
//     fronteras: 0    1         3    4       <- el cursor solo puede estar en estas
//
// Con el cursor en 3 (detrás de la ñ), Retroceso borra los bytes [1, 3) y el cursor
// pasa a 1. Si borrase solo uno, quedaría 0xC3 suelto: un carácter roto.
namespace luxel::ui
{
	// La frontera anterior a "caret": dónde empieza el carácter que hay justo delante.
	// En 0 devuelve 0.
	std::size_t previous_boundary(std::u8string_view text, std::size_t caret);

	// La frontera siguiente: dónde acaba el carácter que empieza en "caret".
	// En text.size() devuelve text.size().
	std::size_t next_boundary(std::u8string_view text, std::size_t caret);

	// Un carácter Unicode en UTF-8 (de 1 a 4 bytes).
	std::u8string encode_utf8(char32_t codepoint);

	// Cuántos caracteres hay (no bytes): "aña" tiene 3.
	std::size_t count_codepoints(std::u8string_view text);

} // namespace luxel::ui
