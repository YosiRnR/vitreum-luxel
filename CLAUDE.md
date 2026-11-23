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
  Excepción razonada: `stb_image.h` para decodificar imágenes (hoy en
  `tests/test_1/src`; si lo usa el motor irá a una carpeta `third_party/`).
  Hacerlo a mano (PNG + inflate/zlib) queda como posible hito de aprendizaje,
  detrás de una función tipo `load_image(path)`. El núcleo de luxel no lee
  ficheros: recibe bytes ya cargados.
- C++20 en Windows 11. Solo puedo probar en Windows, pero el código de
  plataforma va detrás de interfaces (pimpl) para poder portarlo.
- Namespaces: `vitreum` (plataforma/ventana), `luxel` (render).
- Ignora las carpetas out de los proyectos pues son solo las carpetas de generación de las compilaciones de CMake.

## Arquitectura
- Window (pimpl) con implementación Win32; eventos propios (WinEvent) en
  una cola que se consume con pop_event().
- NativeWindowHandle es el puente hacia la creación de la superficie Vulkan.
- Diseño completo del RHI de luxel en `docs/rhi-design.md` (léelo antes de tocar luxel).
- Capas por encima del RHI (Renderer2D, Renderer3D, UI, escena) y su plan de
  hitos (R1-R6) en `docs/renderers-design.md`.
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
  5d hecho: `GraphicsPipeline` (sin objeto `Shader` público: el descriptor
  lleva el SPIR-V de cada etapa como `span<const uint32_t>`, nombres de
  entrada `vs_main`/`ps_main`, `Format` y `PrimitiveTopology`, que viven en
  `types.h`) creado con `Device::create_graphics_pipeline()` (valida el
  formato). `VulkanGraphicsPipeline` crea módulos temporales, usa dynamic
  rendering, viewport/scissor dinámicos y el layout global de la tabla.
  Destrucción diferida: `on_zero_refs()` saca el `Ref` del device a un local
  antes de encolar (si no, el pipeline en cola mantendría vivo al device);
  probado en `test_deferred` con `ref_count()`. Shaders en `luxel/shaders`
  (`.hlsl` versionados, `.spv` en `.gitignore`, compilados a mano con `dxc`).
