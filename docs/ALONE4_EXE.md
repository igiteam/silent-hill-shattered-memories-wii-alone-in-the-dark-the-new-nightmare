# Lo que se sabe de alone4.exe y de sus archivos

Primera revisión: 2026-11-22, sobre la instalación de GOG en español (solo lectura).

## El ejecutable

- `alone4.exe`: 1.036.288 bytes, Windows de 32 bits (x86), compilado el **2001-06-01** con **Visual C++ 6.0** (enlazador 6.0, mezcla de C y C++).
- **Sin comprimir ni protegido**: tres secciones normales (`.text` 748 KB de código, `.rdata`, `.data` de 1 MB en memoria). Ghidra lo puede leer directamente.
- Dirección base `0x400000`, entrada en `0x4AEA3E`.
- Rutas de su código que quedaron dentro: `C:\work\alone4\master\PC\DRAW\DRAWTEXT\text.cpp` y `C:\work\alone4\master\PC\SCREEN\GLS\gls.c` (el código de PC estaba en una carpeta `PC\` aparte: la base es común con las consolas).
- Muchos textos de depuración en inglés y francés: nombres de clases (`CStandaloneMapsManager`), estados de los monstruos (`ARACHNOCIDE`, `RATLUX`, `PHOTOSAURE`, `TRICERAPTOR`, `HYBRIDE`, `ROTWEILER`…), `LIFE: %3d PROCS: %d DEPTH: %d` (parece un intérprete de scripts, como el "Life" de los primeros Alone), `CS: Found %d functions` (los archivos de `cs\`), `Get latest version of PsxLevelBuilder`, `PSX - Save name is created based on` (**los niveles se exportaban con la herramienta de PlayStation**: la versión de PC usa los mismos formatos que la de PS1). Esos textos son la mejor guía para poner nombre a las funciones.

### Lo que usa del sistema

| Librería | Para qué |
|---|---|
| OpenGL (cargado a mano con `LoadLibraryA`, no en la tabla de importación; también los controladores de 3dfx Voodoo `3dfxvgl.dll`) | Dibujo 3D. Readme: "tarjeta compatible con OpenGL". Textos `Default OpenGL Driver`, `Bound OpenGL ICD ok`, `Using OpenGL to play videos` |
| `DINPUT8` (DirectInput 8) y `WINMM` (`joyGetPos`) | Teclado, ratón y mando |
| `mss32.dll` (Miles Sound System, 30 funciones `AIL_*`) | Sonido, música (MIDI propio con `midi\`) y voces en streaming |
| `binkw32.dll` (Bink, 15 funciones) | Vídeos de `movies\*.BIK` |
| `ADVAPI32` (registro de Windows) | Opciones |
| `USER32`, `GDI32`, `KERNEL32`, `ole32`, `VERSION` | Ventana, archivos, reloj (`QueryPerformanceCounter`) |

Para la reimplementación: OpenGL, DirectInput, Miles y Bink se reemplazan por lo mismo que en AITD (bgfx, SDL3, SoLoud). Los vídeos Bink 1 necesitarán un decodificador propio o uno libre (FFmpeg lo lee).

## Los archivos

Casi todo está comprimido con **gzip** y relleno con ceros hasta múltiplos de 2048 bytes (sectores de CD). Python lo abre con `zlib.decompressobj(31)` (ignora el relleno).

| Carpeta | Archivos | Qué parece ser |
|---|---|---|
| `levels\<n>\` | 4156 (806 MB), 107 niveles numerados | Por cámara: `view_NN` (empieza por `G3D`: datos de la cámara/escena), `view_NND` y `view_NNS` (empiezan por `LMVR`: imágenes del fondo, la D más grande); `Carnby01`, `Aline01` (empiezan por `DRAM`: datos del nivel para cada personaje). Textos: `/LEVELS/%d/VIEW_%02d%c.` |
| `book\`, `doc\` | `.VRM` | Documentos y libros: al descomprimir miden 1.048.672 bytes = 96 de cabecera + **1024×512×2, una copia de la memoria de vídeo de la PlayStation** |
| `item\IT3D_n` | 159 | Objetos del inventario en 3D (`DRAM`) |
| `cs\` | 75 | Sin comprimir; probablemente los scripts de escenas (`CS: Found %d functions`) |
| `Dialog\` | 1460 `.wav` | Voces |
| `Sound2\` | 958 `.wav` | Efectos |
| `xa\XA01.XA`… | 92 | Música/sonido en streaming (cabecera `AFS`; `Xa.txt` lista su contenido) |
| `midi\` | 138 | Música MIDI propia (`MG102B01` + `.def`) |
| `movies\` | 51 `.BIK` | Vídeos; `MOVIES.TXT` los lista |
| `MAPA\`, `MAPC\` | ~95 cada una | Mapas de Aline (A) y Carnby (C) |
| `int\`, `menu\`, `weapon\`, `lefthand\`, `radio\` | | Interfaz, menús, armas, linterna/mano izquierda, radio |
| Raíz | | `NpcCarac.csv` (datos de los monstruos), `aweap.csv`/`cweap.csv` (armas de cada uno), `Text.csv`, `font8.pvr` (fuente, formato `PVRT`), `viewerobj.DCG`, `aitd4.cfg` (`mode_video pal`), `keys.bin`, `settings.bin`, partidas `_Alone4_*.sav` y `quick.sav` |

## Funciones identificadas

**Las direcciones son del `alone4.exe` de la versión española de GOG** (guardado aparte), el que tiene el proyecto de Ghidra. El de la versión inglesa (la base del proyecto desde el 2026-11-23) es otro y sus direcciones no coinciden.

Sus nombres provisionales están puestos en el proyecto de Ghidra desde `tools/ghidra/functionNames.txt` (al entender una función nueva: añadirla a la lista y volver a ejecutar `tools\ghidra\run.ps1 -Write RenameFunctions.java tools\ghidra\functionNames.txt`).

Nombres provisionales hasta ponerlos en Ghidra. Formatos que leen: `docs/FORMATOS.md`.

| Dirección | Qué hace |
|---|---|
| `0x4041FD` | Lee un archivo de trozos: para cada trozo busca su lector por las 4 letras |
| `0x403FC0` | Registra el lector de unas letras de trozo |
| `0x4165FF` | Registra los lectores de `G3DB` (`0x4166AA`) y `LMVR` (`0x4166BF`) |
| `0x44937E`, `0x46D42B` | Registran los lectores de `HAND` (`0x449395`) y `WEAP` (`0x46D8BB`): los dos leen un bloque con punteros (`0x40410A`) |
| `0x44C0CE` | Carga una cámara: `VIEW_NN`, `VIEW_NND` y `VIEW_NNS` |
| `0x4061B0` | Prepara las dos texturas de una cámara para la linterna: pasa D y S a filas; la de S se queda con la mitad de arriba de D (lo oscuro), la de D se queda solo con la marca 0x80 y recibe, en el sitio de cada polígono de la escena, los píxeles iluminados de S (con `0x406690`); luego se cambian las paletas entre sí |
| `0x406690` | Copia un rectángulo de la imagen iluminada (640×480) a la hoja, guardando la marca 0x80 de cada píxel |
| `0x4166AA` → `0x4213AD` → `0x420C64` | Lector de `G3DB`: lee el número de hueco y el bloque, y sube sus texturas (si tiene) |
| `0x40410A` | Lee un bloque con punteros: tamaño, número de punteros, bloque y lista de dónde hay punteros (que se pasan de desplazamiento a dirección) |
| `0x4060C0` | Pasa los píxeles de orden Z (Morton) a filas |
| `0x419253` | Sube un fondo como textura |
| `0x4ABBC4` | `sprintf` |
| `0x4041FD` → `0x403FF3` → `0x404006` | Lector de `DRAM` (datos del nivel en `CarnbyNN`/`AlineNN`): igual que el bloque con punteros de `G3DB`, pero sin número de hueco |
| `0x44BEBD` | Carga los datos del nivel para el personaje: `\LEVELS\%d\%s%02d` (p. ej. `Carnby01`); comprueba la marca `0xDEADBEEF` y la versión `0x0131554C` |
| `0x4254D0` | Prepara un modelo con esqueleto: guarda en `+0x10c` el objeto del banco, reserva los búferes de trabajo por vértice (`+0xe8` posición 16 B, `+0xec` coordenadas de textura 8 B, `+0xf0` color 4 B, `+0x100` normal y luz 32 B) y por hueso (`+0x14`, 0x128 B), y calcula la caja que lo envuelve |
| `0x426AB0` | Pone el esqueleto en pose: para cada hueso, rotación (3 ángulos) y matriz a partir de la de su padre |
| `0x426830` | Mueve la sombra con el esqueleto; avisa si la sombra y el modelo no tienen los mismos huesos ("pivots") |
| `0x41AA80`, `0x41B100` | Luz que recibe un modelo, vértice a vértice: tres tipos de luz (0 puntual, 2 otra puntual, 1 foco = la linterna, con su cono) |
| `0x425320` | Junta las tiras de triángulos de cada material de un modelo en una lista de triángulos (`+0x110` número de materiales, `+0x114` listas, `+0x118` cuántos triángulos) |
| `0x4896E0` | Dibuja un modelo con OpenGL: para cada material elige la textura, las dos caras y la transparencia, y llama a `glDrawElements(GL_TRIANGLES, ...)` |
| `0x427250`, `0x4279C0` | Reproducen una animación: para cada pista, la posición, el giro y la escala del hueso en ese momento; luego los grupos de pistas que se mezclan con pesos |
| `0x413130`, `0x4132E8`, `0x413528` | Valor de una pista en un momento: posición, ángulos (por el camino más corto) y escala, en línea recta entre llaves |
| `0x425199` | Avanza el tiempo de una animación: hacia delante o hacia atrás, repitiéndola o parándose al final |
| `0x4270F0` | Recoge los eventos de una animación que caen entre el momento anterior y el actual |
| `0x436236` | **Cerebro del personaje** (máquina de estados): lee los botones y cambia de estado (`0x435F50`), y en cada estado pide una animación por su "base". Botones (bits de su parámetro): 0x01 adelante, 0x02 atrás, 0x04 correr, 0x08 girar a la izquierda, 0x10 a la derecha, 0x20 media vuelta, 0x40 arma (apuntar), 0x80 sin saber, 0x100 acción o disparo. Estados y bases (deducidos de los botones y comprobados con las animaciones que dan: quieto, andar, correr, media vuelta de 180°, apuntar y disparar con el revólver; el resto sin comprobar): 0 quieto (base 0), 1 andar (1), 8 correr (2), 4 andar hacia atrás (9), 7 girar (6), 0x13 media vuelta (12), 9 apuntar (4), 10 disparar (5), 3 andar apuntando (7), 2 disparar andando (8), 6 atrás apuntando (10), 5 disparar hacia atrás (11), 0x14 media vuelta apuntando (13), 0xB ¿recargar? (15); 0x10, 0x11 y 0x12 usan números de animación fijos (0x8C, 0x8D, 0x0E) o tablas propias (`0x4C5F00`) |
| `0x43A286`, `0x469FF3` | Eligen la animación de un personaje: `0x43A286` calcula la variación (columna 0 a 9: +1 con la linterna encendida, la marca de `+0x84` byte 10) y `0x469FF3` lee el identificador en la tabla de acciones del arma (bloque `WEAP` + 0x28, fila = base); el identificador se convierte en animación con la tabla del banco (fila × 10 + columna) |
| `0x48A1A0` | Dibuja la sombra de un modelo como volumen de sombra (stencil): triángulos de 3 índices y sus vecinos |
| `0x41806D` | Pone una animación a un modelo por su identificador (tabla del banco) y la hace avanzar el tiempo del fotograma (`DAT_0050570c`); devuelve lo que dice el reloj (1 se repitió, 2 terminó). El cerebro la llama en cada fotograma: si es la misma animación, solo avanza |
| `0x4287D0` | Cambia de animación: si es otra, busca su transición (`0x427030`) y cambia de golpe, la mezcla con la anterior o espera a una ventana (ver `docs/FORMATOS.md`); mientras mezcla, solo avanza la mezcla (`0x4282D0`); luego `0x4285A0` |
| `0x427030` | Busca la transición de una animación a otra y la ventana en la que cae el momento actual; falso si hay ventanas y no cae en ninguna (seguir con la actual) |
| `0x4282D0` | Un fotograma de mezcla: pose = la guardada + (la nueva − la guardada) × tiempo / duración, hueso a hueso desde el 1; mueve al personaje a la velocidad de la transición; al acabar pasa a la nueva |
| `0x4285A0` | Avanza el reloj de la animación (`0x425199`, con el ritmo de la animación) y calcula el **movimiento del personaje** en el fotograma: con la marca 0x04, recto a la velocidad de la animación (tiempo × velocidad × 0,030003); sin ella, lo que cambian la posición y el giro de su primera pista (el hueso 0) desde el fotograma anterior. Lo deja en el modelo (`+0xB4` posición, `+0xCC` giro) |
| `0x4339FE` | Construye el personaje que lleva el jugador (`GE_Hero`; tabla de funciones `0x4BD928`) |
| `0x433BF5` | Prepara al personaje: posición (`+8`), ángulos (`+0x14`, `+0x18`, `+0x1C`), modelo (`+0x44`), choques con el escenario (`+0xE4`), arma (`+0x324`), pesos de las mezclas a 0,25, estado 0 y animación 1 (quieto) |
| `0x438920` | Lee el mando o el teclado (`0x5C4738`) y pone las marcas del personaje: bit 4 adelante (`+0x8B`), 8 atrás (`+0x8C`), 0x20 correr (`+0x8D`), 2 girar a la izquierda (`+0x8E`), 1 a la derecha (`+0x8F`), 0x100 apuntar (`+0x92`), 0x200 el botón 0x80 del cerebro (`+0x91`), otro 0x10 acción (`+0x90`). **Media vuelta**: atrás pulsado, soltado y pulsado otra vez en menos de 300 ms (`+0x95`); atrás solo hace andar hacia atrás tras mantenerlo 300 ms. Otro botón 0x80 enciende o apaga la linterna (`0x435751`). También apunta la linterna con los ejes del mando (`+0x98`, `+0x9C`) y busca a quién apuntar |
| `0x436130` | Convierte esas marcas en los botones del cerebro (0x01 adelante... 0x100 acción) y llama al cerebro (`0x436236`); antes, `0x435FFC` aplica el estado elegido en el fotograma anterior (`+0x34C` → `+0x348`) |
| `0x434802` | Gira y desplaza al personaje: suma el giro de la animación a sus ángulos; con `+0x8E` gira a la izquierda y con `+0x8F` a la derecha **6° por fotograma** (6 × milisegundos / 33; el ángulo se pasa de grados con 2π/360, `0x473C8E`); gira el movimiento de la animación según hacia dónde mira y lo deja en `+0x2AC` |
| `0x434D31` | Aplica ese movimiento a la posición comprobando los choques con el escenario (`+0xE4`) y coloca el modelo (`0x416F9F` posición, `0x417B2A` ángulos) |
| `0x44A39D` | Prepara el nivel con su `DRAM` (`nivel + 0x28`): el mapa de choques (`+0x3E4` → objeto de choques del nivel, `nivel + 0x5C`, `0x401021`), el personaje en su posición de entrada (`+0x3B4`), las listas de objetos (`+0x3B8`...) |
| `0x402813`, `0x40294D`, `0x403638`, `0x4035CC` | Preparan el objeto de choques de alguien: marcas (0x01 contra el escenario, 0x02 contra otros, 0x04 resbalar por las paredes), altura del centro y radio (el personaje: 60 y 20 cm), esfera para los choques entre personajes (radio 40 a 60 cm de altura) y esferas pequeñas del cuerpo |
| `0x4029EE` | Mueve a alguien de un punto a otro con choques: primero contra los demás (esferas), luego contra el escenario (`0x401D4A`); si choca, resbala por la pared (marca 0x04) o se queda donde estaba |
| `0x401D4A` → `0x401C78` → `0x40152B`, `0x401955` | Choque de una esfera que va de A a B contra el mapa: recorre las casillas de la rejilla que toca el movimiento y prueba sus planos sueltos y polígonos (`0x405E13`: corte con el plano a la distancia del radio; `0x40597D`: distancia a un borde) |
| `0x4A31EF`, `0x4A33FB` | El detector de disparadores del personaje (`+0x2E4`): con el mapa de disparadores (`nivel + 0xE8` = `+0x3E8` del `DRAM`), cada vez que se mueve prueba su paso a 1 m de altura contra las paredes de disparo (`0x4A36A7`, `0x405F03`) y las zonas (`0x4A372C`), y avisa al oyente (`+0x2B8`, tabla de métodos `0x4BE0EC`) |
| `0x466EE8` | El oyente: al cruzar la pared k, si la cámara actual es la que pide el disparador k (`+0x3AC` del `DRAM`), ejecuta su script de entrar o de salir |
| `0x44BD11` | Pide una cámara (`nivel + 0x14` = hay cambio, `+0x18` = cuál); `0x44B4E1` la carga después con `loadCamera` |
| `0x44B91B` | Entrada al nivel: pide la cámara de la posición de entrada (+0x1A) y ejecuta su script (+0x20) |
| `0x44DF07` → `0x4629A6` → `0x411918` | Ejecuta un script: busca una máquina libre (0x60 bytes cada una) y lo arranca |
| `0x411FC5` | **La máquina de los scripts**: una instrucción por llamada (ver `docs/FORMATOS.md`); `0x412F99`, `0x412FCB`, `0x413025` leen del código un byte, un u16 y un i32; `0x411C06`/`0x411C7E` meten y sacan de la pila |
| `0x4667AC`, `0x47389E` | Dan a cada máquina su tabla de funciones del juego (`0x411F97`): 203 en `0x4C7878`, 72 en `0x4C7EE4` |
| `0x462D39`, `0x4665B4` | Funciones 3 y 199 de los scripts: poner la cámara N (argumento 0 de la pila, `0x411AF8`) |
| `0x436236` (armas) | Apuntar: quieto, andando o andando hacia atrás con el botón 0x40 y un arma en la mano (`0x4696E0`) pasa a 9, 3 o 6; apuntando, 0x100 dispara (el arma, entrada `+0x20` de su tabla, cuando `+0x368` lo permite) y pasa a 10 (o 2 y 5 andando), que juega la animación de la acción 5 hasta el final y vuelve a 9; soltar 0x40 baja el arma. Las armas 2, 6 y 8 disparan seguido (`DAT_00505c04`) |
| `0x49A4DD` | toca un hueco de un banco de sonido como una nota, en el siguiente de 16 canales (cambio de programa MIDI `0xC0 | canal`, `0x49AF2A`); devuelve banco << 16 | canal |
| `0x49A393` | volumen izquierdo y derecho → volumen (I+D)/2 y panorama D·128/(I+D) |
| `0x49A1EA` | lector del trozo `DSND` (banco de sonido de un archivo): lee el banco y llama a `0x49BC7B` |
| `0x49BC7B` | llena los huecos del banco con las muestras de `SOUND2` según las tablas del ejecutable (archivos en `0x4DBF90`, grupos en `0x4E2CD8`) |
| `0x452B31` | el actor atiende los eventos de su animación: 1 golpe al héroe (con la distancia del evento y el daño de su ataque), 2 agarre, 200 sonido... |
| `0x4270F0`, `0x41816B` | juntan los eventos que pasa el reloj de una animación en una cola de 16 (`+0x2C`/`+0x30` del modelo) y los sacan uno a uno |
| `0x435281`, `0x435338` | el héroe recibe un golpe: guarda quién (`+0xD0`), estado 0x11 (herido), resta la vida (estado de la partida `+0xC`); sin vida, estado 0x10 (muerto) |
| `0x451853` | cerebro de la clase base de actor (entrada `+0x24`): comportamientos 1 esperar, 2 atacar, 4 perseguir, 5 ir a un sitio, 7/8 herido |
| `0x4982CF` | lee un archivo de `midi\` (`MIDB`): tablas de programas, secuencias en sus huecos (5), bloque de instrumentos |
| `0x4987F2` | el reproductor de secuencias (200 veces por segundo): eventos, secciones (control 98), volumen que se mueve |
| `0x49B5B6` | toca una nota: busca una de 12 muestras libres, volumen, panorama, velocidad = 44100 × frecuencia de la nota × 100 / raíz, bucle si la marca es 3 |
| `0x49C5AE`, `0x49BFD9` | el instrumento de un programa: de la tabla musical (`0x4E2D98`) o de ambiente (`0x4E3798`) según el archivo (`0x49B090`, lista `0x4E7BE8`); carga `Sound2\<nombre>.wav` |
| `0x4992F7`, `0x4998EB`, `0x499B26`, `0x499589`, `0x499624` | tocar una secuencia por su nombre (volumen, sección, veces), cambiar su volumen, moverlo en N segundos, elegir su sección, pararla |
| `0x45CE8F`, `0x43F8CF` | cerebros propios de PHOTOSAURE (hecho, ver `docs/FORMATOS.md`) y de HYBRIDE (sin hacer) |
| `0x4524EA` → `0x418AB3` | ¿le da la luz de la linterna al actor? (marca 2 de `+0x15C` de su modelo) |
| `0x45E006` | adónde huye el PHOTOSAURE: 0 atrás, 1 o 2 a un lado si a 120 cm hay sitio (rayo `0x403249`) |
| `0x449662` | cada fotograma coloca la mano izquierda y, si su `HAND` tiene luz (`+0x14`), su foco (`0x418461`: misma posición y giro que la mano) y su luz puntual en un punto del modelo de la mano |
| `0x41A2C0` | luz de los vértices de la malla de la escena (el fondo): la misma fórmula del foco (255 × (1 − d²/cono² − p/alcance) × 2 / 255), atenuada fuera de un rectángulo de la pantalla (× 1/(0,05 × píxeles fuera)) |
| `0x44948D` | carga el `HAND`: `+0x15` bit 1 → ¿haz estrecho? (`0x418689`), bit 2 → alcance largo (`0x505730`: 1875 en vez de 625) |
| `0x419D40` | crea una luz: tipo, color 255, radios² 10000 y 2500; la del foco con focal 160 / tan(1,7921 / 2) = 128 (`+0xD4`) |
| `0x42F4DB` | el actor se gira poco a poco hacia un punto (`+0x54`) |
| `0x452277` | ¿ve el actor al héroe? (distancia menor que `+0x258`) |
| `0x455080` | lee `\NPCCARAC.CSV` (características de los monstruos) a `0x507B60` |
| `0x479CD2` | descifra los `.csv` del juego |
| `0x454B20` | fábrica de actores (lista `0x3B8`): la clase según el tipo (`+8` del registro); luego llama a `+0x20` de su tabla (`0x450C42` en todas: lee el registro) |
| `0x45126C` | el actor atiende su cola de órdenes (`+0x2B6`): aparecer, desaparecer, morir... y cambia su presencia (`+0x2B2`, `0x451251`) |
| `0x46850C` | crea una cosa con modelo del nivel (lista `0x3BC` del DRAM): posición, ángulos, modelo, animación, sombra y piezas según sus marcas |
| `0x468AD8`, `0x468C1B` | mostrar u ocultar una cosa con modelo (`+0x94`), y consultarlo (funciones 38 y 39 de los scripts) |
| `0x468C2F`, `0x468CA7` | mostrar u ocultar su sombra (`+0x95`, función 97); ponerle una animación y animarla (`+0x48`, `+0x96`, función 40) |
| `0x476EA9` | matriz de giro por tres ángulos: sobre z, luego y, luego x (`0x47424B` seno, `0x47428A` coseno) |
| `0x46D4E3` | `GE_WeaponManager`: carga la mano derecha: `\weapon\%s\righthd` sin arma, `\weapon\%s\weapon%d` con ella |
| `0x4493C7` | `GE_LeftHandManager`: carga la mano izquierda `\lefthand\%s\hand%d`; la eligen el inicio de la partida (1), el cierre del inventario (`0x4433EC`: 1, 3 o 4 según los objetos 0x55, 0x7F y 0x4F) y un suceso de animación (`0x434409`: 2) |
| `0x466F84`, `0x467031` | Métodos 2 y 3 del oyente del personaje: entrar y salir de una zona del mapa de disparadores (scripts de +4 y +8 de la zona, `+0x3B0` del `DRAM`); la zona con acción en la que se está queda en `nivel + 0xEC` (`0x449C9E`) |
| `0x449CB7` | **El botón de acción**: con zona de acción, comprueba si hace falta mirar hacia un lado (marca 0x02) y ejecuta su script (+0xC); sin ella, el script de `+0x1C` del `DRAM` |
| `0x44B3E1` | Elige la posición de entrada al nivel: la obligatoria (+0x1C = 100), la que viene del nivel anterior (+0x18) o, al empezar, la de −1 |
| `0x44DCEF` → `0x44DD12` | Función 4 de los scripts: apunta el nivel pedido; después del fotograma se cambia de nivel (`setupLevelFromDram` con ese número) |
| `0x464E4D` | Función 108: pregunta con un texto del nivel y sus respuestas ("Yes@No"), `0x43CE5F` |
| `0x465A90` | Función 154: muestra un texto del nivel como mensaje, `0x43D661` |
| `0x4A9EF0` → `0x4AA0F8` | Al arrancar (versión española): carga los textos de `Provider\locale` (`0x4AA2D8` lee uno: UTF-16, se queda con el byte bajo) |
| `0x4A9FAF`, `0x4AA047`, `0x4A9FF7` | (Versión española) busca la ruta del archivo de nivel en la tabla de `0x4F8888` y convierte el texto k del nivel en un texto de `locale` |
| `0x4032C2` → `0x401B4F` → `0x4012C0`, `0x401658` | Pega a alguien al suelo: rayo hacia abajo desde la altura del centro, en la casilla del punto, contra las piezas pisables (`0x405BEC`); también dice el tipo de suelo |

El juego dibuja con **OpenGL** (llamadas a través de punteros a funciones como `DAT_00526fd0` = `glDrawElements`). Para encontrar funciones por su código: `tools\ghidra\run.ps1 DecompileByCode.java <patrón>... <salida.c>` (decompila todo, tarda unos minutos; los patrones sin espacios, paréntesis, comas ni punto y coma, porque el `.bat` de Ghidra corta ahí: `\x20`, `\x28`, `\x29`, `\x2b` y `.*` en lugar de `[^;]` o `{0,60}`).

## La luz de los personajes (`0x41AA80`, `0x41B100`)

Cada luz es un objeto con: tipo (+0x00), encendida (+0x08), posición (+0x0C), color (+0x24), radios al cuadrado (+0x44, +0x48) y, para el foco, una matriz de vista (+0x94 a +0xCC) y una distancia focal (+0xD4). La luz se suma, vértice a vértice, al color de cada vértice del modelo (búfer `+0x100` del modelo, 32 bytes por vértice: normal y color). Constantes leídas del ejecutable:

- **Tipo 0 (luz puntual con dos radios):** con la distancia al cuadrado d², fuerza 1 si d² < interior; si no, (exterior − d²) / (exterior − interior); se apaga a partir del exterior (+ 900 de margen). Luego × lo que la cara mira a la luz (normal · dirección), × color.
- **Tipo 2 (luz puntual con un radio):** fuerza (r² − d²) / r², × 2, × normal · dirección, × color.
- **Tipo 1 (el foco de la linterna):** el vértice se pasa a la vista de la linterna (profundidad p y desplazamiento proyectado con la focal). Solo alumbra si 25 < p < alcance y si el desplazamiento² < cono². **Alcance 625** (6,25 m), **1875** si la marca `0x505730` está puesta; **cono² 10000**, **1250** si la marca `0x505731` está puesta (haz estrecho). Fuerza = 2 × (1 − desplazamiento²/cono² − p/alcance); solo en las caras que miran a la linterna, × lo que la miran × 2 × color. `0x41B100` además la atenúa cerca de los bordes de un rectángulo de la pantalla (× 0,05 por píxel fuera).
- Las escenas de cámara traen dos grupos "posición + intensidad + 20" con la posición muy alta (unos 12 m) y compartida por las cámaras de una sala: seguramente la luz de la escena (la luna) para iluminar y dar sombra a los personajes (`docs/FORMATOS.md`).
- Los colores del trozo `HAND` de las manos con linterna (`dcdcff`, `ffffff`, `6464ff`...) podrían ser los de su luz.
- Sin usar aún en el visor: hace falta la lógica del juego (dónde está y adónde apunta la linterna de quien la lleva).

## Nombres originales y textos (2026-11-23)

- **Clases del motor** (textos del ejecutable, para usar sus nombres): `GE_Object`, `GE_VisualObject`, `GE_Actor`, `GE_Character`, `GE_Hero`, `GE_Carnby`, `GE_Aline`, `GE_Level`, `GE_LevelManager`, `GE_LevelObject`, `GE_Light`, `GE_DynamicLight`, `GE_ParticleGenerator`, `GE_LeftHandManager` (la mano izquierda: linterna...), `GE_WeaponManager`, armas `GE_BasicWeapon`, `GE_ImmediateWeapon`, `GE_ProjectileWeapon`, `GE_PlasmaWeapon`, `GE_LightningWeapon`, `GE_PulsarWeapon`, proyectiles `GE_Projectile`, `GE_Grenade`, `GE_Missile`, `GE_PulsarMissile`. Y `G3D_ResourceBank` para los bancos `G3DB`.
- **Monstruos** (de sus mensajes de depuración "<NOMBRE> STATE: %d"): `ARACHEUR`, `ARACHNOCIDE`, `HYBRIDE`, `PHOCOMELE`, `PHOTOSAURE`, `RATLUX`, `ROTWEILER`, `TRICERAPTOR`, `NPC STANDARD`, y las transformaciones `OBED_TRANS` y `HOWARD_TRANS`.
- Fecha de compilación: `May 31 2001 16:31:23`.
- **Textos del juego en español**: dentro del propio ejecutable (la versión de GOG es la española), en Latin-1, en `.rdata` (por ejemplo, desde `0x4C6B6C`: "Pulsa INTRO para continuar.", "¡GUARDAR o CARGAR!", "Cargando..."; desde `0x4C7638`: "CONFIGURAR CONTROLES", "ARRIBA", "LINTERNA", "CORRER", "APUNTAR"...). `Text.csv` solo tiene 4 mensajes de consola en inglés (restos de Dreamcast). `Dialog\` son las voces en WAV (`AL_`, `CA_`, `TR_`...); no se han encontrado subtítulos.

## Pendiente de averiguar

- Formato de `G3DB` (salvo la escena de cámara), `LMVR` (segunda paleta), `DRAM`, `VRM`, `AFS`, `cs\` y `.def`.
- Cómo dibuja la linterna: la forma de la luz sobre la malla de la escena (`G3DB`). Ya se sabe que los fondos tienen máscara (bit 0x80) y malla 3D.

## Vídeos (2026-11-24)

| Dirección | Qué hace |
|---|---|
| `0x4811CF` | Toca el vídeo número N de `MOVIES.TXT` (`\MOVIES\<nombre>.BIK`) con unas marcas (`0x40800000` en los que se pueden saltar, 0 en los logos la primera vez, `0x800000` en el final). |
| `0x480EE0` | Lee `MOVIES.TXT`. |
| `0x4798C9` | Bucle principal del programa: al arrancar los vídeos 0 y 56 (con marcas 0 la primera vez; si `DAT_005c494c & 2`, con `0x40800000`), el 3, y el título (`0x480AC6`). Al elegir personaje, el vídeo 5 (Carnby) o 4 (Aline) y la partida en el nivel `0x84 - 0x66` = 30 (Carnby) o `0x84` = 132 (Aline), con `CARNBY`/`ALINE`. Al acabar: el 1 (final) o `\DOC\GAMEOVER`. |
| `0x480AC6` | Título: si se elige Nueva partida (`FUN_00403e00() == 1`) y no está la marca `0x200`, el vídeo 2 (intro) antes de elegir personaje. |
| `0x463908` | Función 42 de los scripts: fundido (`0x418845`), reacción del personaje (`0x43540E`), el vídeo del argumento 0 con las marcas del 4, y vuelve a cargar la cámara. |
| `0x480E32`, `0x480ECB` | Audios de `xa\XA%02d.XA` y si han acabado (sin estudiar). |

## Escenas de radio, voces y resplandor (2026-11-24)

| Dirección | Qué hace |
|---|---|
| `0x465B93` | Función 158 de los scripts: `FUN_0044eaa9` guarda el número de script (`+0x2BEC`), carga `radio\N` en `+0x1FD4` y arranca la escena (`+0x1EBC`). |
| `0x430D7A`, `0x431007` | Empezar una escena y ejecutar un bloque (órdenes con `0x8000`: vigilantes, "out of watchers!"). |
| `0x430DFF` | Un fotograma de la escena: los vigilantes que acaban arrancan su bloque; sin ninguno, acaba. |
| `0x44DA0A` | Paso del juego: al acabar la escena, el script `+0x2BEC`. |
| `0x4C5B68` | Tabla de las 54 órdenes (empezar, ¿sigue?). |
| `0x437D10`, `0x436037` | Héroe llevado por el juego (estado 3, botones guardados en `+0x394`) / devuelto al jugador. Funciones 124 y 77 de los scripts. |
| `0x437EBC` | Botones del héroe llevado (función 1). |
| `0x437F7D`, `0x438014`, `0x438147` | Ir a un punto marcado, girarse hacia él (en un tiempo). |
| `0x43838F`, `0x438759` | Animación pedida por identificador hasta su final (`setModelAnimation`). |
| `0x435751` | Linterna encendida o apagada (solo con `HAND +0x14`). |
| `0x480E32` | Tocar la voz N: `Xa.txt` → `XA%02d.XA` y hueco. |
| `0x44948D` | Cargar la mano izquierda: modelo, foco y resplandor (`FLARE`, `GLOW`, `LINE`) si `HAND +0x14`. |
| `0x414AF5`, `0x414FA9` | El resplandor: crear y dibujar (96 px; la raya si la fuerza pasa de 0,8984375). |
| `0x43420E` | Paso del héroe: `FUN_00417dcb` (eje +y del hueso 9 en la cámara) da la fuerza del resplandor. |
| `0x416A87` | Crea objetos de dibujo por tipo (4 resplandor, 7 partículas...). |
| `0x434409` | Eventos de la animación del héroe: 100 pasos, 0x65 sonidos, **0x66 mano izquierda** (2: guarda la mano y pone la 2, la radio, `0x4497BD`; otro: vuelve la guardada y su luz, `0x4497D4`). |
| `0x435447`, `0x435494` | Función 86 de los scripts: vibración del mando unos fotogramas (no es la boca). |
| `0x438410`, `0x438520` | El héroe toma el modelo de una cosa con modelo durante 2 s (`+0x3D5`, `+0x3D8`). |
| `0x44F450`, `0x44F5B5`, `0x44F695` | Las "cosas" (lista `+0x3C4` del nivel, 0x50 bytes): partículas de tipo 7, visibles `+0x34` ms (función 117, orden 20). |
| `0x41F0B7` | Emisor de partículas (tipo 7): N partículas de 0xC4 bytes, registro de 0x60 bytes del banco. |

## Partículas, lluvia y relámpagos (2026-11-24)

| Dirección | Qué hace |
|---|---|
| `0x41F0B7` | Emisor (objeto de tipo 7) de un sistema: copia sus campos, crea sus partículas. |
| `0x41F480`, `0x41F5EB` | Soltar partículas un tiempo; un fotograma (mueve las vivas; suelta mientras `+0x1C` < `+0x20`). |
| `0x41F41A`, `0x41F42F` | Seguir soltando (lo llama cada fotograma una cosa visible); parar. |
| `0x41E51A`, `0x41E632` | Nacer una partícula (del prototipo en `+0x38`); moverla y envejecerla. |
| `0x41FB29` | Los 12 tipos de efecto. `0x420740`, `0x41E080`, `0x41DFD0`, `0x41DF60`, `0x420870`: leer sus claves. |
| `0x41E6DF`, `0x41F6D4` | Dibujar una partícula (punto, plana, raya) y un emisor. |
| `0x4227DF` | Paso de la escena: resplandores, emisores, lluvia. |
| `0x44F5B5`, `0x44F695` | Las cosas del nivel: crear (tipo 7) y su tiempo visible (`+0x34`). |
| `0x44A39D` | Si el nivel tiene la marca `0x02`, crea la lluvia (tipo 9, `0x415CCD`) con el sistema `+0x3F1`. `0x44A179`: lluvia sí/no (función 105). |
| `0x415E9C`, `0x415DD7` | La lluvia: su cuadrado (500 delante de la cámara, 600 de lado, a altura 430) y una gota cada 10 ms en un punto al azar. |
| `0x423059` | Función 174: una luz que llega a toda la escena, fuerza = argumento × 2, 100 ms (relámpago). |
| `0x4891E0` | Los modos de mezcla (alfa 0x7F, 0xFF, 0xFF, 0x3F). |
| `0x417376`, `0x418845` | Orden 12 de las escenas: las bandas negras de cine. |
| `0x47AE93` | La rueda del título; `0x47F633` las fichas de elegir personaje; `0x4802C0` su oscurecido. |

## Luz de los modelos y sombra (2026-11-24)

| Dirección | Qué hace |
|---|---|
| `0x426070` | Luz de los vértices de un modelo: parte de la luz ambiente del juego de luces de la escena (`+0x6C0 + 0x104`) y suma cada luz (`0x41B100` y otras: tipo 0 puntual con radios interior y exterior, 1 la linterna, 2 la de toda la escena). |
| `0x44A39D` → `0x418696` | La luz ambiente es la de la cabecera del nivel: `+0x24` si el nivel está iluminado (`isLevelLit`), `+0x28` si no. |
| `0x41BD50` | La escena de cámara ("LightMesh", versión `0xE6BE50`): copia sus luces (`+0x20`, `+0x34`). |
| `0x423DC0`, `0x489E00`, `0x48A1A0` | La sombra: extruye la malla de sombra de cada modelo desde la luz de la escena y la dibuja con plantilla. |

## Pantalla de apuntar (2026-11-24)

| Dirección | Qué hace |
|---|---|
| `0x46D920` | Cada fotograma da al objeto de las balas (tipo 0xD) las balas del arma (o su porcentaje en las de chorro) y un valor del arma (`0x469F91`). |
| `0x41619A`, `0x4161ED` | Objeto de las balas: la textura `BALLES` del arma; dibuja desde x 32, 59 píxeles sobre el borde de abajo, una por bala (con valor 1, una barra con marcas de 200 píxeles). |
| `0x43A655` | Paso del héroe: con la tecla de apuntar (`+0x92`) y sin escena, muestra la vida (tipo 10) y las balas. |
| `0x41439B`, `0x414422`, `0x4144C3` | La barra de vida: tramo de color y velocidad por la vida (<26, <76, <100, 100) y un pulso que cruza 146×8 en x = ancho − 198, y = alto − 42. |
