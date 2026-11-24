# Formatos de los archivos del juego

Lo comprobado, con la función de `alone4.exe` que lo lee (direcciones de Ghidra). Cada formato nuevo se añade aquí con su lector en `tools/aitd4data.py` (Python) y, cuando el motor lo use, en `src/core/`.

## Comprimidos (gzip)

Casi todos los archivos son gzip, rellenos con ceros hasta un múltiplo de 2048 bytes (sector de CD). gzip normal los rechaza por el relleno: se lee un solo flujo y se ignora lo que sobra (`zlib.decompressobj(31)` en Python, `readDataFile` en `src/util/dataFile.cpp`).

## Contenedor de trozos (`FUN_004041fd`)

Un archivo de nivel es una lista de trozos: cada uno empieza con 4 letras y el juego busca en una tabla quién lo lee. Lectores registrados:

| Letras | Quién lo lee | Dónde se registra |
|---|---|---|
| `DRAM` | `FUN_00403ff3` | tabla fija en `0x4C40C8` |
| `G3DB` | `FUN_004166aa` | `FUN_004165ff` |
| `LMVR` | `FUN_004166bf` | `FUN_004165ff` |

## Cámaras: `levels\<nivel>\view_NN`, `view_NND`, `view_NNS` (`FUN_0044c0ce`)

Para cada cámara el juego abre tres archivos:

- `view_NN`: trozo `G3DB` con la escena de la cámara (ver abajo).
- `view_NND`: fondo **a oscuras**. `LMVR`, 1024×1024.
- `view_NNS`: fondo **iluminado** (lo que muestra la linterna). `LMVR`, 1024×512.

Los prepara para la linterna con `FUN_004061b0` (una textura con lo oscuro y otra con lo iluminado, colocado según la malla de la escena; ver `docs/ALONE4_EXE.md`) y los sube como texturas con `FUN_00419253`.

### LMVR (imagen)

| Posición | Qué |
|---|---|
| 0x00 | `LMVR` (4 archivos del nivel 20 dicen `VRAM`: mismo formato, S de 1024×1024) |
| 0x0C | ancho (u16), siempre 1024 |
| 0x0E | alto (u16): 1024 (D) o 512 (S) |
| 0x3C | ancho × alto bytes: un índice de color por píxel, **en orden Z (Morton)**, no fila a fila |
| después | 2056 bytes: paleta de 256 colores en orden **azul, verde, rojo, A** (BGRA; el byte A no se usa), otra paleta de 256 (sin estudiar) y 8 bytes |

- Orden de los píxeles: `FUN_004060c0` (copiado tal cual en `tools/aitd4data.py` y `src/core/viewPicture.cpp`).
- La imagen de la cámara son los **640×480** de arriba a la izquierda. Lo demás de la hoja: en D, piezas sueltas de la imagen (abajo); en S, vacío.
- **Las paletas solo tienen 128 colores**: los 128 primeros se repiten en los 128 siguientes. El **bit 0x80** de cada píxel no cambia el color: en D es una **máscara**. Con la marca, el píxel es fondo (el personaje pasa por delante); sin ella, es un objeto del primer plano (plantas, muebles...) que tapa al personaje.
- Las piezas sueltas de la parte de abajo de la hoja D son **los objetos del primer plano recortados**; la escena `G3DB` dice dónde va cada uno.
- Comprobado en los 2412 archivos de los 107 niveles (2026-11-22).

### G3DB (banco de recursos 3D; `FUN_004213ad`)

| Posición | Qué |
|---|---|
| 0x00 | `G3DB` |
| 0x04 | número de hueco en el banco de recursos (u32; 1 en las cámaras) |
| 0x08 | tamaño del bloque (u32) |
| 0x0C | número de punteros (u32) |
| 0x10 | el bloque |
| después | la lista de punteros: para cada uno, la posición en el bloque de un u32 que es un desplazamiento desde el principio del bloque (`FUN_0040410a` lo cambia por la dirección) |

El bloque empieza con una cabecera de recursos (texturas en 0x00/0x04, 0x14/0x18 y 0x24/0x28; vacías en las cámaras) y en **0x10 la escena de la cámara**:

| Posición en la escena | Qué |
|---|---|
| 0x00 | versión de "LightMesh" (u32: `0xE6BE50`; el juego rechaza otras) |
| 0x04 | posición de la cámara en el mundo (3 float) |
| 0x10 | ángulos de la cámara (3 float, radianes) |
| 0x1C | **ángulo de visión horizontal** (float, radianes; 0,80 ≈ 46°) |
| 0x20, 0x34 | las dos luces de la escena ("LightMesh", `FUN_0041bd50`): posición en el mundo (3 float), intensidad (u32: 50 a 180) y 20,0 (float). Muy altas (la luna) y compartidas por las cámaras de una sala; la primera a oscuras y la segunda con el nivel iluminado. De ellas sale la sombra de los personajes (`FUN_00423dc0`) |
| 0x48 | número de polígonos, de índices y de puntos (3 u32) |
| 0x94 | dónde están: polígonos, índices y puntos (3 punteros) |
| 0xA0 | puntero a una segunda malla (solo en 15 cámaras; sin estudiar) |

- **Polígono** (12 bytes): grupo, primer índice, número de índices (u32). No es un polígono sino una **tira de triángulos** (de 3 a 29 puntos): cada triángulo es un punto con los dos anteriores. Comprobado: como tira, casi todas tienen todos sus triángulos del mismo lado; como abanico, casi ninguna. Grupo 0: la habitación (paredes, suelo), casi toda con la marca de fondo. Grupos 1 en adelante: objetos del primer plano, casi todos sin la marca. Los del 2 en adelante tienen su textura en la parte de abajo de la hoja D.
- **Índice**: u32, número de punto.
- **Punto** (0x24 bytes): x, y, z ya **en el espacio de la cámara** (x a la derecha, **y es la profundidad**, z hacia arriba); 3 float a 0 (¿normal, calculada al cargar?); u, v (0..1, sobre la hoja de 1024); un float sin usar.
- **Proyección** (`FUN_004061b0`): `focal = 320 / tan(ángulo/2)`; en pantalla `x' = focal·x/y + 320`, `y' = 240 − focal·z/y`. En los polígonos de la habitación, (x', y') coincide con (u, v)·1024: la malla está dibujada exactamente sobre el fondo.
- **La malla no es la habitación entera**: solo lo que la linterna alumbra y lo que tapa. Tiene huecos, y al fondo de un pasillo el suelo puede ser una pared vertical.
- **Orientación de la cámara**: del espacio de la cámara al mundo, `mundo = Rx(a0) · Rz(a2) · Ry(a1) · punto + posición` (a0, a1, a2: los tres ángulos de 0x10). Con eso, el suelo de la malla queda horizontal y "arriba" es z. Ninguna cámara está inclinada de lado; en el nivel 1 están inclinadas hacia abajo entre 8° y 50°. (Hay otra combinación que también deja el suelo horizontal: la misma con a1 y a2 de signo contrario, que da el mundo como en un espejo. **La buena es la primera**: con ella las paredes de las mallas caen sobre las del mapa de choques del nivel, y con la de espejo se salen del nivel; 2026-11-23.)
- **El suelo** casi siempre está a la altura 0 del mundo: la altura de la cámara (z de la posición) coincide con la distancia al suelo de la malla (281 y 281, 626 y 626 en el nivel 1). En algunas salas no (nivel 2, cámara 1: 184 y 189), así que el visor la mide en la malla: la altura donde hay más área mirando hacia arriba.
- **Las medidas parecen centímetros**: cámaras entre 1 y 8 m del suelo y un pasillo del invernadero de unos 90 de ancho.
- Comprobado con `tools/drawViewMeshes.py`: se leen bien las 1206 escenas de cámara (2026-11-22).

## Datos del nivel para cada personaje: `levels\<nivel>\CarnbyNN`, `AlineNN` (`FUN_0044bebd`)

No son solo el personaje: son **los datos del nivel para la aventura de cada uno** (Carnby y Aline recorren los mismos sitios con historias distintas). `Carnby01` está en 95 niveles y `Aline01` en 93; hay variantes `02` a `04` en unos pocos. Tres trozos seguidos:

| Trozo | Qué |
|---|---|
| `DRAM` | la lógica del nivel (en parte estudiada, ver abajo): tamaño (u32), número de punteros (u32), el bloque y la lista de punteros, como en `G3DB` pero sin número de hueco. El bloque empieza con `0xDEADBEEF` y la versión `0x0131554C`, que el juego comprueba |
| `G3DB` (hueco 0) | **los modelos 3D del nivel y sus texturas**: personajes, monstruos y objetos |
| `DSND` | los sonidos (sin estudiar) |

No encajan: `Carnby04` de los niveles 52 y 57 (no empiezan con `DRAM`) y `Aline01` del nivel 63 (mucho más pequeño). Sin estudiar. Además, 33 archivos tienen una **versión anterior** del `DRAM` (`0x01313153`) que el juego rechaza al cargar ("invalid file format"): restos sin usar.

### La cabecera del `DRAM` (`FUN_0044a39d`)

Tiene sitios fijos; el juego los lee al preparar el nivel (los punteros, como desplazamientos dentro del bloque):

