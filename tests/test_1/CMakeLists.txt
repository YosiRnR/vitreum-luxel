cmake_minimum_required(VERSION 3.21)

project(test_1 LANGUAGES CXX)

add_executable(test_1
    src/test_1.cpp
    "src/stb_image.h"
    "src/keyboard_state.h"
    "src/iria_controller.h"
    "src/ui_events.h"
    "src/iria_menu.h"
    "src/gdi_font_atlas.h"
    "src/gdi_font_atlas.cpp"
)

target_link_libraries(test_1
    PRIVATE
        vitreum
        luxel_ui    # arrastra luxel_2d y luxel (los enlaza como PUBLIC)
        gdi32       # GDI de Win32: genera el atlas de la fuente (gdi_font_atlas.cpp)
)

target_compile_features(test_1 PRIVATE cxx_std_20)

target_compile_definitions(test_1 PRIVATE
    UNICODE
    _UNICODE
)

set_target_properties(test_1 PROPERTIES
    CXX_EXTENSIONS OFF
)

# Carpeta de recursos de los tests (texturas, y más adelante mallas), dentro del
# proyecto. Se pasa como una macro del preprocesador con la ruta ABSOLUTA, así
# el exe encuentra los ficheros lo lances desde donde lo lances (Visual Studio,
# la consola...). En VS es como añadir LUXEL_TEST_ASSETS_DIR="G:/..." en
# Propiedades > C/C++ > Preprocesador > Definiciones, pero calculado por CMake.
# Las comillas escapadas (\") hacen que en C++ la macro sea una cadena:
#     stbi_load(LUXEL_TEST_ASSETS_DIR "/iriaBase01.png", ...)
# (dos literales seguidos se concatenan solos).
target_compile_definitions(test_1 PRIVATE
    "LUXEL_TEST_ASSETS_DIR=\"${CMAKE_SOURCE_DIR}/tests/assets\""
)
