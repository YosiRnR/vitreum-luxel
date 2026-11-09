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
- Diseño completo del RHI de luxel en `docs/rhi-design.md` (léelo antes de tocar luxel).
- Capas de luxel: API pública en `luxel/include/luxel/` (solo puede incluir
  otras cabeceras públicas o std; nunca Vulkan); privado en `luxel/src/`,
  con backends en `luxel/src/backends/<api>/`.
- `Device` es una interfaz abstracta (`RefCounted`) creada por la fábrica
  `create_device()`, que es lo único que conoce a `VulkanDevice`.
- `vitreum` y `luxel` no dependen una de otra: la aplicación convierte
  `NativeWindowHandle` -> `luxel::WindowHandle` con `to_luxel()`.
- Decisiones abiertas: modelo de enlace (bindless vs bind groups de WebGPU,
  se decide en el hito 5) y política de errores.

## Estado
- Hecho: capa de ventana Win32, cola de eventos (pop_event con índice),
  evento Close con veto, Alt+F4, native_handle() y open_file_dialog().
  Compilación unificada en la raíz. Hito 1 del RHI: Vulkan oculto tras
  `luxel::Device` (`create_device()`), `tests/test_1` sin cabeceras de Vulkan.
  Hito 2 del RHI: `VulkanDevice` absorbió `VulkanContext`; selección de GPU
  (`vulkan_physical_device.h/.cpp`), dispositivo lógico 1.3 y colas gráfica
  y presentación. Requisitos en una sola fuente: extensiones en
  `k_required_device_extensions` y features en tablas de punteros a miembro
  (se usan para exigir y para activar). La surface vive aún en `VulkanDevice`
  (provisional hasta la swapchain).
- En curso: matemáticas propias en luxel/src/math. Plan de hitos en
  `docs/rhi-design.md`.
- Pendiente en la ventana, antes de la swapchain: WM_SIZE al minimizar
  y tamaños cliente/ventana (set_size).
- Aparcado en la ventana (se retoma más adelante): Raw Input, DPI,
  UTF-8 en títulos, registro único de la clase de ventana.
- Siguiente (hito 3): `Swapchain` y bucle de frame (`begin_frame` /
  `end_frame`, limpiar a un color); la surface sale del device y el
  `WindowHandle` sale de `DeviceDescriptor`. Pendiente menor del hito 2:
  quitar `vulkan_surface.*` (resto del hito 1) o convertirlo en la swapchain.

## Compilación
- Ficheros de CMakeLists.txt tanto en luxel como en vitreum y uno general en librerias.
- Build con MSVC o CLang configurado en los CMakePresets.json
