cmake_minimum_required(VERSION 3.21)

project(test_ttf LANGUAGES CXX)

# Pruebas de luxel_ttf, sin GPU ni ventana. Lee las fuentes de C:\Windows\Fonts.
add_executable(test_ttf
    src/test_ttf.cpp
)

target_link_libraries(test_ttf PRIVATE luxel_ttf)

# Caja blanca: ve las cabeceras privadas (reader.h).
target_include_directories(test_ttf PRIVATE "$<TARGET_PROPERTY:luxel_ttf,SOURCE_DIR>/src")

target_compile_features(test_ttf PRIVATE cxx_std_20)

set_target_properties(test_ttf PROPERTIES
    CXX_EXTENSIONS OFF
)

add_test(NAME test_ttf COMMAND test_ttf)
