cmake_minimum_required(VERSION 3.21)

project(test_ui LANGUAGES CXX)

# Pruebas de la lógica de luxel_ui (árbol, ratón, botones y layout), sin ventana ni
# GPU: los eventos se inventan y los widgets no se pintan.
add_executable(test_ui
    src/test_ui.cpp
)

target_link_libraries(test_ui PRIVATE luxel_ui)

# Caja blanca: ve las cabeceras privadas (distribute.h).
target_include_directories(test_ui PRIVATE "$<TARGET_PROPERTY:luxel_ui,SOURCE_DIR>/src")

target_compile_features(test_ui PRIVATE cxx_std_20)

set_target_properties(test_ui PROPERTIES
    CXX_EXTENSIONS OFF
)

add_test(NAME test_ui COMMAND test_ui)