- En curso: matemáticas propias, públicas en `luxel/include/luxel/math`
  (`luxel::math`). Plan de hitos en `docs/rhi-design.md`.
  5e hecho: triángulo en `test_1` con vértices fijos en el shader. El set
  bindless se enlaza en `VulkanDevice::begin_frame` (hay que hacerlo en cada
  frame: el pool de comandos se resetea), viewport y scissor se fijan en
  `begin_rendering` y `CommandList` tiene `bind_pipeline` y `draw`. Con
  esto el hito 5 queda completo. 5d-4 hecho: `luxel/CMakeLists.txt` compila
  con `dxc` (del SDK de Vulkan) todos los `luxel/shaders/*_vs.hlsl` y
  `*_ps.hlsl` a `<build>/luxel/shaders/*.spv` (target `compile_shaders`,
  dependencia de `luxel`); el perfil y la entrada se deducen del sufijo del
  nombre (desde R1a, además, se incrustan como cabeceras: ver más abajo).
  Hito 6 del RHI (6a-6e hechos): `Buffer`. API pública en
  `luxel/include/luxel/buffer.h`: `BufferUsage` (flags `Storage`, `Index`,
  `TransferDst`, con `|`, `&`, `|=` y `has_flag`), `BufferDescriptor` (con
  `from_data()` para crear el descriptor desde un rango contiguo) y `Buffer`
  (`size()`, `usage()`, `bindless_index()`, que es 0 si no hay hueco);
  `Device::create_buffer()` valida el descriptor y lanza si es incorrecto.
  `VulkanBuffer`: `VkBuffer` + memoria `eDeviceLocal` (una asignación por
  buffer; tipo elegido a mano con `memoryTypeBits`) y hueco bindless solo si el
  uso incluye `Storage`, reservado al final del constructor para no perderlo si
  éste lanza. Guarda un puntero crudo a la `VulkanBindlessTable` porque
  `on_zero_refs()` suelta el `Ref` al device antes de encolar; es válido porque
  `~VulkanDevice` vacía la cola en su cuerpo, antes de destruir la tabla.
  `tests/test_deferred` casos 7-9 (hueco diferido y reutilizado, buffer `Index`
  sin hueco, descriptores inválidos).
  6c hecho: subida inicial por staging. `VulkanDevice::find_memory_type()`
  (`const`, lanza si no hay tipo adecuado) la comparten todos los que reserven
  memoria. `VulkanStagingBuffer` (`vulkan_staging_buffer.*`): buffer
  `eTransferSrc` en memoria visible y coherente, `write_data` mapea, copia y
  desmapea (comprueba el tamaño); no es un `luxel::Buffer`, vive en la pila.
  `VulkanDevice::submit_immediate(recorder)`: command buffer de un solo uso de
  un pool transitorio, enviado a la cola gráfica y `waitIdle()` (bloqueante;
  vale mientras el diseño sea monohilo). El constructor de `VulkanBuffer` sube
  `initial_data` (la copia es del tamaño de los datos, no del buffer) antes de
  reservar el slot. Sin barrera mientras se espere a la GPU. Test 10 de
  `test_deferred`; el contenido se verificará en el 6e. Silenciado `eVerbose`
  en los mensajes de validación.
  6d hecho: asignador temporal por frame. `Frame::allocate(span)` devuelve
  `BufferAllocation { slot_index, offset }`. `VulkanFrameAllocator` (interno):
  buffer de almacenamiento de 1 MB por frame en vuelo, en memoria visible y
  coherente, mapeado de forma persistente, con un único hueco bindless por
  allocator (el índice es el mismo para todas sus reservas). Cursor alineado a
  16, `runtime_error` si se llena, `reset_cursor()` en `begin_frame` tras
  esperar la fence. Vive en un `unique_ptr` dentro de `FrameResources`
  (`create_frames()` va después de crear la tabla bindless). Cuidado:
  `unique_ptr::reset()` destruye el objeto; por eso el método se llama
  `reset_cursor()`. Tests 11 y 12 de `test_deferred`.
  6e hecho (hito 6 completo): triángulo con vertex pulling y una matriz por
  frame, girando en `test_1` sin avisos de validación (esto verifica por fin el
  contenido de los buffers subidos y del asignador por frame).
  `CommandList::bind_constants(span)` (push constants: tamaño > 0, múltiplo de
  4, máximo 128; usa `k_push_constant_range`; el layout global llega a
  `VulkanCommandList` por `reset`). El shader lee los vértices y la matriz de
  `ByteAddressBuffer bindless_buffers[]` con los índices y el offset de las push
  constants; la matriz (column-major en C++) se carga y se traspone en HLSL.
  Matemáticas movidas a la API pública (`luxel/include/luxel/math`, incluidas
  desde `luxel.h`).
  Hito 7 del RHI (completo, 7a-7f): `Texture` y `Sampler`. API pública en
  `luxel/include/luxel/` (`texture.h`, `sampler.h`) y `flags.h`, con
  operadores de bitmask genéricos (`enable_bitmask_operators<T>`, activado por
  enum junto a su definición) que usan `BufferUsage` y `TextureUsage`.
  `TextureDescriptor` (ancho, alto, `Format`, `usage`, `mip_levels` fijo a 1 y
  `initial_data` obligatorio por ahora) y `SamplerDescriptor` (filtros mag/min y
  modos de dirección U/V/W); `format_bytes_per_pixel()` en `types.h`.
  `VulkanSampler`: `VkSampler` + hueco bindless, destrucción diferida.
  `VulkanTexture`: `VkImage` (`eSampled | eTransferDst`) con memoria
  `eDeviceLocal`, vista y hueco; `upload()` sube por staging dentro de
  `submit_immediate` con barreras Undefined -> TransferDst -> ShaderReadOnly
  antes de reservar el hueco. Trampa vista: el staging debe vivir fuera de la
  lambda de `submit_immediate` (la lambda solo graba; si el staging muere ahí,
  la GPU copia desde un buffer destruido y el driver se cae). `create_texture`
  valida tamaño, formato, uso, mips y tamaño de los datos. Tests 13-16 de
  `test_deferred`. 7e: quad texturizado en `test_1` (tablero de ajedrez
  generado a mano, shaders con `Texture2D` y `SamplerState` bindless, `draw(6)`).
  Verificado con una casilla amarilla en el píxel (0,0): +Y va hacia abajo en
  Vulkan, así que los vértices de `y = -0.5` (arriba en pantalla) llevan
  `v = 0`; y el orden de bytes debe coincidir con el `Format` (RGBA vs BGRA).
  7f hecho: `IndexType` (`Uint16`/`Uint32`) en `types.h`,
  `CommandList::bind_index_buffer(Buffer&, IndexType)` (valida
  `BufferUsage::Index` con `has_flag` y traduce el tipo) y
  `draw_indexed(index_count)`. Con vertex pulling `SV_VertexID` vale el índice
  leído, así que el shader no cambia. El quad de `test_1` usa 4 vértices y 6
  índices `uint16`.
  Hito 8 del RHI (sub-asignación de memoria, completo, 8a-8e): decidido hacer
  el asignador propio (sin VMA), con lista libre first-fit. `FreeListAllocator`
  (`luxel/src/free_list_allocator.*`, sin Vulkan): gestiona los rangos libres
  (`MemoryRange { offset, size }`) de UN bloque abstracto, en un `std::vector`
  ordenado por offset, sin huecos de tamaño 0 ni adyacentes (invariante).
  `allocate(size, alignment)` devuelve `std::optional<MemoryRange>`, deja el
  padding de la alineación como hueco libre y evita desbordamientos;
  `free(offset, size)` busca con `lower_bound`, fusiona con los vecinos y, en
  debug, detecta doble free y rangos fuera del bloque. Consultas `capacity()`,
  `free_size()` e `is_empty()` (un único hueco que cubre todo). Tests 6-11 en
  `tests/test_free`.
  8c hecho: `VulkanMemoryBlock` (`vulkan_memory_block.*`): un
  `vk::raii::DeviceMemory` (64 MB por defecto) + su `FreeListAllocator`, tipo de
  memoria y, si el tipo es visible y coherente, el bloque entero se mapea UNA
  vez al crearlo (Vulkan no permite mapear dos veces la misma memoria); no
  copiable ni movible (las reservas guardan un puntero al bloque); devuelve
  `MemoryRange` y el destructor comprueba (assert) que no quedan reservas.
  `VulkanMemoryAllocator` (`vulkan_memory_allocator.*`): tabla fija de
  `ResourceKind` (Buffer/Image, por separado para esquivar
  `bufferImageGranularity`) x `VK_MAX_MEMORY_TYPES` listas de
  `unique_ptr<VulkanMemoryBlock>`; `allocate(requirements, properties, kind)`
  prueba los bloques en orden (first-fit entre bloques) y, si ninguno cabe, crea
  uno de `max(64 MB, tamaño pedido)`; lanza si no hay tipo de memoria.
  `m_blocks` sin llaves de inicialización (con ellas el compilador pedía el tipo
  completo del bloque en la cabecera). Cuelga de `VulkanDevice`
  (`memory_allocator()`), declarado sobre la tabla bindless, los frames y la
  `DeletionQueue` y bajo el `vk::Device`: el destructor del device vacía la cola
  con el asignador vivo y los bloques mueren antes que el `vk::Device`.
  `MemoryAllocation` es RAII y solo movible: (memoria, offset, size, puntero
  mapeado, bloque) con campos privados y accesores, `reset()` idempotente,
  `operator bool` explícito; se libera sola al morir porque ya conoce su bloque
  (no hay `free` en el asignador). Trampa: `CHECK(a)` no compila con el
  `explicit operator bool` (hay que `static_cast<bool>`).
  8d hecho: `VulkanBuffer` y `VulkanTexture` piden su trozo (`ResourceKind::Buffer`
  / `Image`, `eDeviceLocal`) y hacen `bindMemory(memoria, offset)`. La
  `MemoryAllocation` ocupa el sitio del antiguo `DeviceMemory` en la lista de
  miembros: primero muere el `VkBuffer`/`VkImage` y luego el trozo vuelve al
  bloque, también si el constructor lanza a medias.
  8e hecho: `VulkanStagingBuffer` y `VulkanFrameAllocator` también usan el
  asignador (memoria visible y coherente) y escriben en el puntero mapeado de la
  `MemoryAllocation`: el staging ya no mapea/desmapea en cada `write_data`.
  Ningún recurso hace su propio `vkAllocateMemory`. Como los asignadores por
  frame reservan al crear el device, un device nuevo ya tiene un bloque
  host-visible (los tests parten de una línea base de bloques). `trim()`
  destruye los bloques vacíos gigantes y los vacíos redundantes, conservando
  como máximo uno vacío de tamaño estándar por bucket (seguro en cualquier
  momento: un bloque solo está vacío si todas sus reservas se han liberado);
  `create_buffer`/`create_texture` lo llaman tras una subida con
  `initial_data` mayor que un bloque (el staging gigante ya ha muerto). Tests
  17-22 de `test_deferred` (bloque, asignador, device, 5000 buffers en un solo
  bloque, trim, subida grande).
  Sin mutex, a propósito: el asignador no es seguro entre hilos y no tendría
  sentido protegerlo solo a él mientras `submit_immediate` (pool y cola
  compartidos) sea monohilo; contrato escrito en `vulkan_memory_allocator.h`
  con el plan para cuando haya carga multihilo.
