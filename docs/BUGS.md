# Errores arreglados

Qué pasó y cómo se resolvió en cada arreglo.

## Colores cambiados en los fondos (2026-11-22)

- **Qué pasaba:** en el visor y en los PNG exportados, el rojo y el azul salían cambiados de sitio: hojas azules en el fondo iluminado y tono amarillento en el fondo a oscuras.
- **Por qué:** la paleta de los `LMVR` guarda cada color como azul, verde, rojo y un byte que no se usa (BGRA), y se leía como si fuera rojo, verde, azul (RGBA).
- **Arreglo:** leer la paleta en orden BGRA en `src/core/viewPicture.cpp` y en `tools/aitd4data.py`, y volver a exportar los PNG. Corregido también en `docs/FORMATOS.md`.
- **Cómo se comprobó:** nivel 1, cámara 1 (invernadero). Ahora el fondo a oscuras se ve azulado, como luz de luna, y el iluminado se ve cálido, con hojas verdes, como en el juego.

## Triángulos sueltos en la escena de la cámara (2026-11-22)

- **Qué pasaba:** al dibujar la malla de la escena (`G3DB`) aparecían triángulos grandes donde no tocaba. La columna de prueba se veía a través de las hojas en forma de triángulos, y `tools/drawViewMeshes.py` dibujaba contornos raros.
- **Por qué:** cada "polígono" de la escena se leía como un polígono convexo y se cortaba en abanico desde su primer punto. En realidad es una **tira de triángulos** (cada punto forma un triángulo con los dos anteriores).
- **Arreglo:** cortar las tiras como tiras en `sceneTriangles` (`src/core/viewScene.cpp`) y en `strip_triangles` (`tools/aitd4data.py`, que ahora usa `drawViewMeshes.py`). Corregido también en `docs/FORMATOS.md`.
- **Cómo se comprobó:** como tira, casi todas tienen sus triángulos del mismo lado (451 de 451 en el nivel 1, cámara 1); como abanico, solo 15. En el visor, el primer plano resaltado (F) ya no tiene huecos en triángulo y la columna se esconde bien detrás de las plantas.

## Cortes en forma de cuadrado en el primer plano (2026-11-22)

- **Qué pasaba:** en el nivel 144, cámara 1 (la avioneta), el primer plano (hierba, arbustos, valla) se veía cortado por líneas rectas, como cuadrados.
- **Por qué:** el color del primer plano se sacaba de las piezas de la hoja D, que están pegadas unas a otras. Al ampliar la imagen, la tarjeta suaviza mezclando cada píxel con sus vecinos, y en el borde de una pieza esos vecinos son de otra pieza. Con cientos de piezas pequeñas, los bordes se notan. En el nivel 1 casi no se veía porque sus piezas son pocas y grandes.
- **Arreglo:** el primer plano toma el color del fondo en el mismo punto de la pantalla (es el mismo color, sin cortes posibles). De la hoja solo usa la máscara (bit 0x80), leída píxel a píxel y sin suavizar (`foregroundMask` en `src/core/viewScene.cpp`, `foreground_ps.sc`). Sobraba la copia iluminada de la hoja que arma el juego, y se quitó.
- **Cómo se comprobó:** capturas del visor con `--level 144 --camera 1`. Las líneas ya no están, y la columna de prueba sigue escondiéndose detrás de la valla y los arbustos. El nivel 1 sigue igual.

## La linterna oscurecía partes del nivel 144 (2026-11-22)

- **Qué pasaba:** en el nivel 144, cámara 1 (la avioneta), al alumbrar la loma de hierba de abajo se oscurecía en vez de aclararse. La avioneta sí se iluminaba bien.
- **Por qué:** los colores se leían bien; es cosa de los datos. En esa escena la imagen "a oscuras" tiene luz de luna, y la "iluminada" parece tener solo la luz de la linterna. En el 14% de sus píxeles (24% en la cámara 2) la iluminada es más oscura: la hierba, el cielo bajo la avioneta, los arbustos y el agua. En el nivel 1 solo pasa en el 2%. La linterna del visor cambiaba una imagen por la otra, así que restaba la luna.
- **Arreglo (provisional):** donde alumbra la linterna, cada píxel toma el más claro de los dos (`flashlightColour` en `src/platform/shaders/flashlight.sh`). La luz ya nunca oscurece. Falta la fórmula real del juego, y ver la luz completa (luna y demás luces) cuando el motor esté más avanzado.
- **Cómo se comprobó:** captura del visor con `--level 144 --camera 1` y la linterna sobre la hierba: ya no se oscurece.

