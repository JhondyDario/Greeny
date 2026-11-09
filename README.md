# Greeny

El primer sistema de logros no oficial para la Switch.

Greeny corre en segundo plano mientras juegas, lee la memoria del juego y, cuando
cumples las condiciones de un logro, lo guarda, te avisa en pantalla y suena. Los
logros de cada juego se definen en un archivo de texto, así que cualquiera puede
crear los de su juego favorito.

> Proyecto de fans. No está afiliado ni avalado por Nintendo. Requiere una
> consola con Atmosphère. **No lo uses en línea.**

![Pantalla de inicio](docs/img/inicio.png)
![Logros de un juego](docs/img/logros.png)

*Capturas renderizadas en PC; en la consola se usa la fuente del sistema y la
carátula real del juego.*

## Qué trae

| Parte | Qué hace |
|---|---|
| **Sysmodule** (`source/`) | Detecta el juego, lee su memoria por `dmnt:cht`, desbloquea logros, muestra el aviso y reproduce el sonido. |
| **App Greeny** (`app/`) | Lista tus juegos con carátula, progreso y puntos; muestra cada logro, cómo se consigue y si ya lo tienes. Español e inglés. Opciones de idioma, sonido y aviso. |
| **Motor** (`common/`) | Formato de logros y evaluación de condiciones. Portable: se prueba en PC. |
| **Logros** (`juegos/`) | Un archivo por juego. Por ahora: Super Mario 3D World + Bowser's Fury v1.1.0 (12 logros). |

## Instalar

1. Descarga `greeny.zip` y descomprímelo en la raíz de la tarjeta SD.
2. Reinicia la consola (apagar y encender, no suspender).
3. Abre un juego que tenga logros: sale el aviso "LOGROS ACTIVOS".
4. Abre **Greeny** desde el menú de homebrew para ver tu progreso.

```
atmosphere/contents/4200000000010690/   sysmodule
switch/greeny.nro                       app
config/greeny/games/                    archivos de logros, uno por juego
config/greeny/unlocked/                 tu progreso
config/greeny/config.ini                opciones
config/greeny/log.txt                   registro del sysmodule
```

Si venías de una versión anterior (`config/logros`), tu progreso se pasa solo a
`config/greeny` la primera vez. Puedes borrar `switch/logros.nro` y `switch/gree.nro`.

Para desinstalar, borra `atmosphere/contents/4200000000010690/`. Si la consola no
arranca tras instalar, saca la SD y borra esa carpeta desde la PC.

## Crear logros para un juego

Guía completa en [docs/CREAR-LOGROS.md](docs/CREAR-LOGROS.md). En corto:

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
```

El archivo va en `config/greeny/games/<TITLE ID>.txt`.

## Compilar

Hace falta devkitA64 + libnx (con `switch-freetype`), o Docker:

```bash
docker run --rm -v "$PWD":/src -w /src devkitpro/devkita64 make
```

```bash
docker run --rm -v "$PWD":/src -w /src devkitpro/devkita64 make -C app
```

```bash
./scripts/dist.sh
```

Eso deja `dist/` y `greeny.zip` con la estructura de la SD.

Pruebas del motor en PC (no hace falta consola):

```bash
make -C test
```

## Estado

| Función | Estado |
|---|---|
| Lectura de memoria, desbloqueo, guardado | Probado en consola |
| Aviso en pantalla y sonido | Probado en consola |
| Logros de Mario 3D World 1.1.0 | Probados en consola |
| Buscador de punteros (`scan`) | Probado en consola |
| App: interfaz gráfica, idiomas, opciones, carátulas | Revisada en PC; falta probarla en consola |
| Cuentas, web y sincronización | Pendiente ([docs/SUPABASE.md](docs/SUPABASE.md)) |
| Iconos por logro, música y animaciones en la app | Pendiente ([docs/RECURSOS.md](docs/RECURSOS.md)) |

## Limitaciones que conviene saber

- Las direcciones de memoria cambian con cada versión del juego. Cada archivo de
  logros sirve para una versión concreta; si no coincide, Greeny desactiva esos
  logros en vez de leer datos equivocados.
- Greeny mira el estado del juego, no cómo llegaste a él. Con un cheat se puede
  desbloquear un logro. No es un sistema anti-trampas.
- Al engancharse a un juego, `dmnt` queda como depurador, igual que con los
  cheats de Atmosphère.

## Licencia y créditos

GPLv2 (ver [LICENSE](LICENSE)). Incluye código derivado de
[Atmosphère](https://github.com/Atmosphere-NX/Atmosphere) (`dmntcht`) y sigue el
procedimiento de capa de [libtesla](https://github.com/WerWolv/libtesla), ambos
GPLv2. Fuente del aviso: [font8x8](https://github.com/dhepper/font8x8) (dominio
público). Carga de imágenes: [stb_image](https://github.com/nothings/stb)
(dominio público). La mascota y el nombre Greeny son de Jhondy.