- Hecho (cámaras): `Camera2D` y `Camera3D` en `luxel/include/luxel/cameras.h`
  (pública; implementación en `luxel/src/cameras.cpp`, incluida desde
  `luxel.h`). Son solo matrices (posición, rotación en radianes, zoom y
  proyección, `view_proj_matrix()`): cámara = concepto de render. El movimiento
  con teclado salió de `Camera3D` a `FlyCameraController`
  (`tests/test_1/src/fly_camera_controller.h`, lógica de juego que solo usa la
  API pública de la cámara; se mudará a una capa de motor cuando exista).
  `test_1` usa una `Camera2D` por ventana de 2 unidades de alto y
  `2 * aspecto` de ancho (se actualiza cada frame con el extent de la
  swapchain) y pasa `view_proj * rotateZ(angle)` al shader: el quad es cuadrado
  en cualquier ventana. No hizo falta tocar los shaders (el VS ya multiplica
  por la matriz que reciba). Con `orthographic` tipo OpenGL, +Y del mundo va
  hacia abajo en pantalla (Vulkan); decidir la convención de Y de la cámara 2D
  antes de usarla en serio.
- Hecho (R1a, shaders incrustados): `luxel/cmake/embed_spirv.cmake` (script que
  se ejecuta con `cmake -P`; lee el `.spv` con `file(READ ... HEX)` y escribe
  una cabecera con `inline constexpr std::uint32_t <nombre>_spv[]`, dando la
  vuelta a los bytes de cada palabra por ser little-endian; la primera palabra
  es el número mágico `0x07230203`). En `luxel/CMakeLists.txt` cada shader
  tiene dos recetas encadenadas: `.hlsl` -> `dxc` -> `.spv` -> script ->
  `<build>/luxel/shaders/<nombre>_spv.h` (el script está en `DEPENDS` para
  regenerar si cambia); `compile_shaders` pide las cabeceras. `test_1` y
  `test_deferred` incluyen esa carpeta (adiós a `LUXEL_SHADER_DIR` y a leer de
  disco) y pasan el array directamente como `span<const uint32_t>`.
  IntelliSense marca las cabeceras en las configuraciones aún no compiladas
  (no existen hasta compilar): es normal. Pendiente para R2: convertir el
  `foreach` en una función `luxel_embed_shaders(target ...)` para que los
  shaders del motor sean privados de luxel y cada test compile los suyos.
