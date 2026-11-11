# Recursos gráficos y de sonido

Lista de lo que usa (o va a usar) Greeny. Lo que está en `assets/` se convierte
con `python3 scripts/assets.py`.

Estilo: verde, blanco, rojo y amarillo; alegre y limpio. Nada de logos,
tipografías ni personajes de Nintendo.

## Imágenes

| Recurso | Formato | Para qué | Estado |
|---|---|---|---|
| Mascota de cuerpo entero | PNG transparente, 1024×1536 | Cabecera de la app, icono y web | Listo (`assets/mascot.png`) |
| Mascota en poses: celebrando, triste, durmiendo, saludando | PNG transparente, 1024×1536 cada una | Logro desbloqueado, lista vacía, errores, bienvenida | Falta |
| Logo "G" | PNG transparente, 400×400 | Insignia de la cabecera en la app y la web, favicon | Listo (`assets/logo-g.png`) |
| Logo con el nombre "Greeny" completo | PNG transparente, ~1200×400 | Portada de la web y banner | Falta |
| Icono de la app | 256×256, sin transparencia | Menú de homebrew | Listo (`assets/app-icon.png`) |
| Insignia genérica desbloqueada y bloqueada | PNG transparente, 128×128 | Logros sin icono propio | Falta |
| Icono por logro | Cualquier imagen; la web la convierte a PNG cuadrado de 128×128 al subirla | App y web (el aviso en el juego aún no los usa) | Se suben desde la web, en Desarrollador |
| Carátula por juego para la web | JPG o PNG, 512×512 | Solo la web; la app usa la del juego instalado | Falta |
| Fondos claro y oscuro | PNG 1280×720 | Fondo de las pantallas de la app | Listo (`assets/fondo_claro.png`, `assets/fondo_oscuro.png`) |
| Botones L y R | PNG transparente | Pestañas de Opciones | Listo (`assets/icons/`) |
| Banner del repositorio | PNG 1280×640 | README y vista previa al compartir | Falta |
| Iconos de menú: perfil, salir, menú | PNG transparente | Web (perfil y salir ya se usan) | Listo (`assets/icons/`) |
| Iconos de botones (A, B, X, Y, +, −, casa) y sincronizar | PNG transparente | Pistas de botones en la app (casa y sincronizar aún sin usar) | Listo (`assets/icons/`) |
| Cara de la mascota | PNG transparente, 512×512 | Introducción y Acerca de en la app, portada de la web | Listo (`assets/cara.png`) |
| Fotogramas de animación de la mascota (saltando, 8 a 12 cuadros) | PNG transparente, mismo tamaño todos | Animación en inicio y al desbloquear | Opcional, para cuando haya animaciones |

### Recursos de la app y la web

Todo sale de `assets/` con `python3 scripts/assets.py`.

| Recurso | Dónde está | Dónde se usa |
|---|---|---|
| Medallas de bronce, plata y oro | `assets/medals/` | Logro al detalle (app), web |
| Logro bloqueado y desbloqueado | `assets/logro_*.png` | Estado de cada logro |
| Logro y carátula de relleno | `assets/relleno_*.png` | Logros sin icono y juegos sin carátula |
| Trofeo de juego completado | `assets/trofeo.png` | Sobre la carátula al 100 % |
| Iconos de estadísticas | `assets/stats/` | Perfil |
| Iconos de estado del juego | `assets/estado/` | Buscador de la app, web |
| Mascota en 8 poses | `assets/mascota/` | Pantallas vacías, errores, bienvenida, buscador |
| Medallas del perfil (11 + bloqueada) | `assets/badges/` | Perfil |
| Fotos de perfil (18) | `assets/avatars/` | Perfil; se elige una, no se suben fotos propias |
| Flecha, palancas de encendido y apagado | `assets/icons/` | Logro al detalle, Opciones |
| Logos con el nombre | `assets/logo-greeny*.png` | Acerca de, portada de la web |

De las 24 fotos de perfil recibidas se dejaron fuera 6 porque dibujan personajes u objetos de
Nintendo (champiñón, estrella con ojos, fantasma, trifuerza, pokébola y bloque de interrogación).
Hacen falta 6 dibujos propios para completar las 24.

### Medallas del perfil

Once medallas, PNG transparente de 256×256, con el id como nombre, más `bloqueada`.

`primer_logro`, `logros_10`, `logros_50`, `logros_200`, `primer_platino`,
`platinos_5`, `juegos_3`, `juegos_10`, `puntos_100`, `puntos_500`, `puntos_2000`


### Iconos de los 12 logros de Mario 3D World

`prueba_de_vida`, `monedas_5`, `monedas_50`, `monedas_90`, `vidas_10`,
`vidas_30`, `vidas_99`, `reserva_gato`, `reserva_fuego`, `reserva_tanuki`,
`reserva_bumeran`, `reserva_suerte`.

Dibujos propios. No uses arte sacado del juego.

## Sonido

| Recurso | Formato | Para qué | Estado |
|---|---|---|---|
| Logro desbloqueado | WAV 48 kHz 16 bit | Suena dentro del juego. También se usa al completar un juego | Listo (`assets/sounds/logro.wav`) |
| Logros cargados | WAV 48 kHz 16 bit | Al abrir un juego, con el aviso inicial | Listo (`assets/sounds/cargado.wav`) |
| Música de fondo de la app | Bucle sin corte, WAV u OGG 48 kHz estéreo, 30 a 60 s, suave | Solo en la app, con opción de apagarla | Aplazado |
| Navegación: mover, aceptar (dos variantes), abrir pantalla, volver | WAV 48 kHz 16 bit, cortos | La app | Listo (`navegar.wav`, `aceptar_1.wav`, `aceptar_2.wav`, `abrir_menu.wav`, `regresar.wav`) |

Los sonidos que suenan dentro del juego, mejor cortos y suaves: van encima del
audio del juego. Además se guardan dentro del sysmodule, que tiene muy poca
memoria: `scripts/assets.py` los pasa a mono y 32 kHz y les recorta el silencio.