## Los scripts de Ghidra no estaban en Git (2026-11-23)

- **Qué pasaba:** `tools/ghidra/` (`run.ps1`, `DecompileByString.java`, `CountFunctions.java`) nunca entró en los commits.
- **Por qué:** la línea `ghidra/` de `.gitignore`, pensada para el proyecto de Ghidra de la raíz, también ocultaba `tools/ghidra/`.
- **Arreglo:** `/ghidra/` (solo la carpeta de la raíz).
- **Cómo se comprobó:** `git check-ignore` ya no los marca y aparecen como archivos nuevos.

## La media vuelta movía el cuerpo dentro del modelo (2026-11-23)

- **Qué pasaba:** en el visor, la animación de media vuelta (116) giraba y retrocedía el cuerpo entero dentro del modelo; si además se movía al personaje con esa animación, el giro se contaba dos veces.
- **Por qué:** al armar la pose se usaban todas las pistas, también las del hueso 0 (la raíz). El juego se las salta (`FUN_004279c0`, `if (bone != 0)`): esas pistas no son parte de la pose, sino el movimiento del personaje entero, que `FUN_004285a0` lee aparte.
- **Arreglo:** `poseSkeleton` (`src/core/skeletonModel.cpp`) y `pose_skeleton` (`tools/aitd4data.py`) dejan fuera el hueso 0; su movimiento se lee con `rootMotion` y lo aplica el cerebro del personaje (`src/core/characterBrain.cpp`).
- **Cómo se comprobó:** en el visor, con dos toques de ↓, Carnby da la media vuelta y queda mirando al lado contrario (180°, no 360°), sin deslizarse dentro de su sitio.

## La mano derecha vacía, abierta y rígida (2026-11-23)

