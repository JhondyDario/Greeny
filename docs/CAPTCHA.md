# Captcha "no soy un robot" en la web

La web usa **Cloudflare Turnstile**, que es gratis y casi nunca hace que la gente resuelva acertijos.
Lo piden **Entrar**, **Crear cuenta** y **Recuperar contraseña**. **Entrar con Discord** no lo pide,
porque Discord ya tiene su propia protección.

La consola **no** pasa por el captcha. Para entrar con correo y contraseña usa la función
`device_login_password` de la base de datos, que permite 5 intentos fallidos por correo cada 15 minutos.
Por eso hay que seguir el orden de abajo. Si activas el captcha antes, la app vieja (0.1.8 o anterior)
ya no puede entrar con contraseña. Vincular con código sigue funcionando igual.

## Pasos (en este orden)

1. **Base de datos:** corre `supabase/schema.sql` completo en el SQL Editor de Supabase.
2. **App:** instala la 0.1.9 o una más nueva en la consola.
3. **Cloudflare:** entra en dash.cloudflare.com (la cuenta es gratis) → **Turnstile** → *Add widget*.
   - Nombre: `Greeny web`.
   - Dominios: `jhondydario.github.io`.
   - Modo: **Managed**.
   - Te da dos claves: la **Site Key** (pública) y la **Secret Key** (secreta).
4. **Web:** pon la Site Key en `web/js/config.js`, en `turnstileSiteKey: "..."`, y súbelo a `main`.
   Esa sí puede ir en el repositorio: es pública.
5. **Supabase:** Authentication → **Attack Protection** → *Enable Captcha protection* → proveedor **Turnstile**
   → pega la **Secret Key** → Save.
   La Secret Key es secreta: no va en el repositorio, ni en la app, ni en un chat.

Para probar: abre la web en una ventana privada, entra con tu cuenta y marca la casilla.
Si sale "La verificación de no soy un robot falló o venció", vuelve a marcarla.
Si nunca aparece la casilla, revisa que el dominio del widget sea exactamente `jhondydario.github.io`.
