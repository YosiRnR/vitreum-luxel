# Vitreum / Luxel: motor de juego desde cero (proyecto de aprendizaje)

## Cómo quiero que trabajes (lo más importante)
Este proyecto es para APRENDER, no para conseguir código rápido. Eres mi tutor.
- No escribas el código por mí. Explica el concepto, propón la estructura
  y deja que lo escriba yo.
- Cuando te enseñe código, revísalo: señala errores y mejoras y explica el porqué.
  Dame pistas antes que soluciones.
- Si me atasco, da fragmentos pequeños (unas pocas líneas), nunca ficheros enteros.
- No modifiques ficheros del proyecto salvo que te lo pida explícitamente o
  lo hayamos consensuado antes (p. ej. actualizar este fichero o `docs/`).
  El código de los hitos lo escribo yo.
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
- Decidido en el hito 5: modelo de enlace bindless (descartados los bind groups
  de WebGPU por ahora) y shaders en HLSL compilados con `dxc` (SPIR-V; DXIL si
  llega D3D12).
- Decisiones abiertas: política de errores y OpenGL sin bindless (modo degradado).

## Estado
- Hecho: capa de ventana Win32, cola de eventos (pop_event con índice),
  evento Close con veto, Alt+F4, native_handle() y open_file_dialog().
  Compilación unificada en la raíz. Hito 1 del RHI: Vulkan oculto tras
  `luxel::Device` (`create_device()`), `tests/test_1` sin cabeceras de Vulkan.
  Hito 2 del RHI: `VulkanDevice` absorbió `VulkanContext`; selección de GPU
  (`vulkan_physical_device.h/.cpp`), dispositivo lógico 1.3 y colas gráfica
  y presentación. Requisitos en una sola fuente: extensiones en
  `k_required_device_extensions` y features en tablas de punteros a miembro
  (se usan para exigir y para activar).
  Hito 3 del RHI: `Swapchain` (`create_swapchain()`, surface propia en
  `VulkanSwapchain`) y bucle de frame (`begin_frame` / `end_frame`) con 2
  frames en vuelo, barreras synchronization2 y limpiado a un color, sin avisos
  de validación. Recreación al redimensionar u obsoleta (`resize()` solo marca,
  se recrea al inicio de `begin_frame`); minimizada (0x0) => `begin_frame`
  devuelve nullptr. El device se crea sin ventana: extensiones de instancia y
  capacidad de presentar por plataforma de compilación (`vulkan_platform.*`);
  `WindowHandle` sale de `DeviceDescriptor` y va en `SwapchainDescriptor`.
  Ventana: `set_size` con tamaño de cliente (`AdjustWindowRectEx`).
  `vulkan_surface.*` eliminado. Driver decodificado en la información de GPU.
  Hito 4 del RHI: `Ref` sin `adopt` ni `create`; contador de `RefCounted` a 0 y
  `Ref(T*)` incrementa; constructores de `VulkanDevice` y `VulkanSwapchain`
  privados con fábrica estática. Destrucción diferida: `RefCounted::on_zero_refs()`
  virtual y `DeletionQueue` por frame en `VulkanDevice`, indexada por el último
  frame empezado; el destructor del device hace `waitIdle` y vacía la cola.
  `tests/test_deferred` (caja blanca, ve `luxel/src`) cubre cuatro casos.
  `~VulkanSwapchain` espera a la GPU.
  Hito 5 del RHI (5a-5c hechos): bindless. `DeviceDescriptor::device_maximus`
  (`RequestedDeviceMaximums`: texturas, storage buffers y samplers) con topes
  del motor validados en `create_device()`; `rate_physical_device` descarta las
  GPU cuyos límites (por set, por etapa y `maxPerStageResources`) no los cubren.
  Features de descriptor indexing exigidas en `k_features12`.
  `luxel/src/slot_allocator.h` (`SlotAllocator`: índices de tamaño fijo, el 0
  reservado como inválido, con mutex; comprobaciones de doble free bajo
  `#ifndef NDEBUG`) probado en `tests/test_free`. `VulkanBindlessTable`: layout
  con 3 bindings (storage buffers, samplers, texturas) con flags
  `UpdateAfterBind`, pool, un set, `allocate_*_slot`/`free_*_slot` con escritura
  inmediata del descriptor y `VkPipelineLayout` global (set + 128 bytes de push
  constants). Probada con un sampler en `test_deferred`.
- En curso: matemáticas propias en luxel/src/math. Plan de hitos en
  `docs/rhi-design.md`.
- Siguiente (hito 5): 5d `Shader` y `GraphicsPipeline` (HLSL con `dxc`) y 5e
  triángulo con vértices fijos. Los destructores de `Texture`, `Sampler` y
  `Buffer` (hitos 6 y 7) devolverán su hueco a la tabla.
- Pendiente (ventana/bucle): redibujar durante el arrastre del borde (bucle
  modal de Win32); con la ventana minimizada el bucle de la app gira sin dormir
  (esperar mensajes); el tamaño de la swapchain en `test_1` debería salir de la
  ventana y no de un valor fijo; solo se admite una ventana por proceso (la
  clase de ventana tiene nombre fijo y se registra en cada `create`), lo que
  bloquea un editor multi-ventana.
- Aparcado en la ventana (se retoma más adelante): Raw Input, DPI,
  UTF-8 en títulos, registro único de la clase de ventana.

## Compilación
- Ficheros de CMakeLists.txt tanto en luxel como en vitreum y uno general en librerias.
- Build con MSVC o CLang configurado en los CMakePresets.json
