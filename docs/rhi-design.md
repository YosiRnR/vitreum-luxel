# Luxel — Diseño de la arquitectura del RHI

Nov 7, 2026 · Juan Vicente

> Copia en el repo del documento de diseño (originalmente en Claude Docs). El diagrama de capas no se exporta; ver la sección "Capas y dependencias".

## Objetivo y principios

La aplicación habla solo con un RHI propio de Luxel, sin un solo tipo de Vulkan; el backend se elige al crear el dispositivo y todo lo específico de cada API queda privado dentro de `luxel`.

Hoy `tests/test_1` crea un `VulkanContext` e incluye `vulkan_raii.hpp`. Este diseño mueve ese código detrás de interfaces sin tirarlo: `VulkanContext` pasa a ser parte de `VulkanDevice`.

Principios:

- **Pocos pasos para el usuario.** El caso "limpiar la pantalla" cabe en unas 10 líneas y el triángulo en unas 20. Lo repetitivo (frames en vuelo, semáforos, barreras de la swapchain, destrucción diferida) lo hace el RHI.
- **Modelo explícito.** La forma de la API sigue a Vulkan, D3D12 y Metal (dispositivo, swapchain, listas de comandos, pipelines cerrados). OpenGL y D3D11 se adaptan a ese modelo, no al revés.
- **Enlace de recursos sin descriptores visibles.** Un modelo bindless con push constants: el usuario pasa índices, no construye descriptor sets (sección de enlace).
- **Rebanada vertical.** Se diseña solo lo que pide el siguiente resultado visible: limpiar, triángulo, textura. Cada función nueva se contrasta en papel con D3D12 y OpenGL antes de escribirla.
- **Monohilo por ahora.** Una lista de comandos por frame; la grabación en paralelo queda fuera de este diseño.

## Capas y dependencias

`vitreum` y `luxel` no se conocen; la aplicación las une, y dentro de `luxel` solo la API pública es visible.

> [Diagrama omitido en la exportación: capas y dependencias · app, vitreum, luxel y sus backends]

Reglas que lo hacen cumplir:

- **Includes con prefijo.** `#include <luxel/device.h>`, `#include <vitreum/window.h>`. Se acaba el choque de los dos `types.h`.
- **CMake.** `target_include_directories(luxel PUBLIC include PRIVATE src)` y `target_link_libraries(luxel PRIVATE Vulkan::Vulkan)`. Si una cabecera pública necesitara Vulkan, el diseño está mal, no el CMake.
- **Una fábrica por backend.** `create_device()` es lo único que conoce a `VulkanDevice`. Cada backend se compila solo si está disponible (`if(Vulkan_FOUND)`, `if(WIN32)` para D3D12).

## Modelo de uso

El test entero se reduce a crear un dispositivo, crear una swapchain por ventana y repetir `begin_frame` / `end_frame`; esto es la API objetivo, no código para copiar.

**Paso 1: limpiar la pantalla.**

```cpp
#include <luxel/luxel.h>   // ninguna cabecera de Vulkan

auto device    = luxel::create_device({ .backend = luxel::Backend::Vulkan, .debug = true });
auto swapchain = device->create_swapchain({ .window = to_luxel(wnd.native_handle()),
                                            .width = 1920, .height = 1080, .vsync = true });

while (!wnd.should_close()) {
    wnd.poll_events();                       // un Resize llama a swapchain->resize(w, h)
    auto frame = device->begin_frame(*swapchain);
    if (!frame) continue;                    // minimizada o swapchain obsoleta

    auto& cmd = frame->commands();
    cmd.begin_rendering({ .color = { frame->backbuffer(), luxel::ClearColor{ 0.1f, 0.1f, 0.15f, 1.0f } } });
    cmd.end_rendering();
    device->end_frame(*frame);               // submit + present
}
device->wait_idle();
```

**Paso 2: el triángulo** añade tres objetos y tres llamadas:

```cpp
auto vertices = device->create_buffer(luxel::BufferDescriptor::from_data(verts));  // uso Storage por defecto
auto pipeline = device->create_graphics_pipeline({ .vertex = vs, .fragment = fs,
                                                   .color_formats = { swapchain->format() } });
// dentro del frame:
cmd.bind_pipeline(*pipeline);
cmd.push_constants(DrawData{ .vertices = vertices->bindless_index() });
cmd.draw(3);
```

