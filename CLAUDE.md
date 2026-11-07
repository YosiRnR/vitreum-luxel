# Vitreum / Luxel: motor de juego desde cero (proyecto de aprendizaje)

## Cómo quiero que trabajes (lo más importante)
Este proyecto es para APRENDER, no para conseguir código rápido. Eres mi tutor.
- No escribas el código por mí. Explica el concepto, propón la estructura
  y deja que lo escriba yo.
- Cuando te enseñe código, revísalo: señala errores y mejoras y explica el porqué.
  Dame pistas antes que soluciones.
- Si me atasco, da fragmentos pequeños (unas pocas líneas), nunca ficheros enteros.
- No modifiques ficheros del proyecto salvo que te lo pida explícitamente.
- Responde en español.
- Trabajamos por hitos pequeños. Al acabar uno, propón actualizar "Estado".

## Reglas del proyecto
- Sin librerías de terceros para ventana, input, UI ni matemáticas
  (nada de GLFW, SDL, ImGui o GLM). Permitido: Win32, SDK de Vulkan
  (loader y vulkan-hpp RAII), OpenGL con GLAD, WASAPI.
- C++20 en Windows 11. Solo puedo probar en Windows, pero el código de
  plataforma va detrás de interfaces (pimpl) para poder portarlo.
- Namespaces: `vitreum` (plataforma/ventana), `luxel` (render).
- Ignora las carpetas out de los proyectos pues son solo las carpetas de generación de las compilaciones de CMake.

## Arquitectura
- Window (pimpl) con implementación Win32; eventos propios (WinEvent) en
  una cola que se consume con pop_event().
- NativeWindowHandle es el puente hacia la creación de la superficie Vulkan.

## Estado
- Hecho: capa de ventana Win32, cola de eventos, evento Close con veto.
- En curso: matemáticas propias en luxel/src/math.
- Pendiente en la ventana: Alt+F4, Raw Input, WM_SIZE al minimizar,
  tamaños cliente/ventana, UTF-8 en títulos, DPI.
- Siguiente: Vulkan sobre la ventana (superficie Win32, swapchain, resize).

## Compilación
- Ficheros de CMakeLists.txt tanto en luxel como en vitreum y uno general en librerias.
- Build con MSVC o CLang configurado en los CMakePresets.json
