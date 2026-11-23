# Convierte un fichero SPIR-V en una cabecera C++ con un array de uint32_t.
# Uso: cmake -DINPUT=x.spv -DOUTPUT=x.h -DNAME=nombre_array -P embed_spirv.cmake

# Lee el fichero binario como texto hexadecimal: 2 caracteres por byte.
# El SPIR-V empieza por los bytes 03 02 23 07, así que contenido = "03022307...".
file(READ "${INPUT}" contenido HEX)

# El SPIR-V se compone de palabras de 4 bytes = 8 caracteres hexadecimales.
string(LENGTH "${contenido}" longitud)
math(EXPR resto "${longitud} % 8")
if (longitud EQUAL 0 OR NOT resto EQUAL 0)
	message(FATAL_ERROR "${INPUT}: vacío o tamaño no múltiplo de 4 bytes")
endif()

# Cada grupo de 8 caracteres "AABBCCDD" pasa a "0xDDCCBBAA,".
# Se da la vuelta a los bytes porque x86 es little-endian: el primer byte
# del fichero es el MENOS significativo. "03022307" -> "0x07230203" (número mágico).
string(REGEX REPLACE "(..)(..)(..)(..)" "0x\\4\\3\\2\\1," palabras "${contenido}")

file(WRITE "${OUTPUT}"
    "// Generado por embed_spirv.cmake a partir de ${INPUT}. No editar.\n"
    "#pragma once\n"
    "#include <cstdint>\n\n"
    "inline constexpr std::uint32_t ${NAME}[] = {\n${palabras}\n};\n"
)
