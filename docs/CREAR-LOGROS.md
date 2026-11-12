# Crear logros para un juego

Cada juego tiene un archivo en `config/greeny/games/<TITLE ID>.txt` (16 dígitos
hexadecimales en mayúsculas). Hay una plantilla comentada en
[`juegos/0100000000000000.txt`](../juegos/0100000000000000.txt) y un ejemplo
real en [`juegos/010028600EBDA000.txt`](../juegos/010028600EBDA000.txt).

## Formato

```ini
game    = Mi Juego
version = 1.1.0
build   = 3CA12DFAAF9C82DA

[monedas_100]
name    = Bolsillo lleno
desc    = Junta 100 monedas
name.en = Full pockets
desc.en = Collect 100 coins
points  = 10
when    = main+0x1A2B3C40 u32 >= 100

[jefe_1]
name = El primero de muchos
desc = Derrota al primer jefe
when = main+0x2F00A8->0x18->0x3C u8 & 0x04
```

### Cabecera

| Clave | Para qué |
|---|---|
| `game` | Nombre del juego que muestra la app. |
| `version` | Versión del juego para la que sirven las direcciones. La app la muestra. |
| `build` | Primeros dígitos del build id del juego. Si no coincide, los logros se desactivan. Sale en `log.txt` la primera vez que Greeny se engancha. |
| `debug` | `1` anota en `log.txt` el valor que lee cada condición cada vez que cambia. |
| `scan` | Busca a dónde se mudó un puntero (ver abajo). |

### Cada logro

| Clave | Para qué |
|---|---|
| `[id]` | Identificador único: letras, números, `_` y `-`. No lo cambies después, porque el progreso se guarda por id. |
| `name`, `desc` | Nombre y cómo se consigue. |
| `name.en`, `desc.en` | Traducción al inglés (opcional). |
| `points` | Puntos, de 0 a 1000 (10 por defecto). |
| `tier` | Dificultad: `bronce`, `plata` u `oro`. Si no lo pones sale de los puntos: hasta 10 bronce, hasta 25 plata, más de 25 oro. También se cambia desde la web, en Desarrollador. |
| `hold` | Milisegundos seguidos que deben cumplirse las condiciones (500 por defecto). |
| `when` | Condición. Si hay varias, se tienen que cumplir todas a la vez. |

### Condiciones

```
when = <dirección> <tipo> <operador> <valor>
```

| Parte | Valores |
|---|---|
| dirección | `main+0x…` o `heap+0x…`, con `->0x…` por cada nivel de puntero (máximo 4) |
| tipo | `u8 u16 u32 u64 s8 s16 s32 s64 f32 f64` |
| operador | `== != > >= < <=`, `&` (todos esos bits encendidos), `!&` (todos apagados) |
| valor | decimal o `0x` hexadecimal; con punto para `f32`/`f64` |

`main+0x1A2B3C->0x10->0x48` significa: lee un puntero en `main+0x1A2B3C`, súmale
`0x10`, lee otro puntero ahí, súmale `0x48`, y ese es el dato.

Un puntero nulo o una lectura fallida hace la condición falsa; nunca desbloquea.

Límites: 64 logros por juego, 6 condiciones por logro, archivo de 64 KB.

## Validar antes de copiar a la SD

```bash
make -C test lgcheck
```

```bash
test/lgcheck juegos/010028600EBDA000.txt
```

Si hay un error dice la línea y el motivo.

## De dónde salen las direcciones

Esta es la parte laboriosa, y es por juego y por versión del juego.

- **Reciclando cheats existentes.** Un cheat de Atmosphère de tipo 0 tiene la
  forma `0TMR00AA AAAAAAAA VVVVVVVV`: `T` es el ancho en bytes, `M` la región
  (0 = main, 1 = heap) y `AA AAAAAAAA` el offset. Ejemplo:
  `04000000 1A2B3C40 000003E7` → `main+0x1A2B3C40 u32`.
  Los de tipo 5 (`580R0000 …` seguido de `580R1000 …`) son cadenas de punteros:
  cada línea `580R1000 XXXXXXXX` es un `->0xXXXXXXXX`, y la línea `780R0000` final
  es el último offset.
