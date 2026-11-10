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
