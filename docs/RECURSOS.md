# Recursos gráficos y de sonido

Lista de lo que usa (o va a usar) Greeny. Lo que está en `assets/` se convierte
con `python3 scripts/assets.py`.

Estilo: verde, blanco, rojo y amarillo; alegre y limpio. Nada de logos,
tipografías ni personajes de Nintendo.

## Imágenes

| Recurso | Formato | Para qué | Estado |
|---|---|---|---|
| Mascota de cuerpo entero | PNG transparente, 1024×1536 | Inicio de la app y web | Listo (`assets/mascot.png`) |
| Mascota en poses: celebrando, triste, durmiendo, saludando | PNG transparente, 1024×1536 cada una | Logro desbloqueado, lista vacía, errores, bienvenida | Falta |
| Cara de la mascota | PNG transparente, 512×512 | Aviso en el juego, avatar por defecto, favicon | Falta |
| Logo "Greeny" | PNG transparente, ~1200×400 | Cabecera de la app y de la web | Falta |
| Icono de la app | JPG 256×256, sin transparencia | Menú de homebrew | Provisional (generado de la mascota) |
| Insignia genérica desbloqueada y bloqueada | PNG transparente, 128×128 | Logros sin icono propio | Falta |
| Icono por logro | PNG, 128×128, uno por logro, nombrado como el id (`monedas_5.png`) | App, web y aviso en el juego | Falta |
| Carátula por juego para la web | JPG o PNG, 512×512 | Solo la web; la app usa la del juego instalado | Falta |
| Fondo o patrón para la app | PNG 1280×720, muy suave | Fondo de las pantallas | Opcional |
| Banner del repositorio | PNG 1280×640 | README y vista previa al compartir | Falta |
| Iconos de menú: opciones, perfil, sincronizar, cerrar sesión | PNG transparente, 96×96, un color | App y web | Falta |
| Fotogramas de animación de la mascota (saltando, 8 a 12 cuadros) | PNG transparente, mismo tamaño todos | Animación en inicio y al desbloquear | Opcional, para cuando haya animaciones |

### Iconos de los 12 logros de Mario 3D World

`prueba_de_vida`, `monedas_5`, `monedas_50`, `monedas_90`, `vidas_10`,
`vidas_30`, `vidas_99`, `reserva_gato`, `reserva_fuego`, `reserva_tanuki`,
`reserva_bumeran`, `reserva_suerte`.

Dibujos propios. No uses arte sacado del juego.

## Sonido

| Recurso | Formato | Para qué | Estado |
|---|---|---|---|
| Logro desbloqueado | WAV 48 kHz mono 16 bit, menos de 1 s | Suena dentro del juego | Provisional (generado por código) |
| Juego completado al 100 % | WAV 48 kHz mono 16 bit, 1 a 2 s | Al sacar el último logro | Falta |
| Música de fondo de la app | Bucle sin corte, WAV u OGG 48 kHz estéreo, 30 a 60 s, suave | Solo en la app, con opción de apagarla | Falta |
| Navegación: mover, aceptar, volver, error | WAV 48 kHz mono 16 bit, muy cortos | La app | Falta |

Los sonidos que suenan dentro del juego, mejor cortos y suaves: van encima del
audio del juego.