- **Buscando en la consola** con EdiZon SE o Breeze: buscas el valor, lo cambias
  en el juego, filtras y haces la búsqueda de punteros. Una dirección de `heap`
  sin cadena de punteros casi nunca sirve de una sesión a otra.

### Con el buscador de valores de Greeny

Sirve para cualquier número que veas en pantalla (monedas, estrellas, sellos, vidas).
Solo aparece si la cuenta vinculada a la consola es de un administrador.

1. El juego necesita un archivo de logros, aunque sea de prueba con un solo logro,
   para que Greeny se enganche a él.
2. **Antes de abrir el juego**, en Greeny ve a Opciones → Ayuda → Buscador de valores.
   Pon el número que tienes ahora (por ejemplo 10 estrellas) y cuánto cambia cada vez
   (+1), y pulsa A para guardar el plan. Cierra Greeny.
3. Abre el juego. Mantén **L + R + ZL + ZR** y pulsa **arriba** en la cruceta: busca el
   número. Tarda un minuto o más; al terminar sale un aviso con cuántas candidatas hay.
4. Haz que el número cambie (coge una estrella) y repite la combinación. Cada vez busca
   el número siguiente. Sigue hasta que queden pocas candidatas.
5. La misma combinación con **derecha**: busca la cadena de punteros de la primera
   candidata. Puede tardar varios minutos.
6. Cierra el juego y ábrelo otra vez, sin cambiar el número. La combinación con **abajo**
   comprueba qué cadenas siguen funcionando.
7. El resultado queda en `config/greeny/search/result.txt` y se ve en la misma pantalla
   de Greeny. Las líneas marcadas con `SIRVE` son las que van en el `when` del logro.

**Cuando no es un número a la vista** (el traje, si vas montado, un nivel terminado): en el plan,
pon "cada vez cambia en" en 0, que es "No sé el número". Dentro del juego:

- L+R+ZL+ZR + **arriba** la primera vez guarda una foto de toda la memoria en la SD (uno o
  dos GB, varios minutos; hacen falta unos 3 GB libres).
- Cambia solo eso en el juego y pulsa **arriba** otra vez: quedan las direcciones que cambiaron.
- Sin cambiarlo, muévete un poco y pulsa **izquierda**: quedan las que siguen igual. Alterna
  "cambió" y "no cambió" hasta que queden pocas.
- **Derecha** busca los punteros y **abajo** dice qué lee cada cadena en ese momento. Aquí no
  hay un valor que comprobar: sirve la cadena que cambia como esperas al cambiar de estado.

**Busca el número donde el juego lo guarda de verdad.** Muchos juegos llevan un contador
temporal mientras juegas y solo pasan el valor al total de la partida al terminar el nivel o
al guardar. En Super Mario 3D World, las estrellas verdes se suman al total al tocar el
banderín, no al cogerlas. Si buscas dentro del nivel encuentras el contador temporal, que
desaparece al cerrar el juego, y la comprobación da cero. Lo que funcionó:

- Haz cada búsqueda en el mapa del mundo, viendo el número.
- Para cambiarlo, entra a un nivel, coge **una sola** estrella, termina el nivel y vuelve al mapa.
- Busca los punteros también desde el mapa.
- Tras reiniciar el juego, entra a la partida hasta el mapa antes de comprobar.

Mientras el buscador trabaja, Greeny no revisa logros. Cuando termines, borra el plan
(X en esa pantalla): mientras exista, el sistema de logros lee los botones del mando.

### Cuando el juego se actualiza: `scan`

Si una cadena conocida dejó de funcionar en una versión nueva, casi siempre solo
se movió el primer puntero. `scan` lo encuentra:

```ini
scan = main+0x1340000->0x18->0x0->0x138->0x48 u32 0x80000 1 999
```

Recorre `0x80000` bytes desde `main+0x1340000`, prueba cada valor como primer
puntero de la cadena y cada 10 segundos anota en `log.txt` los offsets cuyo dato
final cae entre 1 y 999. El que vaya siguiendo el valor que ves en pantalla es el
bueno. Así se pasaron los logros de Mario 3D World de la 1.0.0 a la 1.1.0.

Máximo 4 `scan` por archivo. Quítalos cuando termines: cuestan CPU.
