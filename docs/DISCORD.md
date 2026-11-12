# Servidor de Discord de Greeny

Guía para montar un servidor pequeño y que se pueda manejar entre una sola persona.
Todo lo que sale aquí es **gratis**. Lo que pide *boosts* (mejoras pagadas) está
marcado con 💎 para que sepas que no hace falta ahora.

Las imágenes ya están hechas con los dibujos del proyecto:

- `docs/discord/icono_servidor.png`: icono del servidor (512x512).
- `docs/discord/emojis/`: 17 emojis propios (mascota, medallas, estrella, trofeo, donador...).
- `docs/discord/cabeceras/`: tres imágenes de 1200x400 para bienvenida, reglas y anuncios.

Para volver a generarlas: `python3 scripts/discord_assets.py`.

---

## 1. Lo primero (10 minutos)

1. Crea el servidor con **Crear el mío → Para mí y mis amigos**. Ponle de nombre `Greeny` y súbele `icono_servidor.png`.
2. En **Ajustes del servidor → Habilitar comunidad**, actívalo. Es gratis y desbloquea:
   canal de anuncios, foros, reglas obligatorias, guía del servidor, estadísticas y AutoMod completo.
3. En **Ajustes → Configuración de seguridad**:
   - **Nivel de verificación: Medio.** La cuenta tiene que tener el correo verificado y más de 5 minutos en Discord. Así se van los bots recién creados.
   - **Filtro de contenido explícito:** para todos los miembros.
   - **Reglas:** actívalo para que tengan que aceptar las reglas antes de escribir.
4. En **Ajustes → Notificaciones predeterminadas: Solo @menciones.** Esto es importante: nadie
   se va de un servidor por tranquilo, pero sí por ruidoso.
5. En **Ajustes → Emojis**, sube los 17 de `docs/discord/emojis/`. Hay 50 espacios gratis.
   Pon los nombres tal cual: `:greeny_hola:`, `:oro:`, `:donador:`...

## 2. Roles

De arriba a abajo (el orden importa: un bot solo puede dar los roles que están **debajo** del suyo).

| Rol | Color | Separado en la lista | Para qué |
|---|---|---|---|
| 🌱 Fundador | `#43D058` (verde Greeny) | sí | Tú. Administrador. |
| 🛡️ Moderador | `#2E9E4A` | sí | Para cuando entre alguien del equipo. |
| 🤖 Bots | gris | no | Ponle este rol a los bots y súbelo por encima de los que tienen que dar. |
| 💚 Donador | `#F7C91E` (dorado) | **sí** | Medalla de donador. |
| 🧪 Beta tester | `#7FE08A` | sí | El rol más pedido en Contribuir. |
| 🎨 Colaborador | `#9B8CFF` | sí | Diseño, arte, música. |
| ✅ Verificado | sin color | no | Lo da el captcha al entrar. Sin él no se ve casi nada. |
| 🇪🇸 Español · 🇬🇧 English | sin color | no | Cada uno escoge el suyo (ver sección 5). |
| 🔔 Avisos | sin color | no | Para mencionarlos en anuncios en vez de usar @everyone. |

**La medalla de donador en Discord:**
- Gratis: el rol 💚 Donador con su color dorado, separado en la lista. Así sale en su propio grupo, arriba,
  con el nombre en dorado. El emoji `:donador:` se usa en el mensaje de agradecimiento.
- 💎 Con nivel 2 de boosts, el rol puede llevar **icono de rol**: sería `emojis/donador.png` al lado del nombre.
- Se da a mano, igual que en la web: te llega el correo de PayPal, buscas a la persona y le das el rol.
  Si tiene su Discord vinculado a Greeny, sale en su perfil de la web como "Discord: nombre".

## 3. Canales

Los canales están pensados para una sola persona: lo que no vas a poder atender, no se crea.
Para el nombre, usa un solo emoji y el separador `・`.

```
INICIO
  👋・bienvenida      solo lectura · aquí saluda el bot y está la invitación
  📜・reglas          solo lectura · reglas en español e inglés
  ✅・verificación    solo lo ven los que no están verificados · captcha
  📢・anuncios        canal de ANUNCIOS (se puede seguir desde otros servidores)
  🛠️・cambios         solo lectura · lo llena GitHub solo (ver truco 1)

COMUNIDAD
  💬・general         español
  💬・english         inglés
  🏆・logros          capturas de logros desbloqueados
  🗳️・encuestas       encuestas nativas de Discord

AYUDA
  ❓・soporte         FORO · etiquetas: instalación, app, web, sysmodule, resuelto
  💡・ideas-de-logros FORO · etiquetas: una por juego + "aceptada"

VOZ
  🔊 Sala             para todos los verificados
  🔒 Equipo           privado: solo Fundador, Moderador, Colaborador y Beta tester

EQUIPO                (categoría privada)
  🔒・equipo          chat del equipo
  🔒・registros       aquí escribe el bot de moderación
```