| Desplazamiento | Qué es |
|---|---|
| 0x000 | `0xDEADBEEF` y la versión `0x0131554C` |
| 0x014 | **el script del nivel**: se ejecuta al entrar, antes del de la posición de entrada (`FUN_0044b91b`). En el nivel 1 prepara muchas cosas (funciones 26, 33, 36, 179, 88...) |
| 0x018 | otro script (`FUN_0044bb59`, ¿al salir del nivel?) |
| 0x01C | el script del **botón de acción fuera de toda zona con acción** (`FUN_00449cb7`) |
| 0x374 | **un puntero** a los textos del nivel (`FUN_004668f0`): allí u32 0, número de textos (u32) y lista de punteros a textos con cero al final. Están **en inglés** aunque el juego sea el español (la traducción estará en otro sitio). Nivel 1: "This door seems sealed up.", "Yes@No", "Do you want to climb down the ladder?"... La función 108 de los scripts los usa por número |
| 0x38C, 0x38E | dos números que recibe el personaje al empezar (`initHero`) |
| 0x3A2 | número de entradas de la lista de `0x3D0` (0x20 bytes cada una) |
| 0x390, 0x392 | número de disparadores de paso (lista de `0x3AC`) y de zonas (lista de `0x3B0`); son los mismos que los del mapa de disparadores |
| 0x3AC | **disparadores de paso** (0x10 bytes cada uno): en +4 la cámara en la que tiene que estar el jugador (i16, −1 cualquiera), en +8 el script al cruzar entrando y en +0xC al cruzar saliendo (`FUN_00466ee8`). En el nivel 1 son los 24 cambios de cámara |
| 0x3B0 | lo que hace cada **zona** del mapa de disparadores (0x20 bytes cada una; `FUN_00466f84` al entrar, `FUN_00467031` al salir): en +0 marcas (0x02: para la acción hay que **mirar hacia un lado**: el ángulo en el suelo del eje "adelante" del personaje, atan2(y, x) en grados, menos el de +0x14, en valor absoluto, no puede pasar del margen de +0x16; el juego no da la vuelta a la diferencia; si no se cumple, la acción no hace nada), en +4 el script al entrar, en +8 al salir, en +0xC el script de **acción** (se ejecuta al pulsar el botón de acción dentro de la zona, `FUN_00449cb7`), en +0x10 otra cosa (`FUN_00449df7`, sin estudiar), en +0x14 el ángulo hacia el que hay que mirar (grados) y en +0x16 el margen. La zona 10 del nivel 1 es la **trampilla abierta** junto a la entrada: con acción pregunta "Do you want to climb down the ladder?" (función 108, textos 2 y 1) y, si se responde Sí, suena (45), espera (16) y va al **nivel 2** (función 4) |
| 0x394 | número de posiciones de entrada (i16) |
| 0x396, 0x398, 0x39A, 0x39C, 0x39E, 0x3A0 | números de cosas de las listas de `0x3B8`, `0x3BC`, `0x3C0`, `0x3C4`, `0x3C8` y `0x3CC` (i16) |
| 0x3A4, 0x3A6, 0x3A8, 0x3AA | números de entradas de `0x3D4` (ajustes de niebla o luz), `0x3D8` (puntos marcados), `0x3DC` (sonidos de cada tipo de suelo) y `0x3E0` (scripts con número) (i16) |
| 0x3B4 | **posiciones de entrada al nivel** (0x24 bytes cada una: posición (3 float), ángulos (3 float, radianes), en +0x18 **de qué nivel se viene** (i16; −1 al empezar la partida), en +0x1A la cámara (i16), en +0x1C un 100 si es obligatoria y en +0x20 un script que se ejecuta al entrar). Se elige (`FUN_0044b3e1`) la obligatoria, si no la del nivel de donde se viene, y al empezar la de −1. La única del nivel 1 viene del 2 (la trampilla); el juego usa la del número de entrada (`nivel + 0x10`, `FUN_0044b91b`). Nivel 1: (−941, 292, 0), mirando a 3,13, cámara 1 |
| 0x3B8, 0x3BC, 0x3C0, 0x3C4, 0x3C8 | listas de objetos del nivel (0xD0, 0xB4, 0x40, 0x1C y 0x10 bytes cada uno). La de `0x3BC`, **las cosas con modelo**, está estudiada (ver abajo); las demás no |

**El cerebro de los actores** (entrada `+0x24` de la tabla de su clase, cada fotograma; la de la clase base es `FUN_00451853`): según su **comportamiento** (`+0x268`, lo ponen los scripts con la función 66):

| Comportamiento | Qué hace | Animación (identificador) |
|---|---|---|
| 0, 6, 10 | nada | — |
| 1 | espera hasta ver al héroe (más cerca que `+0x258`, `FUN_00452277`), y al fotograma siguiente pasa a 4 | 1 |
| 2 | se gira hacia el héroe y ataca; al acabar el ataque, si el héroe está más lejos que `+0x25C`, pasa a 4 | 3 |
| 3, 9, 0x80–0x83 | vuelve a 1 | — |
| 4 | se gira hacia el héroe (`FUN_0042f4db`, suave: `+0x54` de 4096 por fotograma) y va hacia él; a menos de `+0x25C` (y con el héroe vivo), pasa a 2 | 2 |
| 5 | va a un sitio y lanza un script con número (`+0x278`) | 2 |
| 7, 8 | herido; al acabar, sin vida (`+0x252`) muere (orden 5), si no pasa a 4 | 4 |

Al morir (presencia 5) juega la animación 5 y luego queda en 2 (acabado) y oculto (`FUN_0045c01d`). Las animaciones se piden por **identificador** de la tabla del modelo (`setModelAnimation`, `FUN_0041806d`), y `changeAnimation` (`FUN_004287d0`) las cambia con sus transiciones, las avanza y mueve al actor como al personaje. Valores de cada clase en su constructor: base (`FUN_004509c0`) ve a 400 cm, ataca a 120 y gira 0x400; PHOTOSAURE (`FUN_0045ccd0`) ve a 5000; HYBRIDE (`FUN_0043f710`) ve a 2000, gira 0x100 y anda con la animación 2 o 27 (al azar). **Cada clase tiene su propio cerebro** (PHOTOSAURE `FUN_0045ce8f`, HYBRIDE `FUN_0043f8cf`, con más de 30 comportamientos: agarrar al héroe, rodearlo, varios ataques...); el motor usa el de la base para todas salvo el PHOTOSAURE.

**Cerebro del PHOTOSAURE** (`FUN_0045ce8f`, hecho el 2026-11-23 en `src/core/actorBrain.cpp`): 1 espera (animación 1) hasta ver al héroe, ruge (16) y pasa a 4; 2 va hacia el héroe andando (2; corriendo, 3, después de haber huido una vez: `+0x34C` = 1) y, **si la luz de la linterna le da** (`FUN_004524ea` → la marca 2 de `+0x15C` de su modelo, que pone el cálculo de luz), **huye** (0xD a 0x10: animaciones 17, 19, 18 o 20; `FUN_0045e006` elige al azar atrás o a un lado si a 120 cm hay sitio); si tiene al héroe delante: entre 220 y 250 cm **salta** (0x12, animación 4, golpe hasta 300 cm), entre 80 y 100 **muerde** (0x13, animación 6), más cerca **retrocede** (0x11: busca el punto de paso del nivel más cercano a 2 m detrás de él, `FUN_0044cccc`); tras atacar o huir se recupera (0x15 animación 11 o 0x16 animación 10) y vuelve a 2 o 0xC (como 2 pero corriendo, 3, o 10 a menos de 250 cm); herido usa la animación 8; si el héroe muere pasa a 3 (deambular por puntos de paso al azar, `+0x3D0` del nivel). 4 lo busca por los puntos de paso del nivel (`FUN_0045393b`) y 2 comprueba que lo ve con un rayo contra el mapa de choques (`FUN_00403249`): **sin hacer**, por eso en el nivel 1 se queda atascado contra una pared; el motor retrocede en línea recta y se rinde a los 2,5 s.

**Eventos de las animaciones** (8 bytes cada uno, lista en `+0x2C` de la animación y número en `+0x12`): momento (u16, ms), **código** (u16), **valor** (u16) y otro u16 (0xCDCD, relleno). Cada fotograma el juego junta los que el reloj de la animación ha pasado (`FUN_004270f0`, también al dar la vuelta) en una cola de 16 del modelo, y el dueño los atiende (`FUN_0041816b`; los actores en `FUN_00452b31`). Códigos vistos: **1 golpe** (si el héroe está a menos de *valor* cm y delante del actor, `FUN_0045244e`, recibe el daño del ataque de `NpcCarac.csv`), 2 agarre (`FUN_004351ad`), 200 y 201 sonidos (el valor elige cuál), 666 y 0x29A, 0x309 marcas (`+0x27D`) y sonidos, 999 temblor de cámara. Ejemplos: el ataque del HYBRIDE (identificador 10) golpea a los 660 ms hasta 80 cm; el del PHOTOSAURE (6), a los 957 ms hasta 100 cm.

**Los identificadores de animación de cada clase**: los de la clase base (1 quieto, 2 andar, 3 atacar, 4 herido, 5 morir) no valen para las clases con cerebro propio. HYBRIDE: 1 quieto, 2 o 27 andar, **10 ataque** (comportamiento 0x1E, a menos de 70 cm, `+0x344`), 4 herido, 5 caer. PHOTOSAURE: 1 quieto, 2 andar, **6 ataque** (comportamiento 0x13), 4 salto con golpe a 300 cm (0x12); no tiene el 5.

**El héroe herido y muerto** (`FUN_00435281`, `FUN_00435338`): su **vida** está en el estado de la partida (`+0xC`, 100 como mucho; algo la cura +50). Un golpe se la resta; con 0 pasa al estado **0x10** (cae con la animación 0x8C y luego queda tendido con 0x8D; el juego acaba la partida, `FUN_0044d444`); si no, al **0x11** (herido) con la animación de la tabla `0x4C5F00`: 16 tipos de monstruo por fila (identificador y un sonido), la fila según de dónde viene el golpe (detrás 0, de lado 1, delante 2; 3 a 5 con otra condición, `FUN_00434197`): 14 para casi todos, 130 para HYBRIDE, 17/23/19 para ARRACHEUR, 501 para PHOCOMELE; al acabar, quieto.

**`NpcCarac.csv`** (en la carpeta del juego; `FUN_00455080`): **cifrado** byte a byte (`FUN_00479cd2`: cada byte xor su posición módulo 256, y luego sus dos bits bajos pasan arriba y los dos altos abajo). Descifrado es una tabla en francés separada por `;`: por cada monstruo una línea con su nombre y su tipo, 8 líneas con un valor en la quinta celda (peso, puntos de vida, daño de su ataque principal y del secundario, daño por ciclo mientras agarra al héroe, ciclos que lo agarra, pulsaciones de acción para soltarse, daño que recibe mientras tanto) y 24 más (dos por arma: cuánto lo empuja un disparo y desde qué distancia usa la animación de golpe grande). El juego los guarda como 32 números por tipo en `0x507B60`. Tipos: 0 PHOTOSAURE (25 de vida), 1 HOWARD_TRANFORME_LASER, 2 ARRACHEUR, 3 ARACHNOCIDE, 4 OPHTALMICIDE, 5 PHOCOMELE, 6 TYNDALOS, 7 HOWARD_TRANFORME, 8 RATLUX, 9 ALAN_TRANSFORME, 10 HYBRIDE_2, 11 ROTWEILER, 12 OBED_TRANSFORME, 13 TRICERAPTOR, 14 ROTWEILER_ZOMBIE, 15 HYBRIDE (15 de vida). **`cweap.csv` y `aweap.csv`** (las armas de Carnby y de Aline) van cifrados igual. Dos líneas de títulos y una por arma: número (el N de `weapon\<personaje>\weaponN`), nombre, si tiene medidor, si **apunta solo** (autolock), tiros por cargador y por caja, balas por tiro, **daño por tiro**, milisegundos entre tiros de las que disparan seguido (-1: uno por pulsación), objeto del arma y de su munición, y tipo de impacto. Carnby: 0 revólver (daño 1, 6 por cargador, apunta solo), 1 triple cañón (5), 2 fotopulsador (40), 3 bengala (1), 4 lanzacohetes (20), 5 pistola paralizante (0), 6 pistola de plasma (4, cada 250 ms), 7 lanzagranadas (12), 8 arma de rayos (2, cada 100 ms). La de Aline es igual salvo el revólver (otro objeto). Así se entiende `NpcCarac.csv`: el OPHTALMICIDE tiene 10 de vida, "2 tiros de escopeta (10 de revólver)".

