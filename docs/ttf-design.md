# Diseño del rasterizador de TrueType (`luxel_ttf`)

> **Estado: T1, T2 y T3 hechos; siguiente, T4.** Las decisiones están numeradas (F1-F7) para
> poder hablar de ellas una a una.

Leer un fichero `.ttf` y pintar sus letras a mano, sin GDI ni bibliotecas de
terceros, para sustituir el atlas que hoy genera `test_1` con GDI
(`tests/test_1/src/gdi_font_atlas.*`). `BitmapFont` y el `Renderer2D` ya no saben de
dónde sale el atlas, así que el cambio queda por debajo de ellos.

## El recorrido completo, con un escenario

La letra "ñ" de Segoe UI negrita, a 48 px:

```
 bytes del .ttf          luxel_ttf                                          luxel_2d
 (los lee la app)
 ───────────────►  1. directorio de tablas: dónde empieza cada una (head, cmap, glyf...)
                   2. cmap:  U+00F1 'ñ'  ->  glifo nº 245 (un número interno de la fuente)
                   3. loca:  glifo 245   ->  sus bytes dentro de la tabla glyf
                   4. glyf:  contornos (puntos en "unidades de la fuente", 2048 por em)
                             la ñ puede ser COMPUESTA: "n" + "~" movida encima
                   5. escalar 2048 -> 48 px y aplanar las curvas en segmentos
                   6. rasterizar: cuánto tapa la letra cada píxel (0-255)
                   7. colocarla en el atlas, con su avance y su posición  ──────►  BitmapFont
```

## Lo que ya hay en `truetype/` (tu código antiguo)

Hay más de lo que recordabas, y casi todo el esqueleto está bien:

| Pieza | Estado | Nota |
|---|---|---|
| `SpanReader` (big-endian) | bien | no comprueba límites: un fichero roto lee fuera |
| Directorio de tablas, `head`, `maxp`, `hhea`, `hmtx` | bien | |
| `loca` | **fallo** (arreglado en T1) | en formato corto se guarda el offset dividido entre 2 y no se multiplicaba. No se notaba con Segoe UI, Arial ni Consolas (usan el largo); sí con Lucida Console |
| `cmap` formato 4 | bien | solo el plano básico (hasta U+FFFF); un `unordered_map` con todos los códigos |
| `glyf` simple (flags, repetición, coordenadas delta) | bien | |
| `glyf` compuesto | falta | en muchas fuentes las letras con tilde lo son |
| Puntos implícitos entre dos de control | bien | |
| Rasterizador | funciona | líneas de barrido con 8x8 muestras por píxel y corrección de gamma (que sobra: la cobertura debe ser lineal) |
| Atlas por estantes | bien | pero `BitmapFont` espera una rejilla; el espacio (sin contornos) se queda sin avance |
| Clase `TruetypeFont` | a medias | `load` y `generate_atlas` son privados; `load(path)` lee de disco; lanza `const char*` |

Plan: **no se reescribe a ciegas**. Cada sub-hito lleva una pieza a `luxel_ttf`,
la revisamos, la arreglamos y la cubrimos con tests. `truetype/` se queda como
referencia hasta el final; entonces decides si se borra.

## F1. Dónde vive

Biblioteca CMake aparte, `luxel_ttf` (namespace `luxel::ttf`), **solo std**: no
enlaza con `luxel` ni con `luxel_2d`. Recibe bytes y devuelve píxeles y medidas.

```
  aplicación ──► luxel_ttf      (bytes -> atlas en memoria)
      │
      └────────► luxel_2d ──► luxel   (atlas -> textura -> texto en pantalla)
```

Por qué: se prueba entera sin GPU (`tests/test_ttf`), y como `BitmapFont`, no lee
ficheros (regla del proyecto). Los tests y `test_1` leen las fuentes de
`C:\Windows\Fonts` (no se versionan: tienen licencia).

## F2. Leer sin fiarse del fichero

Un `.ttf` es una lista de offsets que apuntan a otras partes del fichero. Escenario:
un fichero cortado a la mitad dice que `glyf` empieza en el byte 90.000 y solo
tiene 50.000. Hoy, `SpanReader` leería memoria de fuera.

- `Reader` comprueba cada lectura y lanza `std::runtime_error` (con qué tabla falló).
- Las subtablas se leen con su propio `Reader` sobre su trozo (como ya hacías):
  un offset nunca puede salirse de su tabla.

## F3. De carácter a glifo (`cmap`)

Se guarda la tabla tal cual (segmentos) y se busca en ella, en vez de un mapa con
todos los códigos. Formatos:
- **4**: plano básico, por segmentos `[start, end]` (búsqueda binaria por `end`).
- **12**: todo Unicode, por grupos. Lo tiene Arial; sirve para lo que pase de U+FFFF.

Los emoji quedan fuera: Segoe UI Emoji los guarda en color (tablas `COLR`/`CPAL`) y
eso es otro mundo. Seguirán saliendo como "?".

## F4. Contornos

Un glifo son contornos cerrados de puntos "en la curva" (on) y "de control" (off):

```
  on ─── on        dos on seguidos: una recta
  on  off  on      una curva cuadrática (Bézier de 3 puntos)
  on  off  off on  dos off seguidos: hay un on implícito en su punto medio
```