Lo que **no** aparece en la parte de uso: descriptor sets, layouts, semáforos, fences, índices de imagen de la swapchain, barreras de la imagen de la swapchain ni el orden de destrucción.

`to_luxel()` es la única función que conoce a la vez `vitreum` y `luxel`. Vive en la aplicación (o en una librería puente diminuta), nunca dentro de ninguna de las dos.

## Objetos del RHI

Todo nace del `Device` y se devuelve como `Ref<T>` a una interfaz abstracta; cuando el último `Ref` muere, el dispositivo destruye el recurso más tarde, cuando la GPU ya no lo usa.

| Objeto | Para qué | Lo crea | Vida | Vulkan | D3D12 | OpenGL |
| --- | --- | --- | --- | --- | --- | --- |
| `Device` | Punto de entrada; fábrica de todo lo demás | `create_device()` | Toda la aplicación; muere el último | Instancia + físico + lógico + colas + tabla bindless | Factory + device + queue + heaps | Contexto GL |
| `Swapchain` | Imágenes de una ventana; una por ventana | `Device` | La de su ventana | Surface + swapchain | IDXGISwapChain | Contexto por ventana + SwapBuffers |
| `Frame` | Un frame en curso: lista de comandos, backbuffer, memoria temporal | `begin_frame()` | Hasta `end_frame()` | Fence, semáforos, command pool por frame | Allocator + fence por frame | Casi vacío |
| `CommandList` | Grabar trabajo de GPU | Pertenece al `Frame` | La del frame | Command buffer | Command list | Llamadas directas o grabadas |
| `Buffer` | Vértices, índices, datos | `Device` | `Ref` + destrucción diferida | VkBuffer + memoria | Resource | Buffer object |
| `Texture` | Imágenes, render targets | `Device` | `Ref` + destrucción diferida | VkImage + vista | Resource + SRV | Texture object |
| `Sampler` | Filtrado y direccionamiento | `Device` | `Ref` + destrucción diferida | VkSampler | Sampler heap | Sampler object |
| `GraphicsPipeline` | Estado completo de dibujo; recibe el bytecode de cada etapa en su descriptor | `Device` | `Ref` + destrucción diferida | VkPipeline (layout global); los `VkShaderModule` son temporales dentro de la creación | PSO (root signature global) | Programa + estado emulado |

No hay un objeto `Shader` público: D3D12 crea el PSO con los bytes de cada etapa, OpenGL compila y enlaza de una vez, y el `VkShaderModule` solo hace falta mientras se crea el pipeline. El núcleo de luxel solo recibe bytes ya cargados (no lee ficheros); los shaders propios del motor se incrustarán en el ejecutable y los del usuario se cargan fuera del núcleo. Si compartir módulos entre pipelines llegara a importar, se añade un `Shader` encima sin romper la API.

Tres reglas de propiedad:

1. El `Device` sobrevive a todo. Cada objeto guarda un `Ref` a su dispositivo, así que el orden de destrucción del usuario deja de importar.
2. Soltar un `Ref` no destruye en el acto: el recurso entra en una cola y se libera cuando terminan los frames en vuelo que podrían usarlo.
3. `Frame` y `CommandList` no se guardan entre frames; son temporales y los posee el dispositivo.

El `Device` es una sola interfaz aunque por dentro tenga instancia, físico y lógico. Esas piezas son detalles de Vulkan; D3D12 y OpenGL no tienen esa división.

## Modelo de enlace de recursos

**Decisión (hito 5): bindless.** Se prioriza aprender el modelo moderno; OpenGL queda como modo degradado para más adelante (decisión 3) y no se pasa antes por el modelo clásico, porque bindless sigue usando descriptor sets, layout, pool y `vkUpdateDescriptorSets`: solo cambia que hay un único set enorme y nunca cambia. Los bind groups de WebGPU quedan descartados por ahora.

Cada buffer, textura y sampler recibe al crearse un índice en una tabla global, y el shader recibe esos índices por push constants. El usuario nunca crea ni enlaza descriptor sets.

**Por qué se complica el modelo clásico.** En Vulkan clásico cada pipeline tiene su layout y cada material sus descriptor sets, agrupados por frecuencia (por frame, por material, por draw). Eso obliga a mantener tres cosas sincronizadas: lo que declara el shader, el layout del pipeline y los sets que enlaza el código. Cuando el proyecto crece, la mayoría de los errores nacen ahí. Bindless elimina esa triple coincidencia: solo hay un layout y no cambia nunca.

