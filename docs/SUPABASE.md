# Cuentas, web y sincronización

**Estado: la base de datos está definida y probada ([`supabase/schema.sql`](../supabase/schema.sql)). Falta la web y la conexión desde la app.**

## Proyecto

| Dato | Valor |
|---|---|
| Project URL | `https://mlwzlirkjgpyyldbtoci.supabase.co` |
| Publishable key | `sb_publishable_r4Y8LjTEzu7A2Z9ecr2hEw_Jr-OT8RC` |

Estos dos datos son públicos por diseño: van dentro de la web y de la app. Lo
que protege los datos son las reglas de seguridad por fila de la base de datos.

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

## Crear las tablas

1. En Supabase: **SQL Editor → New query**.
2. Pega todo el contenido de [`supabase/schema.sql`](../supabase/schema.sql) y pulsa **Run**.
3. Regístrate (o crea tu usuario en Authentication → Users) y hazte
   administrador con la consulta que está al final del archivo.

Se puede correr más de una vez: no borra datos.

## Datos que se guardan

| Tabla | Contenido |
|---|---|
| `profiles` | Usuario, nombre visible, frase (caption), avatar, Discord |
| `caption_presets` | Frases sugeridas para el caption ("Cazatrofeos", etc.), en español e inglés |
| `games` | Title ID, nombre, versión y el archivo de logros completo |
| `achievements` | Logros de cada juego: textos, puntos e icono |
| `unlocks` | Usuario, juego, logro, fecha |
| `devices` | Consolas vinculadas a un usuario |
| `admins` | Quién puede publicar juegos, logros e imágenes |

Reglas:

- Todo el mundo puede leer perfiles, juegos publicados, logros y progreso.
- Cada usuario edita solo su perfil (usuario, nombre, caption, avatar).
- **Nadie inserta logros a mano**, ni con sesión iniciada: solo entran desde una
  consola vinculada.
- La consola nunca guarda tu contraseña. Guarda una llave propia que solo sirve
  para subir logros a tu cuenta y que puedes revocar desde la web. En la base de
  datos solo está el hash de esa llave.
- El caption puede ser una frase sugerida o texto libre de hasta 60 caracteres.
  El texto libre no tiene moderación todavía.

### Probar el esquema sin tocar Supabase

`supabase/test/` trae un simulador mínimo de Supabase y las pruebas de permisos:

```bash
docker run -d --name pgtest -e POSTGRES_PASSWORD=x postgres:16
```

```bash
cat supabase/test/stub.sql supabase/schema.sql supabase/test/test.sql | docker exec -i pgtest psql -U postgres -v ON_ERROR_STOP=1 -q
```

## Catálogo en línea: logros, iconos y carátulas

La app no trae dentro los logros ni las imágenes de todos los juegos. Los baja
cuando hay red y los guarda en la SD:

- **Archivo de logros** de cada juego: la app mira qué juegos tienes instalados
  y baja solo los de esos. Así se añaden juegos nuevos sin sacar otra versión de
  la app.
- **Iconos de logros**: se bajan al abrir un juego por primera vez y quedan en
  `config/greeny/cache/`. Sin red se ven los que ya estén guardados; el resto
  muestra el check o el candado de siempre.
- **Carátulas**: en la consola salen del propio juego instalado, sin internet.
  Las del catálogo solo hacen falta para la web.

Las imágenes van en Supabase Storage (archivos), no dentro de las tablas; las
tablas guardan la ruta. Para el aviso dentro del juego, la app deja cada icono
ya convertido a un formato mínimo, porque el sysmodule no tiene memoria para
decodificar PNG.
