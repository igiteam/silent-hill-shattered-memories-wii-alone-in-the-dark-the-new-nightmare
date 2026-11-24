# Las funciones de los scripts

Las 203 funciones del juego que llaman los scripts de los niveles (instrucción 0x12 de la máquina; tabla de `alone4.exe` español en `0x4C7878`, `FUN_004667ac`). Ver la máquina en `docs/FORMATOS.md` ("Los scripts") y su código en `src/scripts/`.

**Cómo leerlas**: los argumentos están en la pila desde su cima ("0" es el primero, "4" el segundo...); si la función da un resultado, lo escribe en el hueco que el script dejó después de sus argumentos (columna "resultado"). Lo que devuelve la función dice si el script sigue (0), termina (1), espera al fotograma siguiente (2) o repite la llamada en el siguiente (3: esperar).

**Objetos**: muchas funciones devuelven un objeto del juego (el juego, el estado de la partida, el nivel, el personaje, un objeto de una lista del nivel) y otras lo reciben como primer argumento. El motor da en su lugar una ficha (tipo y número) que esas funciones entienden.

| Objeto | Cómo se consigue | Qué es |
|---|---|---|
| Juego | 26 | el objeto principal |
| Estado de la partida | 22 | lo que se recuerda entre niveles (juego + 0x100): por nivel un byte (`+0x274`: valor de 4 bits, 0x10 iluminado, 0x20 se puede iluminar), sucesos de la historia (bits, `+0x1080`), una lista ordenada de hasta 16 números (`+0x104C`), dos valores (`+0x1074`), el sonido (`+0x1090`) |
| Nivel | 33 | el nivel: cámara actual (`+0x20`), personaje (`+0x38`), las listas de objetos, el mapa de choques (`+0x5C`) |
| Personaje | 71 | el que lleva el jugador |
| Actores (0xD0) | 65 | lista `+0x3B8` del `DRAM` (¿personajes y monstruos?) |
| Objetos de 0x9C | 37 | lista `+0x3BC` del `DRAM` (0xB4 bytes en el archivo) |
| Objetos de 0x2AC | 166 | lista `+0x3C0` (0x40 en el archivo), con estados 4, 5, 6 |
| Objetos de 0x50 | 116 | lista `+0x3C4` (0x1C en el archivo) |
| Objetos de 0x18 | 129 | lista `+0x3C8` (0x10 en el archivo) |

**Uso**: en cuántos niveles y cuántas veces las llaman los scripts de Carnby (de todos los niveles con su `Carnby01`). Los scripts usan **124 funciones distintas**. Hechas o en parte: **87** de 203, que son **6780 de 8765 llamadas** (77%).

