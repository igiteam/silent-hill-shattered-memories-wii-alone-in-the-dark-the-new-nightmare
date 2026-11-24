# Lecciones para otros proyectos

Lo aprendido al rehacer el motor de *Alone in the Dark: The New Nightmare* que puede servir para otros juegos: qué técnica funcionó, cuál no y por qué. Los detalles de este juego están en `FORMATOS.md` y `ALONE4_EXE.md`; aquí va lo que se puede llevar a otro proyecto.

## Cómo investigar el ejecutable

- **Funciona: Ghidra sin ventana con scripts propios** (`tools/ghidra/`). Hay dos búsquedas que resuelven casi todo:
  - **por texto o dirección** (`DecompileByString.java`): las funciones que usan un mensaje del juego ("Loading VIEW_VRAM") o una dirección concreta;
  - **por patrón de código** (`DecompileByCode.java`): decompila todo el ejecutable (unos minutos) y se queda con las funciones cuyo C contiene un patrón.

  Sirve para encontrar quién lee un campo de una estructura, aunque la función no tenga ningún texto.
- **Funciona: buscar por el desplazamiento del campo, no por el valor.** La velocidad de las animaciones no apareció buscando "un float en +0 de una animación de 0x30 bytes". Apareció al ver que el reproductor guarda la animación actual en `+0x124` y buscar `float **` junto a `0x124`. Primero hay que saber **dónde guarda el juego el puntero** y luego buscar quién lo lee.
- **No funciona: escribir los patrones tal cual.** El `.bat` de Ghidra (`analyzeHeadless.bat`) parte los argumentos en espacios, paréntesis, comas, punto y coma, `|` y `%`. Hay que escribir `\x20`, `\x28`, `\x29` y `\x2b`, y usar `.*` en lugar de `[^;]` o `{0,60}`. El error ("Unclosed character class", "Unclosed counted closure") no dice que la culpa es del `.bat`.
- **Funciona: mirar primero los datos con un script y confirmar después en el código.** Volcar la cabecera de varias animaciones conocidas (quieto, andar, correr) y compararlas enseñó en segundos que el float de `+0` era 0 / 4,44 / 11,94: la velocidad. Ghidra lo confirmó después. Al revés (leer el código sin una idea previa) cuesta mucho más.
- **Funciona: leer las constantes del ejecutable.** Un lector mínimo de PE (cabecera, secciones, dirección → posición en el archivo) da el valor de cada `DAT_...`. `0.030003` = 1/33,33 reveló que las velocidades van en "por fotograma de 30 por segundo", y `6.2831855 / 360` que los ángulos se dan en grados.
- **Funciona: nombres en un archivo de texto** (`functionNames.txt`) que un script aplica al proyecto de Ghidra. Es reproducible, se puede revisar en Git y sobrevive a rehacer el proyecto de Ghidra. Conviene una copia del proyecto antes de la primera escritura.
- **Funciona: abrir el juego original** cuando el código no basta para decidir algo (qué botón es cuál, cómo se ve una luz). Siempre sobre una copia de los datos, nunca sobre la instalación.

- **Funciona: buscar estructuras por la forma de sus punteros.** Los bloques con lista de punteros (como `DRAM`) dicen dónde hay un puntero. Una estructura con punteros en +0x20, +0x24, +0x2C y +0x34 se encuentra recorriendo el bloque, aunque no se sepa dónde empieza. Sirvió para encontrar el mapa de choques: se reconoció porque su caja dividida entre 16 daba justo el tamaño de casilla guardado al lado.
- **Pero no te quedes con el "se parece".** Para leerlo en el motor hay que saber de dónde lo saca el juego. Buscar quién usa el bloque (`nivel + 0x28`) junto con el objeto de destino (`+0x5C`) llevó a la función que lee la **cabecera fija** del `DRAM` (`+0x3E4` el mapa, `+0x3B4` las entradas al nivel...). Los 33 archivos donde la casilla estaba vacía eran de una versión vieja que el juego rechaza: sin la función, se habría pensado que el formato varía.