**Permisos, en pocas palabras:**
- `@everyone`: solo puede **ver** `#reglas` y `#verificación`. Quítale *Ver canal* en las categorías COMUNIDAD, AYUDA y VOZ.
- `✅ Verificado`: puede ver COMUNIDAD, AYUDA y VOZ, pero no `#verificación`.
- `#anuncios`, `#cambios`, `#bienvenida`, `#reglas`: los verificados no tienen *Enviar mensajes*. Para que reaccionen, deja *Añadir reacciones*.
- `#encuestas`: los verificados no tienen *Enviar mensajes*, pero sí pueden votar. Las encuestas las creas tú con el botón **+ → Encuesta**,
  que es nativo y gratis. Así ese canal tiene encuestas y nada más.
- Voz `🔒 Equipo` y la categoría EQUIPO: quítale *Ver canal* a `@everyone` y a `Verificado`, y dáselo a los roles del equipo.

**¿Por qué foros para soporte e ideas?** Cada pregunta queda en su propio hilo, con su etiqueta, y se puede buscar.
Así no tienes que contestar lo mismo diez veces en un chat que no se acaba nunca.

## 4. Bots gratis (solo tres)

| Bot | Para qué | Por qué este |
|---|---|---|
| **Captcha.bot** | Captcha al entrar: el que lo pasa recibe `✅ Verificado`. | Hace una sola cosa y la hace bien. Gratis. |
| **Carl-bot** | Bienvenida con imagen, roles por reacción (idioma y 🔔 Avisos) y registro de moderación en `#registros`. | Un solo bot hace tres trabajos. Lo que necesitas está en la versión gratis. |
| **Discohook** *(no es un bot, es una web)* | Mensajes bonitos (embeds) para `#reglas`, `#bienvenida` y `#anuncios`, con el color verde y las cabeceras. | Con eso el servidor no se ve como todos los demás. |

Para la moderación ya tienes **AutoMod**, que viene en Discord (Ajustes → AutoMod) y es gratis:
- Bloquear spam de menciones (más de 5 en un mensaje).
- Bloquear posible spam.
- Una lista de palabras propia. Agrega ahí enlaces de "nitro gratis", "free nitro", "steamcommunity" falso y cosas así.
- Que avise en `#registros`.

Sin MEE6 ni bots de niveles: Greeny ya tiene niveles, puntos y ranking. Poner otro sistema de niveles en Discord solo confunde.

**Cómo configurar Captcha.bot:**
1. Invítalo desde su web oficial (captcha.bot).
2. En su panel, el canal es `#verificación` y el rol para dar es `✅ Verificado`.
3. Arrastra el rol `🤖 Bots` por encima de `✅ Verificado`, o no va a poder darlo.
4. Si algún día te entra una avalancha de bots: Ajustes → Configuración de seguridad → **Pausar invitaciones**.

## 5. Español e inglés sin duplicar todo

- Un canal general por idioma (`#general` y `#english`). Lo demás se comparte.
- Roles de idioma por reacción con Carl-bot, en un mensaje de `#bienvenida`:
  `🇪🇸 → Español` y `🇬🇧 → English`. Por ahora solo sirven para que se vea quién habla qué.
  Si más adelante hay mucha gente, puedes esconder `#english` a quien no tenga ese rol.
- Anuncios bilingües en **un solo mensaje**: el español normal y el inglés debajo como **subtexto**,
  poniendo `-# ` al principio de la línea. Sale en letra pequeña y gris. Queda limpio y no repites el mensaje.

```
## 🎉 Greeny 0.1.8
Comentarios con me gusta en cada juego. Cada me gusta le da 2 puntos al que comentó.
-# Comments with likes on every game. Each like gives the author 2 points.
<@&ID_DEL_ROL_AVISOS>
```

## 6. Trucos para que no se vea genérico

1. **`#cambios` se llena solo con GitHub.** En el canal: Editar canal → Integraciones → Webhooks → Nuevo webhook → Copiar URL.
   En GitHub: repo → Settings → Webhooks → Add webhook → pega la URL **con `/github` al final**,
   *Content type* `application/json`, y marca solo **Releases** (o *Pushes* si quieres cada cambio).
   Cada versión nueva sale en el canal con su enlace, sin hacer nada.
   La URL del webhook es secreta: no la pongas en el repositorio ni en un chat.
2. **Emojis propios en todo.** Usa `:greeny_hola:` en la bienvenida, `:oro:` `:plata:` `:bronce:` al hablar de logros,
   `:greeny_error:` en soporte y `:donador:` al dar las gracias. Los emojis de serie son lo que hace que todos los servidores se vean iguales.
3. **Cabeceras arriba de los canales de inicio.** Publica primero la imagen sola (`cabeceras/bienvenida.png`, etc.) y debajo el mensaje.
   Con Discohook puedes poner la imagen dentro del embed, con la barra lateral en verde `#43D058`.
