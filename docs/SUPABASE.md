# Cuentas, web y sincronización

**Estado: diseño. Todavía no hay código de esta parte.**

## Cómo va a funcionar

```
Sysmodule ──guarda──▶ SD ──lee──▶ App Greeny ──HTTPS──▶ Supabase ◀── Web (GitHub Pages)
```

- El **sysmodule** no se conecta a internet. Sigue guardando en la SD.
- La **app Greeny** es la que inicia sesión y sube el progreso.
- La **web** es estática (GitHub Pages) y habla con Supabase desde el navegador.
- **Supabase** guarda usuarios, logros desbloqueados y definiciones de juegos.

### Iniciar sesión en la consola

Dos formas, las dos en la app:

1. **Correo y contraseña**, con el teclado de la consola.
2. **Código de vinculación**: la app muestra un código corto; lo escribes en la
   web con tu sesión abierta y la consola queda vinculada. Es la única forma que
   sirve para quien entra con Discord, porque la Switch no tiene navegador para
   hacer el inicio de sesión de Discord.

### Qué no hace

- **No evita trampas.** Evita que alguien copie el archivo de progreso de otro,
  pero un cheat desbloquea logros igual. Las cuentas son para perfil y comunidad.
- **No se vincula con la cuenta Nintendo.** Nintendo no ofrece inicio de sesión
  para terceros, y conectarla a una herramienta de consola modificada es
  arriesgarse a un baneo.

## Lo que hay que crear (lo hace el dueño del proyecto)

1. **Proyecto en Supabase** (plan gratis). De ahí salen dos datos que sí van en
   el código porque son públicos:
   - Project URL (`https://xxxx.supabase.co`)
   - `anon` public key
2. **Aplicación en Discord** (Developer Portal → New Application → OAuth2). El
   Client ID y el Client Secret se pegan en Supabase, en
   Authentication → Providers → Discord. La URL de retorno que pide Discord la
   muestra Supabase en esa misma pantalla.
3. **GitHub Pages** activado en este repositorio (Settings → Pages), apuntando a
   la carpeta de la web cuando exista.

> La clave `service_role` de Supabase y el Client Secret de Discord son
> secretos. No van en el repositorio, ni en la app, ni en un chat.

## Datos que se van a guardar

| Tabla | Contenido |
|---|---|
| `profiles` | Usuario, nombre visible, avatar, Discord vinculado |
| `games` | Title ID, nombre, versión, definición de logros |
| `unlocks` | Usuario, juego, logro, fecha |
| `devices` | Consolas vinculadas a un usuario y códigos de vinculación |

Cada usuario solo puede escribir su propio progreso (reglas de seguridad por
fila de Supabase). Los perfiles se pueden ver públicamente.

## Carátulas en la web

En la consola la carátula sale del propio juego instalado, sin internet. La web
no tiene eso: la app puede subir la carátula una vez por juego al sincronizar, o
se suben a mano por juego. Queda por decidir.
