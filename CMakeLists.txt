cmake_minimum_required(VERSION 3.21)
project(Librerias_workspace NONE)

add_subdirectory(vitreum)
add_subdirectory(luxel)
add_subdirectory(luxel_2d)   # después de luxel: usa su función luxel_embed_shaders
add_subdirectory(luxel_ui)   # después de luxel_2d
add_subdirectory(tests/test_1)
add_subdirectory(tests/test_deferred)
add_subdirectory(tests/test_free)
add_subdirectory(tests/test_cameras)
add_subdirectory(tests/test_renderer2d)
add_subdirectory(tests/test_ui)