**Cosas con modelo** (`0x3BC`, 0xB4 bytes cada una; el juego hace con cada una un objeto de 0x9C bytes, `FUN_0046850c`):

| Offset | Contenido |
|---|---|
| 0x00 | posición en el mundo (3 float, centímetros) |
| 0x0C | ángulos (3 float, radianes: x, y, z); el juego gira sobre z, luego y, luego x (`FUN_00476ea9`). En los niveles vistos solo z cambia |
| 0x18 | modelo (i16) del banco del mismo archivo (`Carnby01`, `Aline01`) |
| 0x1C | animación (i16) que se reproduce si tiene la marca 0x08 (número de animación del modelo) |
| 0x1E | un número (i16; ¿identificador?, sin estudiar) |
| 0x20 | **marcas** (u16): 0x02 se ve al empezar, 0x04 tiene modelo, 0x08 se anima, 0x10 tiene sombra (u otra pieza de tipo 5), 0x20 su sombra se ve, 0x40 algo de la sombra, 0x80 tiene piezas más (4 de tipo 7 en `+0x25 + 0x20·k` y 4 de tipo 8 en `+0xA5 + 4·k`, -1 ninguna), 0x100 fuerza 0x10 |
| 0x22 | un valor (i16) que se pasa al modelo (`FUN_00417924`; sin estudiar) |

Los scripts la cogen con la función 37 y la cambian con la 38 (mostrar u ocultar), la 39 (¿se ve?), la 40 (poner una animación y animarla) y la 97 (mostrar u ocultar la sombra). En todos los niveles de Carnby hay 186; 151 tienen modelo y 7 se animan al empezar. Comprobado en el visor: en el avión del nivel 144 salen una caja de madera, una tela y una herramienta en su sitio.

**Actores** (`0x3B8`, 0xD0 bytes cada uno; monstruos y otros personajes; la fábrica `FUN_00454b20` elige la clase por el tipo y luego todas leen el registro con `FUN_00450c42`):

| Offset | Contenido |
|---|---|
| 0x00 | modelo (i16) del banco del mismo archivo (en el nivel 1, el 8: el monstruo) |
| 0x04 | estado al empezar (i16; "npc start state"): 0 ausente y oculto, 1 presente y visible, 2 presente con comportamiento 2 |
| 0x06 | marcas (u16; 0x04 sin estudiar) |
| 0x08 | tipo (i16): 16 clases (0–15; 6, 11 y 14 comparten clase, y 10 y 15) |
| 0x0C | un valor (i16, va a `+0x2AA`) |
| 0x12 | un valor (i16; menos de 2: `+0x2C6` = −1) |
| 0x18 | posición (3 float) |
| 0x24 | ángulos (3 float; mismo giro que las cosas con modelo) |
| 0x34, 0x50, 0x6C, 0x88 | 4 puntos (posición y ángulos, 0x1C bytes; ¿recorrido?, sin estudiar) |
| 0xA0 | parámetros de comportamiento (bytes, sin estudiar) |

El juego le pone un cilindro de choque (radio 20, alto 100). **Presencia** (`+0x2B2`): 0 ausente, 1 presente, 2 muerto o acabado (lo ponen las clases al terminar la animación 5 del modelo), 3, 4 y 5 de paso. Los scripts no cambian al actor al momento: le dejan **órdenes en una cola** (`+0x2B6`, número en `+0x2B4`) que él atiende en su turno (`FUN_0045126c`): 1 aparecer (si ausente; función 192 con 0), 2 aparecer y actuar (192 con otro valor; pasa a 3), 3 desaparecer (si presente; 193 con 0), 4 (193 con otro valor; pasa a 4 y luego a 0), 5 (195; pasa a 5, ¿morir?), 6 volver a empezar si está en 2 (194, con un comportamiento). Las funciones 196, 197 y 198 preguntan si la presencia es 1, 0 o 2, y la 66 pone el comportamiento (`+0x268`/`+0x26C`, `FUN_00452586`).


| 0x3CC | luces (0x18 bytes: posición (3 float), colores...; 5 en el nivel 1; sin estudiar) |
| 0x3D0 | un **mapa de caminos** (0x20 bytes por punto: x, y, ..., y en +0x1C la lista de sus vecinos); número en `0x3A2`. 17 puntos en el nivel 1. ¿Para los monstruos? |
| 0x3D4 | ajustes de niebla o luz (0x20 bytes cada uno; función 44 de los scripts, `FUN_0044a113`; sin estudiar) |
| 0x3D8 | **puntos marcados** del nivel (0x1C bytes: posición (3 float), ángulos (3 float) y 4 bytes más): la función 72 de los scripts pone al personaje en uno (`FUN_00434a0e`). 8 en el nivel 1 |
| 0x3E0 | **scripts con número** (8 bytes: número (i16), 2 bytes, script): los scripts (funciones 90 y 138–145, con 0 a 8 parámetros) y el juego (teclas: 32000 + tecla) los lanzan por número (`FUN_0044a1b7`) |
| 0x3E4 | **el mapa de choques** (ver abajo) |
| 0x3E8 | **el mapa de disparadores** (ver abajo) |

### El mapa de disparadores (`+0x3E8` del `DRAM`; `FUN_004a33fb`)

Paredes invisibles y zonas que avisan cuando el personaje pasa por ellas. El personaje (`initHero`) tiene un detector (`+0x2E4`) que, cada vez que se mueve, prueba el segmento de su paso **a 1 m sobre los pies** (`FUN_004a32c0(100)`):

| Desplazamiento | Qué es |
|---|---|
| 0x04 | número de paredes de disparo (i16; 24 en el nivel 1, como los disparadores de paso de `0x3AC`) |
| 0x06 | número de zonas (i16; 14 en el nivel 1, como la lista de `0x3B0`) |
| 0x10 | las paredes de disparo (0x68 bytes cada una): un plano (0x14 bytes, como los del mapa de choques), número de bordes (u32, hasta 4) y los bordes |
| 0x14 | las zonas (8 bytes cada una): número de planos (u32, 6 en el nivel 1: cajas) y dónde están; dentro = delante de todos |

- **Cruce** (`FUN_00405f03`, `FUN_004a36a7`): si el paso va de delante del plano a detrás (o al revés) y el punto de corte está dentro de los bordes, avisa con el número de la pared y el sentido: de delante a detrás es "entrar" (script de +8 del disparador), al revés "salir" (+0xC). El disparador k usa la pared k.
- **Zonas** (`FUN_004a372c`): avisan al entrar y al salir (métodos 2 y 3 del oyente).
- En el nivel 1 las paredes van **en parejas** algo separadas, una para cada sentido ("si estás en la cámara 1 y cruzas, pasa a la 11"; "si estás en la 11, vuelve a la 1"): así la cámara no cambia una y otra vez en la frontera.

### Los scripts (`FUN_00411fc5`)

El juego tiene un **lenguaje de scripts propio**, compilado (tras algunos scripts va el nombre del archivo de origen, p. ej. `L:\levels\312\oops\C01_9.dbg`). Una máquina de pila:

- **Script**: u32 2, tamaño de sus variables (u16), longitud del código (u16)...; el código son instrucciones de un byte con sus datos detrás.
- **Instrucciones** (`FUN_00411fc5`): 0x04 mete un número (i32); 0x12 llama a una **función del juego** por su número (u16); 0x14 termina; 0x15/0x16 saltan si sí/si no; 0x17 salta; 0x18 a 0x1D comparan; 0x1E/0x1F y, o; 0x20 no; 0x21 a 0x27 cuentas; 0x05 mete un texto; 0x06 a 0x0D leen y escriben variables; 0x0E a 0x11 manejan la pila y las llamadas; 0x3D a 0x42 variables de otra clase; 0x43 a 0x45 operaciones de bits.
- **Funciones del juego**: tabla de 203 en `0x4C7878` (`FUN_004667ac`). Lo que devuelven dice si el script sigue (0), termina (1), se para hasta el fotograma siguiente (2) o repite la llamada (3, esperar). La **3 pone la cámara N** (`FUN_00462d39`; la 199 igual con otra marca); se escribe `04 N(4 bytes) 12 03 00`. Otra máquina con 72 funciones (`0x4C7EE4`, `FUN_0047389e`), sin estudiar.
- **Argumentos y resultados**: antes de llamar, el script mete los argumentos y, si la función da un resultado, deja un hueco (`room 4`); la función lee el argumento k en la cima de la pila + 4·k y escribe el resultado en el hueco de después; luego el script quita los argumentos (`drop`) y saca el resultado. Por eso una función aún no hecha puede saltarse sin romper la pila.
- **Al entrar al nivel** (`FUN_0044b91b`): se pide la cámara de la posición de entrada, se ejecuta el script del nivel (`+0x14`) y luego el de la entrada (`+0x20`).
- **Al cruzar un disparador de cámara**, el script llama antes a la función 53 (con una variable del juego y 80, 80; sin estudiar: ¿sonido?) y luego a la 3.
- Listado de los scripts de un nivel, instrucción por instrucción: `python tools/listScripts.py --level N` (`disassemble_script` en `tools/aitd4data.py`). En el motor: `src/scripts/scriptMachine.cpp` (la máquina) y `src/scripts/levelScripts.cpp` (las máquinas de un nivel, las variables de la partida y las funciones del juego hechas).
- Funciones ya vistas: **154** muestra un texto del nivel como mensaje y el script sigue (`FUN_00465a90`; el segundo argumento sin estudiar; p. ej. "Crates, from the Boston University of Astronomy." en el nivel 2); **3** y 199 cámara; **4** ir a otro nivel (`FUN_0044dcef` → `FUN_0044dd12`, que lo carga con `setupLevelFromDram`; argumento: el número del nivel); **16** algo con un tiempo (1200; ¿espera?); **45** un sonido (5 argumentos, devuelve un número; `FUN_0049a4dd`); **53** algo de sonido (3 argumentos); **108** muestra un texto del nivel con respuestas ("Yes@No") y devuelve la elegida (0 = la primera; `FUN_0043ce5f`).
- **Las 203 funciones**, una a una, con qué hacen, sus argumentos, cuánto se usan y si están hechas: `docs/SCRIPTS.md`.
- **El script de entrada** del nivel 144 dice "si cierta variable del juego vale 1, cámara 9; si no, cámara 1": la cámara depende de lo hecho en la partida.

