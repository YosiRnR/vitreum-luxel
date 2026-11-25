# Diseño de la librería de UI (`luxel_ui`)

> **Estado: U1 y U2 hechos; siguiente, U3.** Las decisiones están numeradas
> (D1-D9) para poder hablar de ellas una a una.

Widgets (botones, listas, cajas de texto, paneles) por encima del `Renderer2D`,
pensados para un editor tipo Godot y también para los menús de un juego.

**Ya acordado**: modo *retained* (los widgets son objetos que persisten), pero
redibujando el árbol entero en cada frame. Nada de marcar zonas sucias: hoy el
HUD de Iria (191 quads) cuesta 0,1 ms de CPU en Debug.

## El recorrido completo, con un escenario

Un panel con dos botones, "Guardar" y "Salir". El usuario hace clic en "Salir":

```
 vitreum                aplicación               luxel_ui                   luxel_2d
 WinEvent  ──────────►  to_ui(event)  ────────►  ui.on_event(event)
 (ratón en 130, 80)                              1. ¿qué hay en (130,80)?  -> botón "Salir"
                                                 2. se lo da al botón; si no lo quiere,
                                                    a su padre (el panel), y así hacia arriba
                                                 3. el botón llama a su on_click
                        después, una vez por frame:
                        ui.layout(ancho, alto)   4. medir y colocar el árbol
                        ui.draw(r2d)  ────────►  5. recorrerlo y pintar  ────►  rectángulos,
                                                                                texto, recortes
```

## D1. Dónde vive

Biblioteca CMake aparte, `luxel_ui` (namespace `luxel::ui`), que enlaza con
`luxel_2d` y solo usa su API pública. **No conoce `vitreum`**: tiene sus propios
eventos y la aplicación los convierte con una función `to_ui(const WinEvent&)`,
igual que hace `to_luxel()` con la ventana.

```
  aplicación (conoce a todos)
     │            │
  vitreum      luxel_ui ──► luxel_2d ──► luxel
```

Por qué: la UI se puede probar sin ventana (se le inventan eventos) y sigue
sirviendo si algún día hay otra plataforma.

## D2. El árbol y quién es dueño de quién

```cpp
class Widget {
    std::vector<std::unique_ptr<Widget>> m_children;  // el padre es DUEÑO de sus hijos
    Widget* m_parent{ nullptr };                      // solo para subir: no es dueño
    Rect m_rect{};                                    // dónde quedó (lo pone el layout)
};

auto& panel = ui.root().add<Column>();
auto& salir = panel.add<Button>(u8"Salir");           // add devuelve una referencia
salir.on_click = [&] { quit = true; };
```

Es la misma idea que las ventanas de Win32 (al destruir una ventana mueren sus
hijas), pero con `unique_ptr`. No hace falta `Ref`: cada widget tiene un solo
dueño, su padre.

**El problema difícil: borrar un widget desde su propio evento.** Escenario: el
botón "Cerrar" de un diálogo hace `dialog.remove()`. El diálogo destruye sus
hijos, entre ellos el botón... que está a mitad de su `on_click`. Al volver, usa
memoria ya liberada.

Solución, la misma idea que la `DeletionQueue` de luxel: `remove()` solo marca, y
`Ui` borra de verdad al terminar de repartir el evento.

Además, `Ui` guarda punteros a widgets (el que está bajo el ratón, el pulsado, el
que tiene el foco). Cuando uno se borra, esos punteros vuelven a `nullptr`.

## D3. Layout en dos pasadas

1. **Medir** (de abajo arriba): cada widget dice cuánto querría medir.
2. **Colocar** (de arriba abajo): cada padre reparte su rectángulo.

Hacen falta las dos porque el padre no puede colocar a sus hijos sin saber cuánto
ocupan, y los hijos no saben cuánto hay hasta que el padre reparte.

Escenario: una columna de 300 px de alto, con margen interior 10 y separación 8
entre hijos. Tres hijos: título (30), lista (que crece) y botón (40).

```
  medir:    30 + lista 0 + 40 + 2 separaciones de 8 + 2 márgenes de 10 = 106
  sobran:   300 - 106 = 194  ->  para los que "crecen": la lista mide 194
  colocar:  título y = 10..40, lista y = 48..242, botón y = 250..290
```

Dos hijos que crecen se reparten lo que sobra según su peso (`grow`): con pesos 1
y 3, un cuarto y tres cuartos. Es el *stretch ratio* de Godot.

Contenedores del principio: `Row`, `Column` y `Stack` (uno encima de otro).
Unidades: píxeles `float`; al colocar se redondea a píxel entero (con bordes a
medio píxel, los rectángulos saldrían borrosos por el suavizado de R4b).

Se recalcula todo en cada frame. Si algún día pesa, se marca qué ha cambiado.

## D4. Qué widget hay bajo el ratón (*hit test*)

Se baja por el árbol: en cada nivel se miran los hijos **del último al primero**,
porque el último se pinta encima. Si el widget recorta a sus hijos (una lista con
scroll) y el ratón está fuera de su rectángulo, no se baja: una fila escondida no
recibe clics.