**Cómo funciona por dentro (backend Vulkan):**

1. El `Device` crea una vez un descriptor set grande con arrays: texturas muestreadas, storage buffers y samplers. Usa *descriptor indexing*, que es núcleo desde Vulkan 1.2.
2. Hay un único `VkPipelineLayout` global: ese set + un bloque de push constants (128 bytes garantizados). Todos los pipelines lo comparten.
3. Al crear un recurso se toma un hueco libre de la tabla y se escribe su descriptor. Al destruirlo, el hueco vuelve a la lista libre, también de forma diferida.
4. Al empezar cada lista de comandos se enlaza el set global una sola vez.

**Cómo se ve desde el shader (pseudocódigo GLSL):**

```glsl
layout(push_constant) uniform DrawData { uint vertices; uint albedo; uint smp; } pc;
Vertex v = vertex_buffers[nonuniformEXT(pc.vertices)].data[gl_VertexIndex];
vec4 c = texture(sampler2D(textures[pc.albedo], samplers[pc.smp]), uv);
```

**Tres piezas completan el modelo:**

- **Vértices por *vertex pulling*.** El shader lee los vértices de un storage buffer con `gl_VertexIndex`. Así el pipeline no declara formato de vértices. Los index buffers siguen enlazándose de forma clásica (`bind_index_buffer`).
- **Datos por frame con un asignador temporal.** `frame->allocate(datos)` copia a un buffer anillo del frame y devuelve un índice u offset para pasarlo por push constants. Sustituye a los uniform buffers que se recrean.
- **Push constants pequeñas.** Solo índices y algunos valores. Las matrices grandes van en el asignador temporal.

**Correspondencia con los otros backends:**

| Concepto | Vulkan 1.3 | D3D12 | OpenGL 4.6 |
| --- | --- | --- | --- |
| Tabla global | Descriptor set con descriptor indexing | Heap visible al shader, `ResourceDescriptorHeap[]` (SM 6.6) | Sin equivalente directo |
| Índices por draw | Push constants | Root constants | Uniforms o SSBO pequeño |
| Texturas por índice | Array de imágenes | Heap de descriptores | `ARB_bindless_texture` (no universal) o arrays de texturas |
| Buffers por índice | Array de storage buffers | Heap de descriptores | Array de SSBO limitado |

**Costes que se aceptan:** los shaders se escriben en estilo bindless (hay que marcar los índices no uniformes); un índice equivocado no da error de validación, sino basura en pantalla; y OpenGL necesitará un modo degradado. La alternativa es el modelo de *bind groups* de WebGPU, más portátil a OpenGL pero con más pasos en el uso, que es justo lo que se quiere evitar.

## Sincronización y frames en vuelo

El RHI mantiene 2 frames en vuelo y se encarga de toda la sincronización CPU–GPU; el usuario solo decide el orden de sus pases.

**Lo que hace el RHI:**

- `begin_frame` espera la fence del frame que reutiliza (el de hace 2), reinicia su command pool y su asignador temporal, libera lo que estaba en la cola de destrucción y adquiere la imagen de la swapchain.
- `end_frame` envía la lista de comandos y presenta. El semáforo de "render terminado" va por imagen de la swapchain, no por frame, que es el error típico que señala la capa de validación.
- Transiciones de la imagen de la swapchain: a *attachment* al empezar a pintar y a *present* al final.
- Seguimiento simple de estado: cada textura recuerda su layout actual y la `CommandList` inserta la barrera necesaria al usarla como attachment o como textura muestreada.
- Redimensionado: si la swapchain queda obsoleta, o tras `resize()`, se recrea en el siguiente `begin_frame`. Con la ventana minimizada (0×0) `begin_frame` devuelve vacío y no se pinta.

**Lo que queda en manos del usuario:** el orden de los pases dentro del frame y `wait_idle()` antes de salir.

**Lo que queda fuera de este diseño:** varias colas (cómputo asíncrono, copia dedicada), grabación multihilo y un render graph que calcule las barreras de todo el frame. El seguimiento simple de estado es el primer paso hacia ese render graph y no lo bloquea.

## Plan de hitos

Siete hitos pequeños, cada uno con un resultado que se puede ejecutar; el primero esconde Vulkan detrás de un `Device` mínimo.