| Nº | Dirección | Niveles | Llamadas | Argumentos | Resultado | Estado | Qué hace |
|---|---|---|---|---|---|---|---|
| 0 | `462B60` | 6 | 28 | 0 | — |  | Mensaje de depuración con un texto de la pila (FUN_004666a0) |
| 1 | `462CA3` | 9 | 17 | 0,4,8,12 | — | ✔ | Botones del héroe llevado por el juego: avanzar, atrás, correr (FUN_00437ebc, +0x394) |
| 2 | `462D0F` | 2 | 4 | 0,4 | — |  | Objeto de 0x9C: algo con un número (FUN_00468db1) |
| 3 | `462D39` | 74 | 828 | 0 | — | hecha | Poner la cámara N (FUN_0044bd11), salvo si está bloqueada (DAT_00508160) |
| 4 | `462D81` | 74 | 165 | 0 | — | hecha | Ir al nivel N (FUN_0044dcef → FUN_0044dd12) |
| 5 | `462DB1` | 0 | 0 | 0 | — |  | Mensaje de depuración con formato (sprintf) |
| 6 | `462F8E` | 4 | 5 | 0 | 4 | hecha | Milisegundos que le quedan al temporizador N del banco 0 (0 si se acabó; FUN_0044e335) |
| 7 | `462FCF` | 0 | 0 | 0 | 4 | hecha | Igual del banco 1 (FUN_0044e3fe) |
| 8 | `463010` | 12 | 193 | 0,4,136,140 | — | hecha | Imagen a pantalla completa (su nombre, "\doc\101_51_A", en 8; cuánto tiempo en 0x88, ms, −1 hasta una tecla; FUN_00487038 y fundidos FUN_00418845/4a456b): el juego se para y el script espera a que se cierre |
| 9 | `4630F2` | 21 | 37 | 0,4,8 | — | hecha | Estado: añadir un número con su rango a la lista ordenada (hasta 16; FUN_00432282) |
| 10 | `463135` | 23 | 56 | 0,4 | — | hecha | Estado: quitar un número de la lista ordenada (FUN_004323a2) |
| 11 | `463167` | 0 | 0 | — | — |  | Personaje en modo 3 y algo del juego (FUN_0044e02c): ¿fin de partida? |
| 12 | `4631B4` | 0 | 0 | — | — |  | Igual con FUN_0044e0b5 |
| 13 | `4631FF` | 9 | 15 | 0,4 | — | hecha | Inventario (el estado de la partida, función 22): coger uno del objeto N (FUN_00448833) |
| 14 | `463233` | 0 | 0 | 0 | — | hecha | Parar el temporizador N del banco 0 (FUN_0044e371) |
| 15 | `463263` | 0 | 0 | 0 | — | hecha | Parar el temporizador N del banco 1 (FUN_0044e43a) |
| 16 | `463293` | 74 | 218 | 0 | — | se salta | Algo de dibujo con un número (FUN_00418966/418845/419571): ¿fundido? |
| 17 | `4632C5` | 2 | 2 | 0 | — |  | Juego: +0x1ea0 = N (FUN_0044d344): ¿fin del nivel o de la partida? |
| 18 | `4632F5` | 0 | 0 | 128 | — |  | Mensaje con un texto de la pila (FUN_0043ce5f) |
| 19 | `463358` | 0 | 0 | 0 | — |  | Juego: FUN_0044e0eb(N) |
| 20 | `463388` | 4 | 42 | 0 | 4 | hecha | Número al azar de 0 a N−1 |
| 21 | `4633B3` | 1 | 8 | 0 | 4 |  | Ángulo hacia el que mira un objeto, en grados (FUN_0043a35f) |
| 22 | `4633FF` | 61 | 284 | — | 0 | hecha | El estado de la partida (juego + 0x100) |
| 23 | `463436` | 43 | 101 | 0,4 | — | hecha | Como la 13 y lo dice ("You take..." de `item\IT3D_N`, FUN_0044d543); el personaje reacciona (FUN_0043540e, sin hacer) |
| 24 | `463499` | 24 | 36 | 0,4 | — | hecha | Inventario: quitar el objeto N (FUN_00448a5a) |
| 25 | `4634C3` | 18 | 38 | 0,4 | 8 | hecha | Inventario: ¿tiene el objeto N? (FUN_00448b24) |
| 26 | `4634FD` | 47 | 177 | — | 0 | hecha | El juego |
| 27 | `46352D` | 1 | 1 | 0 | — | hecha | Estado: vaciar la lista ordenada |
| 28 | `46354C` | 0 | 0 | — | 8 | hecha | Pone 0 en su resultado (no hace nada más) |
| 29 | `46355F` | 0 | 0 | — | — | hecha | No hace nada |
| 30 | `463566` | 0 | 0 | — | 8 | hecha | Pone 0 en su resultado |
| 31 | `463579` | 19 | 30 | 0,4,8 | — | hecha | Estado: valor (4 bits) del nivel N |
| 32 | `4635AE` | 0 | 0 | 0,4 | 8 | hecha | Estado: leer ese valor |
| 33 | `4635E3` | 60 | 302 | — | 0 | hecha | El nivel |
| 34 | `463613` | 11 | 37 | 0 | 4 | hecha | Nivel: ¿está iluminado el nivel actual? |
| 35 | `463642` | 17 | 31 | 0 | — | en parte | Nivel: encender/apagar la luz del nivel actual (el juego además rehace luces y cámara) |
| 36 | `463661` | 20 | 146 | 0,4,8 | — | hecha | Nivel: activar/desactivar un grupo del mapa de choques (puertas, rejas) |
| 37 | `46369C` | 52 | 324 | 0 | 4 | hecha | Nivel: el objeto N de la lista de 0x9C (+0x3BC del DRAM) |
| 38 | `4636DE` | 50 | 309 | 0,4 | — | hecha | Objeto de 0x9C: mostrar u ocultar (su modelo, su sombra y sus otras partes; FUN_00468ad8, `+0x94`) |
| 39 | `46370E` | 31 | 60 | 0 | 4 | hecha | Objeto de 0x9C: ¿se ve? (FUN_00468c1b, `+0x94`) |
| 40 | `46373D` | 8 | 24 | 0,4 | — | hecha | Objeto de 0x9C: poner una animación y animarlo (FUN_00468ca7: `+0x48` y `+0x96`; la animación aún no se reproduce) |
| 41 | `463767` | 2 | 2 | 0 | — |  | Con un nombre: carga algo (archivos) y dibuja |
| 42 | `463908` | 15 | 22 | 0,4 | — | ✔ | Toca el vídeo número arg0 de `MOVIES.TXT` (marcas arg4) y espera a que acabe; antes fundido y reacción del personaje (`FUN_0043540e`, sin hacer) |
| 43 | `4639B3` | 36 | 199 | 0 | — | hecha | Esperar N milisegundos |
| 44 | `463A01` | 5 | 9 | 0,4 | — |  | Nivel: ajustes de niebla/luz de la lista +0x3D4 del DRAM (FUN_0044a113) |
| 45 | `463A2B` | 64 | 796 | 0,4,8,12,16 | 20 | hecha | Sonido: tocar (5 argumentos), devuelve su número (FUN_0049a4dd) |
| 46 | `463AA3` | 0 | 0 | 0,4,8,12,16,20,24,28,32,36,40,44 | 48 |  | Sonido al azar entre varios |
| 47 | `463BC0` | 0 | 0 | 0 | — | hecha | Sonido: parar N |
| 48 | `463BE3` | 0 | 0 | — | — | hecha | Sonido: parar todos |
| 49 | `463BEF` | 0 | 0 | — | — | hecha | No hace nada |
| 50 | `463BF6` | 0 | 0 | — | — | hecha | No hace nada |
| 51 | `463BFD` | 24 | 99 | 0,4,8 | — | hecha | Canal de música: elegir su sección y cuántas veces (FUN_00499589) |
| 52 | `463CB1` | 52 | 110 | 0 | — | hecha | Canal de música: pararlo (FUN_00499624) |
| 53 | `463D30` | 20 | 113 | 0,4,8 | — | hecha | Canal de música: su volumen izquierdo y derecho, al momento (FUN_004998eb; al cambiar de cámara) |
| 54 | `463DE0` | 0 | 0 | — | — | hecha | No hace nada |
| 55 | `463DE7` | 0 | 0 | — | — | hecha | No hace nada |
| 56 | `463DEE` | 0 | 0 | 0,4 | — |  | Sonido de ambiente: otro valor |
| 57 | `463E82` | 21 | 44 | 0 | — |  | Música: elegir (estado +0x114) |
| 58 | `463ED2` | 20 | 38 | 0 | — |  | Música: otro valor (+0x110) |
| 59 | `463F22` | 0 | 0 | 0,4 | — |  | Música: dos valores (+0x116) |
| 60 | `463F89` | 22 | 29 | 0 | 4 |  | Nivel: +0xF0 (resultado) |
| 61 | `463FB3` | 9 | 21 | 4 | — |  | Pantalla de textos: activar algo (FUN_0043c89f) |
| 62 | `463FE8` | 0 | 0 | — | 256 |  | Pregunta con textos de la pila (FUN_0043ce5f), resultado |
| 63 | `464069` | 15 | 136 | 0,4 | — | hecha | Igual hasta una tecla, y el personaje reacciona (FUN_0043540e, sin hacer) |
| 64 | `464100` | 0 | 0 | 128 | — |  | Mensaje con un texto de la pila |
| 65 | `464163` | 28 | 260 | 0 | 4 | hecha | Nivel: el actor N (lista de 0xD0, +0x3B8 del DRAM) |
| 66 | `4641A5` | 27 | 327 | 0,4 | — | hecha | Actor: poner su comportamiento (`+0x268`, FUN_00452586) |
| 67 | `4641D9` | 0 | 0 | 0 | 4 | hecha | Estado: ¿está iluminado el nivel N? |
| 68 | `464227` | 0 | 0 | 0,4 | — | en parte | Estado: iluminar el nivel N (el juego rehace luces y cámara si es el actual) |
| 69 | `464297` | 0 | 0 | 0 | 4 | hecha | Estado: ¿se puede iluminar el nivel N? |
| 70 | `4642E5` | 0 | 0 | 0,4 | — | en parte | Estado: que se pueda iluminar el nivel N |
| 71 | `464355` | 38 | 154 | — | 0 | hecha | El personaje que lleva el jugador |
| 72 | `46438C` | 14 | 26 | 0,4 | — | hecha | Personaje: ponerlo en el punto marcado N (+0x3D8 del DRAM) |
| 73 | `4643B6` | 0 | 0 | 0 | 4 |  | Personaje: ¿linterna encendida? |
| 74 | `4643ED` | 3 | 3 | 0,4 | 8 |  | Personaje: ¿cerca del punto marcado N? |
| 75 | `46442F` | 47 | 278 | — | — | hecha | Esperar al fotograma siguiente |
| 76 | `464439` | 6 | 12 | 0,4,8,12,16,20,24,28,32 | — |  | Personaje: mover/girar con 9 valores (FUN_00437870) |
| 77 | `4644FE` | 24 | 56 | 0 | — | ✔ | Personaje: devolverle el control (estado 0) |
| 78 | `46451D` | 6 | 10 | 0 | — |  | Personaje: estado 2 |
| 79 | `46453C` | 2 | 6 | 0 | 4 |  | Botones del mando pulsados (FUN_004a42d4/4a4613) |
| 80 | `464597` | 0 | 0 | 0 | 4 |  | Objeto: método 0x1C (resultado) |
| 81 | `4645C4` | 2 | 5 | 0 | 4 |  | Objeto: método 0x1C (resultado) |
| 82 | `4645F1` | 2 | 6 | 0,4 | — |  | Actor: FUN_00452697 |
| 83 | `46461B` | 1 | 1 | 0 | — | hecha | No hace nada |
| 84 | `464632` | 7 | 43 | 0 | — |  | FUN_00480e32(N): ¿vídeo? |
| 85 | `464655` | 3 | 36 | — | 0 |  | FUN_00480ecb (resultado): ¿vídeo terminado? |
| 86 | `46467A` | 22 | 50 | 0,4,8,12 | — |  | Personaje: algo con tiempo (FUN_00435447) |
| 87 | `4646F9` | 16 | 78 | 0 | 4 | hecha | Nivel: la cámara mostrada |
| 88 | `464723` | 33 | 95 | 0,4 | 8 | hecha | Temporizador del banco 0 (FUN_0044e2d4): script numerado en 0, milisegundos en 4; su número (0..7, −1 si no hay sitio) en 8. Al acabarse arranca ese script (FUN_0044e12b) |
| 89 | `464775` | 0 | 0 | 0,4 | 8 | hecha | Igual en el banco 1 (FUN_0044e39d) |
| 90 | `4647C7` | 30 | 55 | 0 | — | hecha | Lanzar el script del nivel con ese número (sin parámetros) |
| 91 | `4647FB` | 0 | 0 | — | — |  | Juego: FUN_0044dfea con un texto de la pila |
| 92 | `46482C` | 3 | 4 | — | — |  | Juego: FUN_0044e014 |
| 93 | `464849` | 53 | 103 | — | — | hecha | Cargar un archivo de `midi\` por su nombre (FUN_004982cf) |
| 94 | `4648E8` | 59 | 190 | 128,132,136,140 | 144 | hecha | Tocar una secuencia por su nombre (izquierdo 0x80, derecho 0x84, sección 0x88, veces 0x8C) en un canal del estado, cuyo número va a 0x90 (FUN_004992f7, FUN_00433122) |
| 95 | `464A0D` | 24 | 69 | 0,4,8,12 | — | hecha | Canal de música: llevar su volumen a izquierdo y derecho en N segundos (FUN_00499b26) |
| 96 | `464B3E` | 0 | 0 | 0 | 4 |  | Actor: consulta (FUN_004525fb) |
| 97 | `464B68` | 0 | 0 | 0,4 | — | hecha | Objeto de 0x9C: mostrar u ocultar su sombra (FUN_00468c2f, `+0x95`) |
| 98 | `464B98` | 1 | 2 | 0,4 | — |  | Actor: poner un número (FUN_0045277d) |
| 99 | `464BC2` | 0 | 0 | 0 | 4 |  | Actor: leerlo |
| 100 | `464BEC` | 0 | 0 | 0,4 | — |  | Actor: poner un ángulo (grados → 4096 por vuelta) |
| 101 | `464C21` | 0 | 0 | 0 | 4 |  | Actor: leer un ángulo |
| 102 | `464C5D` | 1 | 2 | 0,4 | — |  | Actor: poner otro ángulo |
| 103 | `464C92` | 0 | 0 | 0 | 4 |  | Actor: leerlo |
| 104 | `464CCE` | 8 | 56 | 0 | 132 |  | Con un número y un nombre: imagen o documento (FUN_00473a1a) |
| 105 | `464D7D` | 9 | 14 | 0,4 | — | ✔ | Lluvia del nivel sí o no (FUN_0044a179) |
| 106 | `464DAD` | 0 | 0 | 0 | 4 |  | Nivel: leer esa marca |
| 107 | `464DDC` | 37 | 81 | 0,4 | — | hecha | Mensaje con el texto N del nivel (el juego lo muestra con FUN_0043ce5f; el segundo argumento sin estudiar) |
| 108 | `464E4D` | 21 | 54 | 0,4 | 8 | hecha | Pregunta con el texto N y sus respuestas ("Yes@No"), devuelve la elegida |
| 109 | `464EDB` | 14 | 127 | 0,4 | — | hecha | Mensaje con el texto N del nivel (como la 107 con otra marca) |
| 110 | `464F4C` | 3 | 4 | 0 | — |  | Juego: FUN_0044dfea con el texto N del nivel |
| 111 | `464F9D` | 10 | 16 | — | — |  | Pantalla de textos: FUN_0043cbc2 con un texto de la pila |
| 112 | `465008` | 20 | 25 | — | — |  | Personaje en modo 3 y FUN_0044eaa9 con un texto |
| 113 | `465072` | 1 | 6 | 0,4 | 136 |  | Como la 104 con dos números |
| 114 | `465136` | 0 | 0 | — | — | hecha | No hace nada |
| 115 | `46513D` | 4 | 8 | 0,4,8 | — |  | Actor: FUN_004526c9 con dos valores |
| 116 | `465172` | 9 | 75 | 0 | 4 | hecha | Nivel: el objeto N de la lista de 0x50 (+0x3C4 del DRAM) |
| 117 | `4651B4` | 9 | 36 | 0,4 | — | ✔ | Muestra una cosa (sistema de partículas) N ms; −1 siempre (FUN_0044f81d) |
| 118 | `4651DE` | 5 | 12 | 0 | — |  | Objeto de 0x50: FUN_0044f833 |
| 119 | `4651FD` | 0 | 0 | 0,4,8,12,16 | — |  | Objeto de 0x50: FUN_0044f848 con 4 valores |
| 120 | `465248` | 0 | 0 | 0 | — |  | Objeto de 0x50: FUN_0044f896 |
| 121 | `465267` | 0 | 0 | 0,4 | — |  | Actor: FUN_0045460b |
| 122 | `465291` | 0 | 0 | 0,4,8,12 | — |  | Personaje: color (3 valores; FUN_00435701) |
| 123 | `4652D1` | 0 | 0 | 0,4 | — |  | FUN_00418689 (encender/apagar) |
| 124 | `465301` | 15 | 34 | 0 | — | ✔ | Personaje: estado 3 (lo lleva el script) |
| 125 | `465320` | 1 | 6 | — | 0 |  | FUN_00473a10 (resultado) |
| 126 | `465337` | 1 | 1 | 0,4,8 | — |  | Personaje: FUN_0043838f |
| 127 | `465388` | 5 | 18 | 0 | 4 |  | Personaje: consulta +0x3B4 |
| 128 | `4653B7` | 22 | 27 | 0 | 4 | en parte | Nivel: ¿ningún actor ocupado? (sin actores aún: sí) |
| 129 | `4653E6` | 4 | 10 | 0 | 4 | hecha | Nivel: el objeto N de la lista de 0x18 (+0x3C8 del DRAM) |
| 130 | `465428` | 4 | 8 | 0,4 | — |  | Objeto de 0x18: FUN_0044fa60 |
| 131 | `465458` | 2 | 3 | 0 | — |  | Objeto de 0x18: FUN_0044faa5 |
| 132 | `465477` | 2 | 2 | 0,4,8 | — |  | Objeto de 0x9C: FUN_00468e41 |
| 133 | `4654AC` | 1 | 2 | 0,4 | — |  | Objeto de 0x9C: FUN_00468e76 |
| 134 | `4654D6` | 0 | 0 | 0,4 | — |  | Objeto de 0x9C: FUN_00468e5b |
| 135 | `465500` | 0 | 0 | 0,4 | — |  | Objeto de 0x9C: FUN_00468e8e |
| 136 | `46552A` | 0 | 0 | 0 | — |  | Objeto de 0x9C: FUN_00468ec4 |
| 137 | `465549` | 5 | 8 | 0 | 4 | hecha | El parámetro N con el que se lanzó el script |
| 138 | `465577` | 0 | 0 | 0 | — | hecha | Lanzar el script del nivel con ese número, con 1 parámetro |
| 139 | `4655EA` | 0 | 0 | 0 | — | hecha | Ídem con 2 parámetros |
| 140 | `46565D` | 0 | 0 | 0 | — | hecha | Ídem con 3 |
| 141 | `4656D0` | 0 | 0 | 0 | — | hecha | Ídem con 4 |
| 142 | `465743` | 0 | 0 | 0 | — | hecha | Ídem con 5 |
| 143 | `4657B6` | 0 | 0 | 0 | — | hecha | Ídem con 6 |
| 144 | `465829` | 0 | 0 | 0 | — | hecha | Ídem con 7 |
| 145 | `46589C` | 0 | 0 | 0 | — | hecha | Ídem con 8 |
| 146 | `46590F` | 0 | 0 | 128 | — |  | Mensaje con un texto de la pila (FUN_0043d661) |
| 147 | `46596A` | 0 | 0 | 0 | — |  | Dibujo (FUN_00418845, FUN_004a456b) |
| 148 | `46599B` | 5 | 10 | 0,4 | — |  | Actor: FUN_004546c4 |
| 149 | `4659C5` | 5 | 18 | 0,4,8,12 | — |  | Personaje: FUN_0043838f con más valores |
| 150 | `465A29` | 0 | 0 | 0 | — |  | Sonido: FUN_0049972e |
| 151 | `465A43` | 0 | 0 | — | 0 |  | Sonido: FUN_00499885 (resultado) |
| 152 | `465A5F` | 0 | 0 | 0 | — |  | FUN_004a165a: ¿vibración del mando? |
| 153 | `465A79` | 0 | 0 | — | 0 |  | FUN_004a1650 (resultado) |
| 154 | `465A90` | 30 | 56 | 0,4 | — | hecha | Mensaje con el texto N del nivel (FUN_0043d661) |
| 155 | `465AF9` | 1 | 2 | 0,4 | — |  | Personaje: +0x334 |
| 156 | `465B23` | 1 | 1 | 0,4,8 | 12 |  | Personaje: FUN_004357f0 (resultado) |
| 157 | `465B68` | 5 | 13 | 0,4 | — |  | FUN_00417376 (encender/apagar) |
| 158 | `465B93` | 14 | 30 | 128 | — | ✔ | Escena de radio (`\radio\N`); al acabar, el script con número de 0x80 |
| 159 | `465BE7` | 0 | 0 | 4 | — |  | Juego: FUN_004493c7 / FUN_0044970b |
| 160 | `465C42` | 22 | 29 | 0,4 | — | hecha | Inventario: anotar un número una vez (lista de `+0x150`; para qué, sin saber) |
| 161 | `465C74` | 1 | 3 | 0,4,8 | — | hecha | Estado: poner dos valores (+0x1074, +0x1078) |
| 162 | `465CA9` | 0 | 0 | 0 | 4 | hecha | Estado: leer el primero |
| 163 | `465CD3` | 1 | 4 | 0,4,8 | — |  | Personaje: ir al punto marcado N (FUN_00438014) |
| 164 | `465D18` | 2 | 2 | 0,4,8 | — | hecha | Inventario: poner cuántos hay del objeto N (FUN_00448bfb) |
| 165 | `465D5B` | 0 | 0 | 0,4 | 8 | hecha | Inventario: cuántos hay del objeto N (FUN_00448b73) |
| 166 | `465D98` | 4 | 6 | 0 | 4 | hecha | Nivel: el objeto N de la lista de 0x2AC (+0x3C0 del DRAM) |
| 167 | `465DDA` | 0 | 0 | 0,4,8,12 | — |  | Objeto de 0x2AC: tres marcas (FUN_0042a16d) |
| 168 | `465E46` | 3 | 5 | 0,4 | — |  | Objeto de 0x2AC: encender/apagar (FUN_0042a61e) |
| 169 | `465E80` | 0 | 0 | 0,4 | — |  | Objeto de 0x2AC: estado 4 con un valor |
| 170 | `465EB2` | 0 | 0 | 0,4,8 | — |  | Objeto de 0x2AC: ir al punto marcado (FUN_0042a271) |
| 171 | `465EF5` | 3 | 3 | 0,4 | — |  | Objeto de 0x2AC: ponerse en el punto marcado (FUN_0042a526) |
| 172 | `465F27` | 0 | 0 | 0 | 4 |  | Objeto de 0x2AC: ¿estado 4? |
| 173 | `465F56` | 1 | 1 | 0 | 4 |  | Objeto de 0x9C: consulta (FUN_00468cc7) |
| 174 | `465F85` | 11 | 82 | 0 | — | ✔ | Relámpago: luz en toda la escena, fuerza arg0 × 2, 100 ms (FUN_00423059) |
| 175 | `465FAC` | 4 | 6 | 0,4 | — |  | Objeto de 0x2AC: estado 6 o 0 |
| 176 | `465FDC` | 0 | 0 | 0 | 4 |  | Objeto de 0x2AC: ¿estado 6? |
| 177 | `46600B` | 8 | 17 | 0,4 | — |  | Pantalla: FUN_004194cd |
| 178 | `466043` | 0 | 0 | 0 | 4 |  | Objeto de 0x2AC: ¿estado 5? |
| 179 | `466072` | 19 | 36 | 0,4 | 8 | hecha | Estado: ¿ha pasado ya el suceso N de la historia? |
| 180 | `4660B4` | 19 | 19 | 0,4 | — | hecha | Estado: marcar el suceso N de la historia |
| 181 | `4660E6` | 2 | 2 | 0,4,8 | — |  | Pantalla: FUN_004194e7 con 3 valores |
| 182 | `46619F` | 20 | 72 | 0,4,8 | — | hecha | Como la 23 con una cantidad (argumento 8, al menos 1) |
| 183 | `466235` | 0 | 0 | 0,4,8,12,16,20 | 24 |  | Sonido: tocar (6 argumentos) |
| 184 | `4662C4` | 0 | 0 | 0,4 | — |  | Actor: FUN_00453832 |
| 185 | `4662F4` | 1 | 1 | 0,4,8,12 | — |  | Personaje: ir al punto marcado N con más valores |
| 186 | `466350` | 0 | 0 | 0 | — |  | Marca global DAT_004c809c bit 1 |
| 187 | `466399` | 0 | 0 | 0,4 | — |  | Personaje: FUN_00438410 con un objeto |
| 188 | `4663CB` | 1 | 1 | 0,4 | — |  | FUN_004693da |
| 189 | `4663FB` | 0 | 0 | 0 | — |  | Personaje: FUN_00438624 |
| 190 | `46641A` | 0 | 0 | 0,4 | — |  | FUN_00469418 con un ángulo |
| 191 | `466457` | 0 | 0 | 0,4 | — |  | Actor de 0xD0: FUN_0045146e |
| 192 | `466481` | 28 | 274 | 0,4 | — | hecha | Actor de 0xD0: aparecer (0) o aparecer para actuar (otro valor; órdenes 1 y 2 de su cola, FUN_0045148d) |
| 193 | `4664B1` | 15 | 64 | 0,4 | — | hecha | Actor de 0xD0: irse (órdenes 3 y 4, FUN_00451506) |
| 194 | `4664E1` | 16 | 91 | 0 | — | hecha | Actor de 0xD0: volver a empezar si acabó (orden 6, FUN_0045157f) |
| 195 | `466502` | 0 | 0 | 0 | — | hecha | Actor de 0xD0: morir (orden 5, FUN_004515c7) |
| 196 | `466521` | 1 | 3 | 0 | 4 | hecha | Actor de 0xD0: ¿está presente? (`+0x2B2` == 1) |
| 197 | `466553` | 16 | 91 | 0 | 4 | hecha | Actor de 0xD0: ¿está ausente? (== 0) |
| 198 | `466582` | 23 | 152 | 0 | 4 | hecha | Actor de 0xD0: ¿acabó o murió? (== 2) |
| 199 | `4665B4` | 5 | 24 | 0 | — | hecha | Poner la cámara N (con otra marca para el nivel) |
| 200 | `4665FC` | 4 | 5 | 0 | — |  | Personaje: marca +0x340 |
| 201 | `466638` | 0 | 0 | 0,4 | — |  | FUN_004694be |
| 202 | `466668` | 1 | 2 | — | 4 |  | Juego: FUN_004497ac (resultado) |