- **Funciona: buscar quién apunta a una función, no solo quién la llama.** Los métodos de objetos y las funciones de los scripts se usan a través de tablas de punteros, y la búsqueda en el código decompilado no los encuentra. `ReferencesTo.java` lista también los datos que apuntan a una dirección y qué función usa esa tabla. Así se encontró el detector de disparadores del personaje, y desde él el mapa de disparadores.
- **Si una orden del juego tiene la forma "función(intérprete)" y lee sus argumentos de una pila, hay un lenguaje de scripts.** Encontrar su bucle (la función más larga entre sus vecinas, con un `switch` por byte) y su tabla de funciones da la llave de toda la lógica de los niveles. Con dos instrucciones entendidas ("meter número" y "llamar a la función k") ya se pueden buscar en los datos sus usos, como `04 N 12 03 00` = "pon la cámara N".
- **No adivines qué decide algo: busca dónde se pide.** El cambio de cámara se pensaba "por zonas de cada cámara". Siguiendo quién llama a `loadCamera` → quién pide la cámara → quién usa esas funciones, salió que lo hacen scripts lanzados al cruzar paredes invisibles.

## Animaciones (transiciones)

- **Los cambios suaves entre animaciones suelen venir en los datos, no en el código.** Aquí cada animación trae su lista de transiciones: duración, velocidad mientras dura y "ventanas" (en qué momentos se permite el cambio y en qué momento empieza la nueva). Copiarlas da exactamente el ritmo del juego, mejor que una mezcla fija igual para todo.
- **Mezcla los valores de los huesos (posición, ángulos, escala), no las matrices.** Por eso conviene separar "qué dice la animación de cada hueso" de "montar el esqueleto": así se pueden mezclar dos poses, y los ángulos por el camino corto.
- **Lee el orden de los campos en el código antes de interpretar los bytes.** La primera lectura de la tabla de transiciones salió llena de basura por suponer el orden; el código decía +4 duración, +6 ventanas, +8 destino, +0xC lista.

## Archivos que sobran

- **Que un archivo esté en los datos no quiere decir que el juego lo use.** Había dos manos derechas vacías (`righthand` y `righthd`); se eligió la que no era, y se veía rara. Buscar en el ejecutable los textos de las rutas (`\weapon\%s\righthd`) dijo cuál carga el juego, y que la otra no aparece en ningún sitio. Antes de elegir entre archivos parecidos, mira qué rutas construye el ejecutable.

## Objetos del nivel

- **Lee primero la función del ejecutable que crea el objeto a partir del registro**: dice qué campo es cada cosa y, sobre todo, qué marca activa cada parte (modelo, sombra, piezas). Al volcar los registros a ojo se leyeron las marcas dos bytes antes de su sitio, y parecía que casi ningún objeto tenía modelo.
- **Para saber qué cámara ve cada objeto, proyecta su posición con cada cámara** en un script, en vez de recorrer las cámaras a mano: así se eligen bien las capturas de prueba.
- **Un solo modelo para todo** (personaje, manos, objetos), con cada pieza llevando su giro, su posición y dónde empiezan sus texturas: añadir objetos no obliga a tocar el dibujo.

- **Si los scripts dejan órdenes en una cola en vez de cambiar el objeto, mira qué hace cada orden según el estado** (`FUN_0045126c`): con eso se pueden atender al momento mientras no existan los cerebros, terminando de golpe los pasos que el objeto haría solo (aparecer, morir), sin que los scripts que esperan esos estados se queden colgados.
- **Una tecla de depuración que muestre todo lo que está oculto al empezar** (J en el visor) ahorra tener que jugar hasta el momento en que un script lo haga aparecer.