### El mapa de choques (`+0x3E4` del `DRAM`; `FUN_00401d4a`)

Las paredes y suelos con los que choca el personaje, **en coordenadas del mundo** (centímetros, z arriba), en una **rejilla de 16 × 16 casillas** sobre el suelo:

| Desplazamiento | Qué es |
|---|---|
| 0x00 | caja del nivel: mínimo (3 float) y máximo (3 float) |
| 0x18 | tamaño de cada casilla en x y en y (2 float: el ancho de la caja / 16) |
| 0x20 | las 256 casillas (u16 cada una: dónde empieza su lista; 0x8000 = vacía), fila a fila (y × 16 + x) |
| 0x24 | las listas: u16 por pieza, hasta 0xFFFF. Con 0x8000: un plano suelto (número & 0x7FFF); sin ella: un polígono |
| 0x28, 0x2C | número de planos sueltos y dónde están (0x18 bytes cada uno) |
| 0x30, 0x34 | número de polígonos y dónde están (0x6C bytes cada uno) |
| 0x38, 0x3C | número de superficies (grupos) y dónde están (0x14 bytes cada una: tipo (i16; con el bit 0x01 el grupo **empieza activo**, si no, sus piezas no cuentan hasta que un script lo active: función 36, `FUN_00401135`; `setCollisionMap` hace la tabla de activos), tipo de suelo para el **sonido de los pasos** (i16) y su nombre en el editor 3D (16 letras): `Plaque_Fer` chapa de hierro, `Plaque_BrisVerr` cristales rotos, `Sol_Passerelle` pasarela, `Plaque_SonEchel` escalera, `Box01`...) |

- **Pieza** (las dos clases empiezan igual): grupo (i16; el juego mira en una tabla si ese grupo está activo: ¿puertas?), marcas (u8: 0x02 se puede pisar), altura extra (u8, se suma al suelo), y un **plano** de 0x14 bytes: tipo (i32: −1 cualquiera, 0 x fija, 1 y fija, 2 z fija = suelo horizontal), normal (3 float) y d. La distancia de un punto al plano es normal · punto − d.
- **Polígono** (0x6C bytes): la pieza, número de bordes (u32, hasta 4) y los bordes, cada uno otro plano de 0x14 bytes: un punto está dentro si su distancia a todos los bordes es ≥ 0.
- **Choque** (`FUN_00405e13`): el centro de una esfera va de A a B; choca con una pieza si A está delante del plano a más del radio y B ya no, y el punto de corte está dentro de sus bordes. Gana el choque más cercano.
- **Suelo** (`FUN_00401b4f`, `FUN_00405bec`): un rayo hacia abajo en la casilla del punto, contra las piezas pisables; se queda la más alta (más su altura extra).
- Nivel 1 (`Carnby01`): caja de (−1373, −1026, −61) a (821, 1043, 1200), 1 plano suelto (el suelo, z = −0,25), 251 polígonos y 45 superficies.
- **Comprobado** pasando las mallas de las cámaras al mundo: sus paredes caen sobre las del mapa (`mundo = Rx(a0) · Rz(a2) · Ry(a1) · punto + posición`; con la otra combinación, la de espejo, se salen del nivel). Dibujado visto desde arriba: el invernadero con sus parterres.
- **Resbalar** (`FUN_004029ee`, marca 0x04): desde donde choca, 1,5 cm delante de la pieza, el resto del movimiento sin la parte que entra en ella; si así vuelve a chocar, se queda donde estaba.
- Lector: `read_level_logic`, `read_collision_map` y `polygon_corners` en `tools/aitd4data.py`; plano visto desde arriba con las cámaras y la entrada: `python tools/drawCollisionMap.py --level N [--file Aline01]` (en `aitd4-data\exports\collision`). En el motor: `src/core/levelLogic.cpp` (cabecera del `DRAM`) y `src/core/collisionMap.cpp` (`moveSphere`, `floorBelow`).

### El banco de modelos (`G3DB` del nivel)

Cabecera del bloque: en 0x00 el número de **modelos con esqueleto** y en 0x04 dónde están (0x70 bytes cada uno); 0x14/0x18, texturas de partículas (0x60 bytes cada una); 0x24/0x28, texturas sueltas con nombre (0x40 bytes: `COURIER7`, `BLINK`...). En el nivel 1, `Carnby01` trae 9 modelos y `Aline01` 4.

**Modelo con esqueleto** (0x70 bytes; `FUN_004254d0`):

| Posición | Qué |
|---|---|
| 0x00 | número de huesos, de tiras, de índices, de vértices, de materiales y de texturas (6 u32) |
| 0x18 | huesos (0x24 bytes cada uno) |
| 0x1C, 0x20 | dos tablas más de los huesos (0x18 bytes por hueso; la otra sin estudiar) |
| 0x24 | **tiras de triángulos** (12 bytes: material, primer índice, número de índices) |
| 0x28 | índices (u32) |
| 0x2C | **vértices** (0x24 bytes) |
| 0x30 | **materiales** (8 bytes: textura o -1, marcas) |
| 0x34 | **texturas** (0x30 bytes) |
| 0x38 | **la sombra**: una copia más sencilla del modelo con su propio esqueleto, para dibujarla como volumen de sombra (`FUN_0048a1a0`; sin estudiar) |
| 0x6C | **las animaciones** del modelo (0 si no tiene; ver abajo) |

- **Hueso**: desplazamiento desde el padre (3 float), posición en reposo (3 float), padre (i32, -1 ninguno), primer vértice y vértice final (u32). Cada hueso mueve sus vértices enteros, sin mezclas: piezas rígidas.
- **Vértice**: posición (3 float), normal (3 float), u, v (float) y un color (`0xFF212121` en todos los de Carnby). En **centímetros**, z hacia arriba, y el modelo mira hacia +y. Carnby y Aline miden 179.
- **Tiras**: cada índice forma un triángulo con los dos anteriores, uno sí y otro no dado la vuelta (`FUN_00425320`). El juego junta las tiras de cada material en una lista de triángulos y la dibuja con `glDrawElements` (`FUN_004896e0`). Comprobado: así casi todos los lados los comparten dos triángulos y casi todos miran hacia fuera.
- **Material**: marca 0x01, se dibujan las dos caras; 0x02 y 0x04, dos clases de transparencia.
- **Textura**: ancho y alto (u16), marcas (u32) y en 0x28 el puntero a los píxeles. Siempre cuadradas (256, 128 o 64). 16 bits por píxel **en orden Z, como en Dreamcast** (bits de x e y intercalados, y primero). Marca 0x01: color 5-6-5; si no, 1-5-5-5 (el bit alto dice si el píxel es opaco). **Marca 0x04: comprimida como el VQ de Dreamcast** (el juego la sube con el formato 0x300 y `FUN_0048e037` la descomprime): 2048 bytes de libro (256 bloques de 2×2 píxeles de 16 bits, en orden Z) y un byte por bloque de la imagen (en orden Z) con su número de bloque. Marcas vistas: `0x1003`, `0x1012`, `0x207` y `0x216` (VQ).
- **Carnby** es el modelo 1 de `Carnby01` (23 huesos, 1593 vértices, 2238 triángulos, 2 texturas de 256) y **Aline** el 0 de `Aline01` (17 huesos, 1560 vértices). El modelo 8 de `Carnby01` (21 huesos, 118 cm de alto) parece un monstruo.
### Animaciones (`FUN_00427250`)

En `+0x6C` del modelo: número de pistas (u16), número de animaciones (u16), dónde están las pistas, dónde están las animaciones y una tabla de números de animación. Están en la zona del principio del banco (737 KB en `Carnby01` del nivel 1). Carnby tiene **251 animaciones** y 4422 pistas; el monstruo del modelo 8, 22 y 378.

- **Pista** (0x14 bytes): número de llaves de posición, de giro y de escala (3 u16), hueso (u16) y dónde están las tres listas (3 punteros). Lo que no tiene llaves queda como en la pose de reposo.
- **Llave** (0x10 bytes): tres valores (posición, ángulos en radianes o escala) y el momento, en **milisegundos**. Entre dos llaves, en línea recta; los ángulos, por el camino más corto (`FUN_0040602e`).
- **Animación** (0x30 bytes): en 0x00 la **velocidad** (float, centímetros por fotograma de 30 por segundo; andar 4,44, correr 11,94, andar hacia atrás −4,44), en 0x04 la duración (u16, milisegundos; siempre múltiplos de 33: **30 fotogramas por segundo**), en 0x06 las marcas (0x01 se repite, 0x02 hacia atrás, 0x04 avanza a velocidad fija), en 0x08 el número de pistas y en 0x18 la lista (u16); en 0x0A/0x1C, 0x0C/0x20 y 0x0E/0x24 tres clases de grupos de pistas, en 0x10/0x28 las **transiciones** a otras animaciones (ver abajo), en 0x14 el **ritmo** (u16, 0x1000 = normal: el tiempo avanza milisegundos × ritmo / 4096) y en 0x12/0x2C los eventos (8 bytes cada uno: momento en milisegundos (u16), otro u16 y un valor (u32); `FUN_004270f0` junta los que caen entre un fotograma y el siguiente, sin estudiar qué hacen). Los grupos (`FUN_00427250`, en este orden tras las pistas):
  - **con reloj propio** (0x10 bytes: número de pistas, marcas en +4, duración en +6, lista en +8): unos huesos con su propio tiempo y repetición;
  - **mezclas** (0x10 bytes: número de fuentes, marcas, duración y en +8 una lista: pistas por fuente (i32) y por cada fuente su lista de pistas, la casilla de su peso y otro número): por hueso, la suma de lo que dice cada fuente por su peso. Las usan las animaciones de apuntar: 4 fuentes (¿arriba, abajo, a los lados?) con los pesos que el juego calcula al jugar; al crear el modelo los pone a 0,25 (`FUN_004254d0`): apuntar al frente;
  - **poses fijas** (0x14 bytes: número de pistas, lista en +8): leídas en el momento 0.