Se repite también después del layout, aunque el ratón no se mueva: si la lista se
desplaza con la rueda, lo que hay debajo cambia.

## D5. Reparto de eventos

- **Subida (*bubbling*)**: el evento va al widget de debajo del ratón; si devuelve
  `false` ("no es para mí"), se prueba con su padre, y así hasta la raíz. Escenario:
  la rueda sobre un botón dentro de una lista. El botón no la quiere y sube hasta
  la lista, que se desplaza.
- **Captura**: el que recibe la pulsación recibe todo hasta soltar. Escenario:
  pulsas "Salir", te arrepientes, arrastras fuera y sueltas: el botón se entera y
  NO hace clic. Clic = soltar encima del mismo widget que se pulsó.
- **Entrar y salir**: `on_enter` / `on_leave`, para el color de "ratón encima".
- **Foco**: uno como mucho; recibe el teclado y el texto. Se cambia con un clic o
  con Tab.
- **`ui.on_event()` devuelve `bool`**: si la UI ha usado el evento, el juego no
  debe verlo. Escenario: pulsar espacio con un botón enfocado no hace saltar a
  Iria.

## D6. Dibujo

```cpp
virtual void draw(Painter& p) const;
```

`Painter` envuelve al `Renderer2D` con lo que necesita un widget (`rect`, `text`,
`clip`) y el tema. Los widgets no llaman a `begin`/`end`: lo hace `Ui`.

El orden es el del árbol (primero el padre, luego sus hijos), que ya es el de
envío, así que todo va en la misma capa. Lo que flota (menús desplegables,
*tooltips*) va en una lista aparte que se pinta la última y se mira la primera en
el hit test.

## D7. Aspecto (tema)

Una struct `Theme` con la fuente, los colores, los radios y los márgenes; una para
toda la UI. Sin hojas de estilo ni herencia por ahora.

## D8. Avisos a la aplicación

`std::function` como miembro público (`button.on_click = ...`). Regla: lo que
capture la lambda debe vivir más que el widget.

En la otra dirección (el objeto del juego cambia y la etiqueta debe enterarse),
de momento a mano: `label.set_text(...)`. Un sistema de enlaces automáticos es
cómodo pero oculta lo que pasa; se verá cuando haya un inspector de propiedades.

## D9. Pruebas

Casi todo es lógica sin GPU, así que va en `tests/test_ui`: árbol, layout, hit
test y reparto de eventos, con widgets de prueba de tamaño fijo (así no hace
falta una fuente, que necesita un `Device`). El dibujo se ve en `test_1`.

## Plan de hitos

Cada uno acaba en algo visible. Entre paréntesis, la parte más formativa.

- **U1. Árbol y ratón. (Hecho.)** `Widget`, `Ui`, eventos propios y `to_ui`, `Painter`,
  `Panel` y `Label`. Demo: paneles anidados que se iluminan bajo el ratón.
  (*Hit test*: una función recursiva de unas 10 líneas.)
- **U2. Botón. (Hecho.)** Captura, clic, subida de eventos y borrado aplazado. Demo: los
  botones de Iria funcionan; "Salir" cierra la ventana. (El estado del botón:
  normal, encima, pulsado, pulsado pero con el ratón fuera.)
- **U3. Layout.** `Row`, `Column`, `Stack`, márgenes, separación y `grow`. Demo: el
  panel se adapta al tamaño de la ventana. (Repartir el espacio que sobra.)
- **U4. Scroll.** `ScrollView` sobre el recorte de R4d, rueda y barra que se
  arrastra. Demo: la lista de objetos, ahora con la rueda. (Pasar de píxeles de
  barra a píxeles de contenido.)
- **U5. Foco y texto.** Tab, teclado y `TextField`. Demo: escribir un nombre y que
  el HUD salude con él. (Cursor en UTF-8: borrar "ñ" son 2 bytes.)

Después, según lo pida el editor: casillas, deslizadores, menús, árbol de nodos,
paneles que se redimensionan y *docking*.

## Lo que hace falta en vitreum

Ya hay movimiento, botones (con captura de Win32, así que se sigue recibiendo el
ratón fuera de la ventana), rueda, teclado y `CharInput`. Falta o conviene revisar:

| Qué | Para | Nota |
|---|---|---|
| Aviso de que el ratón sale de la ventana (`WM_MOUSELEAVE`) | U1 | si no, el último widget se queda iluminado |
| `MouseScroll` no trae posición | U4 | la UI usa la del último movimiento; vale |
| `ReleaseCapture` solo al soltar el último botón (hecho en U2) | U2 | con dos botones pulsados, soltar uno perdía la captura del otro |
| `CharInput` manda también teclas de control (Retroceso = 8) | U5 | filtrarlas en la UI |
| `CharInput` manda los emoji en dos mitades UTF-16 | U5 | juntarlas en vitreum |
| Cursor de texto y de redimensionar | U5 | hoy solo la flecha |