- **Los archivos de texto del juego pueden estar cifrados**: `NpcCarac.csv` parecía basura. Buscar su nombre en el ejecutable (en mayúsculas: `\NPCCARAC.CSV`) llevó a la función que lo carga, y la primera función que esta llama con los datos y el tamaño era el descifrado (5 líneas).
- **Para un sistema grande (más de 30 estados por clase), empieza por la clase base**: suele ser corta y da el esqueleto común (esperar, perseguir, atacar, morir). Con eso todas las clases ya funcionan, y luego se afina cada una.
- **Reutiliza el cuerpo del personaje para los actores**: separar de su cerebro la parte que reproduce la animación y mueve el cuerpo (`moveBody`) dio a los monstruos transiciones, movimiento por animación y choques con las paredes sin escribirlo dos veces.

- **Los comentarios de los diseñadores dentro de las tablas valen oro**: `NpcCarac.csv` dice en una celda "2 tiros de escopeta (10 de revólver)" para un monstruo de 10 de vida; con eso se comprobó que el daño de `cweap.csv` es por tiro y no por bala.

- **Los golpes no se calculan por distancia en el cerebro: van marcados en la animación** (eventos con momento, código y valor). Leerlos permitió que cada ataque golpee en su instante y con su alcance, sin inventar tiempos.
- **Antes de suponer que todas las clases usan las mismas animaciones, vuelca los eventos de las animaciones de cada modelo**: el ataque "3" de la clase base no golpeaba a nadie en el HYBRIDE; el volcado enseñó en qué identificador estaba el golpe de cada monstruo.

## Sonido

- **Si una tabla del juego está dentro del ejecutable, encuéntrala por su contenido y léela del ejecutable del jugador**: sus direcciones cambian entre versiones (inglés y español) y no se puede copiar al código (son datos del juego). Un texto único de la tabla (`a_action`, `book\docplate.`) y los punteros a él la sitúan en cualquier versión.
- **Un juego con Miles Sound System puede usar el sintetizador para los efectos**: el sonido se pide como cambio de programa y nota en un canal. Entender eso (nota 60 = altura original, volumen izquierdo y derecho) basta para tocar las muestras con un mezclador sencillo, sin emular el sintetizador.
- **Para probar el sonido sin oírlo, graba la salida de Windows** (*loopback*: `pip install soundcard`, una grabación por dispositivo a la vez) y mira los picos: dice si suena y por qué dispositivo. Así se vio que todo salía por la tele HDMI, que era la salida por defecto.
- **Deja elegir la salida de sonido y recuérdala**: la salida por defecto del sistema no siempre es la que el jugador escucha.
- **Antes de suponer que un juego usa el sintetizador de su biblioteca de sonido, mira qué funciones importa**: este importa de Miles solo las de muestras sueltas, así que su música va con un sintetizador propio y hay que estudiar su formato de instrumentos.
- **SDL3 ya lee los `.wav` en IMA ADPCM** y convierte el formato (`SDL_LoadWAV`, `SDL_ConvertAudioSamples`): no hace falta escribir el decodificador.

## Controles

- **Usa los controles del juego desde el principio, no unos propios.** El juego tiene su menú de controles y guarda la elección del jugador (`keys.bin`); leerlo da las teclas que el jugador ya conoce. Las teclas de depuración del motor, en las que el juego no use (F3, Re Pág...).
- **Guarda las teclas como las guarda el juego** (aquí códigos DirectInput) y traduce en la capa de la ventana: así el archivo del jugador vale tal cual.

## Textos e idiomas

- **Un valor de la cabecera puede ser un puntero a la estructura, no la estructura.** Los textos del nivel salían vacíos por leer la lista directamente en `+0x374`; ahí hay un puntero a ella. En los bloques con lista de punteros se ve rápido: si esa casilla está en la lista de punteros, es un puntero.
- **En las fuentes de mapa de bits, mira si las letras están por su código o empezando por el espacio.** Aquí cada letra está en el lugar de su código ASCII y las dos primeras filas van vacías; al suponer que empezaban por el espacio, todo salía corrido dos filas ("$0" en vez de "Do").
- **Un cambio de idioma de GOG cambia archivos que parecían comunes.** Comparar las dos instalaciones archivo a archivo dijo exactamente qué depende del idioma (voces, vídeos, ejecutable) y qué no (niveles, fuente, `locale`), y que la versión inglesa ni siquiera usa el archivo de textos de la española.
- **Los textos que hay en los datos de un nivel pueden no ser los que se ven.** Aquí los niveles traen textos en inglés de partida, y los de verdad salen de un archivo de idioma aparte (`Provider\locale`), unidos por una tabla del ejecutable. Buscar la frase traducida en todos los archivos no sirvió (estaba en UTF-16); seguir el código de "dame el texto k" sí.
- **Apunta desde el principio lo que dependa del idioma** (direcciones del ejecutable, número de textos, juego de letras, fuente), aunque solo se tenga una versión: cuando llegue otra, se sabrá qué comparar.

