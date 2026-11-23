# luxel_shaders.cmake: función luxel_embed_shaders().
#
# Se carga con include() desde luxel/CMakeLists.txt. Las funciones de CMake son
# globales: una vez definidas aquí, cualquier CMakeLists que se procese después
# (los tests, luxel_2d...) puede llamarla. Por eso el CMakeLists raíz hace
# add_subdirectory(luxel) antes que los tests.
#
# Comparación con Visual Studio: es como una hoja de propiedades (.props) que
# "aplicas" a un proyecto, con la diferencia de que además de ajustes le añade
# reglas de compilación (dxc y la conversión a cabecera) para sus .hlsl.

# Busca el SDK de Vulkan y, dentro, el compilador de shaders dxc.
# Deja la ruta del ejecutable en la variable de caché Vulkan_dxc_EXECUTABLE
# (las variables de caché son globales: la función la ve se llame desde donde se
# llame).
find_package(Vulkan REQUIRED COMPONENTS dxc)

# CMAKE_CURRENT_LIST_DIR vale "la carpeta del fichero que se está leyendo ahora",
# o sea luxel/cmake. Dentro de la función esa variable valdría la carpeta de
# QUIEN LLAMA (p. ej. tests/test_1), así que guardamos la ruta del script ahora,
# mientras todavía apunta aquí.
set(LUXEL_EMBED_SPIRV_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/embed_spirv.cmake"
    CACHE INTERNAL "Script que convierte un .spv en cabecera C++")

# Uso:
#   luxel_embed_shaders(<target> shaders/sprite_vs.hlsl shaders/sprite_ps.hlsl ...)
#
# Por cada <nombre>_vs.hlsl / <nombre>_ps.hlsl:
#   .hlsl --dxc--> <nombre>.spv --embed_spirv.cmake--> <nombre>_spv.h
# y el target puede hacer #include "<nombre>_spv.h" (array <nombre>_spv[]).
# Las cabeceras quedan en una carpeta PRIVADA del target: otro target no las ve.
function(luxel_embed_shaders target)
    # "target" es el primer argumento con nombre. El resto de argumentos (la
    # lista de .hlsl) llega en la variable especial ARGN.
    if(NOT TARGET ${target})
        message(FATAL_ERROR "luxel_embed_shaders: '${target}' no es un target")
    endif()

    # Carpeta de salida dentro de la carpeta de compilación de quien llama
    # (CMAKE_CURRENT_BINARY_DIR = out/build/<preset>/tests/test_1, por ejemplo).
    # Lleva el nombre del target para que dos targets del mismo CMakeLists que
    # usen el mismo shader no escriban el mismo fichero.
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}_shaders")

    # Listas que iremos llenando (set sin valor = lista vacía).
    set(hlsl_files)
    set(header_files)

    foreach(hlsl IN LISTS ARGN)
        # Rutas relativas -> absolutas respecto a la carpeta de quien llama.
        # (Las reglas de compilación se ejecutan desde otra carpeta: con
        # rutas relativas dxc no encontraría el fichero.)
        cmake_path(ABSOLUTE_PATH hlsl
            BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE hlsl_path)

        # NAME_WE = nombre sin carpeta ni extensión: ".../sprite_vs.hlsl" -> "sprite_vs".
        get_filename_component(shader_name "${hlsl_path}" NAME_WE)

        # La etapa se deduce del sufijo. MATCHES guarda el grupo () en CMAKE_MATCH_1.
        if(shader_name MATCHES "_(vs|ps)$")
            set(stage "${CMAKE_MATCH_1}")
        else()
            message(FATAL_ERROR
                "luxel_embed_shaders: '${hlsl}' debe acabar en _vs.hlsl o _ps.hlsl")
        endif()

        # Perfil de dxc (etapa + shader model) y nombre de la función de entrada.
        set(profile "${stage}_6_0")      # vs_6_0 o ps_6_0
        set(entry   "${stage}_main")     # vs_main o ps_main

        set(spv_file    "${output_dir}/${shader_name}.spv")
        set(header_file "${output_dir}/${shader_name}_spv.h")

        # Receta 1: .hlsl -> .spv. No se ejecuta ahora (fase de configuración):
        # solo se APUNTA. Se ejecutará al compilar si alguien necesita el .spv y
        # el .hlsl es más nuevo. Es como un "Custom Build Step" de VS con sus
        # Outputs y Additional Dependencies.
        add_custom_command(
            OUTPUT  "${spv_file}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
            COMMAND "${Vulkan_dxc_EXECUTABLE}"
                    -spirv
                    -T "${profile}"
                    -E "${entry}"
                    "-fspv-target-env=vulkan1.3"
                    -Fo "${spv_file}"
                    "${hlsl_path}"
            DEPENDS "${hlsl_path}"
            COMMENT "dxc: ${shader_name}.hlsl -> ${shader_name}.spv"
            VERBATIM
        )

        # Receta 2: .spv -> cabecera. Ejecuta nuestro script con "cmake -P"
        # (CMake como intérprete de scripts, sin proyecto). Depende también del
        # script: si lo cambiamos, se regeneran todas las cabeceras.
        add_custom_command(
            OUTPUT  "${header_file}"
            COMMAND "${CMAKE_COMMAND}"
                    "-DINPUT=${spv_file}"
                    "-DOUTPUT=${header_file}"
                    "-DNAME=${shader_name}_spv"
                    -P "${LUXEL_EMBED_SPIRV_SCRIPT}"
            DEPENDS "${spv_file}" "${LUXEL_EMBED_SPIRV_SCRIPT}"
            COMMENT "embed: ${shader_name}.spv -> ${shader_name}_spv.h"
            VERBATIM
        )

        list(APPEND hlsl_files   "${hlsl_path}")
        list(APPEND header_files "${header_file}")
    endforeach()

    # Los .hlsl se añaden al target solo para verlos en el IDE. HEADER_FILE_ONLY
    # evita que el generador de Visual Studio intente compilarlos con su propio
    # compilador de HLSL (fxc), que no es el que queremos.
    set_source_files_properties(${hlsl_files} PROPERTIES HEADER_FILE_ONLY ON)

    # Añadir las cabeceras generadas como fuentes del target es lo que "tira" de
    # las recetas: para compilar el target, CMake necesita que existan, así que
    # ejecuta antes dxc y el script. (Sustituye al antiguo target
    # compile_shaders + add_dependencies.) Ojo: las recetas de add_custom_command
    # solo las ve un target del MISMO CMakeLists; por eso se crean aquí dentro,
    # en la carpeta de quien llama.
    target_sources(${target} PRIVATE ${hlsl_files} ${header_files})

    # PRIVATE: solo este target puede incluir sus cabeceras de shaders. Si
    # luxel_2d incrusta los suyos, la aplicación que lo use no los verá.
    target_include_directories(${target} PRIVATE "${output_dir}")
endfunction()