- **Los identificadores de animación** (filas de 10 u16, -1 ninguno; en `Carnby01` del nivel 1, 53 filas desde `0xB7FE8` hasta la lista de modelos): **identificador = fila × 10 + columna** → número de animación. Así se piden las animaciones:
  1. el cerebro del personaje (`FUN_00436236`) quiere una **acción** ("base": quieto, andar, correr, apuntar...) con una **variación** (columna 0 a 9, `FUN_0043a286`: +1 con la linterna encendida, +2, +4 o +6 según otras marcas del personaje, `+0xDC` a `+0xE0`, ¿herido?);
  2. la tabla de acciones del **arma que lleva en la mano derecha** (el trozo `WEAP`, ver abajo; `FUN_00469ff3`) da el identificador;
  3. esta tabla del banco da la animación.
  Comprobado: con la mano vacía (`righthd`), acción 0 → identificador 1 → animación 250 (quieto); 1 → 10 → 202 (andar); 2 → 50 → 194 (correr); 12 → 426 → 116 (gira 180°: media vuelta); con el revólver (`weapon0`), 4 → 350 → 154 (apuntar) y 5 → 351 → 153 (disparar).
- **Catálogo**: `python tools/exportAnimations.py [--file Aline01]` dibuja todas las animaciones de un personaje a mitad de recorrido y hace una lista con su duración, si se repiten, qué movimiento son (quieto, pasos, andar, correr; por lo que se separan los pies) y en qué filas y columnas de la tabla aparecen. Carnby: 251; Aline: 233.
- **Esqueleto en pose** (`FUN_00426ab0`): cada hueso gira sobre z, luego y, luego x, se escala, se coloca en su posición y se lleva con su padre (matrices para puntos en fila: hijo × padre). Cada vértice va con su hueso desde la posición de reposo del hueso. **Las pistas del hueso 0 (la raíz) no se usan en la pose** (`FUN_004279c0` se las salta): sirven para mover al personaje entero. Comprobado dibujando varias animaciones: posturas naturales, sin piezas sueltas.
- **Transiciones** (`FUN_00427030`, `FUN_004287d0`, `FUN_004282d0`): cada animación lleva una lista (número en 0x10, 18 en la de andar 201; lista en 0x28; 0x10 bytes cada una) de las animaciones a las que puede pasar: en +0 la **velocidad** del personaje mientras dura la mezcla (float; el juego le quita los decimales: 4,44 → 4), en +4 la duración de la mezcla (u16, ms: 66, 99, 165...), en +6 el número de **ventanas**, en +8 la animación de destino (puntero) y en +0xC la lista de ventanas (8 bytes cada una, 4 u16: desde y hasta qué momento de la animación actual, y desde y hasta qué momento de la nueva; la nueva empieza en el momento que corresponde en proporción, deducido). Ejemplo, de andar a correr: (0–528 → 0–198) y (594–1089 → 396–594): el pie que va delante sigue delante. Si no hay transición, cambia de golpe; si la hay y el momento actual cae en una ventana, **mezcla** la pose de ese momento con la de la nueva en su momento de empezar (quieta) durante la duración (línea recta en posiciones, ángulos por el camino corto y escalas, `FUN_004282d0`), moviendo al personaje a la velocidad de la transición; al acabar, la nueva sigue desde ese momento con el tiempo sobrante. Mientras mezcla, lo que pida el cerebro espera. Sin ventanas pero con duración, mezcla hacia el principio de la nueva; si la hay pero el momento no cae en ninguna ventana, **sigue con la actual** hasta llegar a una (por ejemplo, esperar a que el pie esté en su sitio).
- **Cómo mueve una animación al personaje** (`FUN_004285a0`, cada fotograma):
  - **con la marca 0x04** (andar, correr, andar hacia atrás): avanza recto (+y del modelo) tiempo transcurrido × velocidad × 0,030003 (`DAT_004bd6c8`, fotogramas por milisegundo). Las animaciones de andar y correr no mueven la cadera hacia delante: se quedan en el sitio;
  - **sin ella** (media vuelta...): lo que cambia desde el fotograma anterior la posición y el giro de la primera pista de la animación (la del hueso 0). La media vuelta (animación 116) gira −π (180° a la derecha) y retrocede 9,4 cm.
- En el motor: `poseSkeleton`, `skinVertices`, `advanceAnimation` (el reloj) y `rootMotion` (la raíz) en `src/core/skeletonModel.cpp`; el movimiento en `src/core/characterBrain.cpp`.

- Lector: `read_level_data` y `read_skeleton_models` en `tools/aitd4data.py`; exportar a OBJ: `tools/exportModels.py`; en el motor: `src/core/skeletonModel.cpp`. Leídos 981 modelos de 222 archivos (2026-11-23).

## Manos y armas: `lefthand\<personaje>\handN`, `weapon\<personaje>\...`

**Cuál usa el juego y cuándo** (comprobado en el ejecutable):

- **Mano derecha** (`GE_WeaponManager`, `FUN_0046d4e3`): sin arma carga `\weapon\<personaje>\righthd`; con un arma, `\weapon\<personaje>\weapon%d`. **`righthand` no aparece en el ejecutable**: es un archivo que sobró (una mano abierta en horizontal) y el juego no lo usa.
- **Mano izquierda** (`GE_LeftHandManager`, `FUN_004493c7`, que carga `\lefthand\<personaje>\hand%d`): la partida empieza con la **1** (la linterna; `FUN_0044eb2f`); **al cerrar el inventario** (`FUN_004433ec`) el juego elige la **3** si se tiene el objeto 0x55 y no el 0x4F, la **4** si se tiene el 0x7F y no el 0x4F, y si no la **1** (¿linternas distintas según lo encontrado?); un **suceso de animación** (código 0x66, `FUN_00434409`) pone la **2** mientras dura (¿coger o usar algo?); la función 159 de los scripts también puede elegirla (ningún script de nivel la usa). La elegida se guarda en el estado de la partida (`+0x50`).

Las manos de los personajes no están en su modelo: los huesos de las muñecas (6 y 9 en Carnby y Aline; los dos huesos sin vértices al final de los brazos) llevan una mano que cambia según lo que sostenga. Cada archivo tiene un trozo propio y un banco `G3DB` con un modelo con esqueleto (de 2 o 3 huesos: los dedos), medido desde la muñeca:

| Archivo | Trozo | Qué |
|---|---|---|
| `lefthand\carnby\hand0` | `HAND` | mano izquierda vacía |
| `hand1`, `hand3`, `hand4` | `HAND` | mano izquierda con la linterna; el trozo `HAND` cambia (¿colores de la luz?) |
| `hand2` | `HAND` | mano izquierda con un aparato |
| `weapon\carnby\righthand`, `righthd` | `WEAP` | mano derecha vacía: `righthd` es la que usa el juego; `righthand` sobra |
| `weapon\carnby\weapon0` … `weapon9` | `WEAP` | mano derecha con cada arma; algunos traen un segundo modelo (¿el fogonazo?) |
| `weapon\carnby\lists` | | 132 bytes, sin estudiar |

- **La tabla de acciones del arma**: el bloque de `WEAP` lleva 0x28 bytes de cabecera y luego **una fila de 10 identificadores de animación por acción** del cerebro del personaje (16 o 17 acciones; columna = variación). Así cada arma tiene sus animaciones de apuntar, disparar, andar apuntando... (la mano vacía `righthand` no tiene tabla; `righthd` sí).
- `HAND` (`FUN_00449395`) y `WEAP` (`FUN_0046d8bb`) son bloques con punteros como `DRAM`: tamaño, número de punteros, bloque y lista de punteros (`HAND`: 24 bytes y ningún puntero; `WEAP`: 0x38 a 0x188 bytes y un puntero). Su contenido está sin estudiar (en `HAND`, ¿colores de la luz de la linterna?). Tras ellos va el banco `G3DB` (hueco 4 las manos, hueco 3 las armas).
- La mano va pegada a la muñeca: sus vértices se mueven con la matriz de ese hueso tal cual (sin restar la posición de reposo, porque ya están medidos desde la muñeca). Comprobado en el visor con varias animaciones.
- Lector: `find_g3db_bank` en `tools/aitd4data.py`; exportar: `python tools/exportModels.py --parts`; en el motor: `loadBankModels`, `wristBone` y `attachVertices` en `src/core/skeletonModel.cpp`.

## Imágenes a pantalla completa: `doc\*.VRM`, `book\*.VRM`

Hechas con la herramienta de Dreamcast DcBitmap. 96 bytes de cabecera (`picture`, `USAGE: DcBitmap`; en 0x40 ancho y alto de la hoja, 1024×512; en 0x46 ancho y alto de la imagen, **640×480**) y la hoja: 16 bits por píxel, **fila a fila** (no en orden Z), color **5-5-5**.