## La máquina de scripts

- **Hazla entera desde el principio, con las funciones del juego vacías.** Las instrucciones son pocas (unas 70) y salen de un solo `switch`; las funciones del juego son muchas (203). Con la máquina completa y solo las funciones necesarias hechas, los scripts de verdad (con condiciones, como el de entrada del nivel 144) ya funcionan, y las funciones que faltan se van haciendo según los scripts las piden (el visor cuenta cuántas).
- **Mira cómo devuelven sus resultados las funciones antes de saltarte ninguna.** Aquí el script deja un hueco para el resultado y quita los argumentos él mismo; por eso saltarse una función no descoloca la pila. Si las funciones metieran el resultado en la pila, saltarlas lo rompería todo.
- **Un desensamblador en Python** (`listScripts.py`) es la mejor forma de comprobar la máquina y de decidir qué función hacer después: muestra qué llama cada script y cuántas veces.
- **Ojo con el relleno de las estructuras:** un i16 seguido de un puntero suele llevar 2 bytes de relleno en medio. Al leerlo en Python, `struct` con `<` no rellena (`<hxxII`, no `<hII`): el desensamblador se equivocó así la primera vez.

- **Para un lenguaje con cientos de funciones, decompílalas todas de una vez y resúmelas con un script** (argumentos que leen, dónde escriben el resultado, a qué llaman). Con eso se ven las familias (sonido, textos, objetos) sin leer 4000 líneas, y se elige qué hacer por cuánto se usa cada una en los datos.
- **Si las funciones devuelven punteros a objetos del juego, usa fichas** (un tipo y un número) en su lugar: los scripts solo las pasan de una función a otra, y así cada función sabe a qué objeto se refiere sin que exista la memoria del juego original.
- **Mide las esperas con el tiempo real, no con fotogramas**: el juego contaba fotogramas de 60 por segundo; en una pantalla de 144 Hz las esperas durarían menos de la mitad.

## Coordenadas y choques

- **Para decidir entre dos conversiones posibles, cruza dos fuentes de datos independientes.** Las cámaras admitían dos orientaciones (una de ellas en espejo) y las dos dejaban el suelo horizontal. El mapa de choques, que está en coordenadas del mundo, lo decidió: con la buena, las paredes de las mallas de las cámaras caen sobre las del mapa, y con la otra se salen del nivel.
  - **Ojo con lo que no distingue:** al contar también el suelo, las dos empataban, porque un suelo plano es igual en el espejo. Hay que medir solo lo que cambia entre las dos (las paredes).
- **Un plano visto desde arriba** (mapa en rojo y verde, mallas de las cámaras en azul, cámaras y entrada) aclara en un vistazo lo que los números solo insinúan. Se guarda como herramienta (`drawCollisionMap.py`) porque servirá en cada nivel.
- **Para comprobar los choques, dibuja el recorrido.** Desde la cámara del juego no se distingue "resbala por la pared" de "la atraviesa". Apuntar la posición en cada fotograma (algo temporal, que luego se quita) y dibujar el recorrido sobre el plano lo dejó claro: va pegado a la pared a la distancia del radio.
- **El personaje en coordenadas del mundo, no de la cámara.** Así entra donde lo pone el juego y sigue en su sitio al cambiar de cámara; cada cámara solo decide desde dónde se mira.