4. **Un solo color en todo:** el verde Greeny `#43D058` en los embeds y en el rol Fundador. El dorado `#F7C91E` solo para Donador.
   Es la misma paleta que la app y la web.
5. **La Guía del servidor (Server Guide)** es gratis con Comunidad: Ajustes → Incorporación.
   Pon 3 tareas: "Lee las reglas", "Instala Greeny" (con enlace a la web) y "Preséntate en #general".
   Y 3 recursos que apunten a la web: Instalación, Vincular tu consola y Crear logros (`docs/CREAR-LOGROS.md`).
6. **La web y Discord están unidos.** Se puede entrar a la web de Greeny con Discord. Dilo en la bienvenida:
   "Usa *Entrar con Discord* en la web y tu cuenta queda lista".
7. **Una sola invitación, que no caduque.** Créala desde `#bienvenida`, con duración *Nunca* y usos *Sin límite*.
   Esa es la que va en la app y en la web (sección 8).
8. **Mensajes de sistema apagados:** Ajustes → Vista general → desactiva "Enviar un mensaje de bienvenida aleatorio"
   y los avisos de boost. Ya saluda Carl-bot con la imagen.
9. **Estado de "trabajando en":** en `#anuncios`, un mensaje fijado con lo que viene después. Por ejemplo:
   *"Ahora mismo: logros de trajes en Mario 3D World"*. Da vida aunque publiques poco.

**Lo que NO es gratis (💎), para que no pierdas tiempo buscándolo:**
- **Etiqueta del servidor (server tag)**, la que la gente se pone al lado del nombre: hoy pide boosts (3).
  Cuando la tengas, ponle `GRNY` con el emoji de la G.
- Icono en los roles: nivel 2 (7 boosts).
- Banner del servidor: nivel 2. Fondo de la invitación: nivel 1.
- URL propia `discord.gg/greeny`: nivel 3.

Discord cambia estos requisitos de vez en cuando. Si ves que algo se volvió gratis, ponlo.

## 7. Textos listos para pegar

### #reglas

```
## :greeny_revisa: Reglas · Rules

**1. Respeto.** Nada de insultos, acoso ni odio.
-# Be respectful. No insults, harassment or hate.
**2. Sin piratería.** No se comparten juegos, claves ni enlaces para bajar juegos. Greeny no tiene nada que ver con eso.
-# No piracy. No games, keys or download links.
**3. Sin spam ni publicidad** de otros servidores o productos.
-# No spam or advertising.
**4. Soporte en #soporte**, un hilo por problema. Di qué versión de Greeny y de Atmosphère tienes.
-# Support goes in #soporte, one thread per issue. Include your Greeny and Atmosphère versions.
**5. Nada de trampas en el ranking.** Si alguien desbloquea logros a mano o con trucos, su cuenta puede salir del ranking.
-# No cheating the ranking.
**6. Greeny es un proyecto de fans.** No está afiliado ni avalado por Nintendo.
-# Greeny is a fan project, not affiliated with or endorsed by Nintendo.
```

### #bienvenida (mensaje fijo)

```
## :greeny_hola: ¡Bienvenido a Greeny!
El primer sistema de logros no oficial para la Switch.

:punto: **Web:** https://jhondydario.github.io/Greeny/
:trofeo: **Ranking, perfiles y comentarios** en la web y en la app.
:greeny_busca: **¿Problemas?** Abre un hilo en #soporte.
:donador: **¿Quieres ayudar?** Mira la página Contribuir de la web.

Escoge tu idioma · Pick your language: 🇪🇸 / 🇬🇧
Y si quieres que te avisemos de las versiones nuevas: 🔔
```

### Bienvenida automática (Carl-bot → Welcome)

```
:greeny_hola: ¡Hola {user}! Bienvenido a **Greeny**. Pasa por #reglas y después preséntate en #general.
-# Hi {user}! Check #reglas, then say hi in #english.
```

Ponle la imagen `cabeceras/bienvenida.png` subiéndola a un canal y copiando su enlace.

## 8. Poner la invitación en la app y en la web

Ya están preparadas: solo hay que escribir la invitación en **un archivo**, y las dos la leen de ahí.

1. Edita `web/links.json`:
   ```json
   { "discord": "https://discord.gg/TU_CODIGO" }
   ```
2. Sube el cambio a GitHub (`main`). En un par de minutos:
   - **Web:** sale "Discord" en la barra de arriba y en el pie, y en *Contribuir* una tarjeta "Comunidad en Discord".
   - **App:** en *Opciones → Ayuda → Acerca de* sale un **código QR** para escanearlo con el celular y la invitación en texto.
     Se lee al abrir Acerca de, así que no hace falta sacar otra versión de la app.

Solo se aceptan enlaces `https://discord.gg/...` o `https://discord.com/invite/...`. Con cualquier otra cosa no se muestra nada.