1. **Reorganizar `luxel` y crear un `Device` mínimo.** Cabeceras públicas en `luxel/include/luxel/`, backends privados en `src/backends/` y `Vulkan::Vulkan` en `PRIVATE`. `create_device()` devuelve un `Device` casi vacío que envuelve el `VulkanContext` actual; el `WindowHandle` va de forma transitoria en `DeviceDesc`. Hecho cuando `test_1` no contiene ninguna referencia a Vulkan.
2. **Dispositivo físico y lógico.** `VulkanDevice` absorbe `VulkanContext`, elige la GPU (colas gráfica y de presentación) y crea el dispositivo 1.3 (dynamic rendering, synchronization2, descriptor indexing).
3. **`Swapchain` y el bucle de frame.** La superficie pasa a vivir aquí y el `WindowHandle` sale de `DeviceDesc`. `begin_frame` / `end_frame` y limpiar a un color. Incluye cerrar el pendiente de la ventana con `WM_SIZE` minimizado y el tamaño de cliente.
4. **Destrucción diferida y `Ref`.** Revisar `Ref::adopt` y el contador inicial antes de usarlo en serio.
5. **Tabla bindless y `GraphicsPipeline`.** Triángulo con vértices fijos en el shader.
6. **`Buffer` con subida por staging y asignador temporal por frame.** Triángulo con *vertex pulling* y una matriz por frame (aquí entran las matemáticas, hoy públicas en `luxel/include/luxel/math`).
7. **`Texture` y `Sampler`.** Un quad texturizado.

Pasos del hito 5 (cada uno compila y se ejecuta sin avisos de validación):

- **5a. Features de descriptor indexing (hecho).** Exigir y activar en `VulkanDevice` (tablas de features existentes): `descriptorBindingPartiallyBound`, `runtimeDescriptorArray`, `shaderSampledImageArrayNonUniformIndexing`, `descriptorBindingSampledImageUpdateAfterBind`, `descriptorBindingUpdateUnusedWhilePending` y los equivalentes de storage buffer. Los tamaños de la tabla son configurables en `DeviceDescriptor` (`device_maximus`) y se validan contra los límites de la GPU al elegirla.
- **5b. Tabla global (hecho).** `VulkanBindlessTable`: pool con `UPDATE_AFTER_BIND`, set layout con arrays de tamaño fijo (texturas, samplers, storage buffers), un set, y lista de huecos libres con liberación diferida (reutiliza `DeletionQueue`).
- **5c. Pipeline layout global (hecho).** Un `VkPipelineLayout` con esa tabla y un rango de push constants; test de caja blanca de asignar y liberar huecos.
- **5d. `GraphicsPipeline`** (hecho) (sin objeto `Shader` público; ver la tabla de objetos). Descriptor mínimo: bytes del vertex y del fragment shader (SPIR-V de `dxc`), nombre de la función de entrada de cada etapa (por defecto `vs_main` y `ps_main`), formato de color (`luxel::Format`) y topología (lista de triángulos). Todo lo demás es fijo e interno: sin vertex input (vertex pulling), sin culling, sin profundidad, sin blending, una muestra, viewport y scissor dinámicos (uno de cada). El descriptor irá creciendo cuando haya requisitos reales.
  - 5d-1 (hecho): `shader_vs.hlsl` y `shader_ps.hlsl` en `luxel/shaders`, compilados a mano con `dxc -spirv`.
  - 5d-2 (hecho): `GraphicsPipelineDescriptor`, interfaz pública `GraphicsPipeline` y `Device::create_graphics_pipeline()`.
  - 5d-3 (hecho): `VulkanGraphicsPipeline` con módulos temporales, los bloques de estado y dynamic rendering, usando el layout global.
  - 5d-4 (hecho): CMake compila los shaders con `dxc`. Convención de nombres: `<nombre>_vs.hlsl` y `<nombre>_ps.hlsl` (el sufijo decide el perfil `vs_6_0`/`ps_6_0` y la entrada `vs_main`/`ps_main`; los demás se ignoran). Los `.spv` van a la carpeta de compilación.
- **5e. Triángulo** (hecho) con vértices fijos en el shader, enlazando la tabla una vez por lista de comandos (en cada `begin_frame`, porque el pool se resetea). Viewport y scissor se fijan en `begin_rendering`; `CommandList` expone `bind_pipeline` y `draw`.

Pasos del hito 6 (propuesta, se ajusta sobre la marcha):