## Fondos prerenderizados

- **Colores cambiados** (rojo por azul): la paleta estaba en BGRA. Antes de buscar errores raros, prueba los órdenes de canal.
- **Los "polígonos" de la malla de escena eran tiras de triángulos, no polígonos convexos.** Cortarlos en abanico daba triángulos sueltos. Para distinguirlo hay una prueba objetiva: en una tira bien leída casi todos los triángulos quedan del mismo lado (451 de 451), y en abanico solo 15.
- **Primer plano (lo que tapa al personaje):** el juego lo marca con un bit de la paleta (0x80), y además hay una malla 3D para la profundidad.
  - **No funcionó** tomar el color de las piezas del atlas con filtrado bilineal: al ampliar, cada pieza se mezcla con su vecina y se ven cortes cuadrados.
  - **Funcionó** sacar el color de la imagen completa en el mismo punto de la pantalla, y del atlas solo la máscara, sin filtrar.
- **El suelo:** la malla de la escena está incompleta (solo lo que tapa), así que un rayo contra la malla falla a menudo. Funciona un **plano de suelo** cuya altura es la moda (ponderada por área) de los triángulos horizontales.
- **Luz con dos imágenes** (a oscuras e iluminada): cambiar una por otra donde alumbra la linterna oscurecía algunas escenas. Allí la imagen "a oscuras" tiene luz propia (luna) que la iluminada no tiene. Tomar el más claro de los dos evita el problema hasta tener la fórmula real.

## Modelos y animaciones

- **No adivinar cómo se forman los triángulos.** Se probaron lista y tira única, y las dos fallaron. Lo que funcionó fue buscar la llamada de dibujo: `glDrawElements` con el modo 0x1405 y la función que corta tiras por material. La tabla que parecía de "posiciones" era la de tiras.
- **Orden de los giros y de las matrices:** el juego gira sobre z, luego y, luego x, con matrices para puntos en fila (hijo × padre). Con otro orden las posturas salen casi bien, pero no del todo: hay que copiar el orden del código.
- **Movimiento desde la raíz:**
  - las animaciones de andar y correr se quedan **en el sitio**, y la velocidad va en la cabecera de la animación (con una marca);
  - las que no la tienen (la media vuelta) mueven el **hueso 0**, y ese movimiento se aplica al personaje, no a la pose.

  **Fallo nuestro:** usar las pistas del hueso 0 en la pose hacía que el cuerpo se deslizara dentro del modelo y que el giro se contara dos veces. El juego se salta el hueso 0 al armar la pose (`if (bone != 0)`): busca esa condición en el bucle de pistas.
- **Animaciones que mezclan varias poses** (apuntar arriba, abajo, a los lados): sin los pesos, los brazos salen en cruz. El juego los pone a 0,25 cada uno al crear el modelo (apuntar al frente).
- **Las animaciones se piden por varios pasos:** acción del cerebro → tabla del arma en la mano → identificador → tabla del banco → animación. Si una tabla "no tiene sentido" (filas vacías justo donde se esperaban datos), lo normal es que falte un paso intermedio, en este caso el arma.
- **El cerebro del personaje** es una máquina de estados con botones en bits. Copiar sus estados y transiciones tal cual, con sus números, hace fácil compararlo con el original. Los detalles de los controles (media vuelta con dos toques de atrás en 300 ms, atrás que solo anda tras mantenerlo) están en la función que lee el mando, no en el cerebro.

## Shaders

- **`point` es palabra reservada en HLSL** (geometría): un parámetro con ese nombre en un `.sh` común rompe al compilar para Direct3D con un error poco claro ("')' : Expected" en una línea que no es la del archivo). Usar otros nombres (`at`).
- **Luz sobre un fondo prerenderizado:** si el juego trae una malla de la escena alineada con el fondo, basta con dibujar esa malla entera encima del fondo, tomar el color del fondo en el mismo píxel de la ventana (oscuro o iluminado) y mezclar según la luz calculada en el punto 3D de la malla. Con una vista aparte (su propia profundidad) gana siempre lo más cercano, y los modelos se dibujan después en otra vista que borra la profundidad.