- Hecho (R1b, blending): `BlendMode` (`Opaque`, `Alpha`, `PremultipliedAlpha`,
  `Additive`) en `types.h` y campo `blend_mode` en `GraphicsPipelineDescriptor`
  (por defecto `Opaque`). Se eligió un enum de modos en vez de exponer los
  factores (más difícil de usar mal; se ampliará si hace falta).
  `to_vk_blend()` en `vulkan_graphics_pipeline.cpp` traduce a factores de
  Vulkan; alfa destino siempre `One` / `OneMinusSrcAlpha` (`One` / `One` en
  aditivo). El `switch` cubre todos los casos sin `default` para que `-Wswitch`
  avise si se añade un modo; tras él, `assert`. `test_1` usa `Alpha` con un PNG
  con transparencia (con `Opaque` lo transparente se ve negro: es el RGB del
  PNG). Premultiplicado exige premultiplicar la textura al cargarla (los PNG no
  lo están). Test 23 de `test_deferred`: un pipeline por modo, sin avisos de
  validación.
- Siguiente: R1c de `docs/renderers-design.md`, convención de Y de `Camera2D`.
  Pendiente del hito 8, para más
  adelante: llamar a `trim()` también de forma periódica (p. ej. en
  `begin_frame`) si aparecen más rutas de subida grandes, y asignaciones
  dedicadas si hay recursos enormes de vida larga.