- **6a. Interfaz pública (hecho).** `luxel/include/luxel/buffer.h` (`BufferUsage` como flags con `operator|`, `&`, `|=` y `has_flag`; `BufferDescriptor` con `from_data`; `Buffer` con `size()`, `usage()` y `bindless_index()`, que devuelve 0 si no hay slot) y `Device::create_buffer()`. Pendiente menor: `operator|=` debería devolver `BufferUsage&`. Hasta el 6b hace falta un `create_buffer` provisional en `VulkanDevice` (lanzar excepción) para que compile.
- **6b. `VulkanBuffer` sin datos (hecho; `test_deferred` casos 7-9 en verde, sin avisos de validación).** `VkBuffer` + memoria (elegir el tipo de memoria a mano con `vkGetPhysicalDeviceMemoryProperties`, sin VMA), constructor privado + fábrica estática como el resto, slot de `allocate_storage_buffer_slot` si el uso incluye `Storage`, y destrucción diferida que libera el slot y el `VkBuffer`. Traducción `BufferUsage` -> `vk::BufferUsageFlags` en una función libre (como `vulkan_format.h`): `Storage` -> `eStorageBuffer`, `Index` -> `eIndexBuffer`, y siempre `eTransferDst` (necesario para la subida por staging del 6c).
- **6c. Subida inicial por staging (hecho; `test_deferred` caso 10 en verde, sin avisos de validación).** El staging es una clase interna mínima (`VulkanStagingBuffer`: buffer `eTransferSrc` en memoria `eHostVisible | eHostCoherent`; `write_data` mapea, copia y desmapea; sin `Ref`, sin hueco bindless, sin cola de borrado), no un modo de `VulkanBuffer`. La subida bloquea: `create_buffer` espera a la GPU; encolar la copia en el siguiente frame queda para más adelante. Subpasos:
  - 6c-1 (hecho): `find_memory_type` en `VulkanDevice` (`const`, lanza si no hay tipo adecuado) y `VulkanStagingBuffer`.
  - 6c-2 (hecho): `VulkanDevice::submit_immediate(recorder)`: command buffer de un solo uso reservado de un pool transitorio, enviado a la cola gráfica con `queue.waitIdle()` (es lo que hace el tutorial de Vulkan; su coste es parar también los frames en vuelo; una fence propia sería más fina). Las colas no admiten envíos desde dos hilos a la vez: vale mientras el diseño sea monohilo.
  - 6c-3 (hecho): el constructor de `VulkanBuffer` sube `initial_data` si hay (staging, `copyBuffer` con `size = initial_data.size()` y no el tamaño del buffer) antes de reservar el slot bindless. Sin barrera: como se espera a la GPU antes de volver, la validación no la pide. Pasa a ser obligatoria (`TRANSFER_WRITE` -> lectura como storage o índice) si algún día la copia se encola sin esperar. El contenido no se verifica por lectura de vuelta (el buffer no es fuente de copia); se verá en el 6e.
- **6d. Asignador temporal por frame.** Decidido: un único buffer grande por frame en memoria visible, mapeado de forma persistente y registrado una sola vez en la tabla bindless al crearse (un hueco por frame en vuelo); no un hueco por reserva (cada uno costaría un `vkUpdateDescriptorSets`, un hueco del `SlotAllocator` y su liberación diferida). `frame->allocate(datos)` devuelve `{ índice del buffer, offset en bytes }` (el índice es el mismo para todas las reservas del frame; lo que cambia es el offset) y el shader lee con `ByteAddressBuffer`. El cursor avanza con alineación (16 bytes como mínimo; `minStorageBufferOffsetAlignment` no aplica porque el offset se calcula en el shader y no en el descriptor), se reinicia en `begin_frame` tras esperar la fence del frame y, si se llena, lanza. Subpasos (todos hechos):
  - 6d-1 (hecho): `BufferAllocation { slot_index, offset }` y `Frame::allocate(std::span<const std::byte>)` en `luxel/frame.h`; `VulkanFrame::allocate` delega en el allocator del frame.
  - 6d-2 (hecho): `VulkanFrameAllocator` (interno, `vulkan_frame_allocator.*`): buffer de almacenamiento de 1 MB (`k_frame_allocator_capacity`) en memoria visible y coherente, mapeado de forma persistente, con su hueco bindless pedido al final del constructor y liberado en el destructor (puntero crudo a la tabla, que sobrevive a los frames). `allocate` alinea el cursor a 16, lanza `runtime_error` si no cabe y devuelve el offset de inicio de la reserva; `reset_cursor()` lo pone a 0.
  - 6d-3 (hecho): un `std::unique_ptr<VulkanFrameAllocator>` por `FrameResources`, creado en `create_frames()` (la tabla bindless se crea antes que los frames). `begin_frame` llama a `reset_cursor()` tras esperar la fence y pasa el allocator a `VulkanFrame::reset`. Trampa vista: con `unique_ptr`, `.reset()` destruye el allocator y `->reset()` llama al suyo; de ahí el nombre `reset_cursor()`.
  - 6d-4 (hecho): tests 11 (allocator aislado: offsets, alineación, reset, desbordamiento, reutilización del hueco) y 12 (`Frame::allocate` en frames reales: reinicio por frame y huecos distintos entre frames en vuelo) en `test_deferred`. El contenido escrito no se verifica por lectura; se verá en el 6e.