- **Pintar en colores lo que calcula un shader** (rojo la fuerza de la luz, verde y azul la profundidad delante o detrás del foco) encuentra en una captura lo que no se ve mirando la imagen final: aquí, que el haz estaba bien calculado pero apuntaba mal.
- **Al restaurar un shader copiándolo, la copia conserva su fecha** y la compilación no lo vuelve a generar: hay que tocar la fecha del archivo o el programa sigue con el shader de prueba.
- **Las luces de un juego antiguo suelen ir atadas a un hueso:** antes de inventar hacia dónde apunta una linterna, buscar quién coloca la luz cada fotograma; aquí copia la posición y el giro de la mano.

- **Comprobar un formato por su tamaño exacto:** al leer `DR2D` con alineado a 32, el final de la última imagen cae justo en el tamaño del trozo. Si hubiera un byte de más o de menos, el alineado estaría mal.
- **Colores raros pero formas bien = formato de píxel equivocado:** fondo rojo oscuro donde debería haber negro opaco delataba 1-5-5-5 (bit de opacidad arriba) leído como 5-6-5.
- **El orden de las letras de una fuente** se puede sacar mirando su imagen: aquí siguen los códigos Latin-1 saltando los que no dibujan, y contar (170 = 161 comunes + los propios) confirma la lista.
- **Para saber dónde empieza la partida**, si el código no lo dice a la primera: buscar en los datos la entrada que no viene de ningún nivel y mirar su fondo (el tejado con las cuerdas del paracaídas de Aline).

- **No arrancar el juego original para mirarlo** si va a pantalla completa: cambia la resolución de todo el escritorio, no la devuelve al matarlo y la captura de su ventana sale negra. Si pasa, `ChangeDisplaySettings(NULL, 0)` devuelve el modo guardado de Windows.
- **Un menú que se elige al pasar el ratón** debe contar solo el ratón que se mueve desde que se muestra: si no, el puntero que ya estaba encima de "Salir" lo elige solo y la primera tecla de confirmar cierra el programa.

- **Antes de inventar una lista de nombres, buscar el texto que ya los dice:** los objetos no traen su nombre aparte, pero sí el mensaje "You take a Small Steel Key.", de donde sale.
- **Una herramienta que lista solo parte de los scripts engaña:** buscar quién llama a una función en los scripts del nivel, la entrada y las paredes no la encontraba; estaba en los scripts de las zonas de acción. Listar todos los sitios donde hay scripts.
- **Un atajo de prueba con variable de entorno** (dar objetos al empezar) permite probar una pantalla sin recorrer el nivel; se quita antes del commit.

- **El ratón "se mueve" solo al abrir la ventana:** SDL no sabe dónde está el puntero hasta su primer evento, y cuando se entera parece un movimiento. Para saber si el jugador lo movió de verdad, comparar la posición en el escritorio (`SDL_GetGlobalMouseState`, se sabe siempre) con un margen de unos píxeles, y que cualquier tecla le quite el control al ratón.
- **Probar con el usuario delante:** su ratón quieto encima de la ventana cambia lo que se elige en las pruebas automáticas; mirar dónde está el puntero (`Cursor.Position`) cuando una prueba de menú da resultados raros.
- **Una partida guardada en texto "clave valor"** se lee a ojo, se puede comparar entre versiones y una versión nueva puede saltar las claves que no conoce.

- **En fondos prerenderizados con malla, los objetos del primer plano son "cartones":** piezas planas mucho mayores que el objeto, recortadas por una máscara. Todo lo que se haga con esa malla (luz, profundidad, colisión con la vista) tiene que respetar la máscara o aparecen polígonos donde no hay nada.
- **Las fichas de un menú pueden ser un solo texto con saltos de línea:** buscar el texto por su comienzo y partirlo, en vez de esperar una línea por texto.

