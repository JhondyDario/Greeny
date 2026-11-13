// Español e inglés. La web se escribe en español (app.js) y aquí se traduce lo que ya está en la
// página: cada texto se busca tal cual en el diccionario. Lo que no esté, se queda en español.
// No se traducen nombres de usuario, frases ni nombres de juegos: solo coincide el texto exacto.
(function () {
  "use strict";

  const EN = {
    // barra y pie
    "Juegos": "Games", "Ranking": "Ranking", "Donar": "Donate", "Entrar": "Sign in", "Crear cuenta": "Create account",
    "Mi cuenta": "My account", "Mi perfil": "My profile", "Ajustes": "Settings", "Salir": "Sign out", "Desarrollador": "Developer",
    "Saltar al contenido": "Skip to content", "Principal": "Main", "Cargando…": "Loading…",
    "Proyecto de fans. No está afiliado ni avalado por Nintendo.": "Fan project. Not affiliated with or endorsed by Nintendo.",
    "Código y contacto en GitHub": "Source and contact on GitHub",
    // estados y dificultad
    "Sin empezar": "Not started", "Todavía no tiene logros útiles.": "No useful achievements yet.",
    "En prueba": "Testing", "Tiene logros, pero pueden fallar o cambiar.": "Has achievements, but they may fail or change.",
    "Jugable": "Playable", "Funciona bien; faltan logros por añadir.": "Works well; more achievements to come.",
    "Perfecto": "Perfect", "Lista de logros completa y probada.": "Achievement list complete and tested.",
    "Bronce": "Bronze", "Plata": "Silver", "Oro": "Gold", "Dificultad: ": "Difficulty: ",
    // errores
    "Correo o contraseña incorrectos.": "Wrong email or password.",
    "Todavía no has confirmado tu correo. Revisa tu bandeja de entrada.": "You have not confirmed your email yet. Check your inbox.",
    "Ya existe una cuenta con ese correo.": "An account with that email already exists.",
    "La contraseña es muy corta.": "The password is too short.",
    "Ese nombre de usuario ya está ocupado.": "That username is already taken.",
    "El usuario solo puede tener letras, números y _ (de 3 a 20).": "Usernames can only have letters, numbers and _ (3 to 20).",
    "La frase puede tener hasta 60 caracteres.": "The caption can be up to 60 characters.",
    "La contraseña nueva tiene que ser distinta a la anterior.": "The new password must be different from the old one.",
    "Vincular Discord a una cuenta existente todavía no está activado en el servidor.": "Linking Discord to an existing account is not enabled on the server yet.",
    "Esa cuenta de Discord ya está vinculada a otra cuenta de Greeny.": "That Discord account is already linked to another Greeny account.",
    "Demasiados intentos. Espera un momento y prueba de nuevo.": "Too many attempts. Wait a moment and try again.",
    "No se pudo conectar. Revisa tu conexión.": "Could not connect. Check your connection.",
    "No hay nadie con ese código de amigo.": "Nobody has that friend code.",
    "El código de amigo tiene 6 números.": "Friend codes have 6 digits.",
    "Ese es tu propio código.": "That is your own code.",
    "El servidor todavía no tiene esta función. Hay que actualizar la base de datos.": "The server does not have this feature yet. The database needs updating.",
    "Código incorrecto o vencido. Pide uno nuevo en la consola.": "Wrong or expired code. Get a new one on the console.",
    "Tienes que iniciar sesión.": "You need to sign in.", "No tienes permiso para hacer eso.": "You are not allowed to do that.",
    "Ese nombre de usuario está reservado. Elige otro.": "That username is reserved. Pick another.",
    "Ese nombre de usuario ya está ocupado. Elige otro.": "That username is already taken. Pick another.",
    "Algo salió mal": "Something went wrong", "Reintentar": "Try again",
    "No encontrado": "Not found", "Esa página no existe.": "That page does not exist.", "Volver al inicio": "Back to home",
    // portada
    "Logros para tu Switch": "Achievements for your Switch",
    "Greeny corre en segundo plano mientras juegas, te avisa cuando desbloqueas un logro y guarda tu progreso. Aquí lo ves, lo compartes y lo comparas.":
      "Greeny runs in the background while you play, tells you when you unlock an achievement and saves your progress. See it, share it and compare it here.",
    "Vincular mi consola": "Link my console", "Ver juegos": "Browse games", "Descargar en GitHub": "Download on GitHub",
    "Greeny, la mascota: una plantita verde saludando": "Greeny, the mascot: a little green sprout waving",
    "Números de Greeny": "Greeny in numbers", "Greeny en números": "Greeny in numbers",
    "jugadores": "players", "logros desbloqueados": "achievements unlocked", "juegos con logros": "games with achievements",
    "Últimos logros": "Latest achievements", "Todavía nadie ha subido logros. ¡Sé el primero!": "Nobody has uploaded achievements yet. Be the first!",
    "Cómo funciona": "How it works",
    "Instala Greeny en tu Switch con Atmosphère (las instrucciones están en GitHub).": "Install Greeny on your Switch with Atmosphère (instructions are on GitHub).",
    "Juega. Los logros salen en pantalla y se guardan en la consola.": "Play. Achievements pop up on screen and are saved on the console.",
    "Crea tu cuenta aquí y vincula tu consola con un código.": "Create your account here and link your console with a code.",
    "Tu progreso aparece en tu perfil para que lo compartas.": "Your progress shows on your profile so you can share it.",
    // juegos
    "Todavía no hay juegos publicados.": "No games published yet.",
    "Cada lista de logros sirve para una versión concreta del juego.": "Each achievement list works with one specific version of the game.",
    "Versión ": "Version ", "Versión sin indicar": "Version not specified", " · sin publicar": " · unpublished",
    "Ese juego no está en Greeny.": "That game is not on Greeny.", "Logros para la versión ": "Achievements for version ",
    "Tu progreso": "Your progress", "Logros": "Achievements", "Sin descripción": "No description",
    "Desbloqueado": "Unlocked", "Bloqueado": "Locked",
    "Porcentaje de jugadores de este juego que lo tienen": "Share of this game's players who have it", "Nadie lo tiene aún": "Nobody has it yet",
    // perfil
    "No existe ningún jugador con ese nombre.": "There is no player with that name.",
    "Siguiendo": "Following", "Seguir": "Follow", " · Discord: ": " · Discord: ", "Código de amigo: ": "Friend code: ",
    "En Greeny desde ": "On Greeny since ", "Editar": "Edit", "Medallas ": "Badges ", "Conseguida": "Earned", "Completado": "Completed",
    "puntos": "points", "logros": "achievements", "mis juegos": "my games", "platinados": "platinum", "seguidores": "followers", "siguiendo": "following",
    "Todavía no has subido logros. Vincula tu consola en Ajustes.": "You have not uploaded achievements yet. Link your console in Settings.",
    "Este jugador todavía no ha subido logros.": "This player has not uploaded achievements yet.",
    "Copiar": "Copy", "Copiado": "Copied", "Donador": "Supporter", "Apoya a Greeny con una donación.": "Supports Greeny with a donation.",
    // medallas
    "Primer brote": "First sprout", "Desbloquea tu primer logro.": "Unlock your first achievement.",
    "Coleccionista": "Collector", "Desbloquea 10 logros.": "Unlock 10 achievements.",
    "Cazatrofeos": "Trophy hunter", "Desbloquea 50 logros.": "Unlock 50 achievements.",
    "Leyenda": "Legend", "Desbloquea 200 logros.": "Unlock 200 achievements.",
    "Primer platino": "First platinum", "Consigue todos los logros de un juego.": "Earn every achievement in a game.",
    "Completista": "Completionist", "Platina 5 juegos.": "Platinum 5 games.",
    "Explorador": "Explorer", "Consigue logros en 3 juegos distintos.": "Earn achievements in 3 different games.",
    "Trotamundos": "Globetrotter", "Consigue logros en 10 juegos distintos.": "Earn achievements in 10 different games.",
    "Cien puntos": "A hundred points", "Llega a 100 puntos.": "Reach 100 points.",
    "Quinientos": "Five hundred", "Llega a 500 puntos.": "Reach 500 points.",
    "Imparable": "Unstoppable", "Llega a 2000 puntos.": "Reach 2000 points.",
    // entrar y cuenta
    "Continuar con Discord": "Continue with Discord", "Hola de nuevo": "Welcome back",
    "Entra para ver tus logros, tu nivel y tu puesto en el ranking.": "Sign in to see your achievements, your level and your rank.",
    "Entra a tu cuenta": "Sign in to your account", "Vincula tu consola": "Link your console", "Juega y desbloquea logros": "Play and unlock achievements",
    "Correo": "Email", "Contraseña": "Password", "¿Olvidaste tu contraseña?": "Forgot your password?",
    "¿No tienes cuenta? ": "No account yet? ", "Créala aquí": "Create one here",
    "Únete a Greeny": "Join Greeny", "Crea tu cuenta gratis y lleva la cuenta de tus logros de Switch.": "Create a free account and keep track of your Switch achievements.",
    "Crea tu cuenta": "Create your account", "Nombre de usuario": "Username",
    "Letras, números y _ (de 3 a 20). Es el de tu perfil público.": "Letters, numbers and _ (3 to 20). It is the one on your public profile.",
    "Mínimo 8 caracteres.": "At least 8 characters.",
    "Te enviamos un correo para confirmar tu cuenta. Ábrelo y sigue el enlace.": "We sent you an email to confirm your account. Open it and follow the link.",
    "¿Ya tienes cuenta? ": "Already have an account? ", "Entra aquí": "Sign in here",
    "Recuperar contraseña": "Reset password",
    "Escribe el correo de tu cuenta y te enviamos un enlace para poner una contraseña nueva.": "Type your account email and we will send you a link to set a new password.",
    "Enviar enlace": "Send link",
    "Si hay una cuenta con ese correo, te llegará un enlace en unos minutos. Revisa también el correo no deseado.": "If there is an account with that email, a link will arrive in a few minutes. Check your spam folder too.",
    "Volver a entrar": "Back to sign in", "Nueva contraseña": "New password",
    "El enlace ya no sirve o venció. Pide uno nuevo.": "The link no longer works or has expired. Request a new one.",
    "Contraseña nueva": "New password", "Guardar contraseña": "Save password", "Contraseña cambiada.": "Password changed.",
    "Ver": "Show", "Ocultar": "Hide", "Ver contraseña": "Show password", "Ocultar contraseña": "Hide password",
    "No se encontró tu perfil. Cierra sesión y vuelve a entrar.": "Your profile was not found. Sign out and sign in again.",
    // ajustes
    "Tu perfil público: ": "Your public profile: ", "Perfil": "Profile", "Consola": "Console", "Amigos": "Friends", "Seguridad": "Security", "Secciones": "Sections",
    "Foto de perfil": "Profile picture", "Elige una. Es la misma en la app y en la web.": "Pick one. It is the same in the app and on the website.",
    "Escribe tu frase": "Type your caption", "Frase personalizada": "Custom caption", "Sin frase": "No caption", "Escribir la mía…": "Write my own…",
    "Si lo cambias, cambia el enlace de tu perfil.": "Changing it changes your profile link.",
    "Nombre visible": "Display name", "Opcional. Hasta 32 caracteres.": "Optional. Up to 32 characters.",
    "Frase de tu perfil": "Profile caption", "Elige una o escribe la tuya (hasta 60 caracteres).": "Pick one or write your own (up to 60 characters).",
    "País": "Country", "Opcional. Sale como bandera en tu perfil y en el ranking.": "Optional. Shown as a flag on your profile and in the ranking.",
    "Sin indicar": "Not set", "Guardar perfil": "Save profile", "Perfil guardado.": "Profile saved.",
    "Código que muestra tu consola": "Code shown on your console", "Vincular consola": "Link console",
    "Vincular una consola": "Link a console",
    "En tu Switch, abre Greeny y elige vincular cuenta. Te mostrará un código de 6 caracteres que dura 10 minutos.": "On your Switch, open Greeny and choose to link an account. It will show a 6-character code that lasts 10 minutes.",
    "Consolas vinculadas": "Linked consoles", "Vinculada el ": "Linked on ", " · última vez ": " · last seen ", " · todavía no ha subido nada": " · has not uploaded anything yet",
    "Desvincular": "Unlink", "No tienes ninguna consola vinculada.": "You have no linked consoles.",
    "Tu código: ": "Your code: ", "Compártelo para que te sigan.": "Share it so people can follow you.",
    "Seguir a alguien con su código": "Follow someone with their code", "6 números. El GR y el guion son opcionales.": "6 digits. The GR and the dash are optional.",
    "Cambiar contraseña": "Change password", "Cuenta de Discord": "Discord account", "Puedes entrar a Greeny con Discord.": "You can sign in to Greeny with Discord.",
    "Vinculado": "Linked", "Sin vincular": "Not linked", "Vincúlalo para entrar con Discord a esta misma cuenta.": "Link it to sign in to this same account with Discord.",
    "Vincular Discord": "Link Discord",
    // ranking
    "Los 10 jugadores con más puntos. Toca a alguien para ver su perfil y seguirlo.": "The 10 players with the most points. Tap someone to see their profile and follow them.",
    "Global": "Global", "Por país": "By country", "Clasificar por": "Rank by", "Puntos totales": "Total points",
    "Para ver el ranking de tus amigos tienes que ": "To see your friends' ranking you need to ", "entrar": "sign in",
    "Todavía no hay nadie con puntos aquí.": "Nobody has points here yet.", "Tu posición": "Your position",
    "Todavía no apareces aquí: te falta tener puntos en esta clasificación.": "You are not here yet: you need points in this ranking.",
    // donar
    "Apoya a Greeny": "Support Greeny",
    "Greeny es gratis y lo va a seguir siendo. Lo hace una sola persona en su tiempo libre.": "Greeny is free and will stay free. It is made by one person in their spare time.",
    "Con tu donación se puede:": "Your donation helps to:",
    "Tener una página web con dominio propio.": "Get the website its own domain.",
    "Ampliar el catálogo de juegos con logros.": "Grow the catalogue of games with achievements.",
    "Mejorar la comunidad de Discord.": "Improve the Discord community.",
    "Darle mantenimiento a todo el proyecto.": "Keep the whole project maintained.",
    "Donar con PayPal": "Donate with PayPal",
    "Medalla de donador": "Supporter badge",
    "Contribuir": "Contribute", "Ver en español": "Ver en español", "View in English": "View in English",
    "Greeny es gratis y lo va a seguir siendo. Lo hace una sola persona en su tiempo libre: hay dos formas de echar una mano.":
      "Greeny is free and will stay free. It is made by one person in their spare time: there are two ways to help.",
    "Únete al equipo": "Join the team", "Fundador y programador": "Founder and programmer", "Se busca": "Wanted",
    "Beta tester": "Beta tester", "Moderador de Discord": "Discord moderator", "Diseño y arte": "Design and art", "Música": "Music",
    "Prueba la app y busca los valores de memoria con los que se hacen los logros. Es lo que más falta hace ahora mismo.":
      "Tests the app and finds the memory values achievements are built from. This is what we need most right now.",
    "Cuida la comunidad de Discord: responde dudas y mantiene el orden.": "Looks after the Discord community: answers questions and keeps things friendly.",
    "Iconos de logros, medallas, mascotas y todo lo que se ve en la app y la web.": "Achievement icons, badges, mascots and everything you see in the app and on the website.",
    "Canciones de fondo y sonidos para la app.": "Background songs and sounds for the app.",
    "Tus postulaciones": "Your applications", "En revisión": "Under review", "Aceptada": "Accepted", "No esta vez": "Not this time",
    "Postúlate": "Apply", "Rol": "Role", "Cómo contactarte": "How to reach you",
    "Tu usuario de Discord, o un correo si lo prefieres.": "Your Discord username, or an email if you prefer.",
    "Cuéntanos de ti": "Tell us about yourself",
    "Opcional. Qué juegos tienes, qué sabes hacer, cuánto tiempo puedes dedicarle.": "Optional. Which games you have, what you can do, how much time you can give.",
    "Enviar postulación": "Send application", "¡Gracias! Te escribiremos por el contacto que dejaste.": "Thank you! We will reach out using the contact you left.",
    "Para postularte tienes que ": "To apply you need to ", " con tu cuenta de Greeny.": " with your Greeny account.",
    "Quien dona recibe esta medalla en su perfil. Para que sepamos a quién dársela, escribe tu nombre de usuario de Greeny en la nota de la donación.":
      "Everyone who donates gets this badge on their profile. So we know who to give it to, write your Greeny username in the donation note.",
    "La medalla se pone a mano, así que puede tardar un poco en aparecer.": "The badge is added by hand, so it may take a little while to show up.",
    "Donar es opcional: no desbloquea logros ni da ventajas.": "Donating is optional: it does not unlock achievements or give any advantage.",
    "Proyecto": "Project", "Ayuda": "Help", "Cuenta": "Account", "Cómo funciona": "How it works", "Descargar": "Download",
    "Preguntas frecuentes": "FAQ", "Comunidad en Discord": "Discord community", "Reportar un problema": "Report a problem",
    "Términos del servicio": "Terms of service", "Política de privacidad": "Privacy policy", "Avisos": "Notifications", "Síguenos": "Follow us",
    "El primer sistema de logros no oficial para la Switch. Gratis, de fans y de código abierto.": "The first unofficial achievement system for the Switch. Free, fan-made and open source.",
    "Marcar todo como leído": "Mark all as read", "jugadores activos hoy": "players active today", "¿Nuevo por aquí?": "New here?",
    "Mira en un minuto qué es Greeny y cómo empezar.": "See what Greeny is and how to start, in a minute.", "Cómo funciona →": "How it works →",
    "Ver todas las preguntas": "See all questions", "Borrar mi cuenta": "Delete my account", "Borra todo de una vez": "Delete everything at once",
    "No tienes avisos todavía. Aquí sale cuando alguien te sigue, le da me gusta a tu comentario o consigues algo nuevo.": "No notifications yet. They show up when someone follows you, likes your comment or you earn something new.",
    "¿Qué es Greeny?": "What is Greeny?", "¿Está Greeny afiliado a Nintendo?": "Is Greeny affiliated with Nintendo?", "¿Qué necesito para usarlo?": "What do I need?",
    "¿Greeny me da juegos o los modifica?": "Does Greeny give me games or change them?", "¿Puedo usarlo jugando en línea?": "Can I use it while playing online?",
    "Me gusta recibidos en sus comentarios": "Likes received on their comments",
    "Amigo": "Friend", "Tú": "You",
    "Ver todos": "See all", "Ningún juego tiene ese estado todavía.": "No game has that status yet.",
    "Código de amigo": "Friend code", "En Greeny desde": "On Greeny since", "Editar perfil": "Edit profile",
    "Comentarios": "Comments", "Tu comentario": "Your comment", "Escribe un comentario": "Write a comment", "Publicar": "Post",
    "Guardar": "Save", "Borrar": "Delete", "Listo.": "Done.", "Entra": "Sign in", " para comentar y dar me gusta.": " to comment and like.",
    "Nadie ha comentado este juego todavía. ¡Sé el primero!": "Nobody has commented on this game yet. Be the first!",
    "No se pudieron cargar los comentarios.": "Comments could not be loaded.", "Escribe un poco más.": "Write a bit more.",
    "¿Borrar tu comentario?": "Delete your comment?", "Es tu comentario": "It's your comment", "Entra para dar me gusta": "Sign in to like",
    "¿Qué te parecen los logros de este juego?": "What do you think of this game's achievements?", "Menú": "Menu",
    "Comunidad en Discord": "Discord community", "Entrar al Discord": "Join the Discord",
    "Avisos de juegos nuevos, ayuda con la instalación, ideas de logros y encuestas.": "New game announcements, install help, achievement ideas and polls.",
    "Marca la casilla de verificación (no soy un robot).": "Tick the verification box (I'm not a robot).",
    "La verificación de \"no soy un robot\" falló o venció. Márcala de nuevo.": "The \"I'm not a robot\" check failed or expired. Tick it again.",
    "No puedes darle me gusta a tu propio comentario.": "You can't like your own comment.",
  };

  // Textos con números: se traducen por partes. Solo se aplican si el texto trae algún número.
  const RULES = [
    [/(\d+) de (\d+) logros/g, "$1 of $2 achievements"], [/(\d+) de (\d+) puntos para el nivel (\d+)/g, "$1 of $2 points to level $3"],
    [/(\d+) \/ (\d+) pts para el nivel (\d+)/g, "$1 / $2 pts to level $3"], [/^(\d+) de (\d+)$/g, "$1 of $2"], [/(\d+) de (\d+) · /g, "$1 of $2 · "],
    [/ logros · /g, " achievements · "], [/ logros$/g, " achievements"], [/ jugador\(es\) en esta clasificación\./g, " player(s) in this ranking."],
    [/ jugadores/g, " players"], [/ jugador\b/g, " player"], [/Nivel (\d+)/g, "Level $1"], [/(\d+)% lo tiene/g, "$1% have it"],
    [/^Foto (\d+)$/g, "Picture $1"], [/^Ver todos \((\d+)\)$/g, "See all ($1)"], [/^Logros actualizados el /g, "Achievements updated on "],
    [/ pts · /g, " pts · "], [/ · editado$/g, " · edited"], [/ con Greeny abierto ahora$/g, " with Greeny open right now"],
  ];

  let lang = "es";
  try { lang = localStorage.getItem("greeny-lang") || ((navigator.language || "es").toLowerCase().startsWith("es") ? "es" : "en"); }
  catch (e) { /* sin almacenamiento: español */ }
  if (lang !== "en") lang = "es";

  function translate(text) {
    const core = text.trim();
    if (!core) return text;
    const lead = text.slice(0, text.indexOf(core.charAt(0))), tail = text.slice(lead.length + core.length);
    if (Object.prototype.hasOwnProperty.call(EN, text)) return EN[text];
    if (Object.prototype.hasOwnProperty.call(EN, core)) return lead + EN[core] + tail;
    if (!/\d/.test(core)) return text;
    let out = text;
    for (const [re, to] of RULES) out = out.replace(re, to);
    return out;
  }

  const original = new WeakMap(); // nodo de texto -> su texto en español
  const ATTRS = ["placeholder", "title", "aria-label", "alt"];

  function applyNode(node) {
    if (node.nodeType === 3) {
      if (!original.has(node)) original.set(node, node.nodeValue);
      const es = original.get(node), want = lang === "en" ? translate(es) : es;
      if (node.nodeValue !== want) node.nodeValue = want;
      return;
    }
    if (node.nodeType !== 1 || node.tagName === "SCRIPT" || node.tagName === "STYLE" || node.tagName === "CODE") return;
    for (const a of ATTRS) {
      if (!node.hasAttribute(a)) continue;
      const key = "data-es-" + a;
      if (!node.hasAttribute(key)) node.setAttribute(key, node.getAttribute(a));
      const es = node.getAttribute(key), want = lang === "en" ? translate(es) : es;
      if (node.getAttribute(a) !== want) node.setAttribute(a, want);
    }
    for (const kid of node.childNodes) applyNode(kid);
  }

  function apply() {
    document.documentElement.lang = lang;
    applyNode(document.body);
  }

  window.GreenyI18n = {
    get lang() { return lang; },
    set(next) {
      lang = next === "en" ? "en" : "es";
      try { localStorage.setItem("greeny-lang", lang); } catch (e) { /* da igual */ }
      apply();
    },
    apply,
  };

  // Lo que la página añade después (cada vista, cada mensaje) se traduce al aparecer.
  document.addEventListener("DOMContentLoaded", () => {
    apply();
    new MutationObserver((list) => {
      for (const m of list) for (const n of m.addedNodes) applyNode(n);
    }).observe(document.body, { childList: true, subtree: true });
  });
})();