- `doc\`: primeros planos de puzles y objetos, a menudo en versiones `A` y `B` (antes y después: una balanza vacía y con una bolsa). Nombre: `<nivel>_<número>_<versión>`.
- `book\`: páginas de libros y álbumes (`L001_004`: fotos antiguas pegadas en un álbum; `_CV`: ¿portada?).
- 434 imágenes leídas (2026-11-23). No son de este tipo 94 archivos de esas carpetas (más pequeños, o que empiezan con `DRAM`, como `book\STELE53` y `book\docplate`): sin estudiar.
- Los scripts las muestran por su nombre (funciones 8 y 63: `\doc\101_51_A`, sin `.VRM`; el nombre puede traer un salto de línea delante). Lector del motor: `src/core/documentPicture.cpp`.
- Lector: `read_vrm` en `tools/aitd4data.py`; exportar: `python tools/exportDocuments.py` (a `aitd4-data\exports\documents`).

## Menús: `menu\` (primer vistazo, 2026-11-23)

Siete archivos comprimidos (gzip) con el mismo contenedor de trozos que los niveles:

| Archivo | Tamaño sin comprimir | Trozos |
|---|---|---|
| `start` | 148 B | `DRAM` casi vacío (¿lo primero que carga el juego?) |
| `menu2` | 1,4 MB | `DRAM`, `G3DB` (modelos 3D del menú), `DR2D` (imágenes), `DSND` (sonidos) |
| `select2` | 2,5 MB | `DRAM`, `DR2D`, `DSND` (¿elegir personaje?) |
| `ingame`, `ingame2` | 265 y 270 KB | `DRAM`, `DR2D` (+ `DSND` el 2): ¿pausa, inventario? |
| `gamesnd` | 4,8 KB | `DR2D` vacío y `DSND` |
| `mire.VRM` | | imagen a pantalla completa (¿la mira?) |

**`DR2D` (las imágenes, `FUN_0048875a`; lector registrado en `FUN_00486910`):** u32 tamaño, y dentro: u32 número de imágenes y cada una **alineada a 32 bytes**: nombre (32 bytes), ancho y alto (u16), un tipo (i16, 0 en todos los archivos; el juego usa el formato 0x900 si es 0 y 0xB00 si no), ancho y alto que se usan de ella (u16, desde arriba a la izquierda) y, alineados a 32, los píxeles en 16 bits **1-5-5-5** (el bit alto: opaco). Ejemplos: `menu2` tiene el logo (`LOGO_FRONT`, 358 × 226 usados), un resplandor, las dos fuentes y los botones de un mando de PlayStation (`IDB_CIRCLE`, `IDB_R1`...); `select2`, Carnby y Aline (320 × 480) y otra versión de cada uno; `ingame2`, la fuente de la pausa.

**El `DRAM` de los menús** no tiene la cabecera de los niveles: números en `+0x00` a `+0x1C` y punteros en `+0x20` a `+0x5C` (`FUN_0046fef9` crea con ellos las páginas: listas de 0x1C, 0x20, 0x3C, 0x4C... bytes). Entendido:
- `+0x12` cuántas **fuentes** y `+0x48` dónde (0x28 bytes cada una): u32 dónde están sus letras, nombre de su imagen (32 bytes). Letras: "SPC\0", u16 cuántas, u16 0, y 8 bytes por letra: arriba, fin derecho de su tinta, abajo, y dónde empieza la siguiente (0: la siguiente empieza fila). **Las letras van en el orden de los códigos Latin-1** que dibujan: de `!` a `~`, `ˆ Š ‹ ˜ š › Ÿ « » ¿`, las mayúsculas y minúsculas con acento (sin `Æ`, `×`, `Þ`, `æ`, `ð`, `÷`, `þ`) y `ÿ`; luego `BMP_COURIER7` (170 letras) añade `¡ © ®`, una flecha, cuatro botones y `°`, y `BMP_OLDBOOK` (166) tres parecidas a `¡`, `© ®` y `ÿ`. **Tienen `ñ`, acentos, `¿` y `¡`**: sirven para el español (la fuente `font8.pvr` no los tiene).
- `+0x50` los **textos**: 0, cuántos, dónde está la lista de punteros. En inglés ("Press START Button", "New Game", "Load Saved Game", "Options", "Credits", los créditos...). La versión española los cambia por los de `Provider\locale` (la tabla del ejecutable tiene la entrada `\menu\menu2`).

**Quién los carga:** `FUN_0047acab` (`\MENU\MENU2`), `FUN_0047f633` (`\MENU\SELECT2`: dos botones con las órdenes 0x2001 Carnby y 0x2002 Aline; `FUN_0047f8b9` los mueve con izquierda y derecha y, al confirmar, marca el bit 4 de las opciones de la partida si es Carnby), `FUN_0047eeea` (`\MENU\MIRE`), `FUN_0044fd55` (`\MENU\INGAME2`). Las pantallas de `int\` (`MGnnn_NN`, cada una con un `.def` de nombres en francés: `BACKGROUND_GAUCHE`, `FLECHE`, `LETTRAGE_0`..., `SND_SELECT`, `FNT_COURIER`) son interfaces de puzles con el mismo esquema.

**Dónde empieza cada uno** (buscado en los datos, no en el ejecutable): Carnby en el nivel 1 (el patio, primera entrada); Aline en el **nivel 132**, la entrada "desde ningún nivel" (cámara 5: el tejado con las cuerdas del paracaídas).

## Objetos del inventario: `item\IT3D_N`

Un archivo por objeto (N = su número en el inventario; 158 archivos, más `splat.bin`), con el mismo contenedor que los menús: un `DRAM` con **dos textos** (en `+0x50`, como los menús): la descripción ("A small steel key. It is engraved "1ST F WEST".") y lo que dice el juego al cogerlo ("You take a Small Steel Key."; si se pueden coger varios, dos partes separadas por `|`: "You take a Gas Cartridge.|You take %d Gas Cartridges."); un `G3DB` con su **modelo 3D** (el del inventario del juego) y un `DR2D` vacío. El nombre del objeto no está aparte: el motor lo saca de "You take a ...". La versión española toma los textos de `Provider\locale`. Lector: `src/core/itemFile.cpp`.

**El inventario** (en el estado de la partida, el objeto de la función 22; `FUN_00448833` y siguientes): i16 cuántos, i16 el último cogido, y hasta 0x53 parejas (i16 objeto, i16 cantidad); en `+0x150` otra lista de números anotados una vez (función 160). Al coger (`FUN_00448833`): el objeto 0x7E cuenta 5 por cada uno; el 0x7C no se guarda: suma 33 al 0x5A hasta 100 (¿pilas de la linterna?); coger el 0x5A lo pone a 100; los 0x1A y 0x97 vienen con 4 la primera vez; la munición de un arma suma *cajas × tiros por cargador × cargadores por caja* (tabla de 9 armas de 0x10 bytes en `DAT_005078f0`, sacada de `cweap.csv`/`aweap.csv`: `+2`, `+6`, `+0xC` objeto del arma, `+0xD` objeto de su munición); coger un arma la da al personaje (`FUN_004490d1`). Lo hace `src/core/inventory.cpp`.

## Controles: `keys.bin`

Las teclas que el jugador elige en el menú "CONFIGURAR CONTROLES" del juego (96 bytes en la carpeta del juego): **12 entradas de 8 bytes**, el código DirectInput de la tecla (u32, el código de teclado del PC; las teclas con prefijo 0xE0 como 0x80 + código) y 4 bytes a 0, en el orden del menú:

| Acción | Tecla del juego | Código |
|---|---|---|
| Arriba (andar) | ↑ | 0xC8 |
| Abajo (atrás; dos veces: media vuelta) | ↓ | 0xD0 |
| Izquierda | ← | 0xCB |
| Derecha | → | 0xCD |
| Usar / disparar | Espacio | 0x39 |
| Linterna | S | 0x1F |
| Correr | A | 0x1E |
| Inventario | I | 0x17 |
| Mapa | M | 0x32 |
| Radio | R | 0x13 |
| Vista libre | Mayús izquierda | 0x2A |
| Apuntar | Ctrl izquierdo | 0x1D |

Los nombres salen del menú del ejecutable (`.rdata`, tras "CONFIGURAR CONTROLES" en el español). En el motor: `src/core/gameControls.cpp` (si falta el archivo, las teclas de la tabla); la ventana da las teclas en códigos DirectInput (`keyHeld`).

## Textos e idiomas

Se estudió primero con la **versión española de GOG**; desde el 2026-11-23 el idioma base del proyecto es el **inglés** (la copia de trabajo es la inglesa; los archivos españoles que cambian se guardan aparte). Lo que sigue es lo que habrá que vigilar para que el motor sirva con las demás versiones del juego original (español, francés, alemán, italiano...).

- **Qué cambia entre la versión inglesa y la española de GOG** (comparadas archivo a archivo): `alone4.exe` (menús y mensajes, y el código: tamaños distintos, direcciones distintas), las **voces** (1445 archivos de `Dialog\`: los nombres como `AL_ING_01.wav` son los mismos en los dos idiomas), 8 vídeos de `movies\`, un sonido de `Sound2\` y el Readme. **No cambian** los niveles, `Provider\locale` ni `font8.pvr`.
- **La inglesa no usa `Provider\locale`** (su ejecutable no menciona ese archivo): muestra directamente los textos en inglés que trae cada nivel en su `DRAM`. La española los cambia por los de `locale`.

- **Textos de los niveles**: en `+0x374` del `DRAM` hay textos **en inglés** ("Do you want to climb down the ladder?"): los que muestra la versión inglesa.
- **En la española** salen de **`Provider\locale`** (cargado al arrancar, `FUN_004a9ef0` → `FUN_004aa0f8`): cabecera de 3 u32 (1, 2000, número de textos = 1738 en la española), una tabla de posiciones (u32 por texto, en letras desde el final de la tabla) y los textos en UTF-16 terminados en cero; el juego se queda solo con el byte bajo de cada letra (Latin-1) y salta los `\r` (`FUN_004aa2d8`). Ej.: 303 "¿Quieres subirte a la escalera?", 304 "¿Quieres bajarte de la escalera?".
- **Cómo se unen**: el texto k de un archivo de nivel se convierte en un número de `locale` con una **tabla dentro de `alone4.exe`** (`0x4F8888`, 447 entradas de 8 bytes: puntero a la ruta del archivo, como `\LEVELS\1\Carnby01` o `\book\docplate`, y puntero a una lista de números terminada en −1; `FUN_004a9faf`, `FUN_004a9ff7`). La ruta se compara con la del archivo cargado (`DAT_004fc9e0`).
- **Menús y mensajes del propio programa** ("Pulsa INTRO para continuar", "CONFIGURAR CONTROLES"...): en el `.rdata` de `alone4.exe`, en Latin-1.

- **La fuente `font8.pvr`**: PVRT de 128 × 128 (cabecera: "PVRT", tamaño, formato de píxel 1 = 5-6-5, orden 1 = Z como las texturas de los modelos, ancho, alto), letras de 8 × 8 **cada una en el lugar de su código ASCII** (16 por fila: las dos primeras filas, vacías; del espacio 0x20 a 0x7F). Letras de color sobre negro. **Solo las inglesas.** El motor la usa para sus textos (`src/core/gameFont.cpp`); la caja de las preguntas y mensajes del juego (`FUN_0043ce5f`, `FUN_0043d661`) está sin estudiar, así que el visor dibuja la suya.

**Dificultades para otros idiomas** (a tener en cuenta al hacerlo):

1. La tabla que une textos de nivel y `locale` (versión española) está **en el ejecutable**, en una dirección que puede cambiar en otra versión: el motor no debe usar `0x4F8888` a ciegas, sino buscarla por su contenido (las rutas `\LEVELS\...` y `\book\...`) o comprobar que el ejecutable es uno conocido (tamaño y CRC) y tener las direcciones de cada versión.
2. **El número de textos de `locale` puede variar** de un idioma a otro (la cabecera lo dice); no suponer 1738.
3. **Solo Latin-1**: el juego usa el byte bajo de cada letra. Para los idiomas de Europa occidental basta; si algún día se añaden otros (ruso, polaco...), habrá que usar la letra entera de 16 bits y una fuente con esas letras.
4. **La fuente** `font8.pvr` (la misma en inglés y español) **no tiene acentos ni `¿ ¡ ñ`**: la versión española tiene que usar otra para sus textos (sin encontrar). Habrá que buscarla, o hacer una fuente propia con esas letras.
5. Los **menús** están en el ejecutable de cada idioma: para el motor convendrá llevarlos a archivos de texto propios (con los del ejecutable como origen), como en *Alone in the Dark: Classic Reborn*.
6. Las **voces** (`Dialog\`, nombres como `AL_ING_01.wav`) y los vídeos pueden ser distintos por idioma; sin comprobar.
7. Para cada idioma hay que ver si su ejecutable usa `locale` (como el español) o los textos del nivel (como el inglés): el motor tendrá que saber qué versión tiene delante (por el tamaño y CRC de `alone4.exe`, o por el idioma del `goggame-*.info`).
8. Las **direcciones** de `docs/ALONE4_EXE.md` son del ejecutable español; las de otros idiomas no coinciden.
9. Ya están comparadas inglés y español; para los demás idiomas hace falta instalar cada versión en GOG y comparar igual (`alone4.exe`, `Dialog\`, `movies\`, `Sound2\`, `Provider\`).

## Pendientes

`DRAM` (salvo la entrada al nivel, el mapa de choques y el de disparadores: las listas de objetos, las funciones de los scripts, los grupos de piezas que se activan), `HAND`, `WEAP`, `weapon\*\lists`, la sombra de los modelos, qué hacen los eventos de las animaciones y la mezcla entre una animación y la siguiente (`FUN_00427030`), los archivos de `doc\` y `book\` que no son `VRM` de 640×480, `view_NNB` (solo nivel 20), `AFS` (`xa\`), `cs\`, `.def` de `midi\`, `PVRT` (`font8.pvr`), `DCG`.

## Sonido

El juego usa **Miles Sound System** (`Mss32.dll`) como un sintetizador: cada sonido es un "instrumento" (programa MIDI) de un banco, y se toca en uno de 16 canales.

- **`Sound2\*.wav`**: las muestras, todas en **IMA ADPCM** (formato 0x11), mono, 44100 Hz. Sin carpetas, aunque el juego las nombra con una (`porte\0001_b` es `Sound2\0001_b.wav`).
- **El trozo `DSND`** (al final de los archivos de los niveles, `levels\N\CarnbyNN`, y de otros): número de banco (u32; 0 para un nivel) y un bloque con punteros (los instrumentos del sintetizador; sin estudiar). Su lector es `FUN_0049a1ea` (registrado como `DSND`).
- **Qué muestra va en cada hueco del banco**: no está en los datos sino **en el ejecutable** (`FUN_0049bc7b`), en dos tablas:
  - **archivos** (267): nombre del archivo en minúsculas con un punto al final (`book\docplate.`, `levels\1\carnby01.`) y una lista de pares (grupo, elemento) que acaba en -1: el hueco k es el par k;
  - **grupos** (42: `a_action`, `c_marche`, `porte`, `hybride`, `phtsaure`...): una cabecera de 0x14 bytes (0, nombre del grupo, 0, un float 26163, 0) y luego 0x14 bytes por muestra: un valor, el **nombre** (`porte\0001_b`) y tres valores (dos float, ¿distancias?).
  Las direcciones cambian entre el ejecutable inglés y el español; el motor las **encuentra por su contenido** (`core/soundBanks`): el texto `a_action` y el puntero a él (cabecera del primer grupo), el puntero a esa cabecera (tabla de grupos), y el texto `book\docplate.` y el puntero a él (tabla de archivos). Comprobado con los dos: nivel 1, 56 huecos (43 con muestra); nivel 144, 37.
- **Función 45 de los scripts** (`FUN_0049a4dd` con el banco 0): toca el hueco *argumento 0* como la **nota** *argumento 4* (60 es la altura original; cada paso, un semitono) con **volumen izquierdo y derecho** *argumentos 12 y 16* (0..127; el juego los convierte en volumen (I+D)/2 y panorama D·128/(I+D), `FUN_0049a393`), en el siguiente de 16 canales; devuelve `banco << 16 | canal`. La 47 para uno, la 48 todos.
- **Qué hueco toca cada sonido del juego**: la cabecera del nivel (DRAM) lleva en `+0x34` 32 huecos generales (i16, -1 ninguno; `FUN_0044cac0`) y en `+0x74` 24 huecos por cada tipo de monstruo (0x30 bytes por tipo, `FUN_0044cb04`; justo antes de los textos en `+0x374`). En el nivel 1 el PHOTOSAURE usa los huecos 32 a 55 (`2840_a`...), en el 144 el HYBRIDE del 20 al 43.
- **Los pasos y sonidos del héroe** (`FUN_00434409`): eventos de código **100** de sus animaciones, el hueco de su valor en la **tabla del suelo** que pisa: la cabecera del nivel lleva en `+0x3DC` (número en `+0x3A8`) una tabla de 0x90 bytes (72 huecos i16) por cada tipo de suelo, y cada superficie del mapa de choques dice su tipo (`+2`); al pisarla el juego toma su tabla (`FUN_0042f0ab`). En el nivel 1 hay 5 tipos: los valores 0–1, 2–3, 4–5 son pares de pisadas (andar, correr...), 6–11 otros. El código **0x65** con 100, 0x67 y 0x6E toca los sonidos generales 4, 5 y 9; con 1 a 3, sonidos del arma (sin hacer).
- **Los sonidos de las armas**: cada arma (`weapon\<personaje>\weaponN`) trae su `DSND` con el **banco 1** y su lista en la tabla del ejecutable (`weapon\carnby\weapon0.`: `0160_a`, `0150_a`, `0190_a`, `0165_a`, `0150_a`); los bytes `+0xC` a `+0x10` de su bloque `WEAP` dicen qué hueco toca cada cosa: **`+0xC` el disparo** (`FUN_0046a0e2`, que además gasta la bala, muestra el fogonazo y suelta las partículas del arma), **`+0xF` el clic sin balas** (`FUN_00469cd4`), `+0xD` el evento 0x65/1 del héroe, `+0x10` los 0x65/3 (y 0x65/2 si `+0xE` es -1) (`FUN_0046cda4`); siempre con la **nota 64** y al menos 50 de 127 por lado.
- **Los sonidos de los monstruos** son eventos de código **201** de sus animaciones (`FUN_00452b31`): con valor 0–23 su sonido número *valor*; 100, 0x65–0x68, 200, 0xC9–0xCE, 300 y 0x12D eligen al azar entre unos pocos (algunos solo de vez en cuando). Se tocan con la nota 60 y un volumen por posición (`FUN_00418c9a`): por el lado de la pantalla donde está (el otro baja hasta 60 de 127) y más bajo lejos de la cámara a partir de 4 m (hasta 8). El código 200 son sus pasos (otra tabla del actor, `+0x58`; sin hacer).
- **Música y ambientes** (funciones 93, 94, 95, 51–53, 57, 58): `midi\<personaje>\<nombre>` son archivos de trozos con un `MIDB` y varias **secuencias** `DSEQ` con nombre (`orage_l`, `mer_c`, `pluie_f`, `grillon`, `vent_v`...); la 93 carga uno, la 94 toca una secuencia por su nombre (con volumen izquierdo y derecho) y devuelve un número, la 53 le cambia el volumen al cambiar de cámara y la 52 la para. **Sin hacer**: hace falta un secuenciador MIDI y entender los instrumentos. Lo que se sabe (lector `FUN_004982cf`, registrado como `MIDB`):
  - cabecera: `MIDB`, banco (u8), número de secuencias (u8), número de tablas (u8) y esas tablas de **128 bytes: programa MIDI → instrumento del archivo** (0xFF ninguno; en `jardind1` los programas 118–121 y 125–126 son los instrumentos 0–5);
  - cada secuencia: 4 bytes (el primer i16 su tamaño), 8 de **nombre** (`pluie_f`) y el `DSEQ` (sin alinear en el archivo). El `DSEQ`: `DSEQ`, versión (u8), otro u8, **tempo** (3 bytes, µs por negra: 0x07A120 = 120 por minuto), resolución (u16, 0x0BB8 = 3000), y luego **eventos MIDI estándar** con tiempos delta de longitud variable y *running status*: control 0x62, panorama (10), volumen (7), programa, nota 60, **desviación de tono** continua (la lluvia cambia de tono sin parar), etc. Parecido al SEQ de PlayStation;
  - al final, un bloque con punteros (como el de `DSND`) con los **instrumentos y sus muestras dentro, en PCM de 16 bits** (en `act_b1` se ven tras las secuencias). El juego **no** usa el sintetizador DLS de Miles: tiene uno propio (`FUN_00497ab2`, `FUN_00497b07`...) y solo usa de Miles las muestras sueltas (`AIL_start_sample`, `AIL_set_sample_playback_rate`...). Falta estudiar el formato de esos instrumentos (zonas de notas, nota raíz, bucles, envolventes).

- **Cómo suenan (estudiado el 2026-11-23 en el ejecutable; hecho en `src/core/musicFile.cpp`, `musicSequencer.cpp` y `src/app/musicPlayer.cpp`):**
  - **Los instrumentos son muestras de `Sound2`** (no el bloque de instrumentos del final del archivo, que se lee pero no se usa para ellas): dos tablas de 128 programas MIDI en el ejecutable (`0x4E2D98` musical: `bass01c`, `drums01c`, `strng01c`, `voice01c`...; `0x4E3798` de ambiente: `6000_a`, `6008_a`, `6420_a`, `avion01`...), 0x14 bytes cada entrada: marca (3: se repite en bucle), puntero al nombre, un valor (sin usar al tocar), **frecuencia de su nota raíz × 100** (26163 = 261,63 Hz) y 0. Para cada archivo de `midi\` (`act_b12.`, `riviere.`...; lista en `0x4E7BE8`, 0x42 entradas de nombre y 128 bytes) dice qué programas usan la tabla de ambiente (valor 2). El motor las busca por su contenido en el `alone4.exe` del jugador (`core/soundBanks`).
  - **Una nota** (`FUN_0049b5b6`): hasta 12 a la vez; volumen = volumen del canal (control 7) × fuerza / 127 × volumen de la secuencia / 127 (`0x49AD30`); panorama del canal (control 10); **velocidad = 44100 × frecuencia de la nota × 100 / raíz** (tabla de frecuencias de las 128 notas en `0x4E7F68`: 8,18 Hz la 0); las de marca 3 en bucle. El volumen y el panorama del canal cambian también lo que ya suena (`0x49B906`, `0x49B988`); la desviación de tono se guarda pero **no cambia lo que suena** (solo la nota al empezar fija la velocidad: es la única llamada a `AIL_set_sample_playback_rate`). Al soltar la nota se apaga con un temporizador (`0x49B7EA`; su ritmo, sin estudiar: el motor, una décima de segundo).
  - **El reproductor** (`FUN_004987f2`, temporizador de Miles a **200 por segundo**, `0x4866FF`): cada paso suma a su reloj el valor de la cabecera `+10` (big endian) / 200 (`DAT_004cdc00`, un i16 = 200) y toca cada evento cuyo retardo × 1000 ya alcanzó. Eventos **sin running status**: `8x` nota fuera (1 byte: apaga la nota del canal; una nota por canal), `9x` nota (nota y fuerza; **el juego toca la nota − 12**), `Bx` control (7 volumen, 10 panorama, **98 marcas de sección**, 0x67 y 0x69 con un byte más), `Cx` programa, `Ex` desviación (2 bytes), `FF 2F 00` fin (para), `FF 33` + 3 bytes otro tempo. **Secciones**: tabla de marcas en `+0x10` (big endian, dos por sección: dónde empieza y desde dónde se repite); al llegar a la marca de fin de la sección elegida (control 98 = sección + 1) vuelve a su punto de repetición (las veces dichas, 0 siempre); con otra sección elegida salta a su inicio. Así `act_b1` toca una introducción una vez y repite el resto; la lluvia se repite entera.
  - **Volumen de una secuencia**: izquierdo y derecho (0–127, guardados × 128): volumen = la media, panorama = derecho × 128 / suma (`0x4998B3`); la función 95 los lleva a otros valores en N segundos (un paso cada 1/200 s, `0x499B26`).

## Vídeos: `movies\*.BIK` y `MOVIES.TXT`

- **Bink 1** (`BIKi`), 640×320 a 30 imágenes por segundo, con una pista de sonido. El juego los toca con `binkw32.dll` (32 bits); el motor los lee con FFmpeg, que trae un decodificador de Bink.
- **`MOVIES.TXT`** (en la carpeta del juego, `FUN_00480ee0`): una línea por vídeo, `número,nombre,1,imágenes` (p. ej. `2,intro,1,...`). El juego los pide por número (`FUN_004811cf`, tabla de 10 bytes por entrada en `DAT_0050a250`).
- Números con sentido fijo: 0 logos (Infogrames, Darkworks, Miles, Bink), 1 final (`extro`), 2 intro, 3 tráiler, 4 `select_a` (Aline salta), 5 `select_c` (Carnby salta), 56 `spiral` (Spiral House), 57 `rad`, 58 `dark`. Del 10 en adelante, los de las escenas, que piden los scripts (función 42).

## Escenas de radio: `radio\N`

Las pide la función 158 de los scripts (`\radio\76`). Cabecera de 16 bytes: `+4` dónde empieza la tabla de bloques y `+8` cuántos hay (los dos contados desde `0x10`). Cada bloque es una lista de órdenes que acaba en `FFFF`; una orden es su número (16 bits), si lleva `0x8000` el bloque que se arranca cuando termine (16 bits), el tamaño de sus argumentos (16 bits) y los argumentos (32 bits cada uno; el primero no se usa en las órdenes del héroe). La escena empieza con el bloque 0; las órdenes con bloque se quedan "vigilando" (16 a la vez) hasta que acaban, y entonces arrancan su bloque. Cuando no queda ninguna, la escena acaba. Órdenes (tabla `0x4C5B68`, dos funciones cada una: empezar y "¿sigue?"):

| Orden | Qué hace |
|---|---|
| 3 | esperar N ms |
| 4 | pedir la cámara N y esperar a que se vea |
| 5, 6 | el héroe lo lleva la escena (estado 3) / vuelve al jugador |
| 7 | botones del héroe: avanzar, atrás, correr |
| 8 | el héroe va al punto marcado N (hasta pasarlo) |
| 9 | el héroe se gira hacia el punto N en M ms |
| 10 | animación del héroe (identificador; 400-404 son las de la radio) hasta su final |
| 11, 12 | marcas de la caja de textos y `FUN_00417376` (sin saber) |
| 13 | voz N de `xa\` y esperar a que acabe |
| 16 | el héroe coge el objeto N |
| 20 | la cosa N (partículas) se ve M ms (−1: siempre) |
| 28 | linterna encendida o apagada (solo si la mano lleva linterna) |
| 37 | el héroe en el punto N |

Las demás (actores, objetos, cosas con modelo, sonidos...) no las usan las radios. Los archivos de **`cs\`** (75, como `a112_020`) tienen el mismo formato: son las escenas cortas del juego.

Las animaciones de la radio del héroe: 400 escuchar (bucle), 401 sacar la radio (evento 0x66 con 2: la mano 2, que es la radio), 402 y 404 hablar (bucle), 403 guardarla (401 al revés, evento 0x66 con 1: vuelve la mano de antes).

## Voces: `Xa.txt` y `xa\XaNN.xa`

- **`Xa.txt`** (carpeta del juego) está cifrado como las tablas `.csv`; una línea por voz: `archivo,hueco` (1459 voces).
- **`xa\XaNN.xa`**: contenedor **AFS** (`AFS\0`, cuántos, y de cada uno posición y tamaño), 16 sonidos cada uno, ordenados por duración.
- Cada sonido es **CRI ADX** (empieza por `80 00`, `(c)CRI`): mono, 22050 Hz, codificación 3, tramas de 18 bytes (escala de 16 bits y 32 muestras de 4 bits) predichas con las dos anteriores, con coeficientes que salen de la frecuencia de corte (500 Hz).

## La luz de la mano: trozo `HAND` de `lefthand\<personaje>\handN`

Datos en `+0x0C` (0x18 bytes): `+4` color del resplandor (cada parte ×2 hasta 255), `+8` color del foco, `+0x10` color de la raya (3 bytes), `+0x14` si es una luz, `+0x15` marcas (1 haz estrecho, 2 alcance largo). En los colores, sus bytes son rojo, verde y azul (`FUN_00414fa9` pone el primero en el rojo de sus vértices): la raya de `hand1` (`32 32 FF`) es azul. El banco `G3DB` que sigue trae, además del modelo, tres texturas con nombre (`FLARE`, `GLOW` 64×64 y `LINE` 64×8): el nombre ocupa 16 bytes antes del registro de textura de siempre, y sus píxeles están **16 bytes más allá** de lo que dice el registro; 5-6-5 en orden Z (las rectangulares, cuadrados del lado corto uno tras otro).

### Páginas de los menús (bloque `DRAM` de `menu2`, `select2`; `FUN_0046fef9`)

La cabecera da cuántos elementos hay de cada tipo (16 bits en `+0x00`...`+0x1C`) y dónde está su lista (`+0x20`...`+0x5C`): `+0x00`/`+0x20` imágenes (0x1C bytes: x, y, color, número de imagen << 16 | 1), `+0x02`/`+0x24` textos (0x20: fuente, x, y, color, número de texto << 16 | alineación), `+0x0A`/`+0x30` polígonos de color (0x4C: el degradado del fondo), `+0x0E`/`+0x38` luces, `+0x10`/`+0x40` cámaras, `+0x08`/`+0x3C` y `+0x1C`/`+0x5C` objetos 3D (los rayos del título), `+0x12`/`+0x48` fuentes, `+0x50` textos. En `select2`: Aline (imagen 1) en x 0, Carnby (imagen 0) en x 320, y "Choose a character" (texto 2) en y 400. Lo que se puede elegir (la rueda del título, las fichas) lo crea el código.

## Sistemas de partículas (bloque `G3DB` de `levels\N\CarnbyNN`, `AlineNN`, y de las armas)

En el bloque: `+0x14` cuántos, `+0x18` dónde (0x60 bytes cada uno). Lluvia, relámpagos en el cielo, la luz del otro personaje a lo lejos, salpicaduras...

| Campo | Qué es |
|---|---|
| `+0x00` | partículas como mucho |
| `+0x04` | vida de cada una (ms) |
| `+0x08` | suelta una cada tantos ms |
| `+0x0C` | 1: se dibujan solo las vivas |
| `+0x10` | 1: se quedan donde está el emisor |
| `+0x14` | 1: cada una empieza girada al azar |
| `+0x18` | sigue soltando tantos ms después de mostrarse |
| `+0x1C` | tamaño al nacer (1/8192 de unidad en las de punto) |
| `+0x20` | las planas se cortan en tantos cuadros + 1 por lado |
| `+0x24` | forma: 0 punto (cuadrado girado de cara a la cámara), 1 plana en el suelo, 2 raya (desde la partícula hacia atrás según su velocidad, 1 píxel de ancho: la lluvia) |
| `+0x28` | su textura (registro de 0x30 bytes, píxeles en orden Z) |
| `+0x58`, `+0x5C` | cuántos efectos y dónde (0x0C bytes: tipo, cuántas claves, dónde) |

Efectos (sobre la edad de la partícula; la clave anterior vale hasta la siguiente): 0 y 2 aceleran (claves de 3 `float` + tiempo `int`; 2 girado como el emisor), 1 y 3 lo mismo al azar entre −valor y +valor; 4 y 5 giro; 6 color (claves r, g, b, tiempo de 16 bits, interpoladas; ×2 hasta 255); 7 y 10 crecer (10 al azar); 8 visible; 9 forma de mezclar (0 a 3; 3 = sumar luz a un cuarto); 11 trozo de la textura (x, y, lado, tiempo). Las demás claves son valor y tiempo de 16 bits.

Quién los usa: las "cosas" del nivel (lista `+0x3C4` del `DRAM`, 0x1C bytes: `+1` sistema, `+2` marcas (1 siempre visible), `+4` sitio, `+0x10` giros), que los scripts muestran unos milisegundos (funciones 116 y 117) y las escenas (orden 20); la lluvia, si la cabecera del nivel tiene la marca `0x02` en `+0x10` (sistema en `+0x3F1`; `0x04` llueve desde el principio, función 105 la enciende o apaga); `+0x3F3` salpicaduras bajo los pies (marca `0x08`), `+0x3F5`, `+0x3F7`, `+0x3F9` efectos de actores y del héroe.

- **`BALLES`** (textura con nombre del banco de cada `weapon\<personaje>\weaponN`): la bala de la pantalla de apuntar (8×32, 8×16 o 32×32, 1-5-5-5 con transparencia).