- **Para saber cómo se mueve una pantalla, leer las funciones virtuales de sus objetos:** la tabla de métodos de la clase del texto (`0x4BE4BC`) dijo de golpe la animación (entra deslizándose, se oscurece su mitad, letra a letra con un clic); mirando solo qué carga la pantalla no se ve.
- **Los bancos de sonido de los menús dicen qué suena** (`menu\select2.`: `5201_d`, `intro\bip`, `intro\aitd4`, `5204_a`) y las llamadas `FUN_00471870(página, N)` dicen cuándo.

- **Los textos que pasan los scripts pueden traer caracteres de control** (un salto de línea delante de `\doc\101_51_A`): limpiar antes de usarlos como nombre de archivo. Un registro de una línea con el nombre entre corchetes lo descubrió.
- **Arrancar a mano el script de una zona** (con un atajo temporal) prueba una función de los scripts sin llevar al personaje hasta allí.

- **Lo que Ghidra no enseña está en el ensamblador:** las cuentas en coma flotante (x87) desaparecen en el C de Ghidra; con `capstone` en Python se leen las instrucciones alrededor de `__ftol` y sale la fórmula exacta (velocidad = 44100 × frecuencia × 100 / raíz).
- **Un valor que no cuadra puede ser de otro tamaño:** `DAT_004cdc00` parecía 3277000, pero el código lo lee como i16 (`movsx word`): 200, la frecuencia del temporizador.
- **Antes de inventar un sintetizador, mirar de dónde saca el juego las muestras:** los "instrumentos dentro del archivo" no se usaban; las notas tocan muestras de `Sound2` por nombre desde tablas del ejecutable.
- **Los textos de una máquina de scripts pueden llevar su longitud delante:** el "salto de línea" de un nombre era el byte de longitud (13). Leer el formato de la máquina antes de limpiar caracteres raros.
- **Comprobar qué hace el juego aunque parezca un error:** toca cada nota una octava más baja (resta 12) y no lo compensa; así está hecha su música, y así se copia.

- **Una textura que sale como ruido de colores suele estar comprimida:** mirar las marcas de su cabecera frente a las de las demás y seguir en el ejecutable qué hace el juego con esa marca (aquí, el VQ de Dreamcast: libro de bloques 2×2 + índices).
- **Un susto o una escena puede depender de una condición** (aquí, la luz del nivel encendida): leer el principio del script antes de concluir que algo no funciona.

## Probar sin estar delante

- **Un registro temporal a un archivo** (activado con una variable de entorno, quitado antes del commit) dice lo que las capturas no: en qué comportamiento está cada monstruo, dónde y si la luz le da. Con él se vio en segundos que el PHOTOSAURE del nivel 1 se atasca contra una pared y que en el nivel 14 hace todo el ciclo.
- **Elegir el nivel de prueba con los datos:** una lista de qué niveles tienen ese monstruo y a qué distancia de la entrada evita probar donde no se ve nada.

- **Abrir directamente donde interesa** (`--level N --camera N`): si no, las teclas enviadas desde fuera acaban en el nivel 1.
- **Capturas solo de la ventana** (`PrintWindow`), nunca de la pantalla entera.
- **Teclas mantenidas:** `SendKeys` solo pulsa y suelta; para andar hace falta `keybd_event` con la tecla abajo durante un tiempo. Las flechas y Re Pág/Av Pág necesitan la marca de tecla extendida.
- **Ratón:** moverlo en pasos pequeños, no de golpe, para que el programa vea el recorrido.

## Organización

- **`.gitignore` con `ghidra/` también ignora `tools/ghidra/`.** Los scripts no entraron en Git durante días. Hay que usar `/ghidra/` (solo la raíz) y comprobarlo con `git check-ignore`.
- **Al generar scripts desde la consola, las barras invertidas pueden perderse** (`\r`, `\f` dentro de rutas de Windows se convierten en caracteres de control). Es mejor escribir los scripts en un archivo y buscar caracteres de control en lo que cambian.
- **Una rama por tarea, encadenadas.** Si se trabaja muchas horas sin probar, cada rama sale de la anterior; la última lo contiene todo y basta con probar esa.

