# Diseño de los renderers de luxel

Este documento cubre lo que va **por encima del RHI** (`docs/rhi-design.md`):
los renderers de alto nivel y, más adelante, la capa de motor. El objetivo
final es que el motor sirva tanto para aplicaciones con UI como para juegos 2D
y 3D.

## Capas

```
  Aplicación / juego / editor
  ─────────────────────────────────────────────
  Capa de motor:   Scene, entidades, componentes,     <- "qué hay en el mundo"
                   librería de UI (widgets, layout)
  ─────────────────────────────────────────────
  Renderers:       Renderer2D, Renderer3D             <- "cómo se dibuja eficientemente"
                   (batching, materiales, mallas)
  ─────────────────────────────────────────────
  RHI (luxel hoy): Device, Buffer, Texture,           <- "cómo se habla con la GPU"
                   Pipeline, CommandList
```

Reglas:

- **Los renderers no conocen la `Scene`.** La escena (o la UI, o la aplicación
  directamente) recorre lo que tiene y se lo envía al renderer en cada frame:
  "dibuja este sprite aquí", "dibuja esta malla con este material". Así el
  `Renderer2D` sirve igual para un juego que para una herramienta que solo
  pinta rectángulos. Es el mismo criterio que separó `Camera3D` (render) de
  `FlyCameraController` (lógica).
- **Los renderers solo usan la API pública del RHI.** Si un renderer necesita
  algo que el RHI no ofrece (blending, profundidad...), se añade al RHI; el
  renderer nunca ve Vulkan.
- **Cada renderer es dueño de sus shaders**, que van incrustados en el
  ejecutable: la aplicación no debe tener que buscarlos en disco.

## Renderer2D y batching

Problema: 10.000 sprites con un `draw()` cada uno son 10.000 llamadas por frame;
la CPU se ahoga antes que la GPU.

Solución (batching):

1. `draw_quad(...)` no dibuja: añade un registro (posición, tamaño, rotación,
   color, índice de textura, UVs, capa) a un `std::vector` en CPU.
2. Al cerrar el lote, el vector se copia al asignador por frame
   (`Frame::allocate`).
3. Un único `draw(6 * n)`. El vertex shader saca el quad con
   `SV_VertexID / 6` y la esquina con `SV_VertexID % 6` (vertex pulling, como ya
   hace `test_1`).

Con bindless, **cambiar de textura no rompe el lote**: cada quad lleva su
`texture_index`. Lo que sí obliga a cortar: cambio de pipeline (otro shader u
otro modo de blending) o de recorte (scissor).

Orden: los quads se dibujan en el orden de envío, o se ordenan por capa antes
de subirlos. Con transparencia el orden importa (de atrás hacia delante).

### Decisiones de R2 (acordadas)

1. **Dónde vive**: biblioteca CMake aparte, `luxel_2d`, en el namespace `luxel`,
   que enlaza con `luxel` como cualquier aplicación. Así el compilador hace
   cumplir que solo use la API pública (no puede incluir `luxel/src/`).
2. **Uso**, dentro de `begin_rendering`/`end_rendering` (que siguen siendo de
   la aplicación):
   ```cpp
   Renderer2D r2d(device, swapchain_format);   // pipeline, textura blanca 1x1, sampler
   r2d.begin(frame, camera.view_proj_matrix());
   r2d.draw_quad({ .position = {0, 0}, .size = {1, 1}, .texture = tex.get() });
   r2d.end();                                   // ordena, copia y dibuja
   ```
   `begin` recibe una **matriz**, no una `Camera2D`: la UI le pasará su
   proyección en píxeles.
3. **`draw_quad(const Quad&)`**: struct con valores por defecto y designated
   initializers: `position` (centro), `size`, `rotation` (radianes,
   antihorario), `color`, `texture` (`nullptr` = textura blanca, es decir,
   color liso), `sampler` (`nullptr` = lineal con ClampToEdge; añadido en
   R2b, ocupa parte del relleno), `uv_min` (arriba a la izquierda) /
   `uv_max` (abajo a la derecha), `layer`.
4. **Registro en GPU (`QuadInstance`, 64 bytes)**: `float2 position`,
   `float2 size`, `float2 uv_min`, `float2 uv_max`, `float4 color`,
   `float rotation`, `uint texture_index`, `uint sampler_index`, `uint` de
   relleno. Las esquinas se
   calculan en el vertex shader. La capa no va a la GPU (solo ordena en CPU).
   `static_assert(sizeof(QuadInstance) == 64)` para no desincronizarse del HLSL.
   El color en `float4` por sencillez (empaquetar en RGBA8 queda como mejora).
5. **Push constants**: la matriz `view_proj` (64 bytes) + slot y offset de las
   instancias = 72 de 128 bytes. La matriz no pasa por `Frame::allocate`.
6. **Orden**: `std::stable_sort` por `layer` en `end()`; dentro de una capa se
   respeta el orden de envío.

Límite: el asignador por frame tiene 1 MB, unos 16.000 quads de 64 bytes; si
no caben, `end()` parte el lote en varios `draw` (R2d).

## UI

La UI se divide en dos partes que no conviene mezclar:

- **Render de la UI**: casi todo son rectángulos (con color, redondeados, con
  borde, con imagen) y texto. No es un tercer renderer, sino el `Renderer2D`
  con más primitivas:
  - rectángulos redondeados y bordes resueltos en el pixel shader con una
    función de distancia (SDF): el mismo quad con otros parámetros;
  - recorte (clipping) para paneles con scroll: scissor o rectángulo de recorte
    por quad;
  - texto: atlas de glifos, cada letra es un quad.
- **Librería de UI** (capa de motor): layout, widgets, foco, clics y estado.
  Consume eventos de `vitreum` y emite rectángulos al `Renderer2D`. Decisión
  pendiente: *immediate mode* (como ImGui, `if (button("OK"))` cada frame; el
  camino corto para herramientas) o *retained* (árbol de widgets persistente).

## Renderer3D

- **Mesh**: buffer de vértices (storage, leído por vertex pulling) + buffer de
  índices + número de índices.
- **Material**, en dos niveles:
  - *tipo* de material: pipeline + shaders ("opaco con luz", "sin luz",
    "transparente");
  - *instancia*: parámetros (color base, índice de textura, rugosidad...). Con
    bindless, una instancia es una struct dentro de un storage buffer y cada
    objeto pasa el índice de su material en las push constants.
- **API**: `submit(mesh, material, transform)` acumula; al final se ordena por
  pipeline (cambiar de pipeline sí es caro) y se dibuja.
- Necesita del RHI: depth buffer (formato de profundidad,
  `TextureUsage::DepthStencil`, adjunto en `begin_rendering`, depth test en el
  pipeline).

## Lo que le falta hoy al RHI

- ~~**Blending**~~: hecho en R1b.
- **Profundidad**: no hay depth buffer. Hace falta para el 3D.
- ~~**Shaders incrustados**~~: hecho en R1a.

## Plan de hitos

- **R1. Cimientos.**
  - R1a (hecho): shaders incrustados en el ejecutable. Cadena de recetas en
    `luxel/CMakeLists.txt`: `.hlsl` -> `dxc` -> `.spv` ->
    `cmake/embed_spirv.cmake` -> `<nombre>_spv.h` con un
    `constexpr std::uint32_t[]` (alineado a 4, encaja con
    `span<const uint32_t>` sin casts). Desde R2a, función
    `luxel_embed_shaders(target ...)` para separar shaders del motor
    (privados) y de cada test.
  - R1b (hecho): blending configurable en `GraphicsPipelineDescriptor` con
    `BlendMode` (`Opaque`, `Alpha`, `PremultipliedAlpha`, `Additive`).
  - R1c (hecho): convención de ejes. Mundo +Y arriba (ángulo positivo =
    antihorario); NDC de luxel +Y arriba, que Vulkan cumple con viewport de
    altura negativa; UV (0,0) arriba a la izquierda; la UI usará su propia
    proyección en píxeles ((0,0) arriba a la izquierda, +Y abajo). Pendiente
    para R5: `frontFace` con el viewport invertido y rango de Z [0, 1].
- **R2. Renderer2D con batching**: quads con color, textura, rotación y capa;
  un draw por lote. Primer resultado visible; usa asignador por frame,
  bindless y cámara. Diseño en "Decisiones de R2".
  - R2a (hecho): función CMake `luxel_embed_shaders(<target> <.hlsl...>)`
    (`luxel/cmake/luxel_shaders.cmake`); cada target
    compila e incrusta sus propios shaders (los de `test_1` y `test_deferred`
    pasan a `tests/shaders/`; `luxel` se queda sin shaders).
  - R2b (hecho): biblioteca `luxel_2d` (`luxel_2d/`) y `Renderer2D` con
    `begin`/`draw_quad`/`end`. Salió completo de una vez: color, textura,
    sampler, UVs y rotación. `test_1` dibuja rectángulos de color, un sprite
    girando y un rectángulo semitransparente en un solo draw.
  - R2c (hecho): pruebas a fondo en `test_1`: 10.000 quads girando en un
    lote (0,87 ms de CPU en Debug), hoja de sprites animada recortada por UV y
    comparación `Nearest`/`Linear` (el borde de 1 píxel entre celdas evita el
    "bleeding" del filtrado lineal). Recursos de los tests en `tests/assets/`.
  - R2d: orden por capa y lotes partidos si no caben; test sin GPU de la parte
    de CPU.
- **R3. Texto**: primero fuente bitmap (imagen con los caracteres en rejilla);
  más adelante, rasterizar TTF a mano (hito de aprendizaje, como el PNG).
- **R4. Primitivas de UI**: rectángulos redondeados con SDF, bordes, recorte.
  Después, librería de UI mínima con input.
- **R5. 3D**: depth buffer, `Mesh`, materiales, una luz direccional. Después,
  carga de modelos (OBJ a mano; glTF necesita un parser de JSON, otro posible
  hito).
- **R6. Capa de motor**: `Scene`, entidades y componentes. Va al final a
  propósito: con renderers que funcionen se sabe qué necesita la escena.

## Aparcado a propósito

Tienen sentido, pero cuando aparezca el problema que resuelven: render graph,
ECS "de verdad", rendering dirigido por GPU (draws indirectos), PBR. Del RHI:
asignaciones dedicadas y `trim()` periódico (hito 8).
