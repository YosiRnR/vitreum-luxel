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
auto vertices = device->create_buffer({ .size = sizeof(verts), .usage = luxel::BufferUsage::Storage,
                                        .initial_data = verts });
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
| `Shader` | Bytecode de una etapa | `Device` | Hasta crear los pipelines | Shader module | Blob DXIL | Shader object |
| `GraphicsPipeline` | Estado completo de dibujo | `Device` | `Ref` + destrucción diferida | VkPipeline (layout global) | PSO (root signature global) | Programa + estado emulado |

Tres reglas de propiedad:

1. El `Device` sobrevive a todo. Cada objeto guarda un `Ref` a su dispositivo, así que el orden de destrucción del usuario deja de importar.
2. Soltar un `Ref` no destruye en el acto: el recurso entra en una cola y se libera cuando terminan los frames en vuelo que podrían usarlo.
3. `Frame` y `CommandList` no se guardan entre frames; son temporales y los posee el dispositivo.

El `Device` es una sola interfaz aunque por dentro tenga instancia, físico y lógico. Esas piezas son detalles de Vulkan; D3D12 y OpenGL no tienen esa división.

## Modelo de enlace de recursos

Propuesta: bindless. Cada buffer, textura y sampler recibe al crearse un índice en una tabla global, y el shader recibe esos índices por push constants. El usuario nunca crea ni enlaza descriptor sets.

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
5. **Tabla bindless, `Shader` y `GraphicsPipeline`.** Triángulo con vértices fijos en el shader.
6. **`Buffer` con subida por staging y asignador temporal por frame.** Triángulo con *vertex pulling* y una matriz por frame (aquí entran las matemáticas de `luxel/src/math`).
7. **`Texture` y `Sampler`.** Un quad texturizado.

Al cerrar cada hito: ejecutar con validación sin avisos y repasar en papel cómo sería cada función nueva en D3D12 y en OpenGL.

## Decisiones abiertas

Seis decisiones son tuyas; ninguna bloquea el hito 1, pero conviene cerrar la 1 y la 2 antes del hito 2.

| # | Decisión | Opciones | Mi inclinación |
| --- | --- | --- | --- |
| 1 | Cómo se referencian los recursos | `Ref<T>` a interfaces / handles con generación (`BufferHandle{index, gen}`) | `Ref<T>`: ya lo tienes y es más fácil de aprender; los handles valen la pena con miles de objetos |
| 2 | Lenguaje de shaders | GLSL → SPIR-V (`glslc`) / HLSL → SPIR-V y DXIL (`dxc`) | HLSL con `dxc` si D3D12 va en serio: un solo fuente para ambos backends |
| 3 | OpenGL sin bindless | Modo degradado con slots fijos / exigir `ARB_bindless_texture` / no soportar OpenGL | Modo degradado, y más adelante |
| 4 | Frames en vuelo | Fijo en 2 / configurable | Fijo en 2 |
| 5 | Errores | Excepciones / `std::expected` (C++23) / códigos propios | Excepciones al crear, nada en el bucle de frame |
| 6 | Dónde vive `to_luxel()` | En cada aplicación / librería puente `vitreum_luxel` | En la aplicación hasta que haya dos que la repitan |