## Vídeos

- **Bink 1 se lee con FFmpeg** (decodificador `binkvideo` y `binkaudio_rdft`/`dct`): no hace falta la `binkw32.dll` del juego, que además es de 32 bits y no se puede cargar desde un programa de 64. Basta la compilación LGPL en DLL (avcodec, avformat, avutil, swresample, swscale); sus cabeceras dan avisos con `/W4`, que se callan solo alrededor de los `#include` con `#pragma warning(push, 0)`.
- **Sincronizar sin hilos:** cada fotograma se decodifica por delante lo justo (dos imágenes en cola); el sonido se manda a su propio flujo de SDL según sale y la imagen se elige por su marca de tiempo frente al reloj desde que empezó. Para vídeos cortos y a 30 imágenes por segundo no se nota desfase.
- **El orden de los vídeos y dónde empieza la partida están en el bucle principal del ejecutable**, no en los datos: deducirlo de los datos llevó a empezar en el nivel equivocado.

## Escenas y voces

- **Mirar qué órdenes usan de verdad los archivos** antes de hacer un intérprete entero: el motor de escenas tiene 54 órdenes, pero las radios solo usan 16; un lector en Python que cuenta las órdenes de todos los archivos dijo cuáles hacer primero.
- **Las órdenes que "esperan" algo hay que comprobarlas por su objetivo**, no por el estado general: al pedir una animación, la anterior puede seguir hasta una ventana de su transición; si se da por acabada la pedida porque la anterior dio la vuelta, la escena se salta animaciones.
- **CRI ADX se decodifica en pocas líneas** (tramas de 18 bytes, predicción con dos muestras y coeficientes de la frecuencia de corte). Para comprobarlo sin oírlo: la fuerza por décimas de segundo sube y baja con pausas, sin saturar, y cruza el cero pocas veces (voz, no ruido).
- **Texturas con nombre**: cuando los píxeles salen con rayas o ruido, probar todas las combinaciones (orden Z o lineal, 5-6-5, 1-5-5-5, 4-4-4-4) en una sola imagen de muestra; aquí además el lugar de los píxeles estaba desplazado 16 bytes (el bloque de datos empezaba en otro sitio).
- **Al añadir animaciones nuevas, revisar sus eventos y quién los trata en el ejecutable.** Las órdenes de una escena no lo dicen todo: la radio aparece en la mano por un evento de la propia animación (0x66 en `0x434409`), y se pasó por alto por mirar solo las órdenes.
- **Varias texturas en una imagen se pisan al suavizar**: el borde de una toma una fila de la vecina (salía una raya finísima bajo el resplandor). Dejar un margen negro entre ellas.
- **Luz que se corta en bordes duros sobre una escena de planos**: calcularla en una imagen aparte y difuminarla antes de usarla da una caída suave parecida a la luz por vértices de los juegos de la época, sin tocar la geometría.
- **Al probar con un punto colocado a mano, comprobar dónde ha caído el personaje**: elegir el suelo bajo un punto de la imagen puede dejarlo detrás del primer plano y parecer un fallo de dibujo.

## Partículas

- **Un sistema de partículas se reconoce por sus números**: cuántas, cuánto viven, cada cuánto nacen, forma. Con eso y la textura (8×32 alargada = lluvia) se adivina para qué es cada uno antes de leer todo el código.
- **Seguir quién crea cada tipo de objeto del dibujo** (llamadas a la función que los crea, con el tipo como primer argumento) lleva a todas las fuentes de efectos: lluvia, cosas del nivel, armas, actores.
- **Para comprobar un efecto que sale de vez en cuando** (relámpagos): muchas capturas seguidas y medir el brillo medio de cada una; los picos dicen cuáles mirar.
- **Para saber qué sonido es cada muestra sin oírla**: decodificarla y medir su fuerza cada 50 ms. Un disparo es fuerte y se apaga despacio; un clic es débil y corto; una recarga son varios chasquidos.