- Entorno de compilación (para el asistente): desde Git Bash, cargar
  `vcvars64.bat` de VS 2026 (`C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build`)
  y `cmake --build out/build/clangcl-debug`. No hay presets de build, solo de
  configuración.
- Hecho (ventana/bucle): `Window::wait_events()` (`WaitMessage`, bloquea hasta
  el siguiente mensaje). El bucle de la app lo llama en cada vuelta mientras la
  ventana está minimizada (`width()`/`height()` a 0) y hace `continue`; así la
  CPU baja a ~0% (no debe ir dentro de `poll_events()`, que es no bloqueante, ni
  en el manejador de `Resize`, que solo dormiría una vez). El tamaño de la
  swapchain de `test_1` ya sale de la ventana.
- Hecho (varias ventanas): la clase de ventana se registra una sola vez por
  proceso (marca aparte, nombre constante `k_window_class_name`; todas las
  ventanas comparten `StaticWndProc` y encuentran su `Impl` por
  `GWLP_USERDATA`, puesto en `WM_NCCREATE`). Un contador de ventanas vivas hace
  que `PostQuitMessage` solo se publique al destruir la última (si no, una
  ventana que muere hace que `poll_events` de las demás vea `WM_QUIT`).
  `WM_CLOSE` solo pide el cierre (`should_close` y evento `Close`); la
  destrucción es la del objeto `Window`. `test_1` abre una segunda ventana con
  su swapchain en una struct `RenderWindow` (ventana declarada antes que la
  swapchain, así la swapchain muere primero). El bucle de `test_1` es genérico:
  `std::vector<std::unique_ptr<RenderWindow>>` (la ventana no se mueve), cada una
  con su cola de eventos (`process_events`, que atiende `Resize` y `Close`, con
  confirmación solo en la principal), `std::erase_if` por `should_close`, `wait_events` si
  todas están minimizadas y `render_window_frame` por ventana (si `begin_frame`
  devuelve `nullptr` solo salta esa ventana). Termina al cerrar la última.
- Pendiente (ventana/bucle): redibujar durante el arrastre del borde (bucle
  modal de Win32; aparcado, aporta poco: haría falta un callback de la app desde
  `WM_ENTERSIZEMOVE`/`WM_SIZE`; `render_window_frame` ya es una función).
- Aparcado en la ventana (se retoma más adelante): Raw Input, DPI,
  UTF-8 en títulos.

## Compilación
- Ficheros de CMakeLists.txt tanto en luxel como en vitreum y uno general en librerias.
- Build con MSVC o CLang configurado en los CMakePresets.json