Salida: un `Outline` en píxeles (ya escalado, +Y abajo) con segmentos y cuadráticas.
Los **compuestos** son una lista de "pinta el glifo X movido (y quizá escalado)":
la ñ = la n + la virgulilla desplazada hacia arriba. Se resuelven de forma recursiva,
con un tope de profundidad (un fichero malicioso podría hacer que un glifo se incluya
a sí mismo).

Sin *hinting*: las instrucciones de la fuente (que ajustan los contornos a la rejilla
de píxeles a tamaños pequeños) se saltan, como ya hacías. A tamaños de UI (16-48 px)
se nota poco; GDI sí lo usa, así que a 14 px la suya se verá algo más nítida.

## F5. Rasterizar: cobertura exacta en vez de muestras

Tu rasterizador toma 64 muestras por píxel y cuenta cuántas caen dentro. Funciona,
pero cuesta 64 veces más que hacerlo una vez y los cantos casi horizontales siguen
viéndose escalonados (solo 8 niveles en vertical).

La alternativa (la de *font-rs* y la versión 2 de *stb_truetype*): calcular **el
área exacta** de cada píxel que queda dentro. Escenario: una recta vertical que
cruza un píxel justo por la mitad -> el píxel está tapado al 50 % -> alfa 128.

La idea en dos pasos:
1. Cada segmento deja en su fila, en un buffer de `float`, "cuánto cambia la
   cobertura" en cada píxel que toca, con signo: positivo si el segmento baja y
   negativo si sube (los contornos de una letra van en un sentido y los agujeros,
   como el de la "o", en el contrario).
2. Una suma acumulada por fila convierte esos cambios en cobertura: lo que entra a
   la izquierda de la letra se mantiene hasta que otro segmento lo quita. Al final,
   valor absoluto y tope en 1: equivale a la regla non-zero de tu rasterizador
   (dos contornos que se pisan no se "restan").

Las cuadráticas se aplanan en segmentos (como ya hacías) con un número de pasos
según su tamaño. Sin gamma: la cobertura es lineal y la mezcla alfa del
`Renderer2D` ya hace lo correcto.

## F6. El atlas y lo que necesita `BitmapFont`

Hoy `BitmapFont` exige una rejilla de celdas iguales y dibuja la celda entera. Con
TTF sabemos la caja exacta de cada letra, así que se puede hacer mejor:
- **Atlas por estantes** (tu `generate_atlas`): cada glifo ocupa lo justo.
- `BitmapFont` aprende un segundo modo: por glifo, su rectángulo en el atlas, su
  desplazamiento respecto al lápiz (*bearing*) y su avance. El de rejilla se queda
  (lo sigue usando quien lo quiera).
- Los glifos sin contorno (el espacio) también tienen avance.
- Medidas de la línea desde `hhea`: `ascent`, `descent` y `line_gap`; la línea de
  base queda a `ascent` píxeles de la parte de arriba.

## F7. Pruebas

`tests/test_ttf`, sin GPU, con las fuentes del sistema (si no existen, el test lo
dice y se salta ese caso):
- tablas: `units_per_em` = 2048 en Segoe UI, número de glifos, `loca` creciente;
- `cmap`: 'A', 'ñ', '€' tienen glifo; un código que no existe da el 0 (`.notdef`);
- contornos: 'l' tiene 1, 'o' tiene 2, 'ñ' (compuesta o no) tiene los de la n más la
  virgulilla;
- rasterizador con formas conocidas: un cuadrado que tapa medio píxel = 128, un
  triángulo, dos contornos superpuestos (non-zero);
- atlas: ningún glifo se pisa con otro y todos caben.

Y a ojo en `test_1`: el HUD de Iria con la fuente propia al lado de la de GDI.

## Plan de hitos

Entre paréntesis, la parte más formativa (la que escribes tú).

- **T1. Lector y tablas. (Hecho.)** `luxel_ttf` con `Reader` que comprueba límites,
  directorio de tablas, `head`, `maxp`, `hhea`, `hmtx` y `loca`. (Encontrar y
  arreglar el fallo de `loca` sin que te diga dónde está: con el test delante.)
- **T2. `cmap`. (Hecho.)** Formatos 4 y 12, búsqueda en los segmentos. (La búsqueda del
  formato 4, con su `idRangeOffset`, que es la parte retorcida.)
- **T3. Contornos. (Hecho.)** `glyf` simple y compuesto, puntos implícitos, `Outline` en
  píxeles. (Los compuestos: aplicar el desplazamiento y la escala de cada pieza.)
- **T4. Rasterizador.** Cobertura exacta por área acumulada. (El paso de un segmento
  al buffer de cambios: el corazón de todo.)
- **T5. Atlas y `BitmapFont`.** Estantes, métricas, el modo por glifo de
  `BitmapFont` y `test_1` con la fuente propia. (Colocar cada letra respecto a la
  línea de base.)
- **T6 (opcional). Kerning.** Pares de la tabla `kern` (la "AV" que se junta). GPOS
  es bastante más complejo; se verá si hace falta.

## Decisiones abiertas

- Varios tamaños de la misma fuente: hoy, un atlas por tamaño. Más adelante, un
  atlas que se rellena bajo demanda o campos de distancia (SDF) para escalar sin
  perder nitidez.
- Qué pasa con `truetype/` al acabar: borrar, o dejarlo como historia.