- **6e. Triángulo con vertex pulling y una matriz por frame (hecho).** Verificado visualmente en `test_1`: triángulo girando, sin avisos de validación. Con esto queda comprobado el contenido de los buffers subidos por staging y del asignador por frame. Subpasos:
  - 6e-1 (hecho): `CommandList::bind_constants(std::span<const std::byte>)`, tamaño > 0, múltiplo de 4 y máximo 128 bytes (lanza si no). `VulkanCommandList` guarda el `vk::PipelineLayout` global (llega por `reset`, desde `begin_frame` a través de `VulkanFrame::reset`) y usa `stageFlags`, offset y tope de `VulkanBindlessTable::k_push_constant_range`. En D3D12 serían root constants y en OpenGL un uniform o un buffer pequeño.
  - 6e-2 (hecho): `shader_vs.hlsl` con `[[vk::push_constant]]` (`vertex_buffer_slot`, `transform_matrix_buffer_slot`, `transform_matrix_buffer_offset`), `ByteAddressBuffer bindless_buffers[]` en el binding 0 del set 0, `Load2` para el vértice y cuatro `Load4` para la matriz. `NonUniformResourceIndex` no hace falta: el índice viene de una push constant y es uniforme en el draw. Trampa vista: la matriz de C++ es column-major y el constructor `matrix(a, b, c, d)` de HLSL toma filas, así que se carga y se traspone (`transpose(matrix(col0, col1, col2, col3))`) para que `mul(mat, v)` equivalga a `M * v`.
  - 6e-3 (hecho): las matemáticas pasan a ser públicas: `luxel/include/luxel/math/vectors.h` y `matrix4.h` (namespace `luxel::math`), incluidas desde `luxel.h`. La aplicación y el motor comparten tipos sin capa de traducción.
  - 6e-4 (hecho): `test_1` crea el buffer de vértices con `from_data(..., Storage)`, y cada frame escribe `rotateZ(angle)` con `frame->allocate`, empuja `{ vb_slot, slot, offset }` con `bind_constants` y dibuja.

Al cerrar cada hito: ejecutar con validación sin avisos y repasar en papel cómo sería cada función nueva en D3D12 y en OpenGL.

## Decisiones abiertas

Seis decisiones son tuyas; ninguna bloquea el hito 1, pero conviene cerrar la 1 y la 2 antes del hito 2.

| # | Decisión | Opciones | Mi inclinación |
| --- | --- | --- | --- |
| 1 | Cómo se referencian los recursos | `Ref<T>` a interfaces / handles con generación (`BufferHandle{index, gen}`) | `Ref<T>`: ya lo tienes y es más fácil de aprender; los handles valen la pena con miles de objetos |
| 2 | Lenguaje de shaders | GLSL → SPIR-V (`glslc`) / HLSL → SPIR-V y DXIL (`dxc`) | **Decidido (hito 5): HLSL con `dxc`.** Un solo fuente para ambos backends; `dxc` viene en el Vulkan SDK |
| 3 | OpenGL sin bindless | Modo degradado con slots fijos / exigir `ARB_bindless_texture` / no soportar OpenGL | Modo degradado, y más adelante |
| 4 | Frames en vuelo | Fijo en 2 / configurable | Fijo en 2 |
| 5 | Errores | Excepciones / `std::expected` (C++23) / códigos propios | Excepciones al crear, nada en el bucle de frame |
| 6 | Dónde vive `to_luxel()` | En cada aplicación / librería puente `vitreum_luxel` | En la aplicación hasta que haya dos que la repitan |
