cmake_minimum_required(VERSION 3.21)

project(luxel_ui LANGUAGES CXX)

# luxel_ui: librería de widgets por encima del Renderer2D (docs/ui-design.md).
#
# Enlaza solo con luxel_2d. No conoce vitreum: la aplicación le traduce los
# eventos de la ventana (to_ui en tests/test_1). Como no está enlazada con
# vitreum, un #include <vitreum/...> aquí no compilaría: la regla la vigila el
# compilador, igual que luxel_2d no puede ver Vulkan.
add_library(luxel_ui STATIC
    "include/luxel/ui/geometry.h"
    "include/luxel/ui/event.h"
    "include/luxel/ui/theme.h"
    "include/luxel/ui/painter.h"
    "include/luxel/ui/widget.h"
    "include/luxel/ui/widgets.h"
    "include/luxel/ui/ui.h"
    "src/painter.cpp"
    "src/widget.cpp"
    "src/widgets.cpp"
    "src/button.cpp"
    "src/hit_test.h"
    "src/hit_test.cpp"
    "src/ui.cpp"
)

target_include_directories(luxel_ui
    PUBLIC
        include
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# PUBLIC: painter.h incluye renderer2d.h, así que quien use luxel_ui también
# necesita ver luxel_2d (y, a través de él, luxel).
target_link_libraries(luxel_ui PUBLIC luxel_2d)

target_compile_features(luxel_ui PUBLIC cxx_std_20)

set_target_properties(luxel_ui PROPERTIES
    CXX_EXTENSIONS OFF
)