- **Qué pasaba:** sin arma, la mano derecha (sobre todo la de Aline, que va sin abrigo) se veía abierta en horizontal, con los dedos rígidos, quieta, corriendo o andando.
- **Por qué:** para la mano vacía hay dos modelos en `weapon\<personaje>\`: `righthand` (la mano abierta, para algún gesto) y `righthd` (la que cuelga, la que lleva la tabla de acciones de "sin arma"). El visor ponía la primera.
- **Arreglo:** la mano vacía por defecto es `righthd` (`loadCharacter` en `src/app/main.cpp`); `righthand` sigue en la lista de G.
- **Cómo se comprobó:** capturas de Aline en el nivel 144 con las dos manos: con `righthd` la mano cuelga natural.

## No se oía nada (2026-11-23)

- **Qué pasaba:** con el sonido hecho, no se oía nada por los altavoces.
- **Por qué:** dos cosas. La salida por defecto de Windows era la tele por HDMI (SAMSUNG), no los altavoces, y el visor usaba siempre la de por defecto. Además, al probar, los scripts del nivel 1 solo tocan efectos en momentos concretos, así que andando no sonaba nada.
- **Arreglo:** **F4** cambia de salida (la de por defecto y luego cada dispositivo) y la elegida se guarda en `UserData\soundOutput.txt`, junto al ejecutable, para la próxima vez; el título dice por cuál suena. **P** toca el siguiente sonido del banco del nivel, para comprobarlo cuando se quiera. El volumen general va siempre al 100 % (`SDL_SetAudioStreamGain`).
- **Cómo se comprobó:** grabando a la vez la salida de los tres dispositivos (captura *loopback* con el paquete `soundcard` de Python) mientras el visor tocaba sonidos con P: con la salida por defecto sonaba solo por la tele; con Altavoces, por los altavoces (picos de 0,6 a 0,8); y al abrir de nuevo, sigue en Altavoces.

## La luz de la linterna dejaba trozos grandes sin iluminar (2026-11-23)

**Qué pasaba:** con la linterna encendida solo se iluminaban manchas sueltas (los objetos del primer plano) y el resto del haz no se veía donde debía.

**Por qué:** el haz salía en la dirección hacia donde mira el personaje con una inclinación puesta a ojo, no hacia donde apunta la linterna. Casi horizontal y desde la cadera, el cono pasaba por encima de lo que se ve y solo tocaba paredes y objetos sueltos. Para encontrarlo se pintó en colores la fuerza de la luz y la profundidad en el haz de cada punto de la malla.

**Arreglo:** como el juego (`0x449662` → `0x418461`), el foco se pone en la muñeca izquierda con el giro de la mano, y alumbra a lo largo del eje +y de la mano (la profundidad de la vista del foco en `0x41A2C0`). Así sigue al brazo en cada animación.

## La linterna iluminaba polígonos grandes fuera del haz (2026-11-23)

**Qué pasaba:** con la linterna, algunas zonas se iluminaban como polígonos grandes de bordes rectos, fuera de donde debía llegar la luz (por ejemplo, un rectángulo alrededor del globo en el nivel 14).

**Por qué:** la malla de cada cámara tiene la habitación (grupo 0) y, para los objetos del primer plano, piezas planas grandes ("cartones") que tapan mucho más que el objeto; la máscara del fondo (bit 0x80) dice qué parte es el objeto de verdad. La pasada de luz dibujaba los cartones enteros sin mirar la máscara: si un cartón caía en el haz, se iluminaba todo su polígono, también lo que en la imagen es pared o suelo detrás.

**Arreglo:** la malla se separa en triángulos de la habitación y de los objetos (`sceneTriangles` los da en ese orden); en la pasada de luz la habitación se dibuja entera y los objetos solo donde la máscara dice que hay objeto (allí detrás sigue la habitación, con su propia luz).

## El sonido no seguía la salida predeterminada de Windows (2026-11-23)

**Qué pasaba:** desde el arreglo anterior, la salida elegida con F4 ("Altavoces") quedaba guardada en `UserData\soundOutput.txt` y se usaba siempre, aunque en Windows se pusieran por defecto los auriculares o el HDMI.

**Arreglo:** el motor ya no elige ni guarda salida: abre siempre la predeterminada de Windows (`SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK`, que SDL cambia sola si la predeterminada cambia con el juego abierto), con el volumen general al 100 %. Se quitaron F4 y el archivo. Lo que el usuario quiere es que el sonido salga siempre por donde Windows diga; el volumen lo bajará él cuando haya opciones.

## Los monstruos del susto del nivel 12 salían con colores raros (2026-11-23)

**Qué pasaba:** en el nivel 12, cámara 1, al pasar por cierto sitio con la luz encendida aparecen de golpe seis monstruos (el script de la zona 7 los muestra y oculta varias veces). Salían con la textura convertida en ruido de colores.

**Por qué:** su textura tiene la marca `0x04` (la de las demás es `0x1003` o `0x1012`): el juego la sube con el formato 0x300, que `FUN_0048e037` descomprime antes; es el **VQ de Dreamcast** (un libro de 256 bloques de 2×2 píxeles y un byte por bloque de la imagen, ambos en orden Z). El motor la leía como píxeles sueltos.

**Arreglo:** `readTexture` (`src/core/skeletonModel.cpp`) y `read_texture` (`tools/aitd4data.py`) descomprimen las texturas con la marca 0x04.

## La partida de Carnby empezaba en el sitio equivocado (2026-11-24)

**Qué pasaba:** Carnby empezaba en el jardín del nivel 1, sin vídeos antes.
**Por qué:** el nivel de comienzo se había deducido mirando los datos, no el ejecutable.
**Arreglo:** el bucle principal del juego (`0x4798C9`) calcula el nivel como `0x84 - 0x66` = **30** para Carnby (el bosque donde cae en paracaídas) y `0x84` = 132 para Aline, después de su vídeo (`select_c`, `select_a`). Ahora se hace igual, con los vídeos del arranque y la intro en su orden.

## Saltos en las animaciones de la escena de radio (2026-11-24)

**Qué pasaba:** en la conversación por radio Carnby volvía de golpe a la postura de reposo y saltaba a la animación siguiente.
**Por qué:** se daba por acabada una animación pedida por la escena en cuanto daba la vuelta; las de hablar (400, 402, 404) se repiten en bucle. Antes, además, se daba por acabada cuando la que sonaba aún era la anterior, esperando la ventana de su transición.
**Arreglo:** como `0x438759`, solo acaba la pedida, cuando suena ella misma y llega a su final (las que se repiten siguen hasta que se pide otra). Comprobado con un registro de cada fotograma: ahora todas las del diálogo pasan mezclándose.

## La linterna en la mano durante la radio, y su resplandor con otros colores (2026-11-24)

**Qué pasaba:** hablando por radio Carnby seguía con la linterna en la mano; el resplandor era demasiado fuerte y su raya salía roja y de golpe.
**Por qué:** faltaba el evento 0x66 de las animaciones (`0x434409`): la de sacar la radio (401) lo lleva con 2 al empezar y la de guardarla (403) con 1 al final; con 2 el juego guarda la mano y pone la 2 (la radio), si no vuelve a la guardada. En el resplandor se leyeron los colores al revés y sin las fuerzas del juego.
**Arreglo:** el evento 0x66 cambia la mano como el juego. El resplandor sigue el ensamblador de `0x414FA9`: `FLARE` a 0,5 × lo que apunta a la cámara, `GLOW` a eso al cuadrado y la raya (azul en `hand1`) con (fuerza − 0,898) × 9,846; los bytes de los colores son rojo, verde y azul.

## La luz de la linterna cortada en recuadros (2026-11-24)

**Qué pasaba:** en el bosque (nivel 30, cámara 15) la luz caía sobre una pieza del primer plano (un tocón) y se cortaba en seco con un borde recto, dejando un recuadro oscuro al lado; en el suelo apenas se notaba.
**Por qué:** la luz se calculaba en cada píxel con la profundidad de cada pieza, y las piezas del primer plano son planos con su máscara: la luz terminaba justo en sus bordes. El juego ilumina los vértices de la malla y reparte la luz entre ellos (`0x41A2C0`), con su imagen iluminada preparada por rectángulos (`0x4061B0`): otra manera de dibujar que no se ha copiado.
**Arreglo (propio):** la luz se dibuja primero en una imagen aparte (cuánto alumbra la linterna cada punto de la malla), se difumina unos píxeles (dos pasadas, horizontal y vertical) y con ella se iluminan el fondo y el primer plano; además, donde la imagen iluminada del juego casi no cambia, el haz aclara un poco la oscura con un tono cálido, para que se vea siempre. Direct3D 11, 12 y Vulkan.

## El título y la elección de personaje no eran como los del juego (2026-11-24)

**Qué pasaba:** el título enseñaba las cuatro opciones a la vez; al elegir personaje, Carnby estaba a la izquierda y su ficha se escribía sobre él mismo.
**Por qué:** la disposición era propia; se había leído mal `FUN_0047f633` (la ficha de Carnby va a la izquierda, pero su imagen, puesta por los datos de `select2`, a la derecha) y el oscurecido estaba al revés.
**Arreglo:** comparado con un vídeo del original: el título es una rueda de tres filas (`FUN_0047ae93`) y la elección de personaje sigue la página de `select2` (ver `docs/FORMATOS.md`, páginas de los menús).

## El disparo sonaba como un clic (2026-11-24)

**Qué pasaba:** al disparar se oía un clic débil en vez del estallido.
**Por qué:** se tomó `FUN_00469cd4` (hueco `+0xF` del arma) por el sonido del disparo; es el del gatillo en vacío. El disparo de verdad lo toca la función de disparo de cada clase de arma (`FUN_0046a0e2`) con el hueco `+0xC` (en la pistola, `0160_a`: un estallido de 0,7 s; `0165_a` es un clic de 0,1 s).
**Arreglo:** el disparo toca `+0xC`. Comprobado decodificando las muestras (IMA ADPCM) y midiendo su fuerza en el tiempo.
