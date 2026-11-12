// Paginas de texto de la web: como funciona, preguntas frecuentes, terminos y privacidad.
// Escritas para cualquier persona, sin palabras tecnicas. Reciben h() de app.js.
(function () {
  "use strict";

  const UPDATED = "12 de noviembre de 2026";
  const UPDATED_EN = "November 12, 2026";
  const isEn = () => window.GreenyI18n && window.GreenyI18n.lang === "en";

  function step(h, n, title, ...body) {
    return h("div", { class: "card how-step reveal" },
      h("span", { class: "how-num", "aria-hidden": "true" }, String(n)),
      h("div", {}, h("h3", {}, title), ...body));
  }
  const p = (h, ...t) => h("p", {}, ...t);
  const li = (h, ...t) => h("li", {}, ...t);

  // ---------- preguntas frecuentes ----------
  const FAQ = [
    ["¿Qué es Greeny?",
      "Un sistema de logros para la Switch, hecho por fans. Mientras juegas, Greeny se fija en lo que haces (monedas, vidas, estrellas…) y cuando cumples un reto te avisa con un mensaje y un sonido, como en otras consolas."],
    ["¿Está Greeny afiliado a Nintendo?",
      "No. Es un proyecto de fans, gratis y sin ánimo de lucro. No está afiliado ni avalado por Nintendo, y no usa nada de Nintendo: los dibujos, la mascota y los sonidos son propios."],
    ["¿Qué necesito para usarlo?",
      "Una Switch con Atmosphère (el sistema personalizado más usado), una tarjeta SD y el juego. Internet solo hace falta si quieres subir tus logros a la web, ver el ranking o bajar logros de juegos nuevos."],
    ["¿Greeny me da juegos o los modifica?",
      "No. Greeny no trae juegos ni los cambia: solo mira, sin tocar nada, y anota cuando consigues algo. Tienes que tener tus propios juegos."],
    ["¿Puedo usarlo jugando en línea?",
      "Te recomendamos que no. Juega sin conexión cuando uses una consola con sistema personalizado. Greeny no hace trampas, pero el sistema personalizado en sí puede dar problemas en línea."],
    ["¿Y si un juego no tiene logros todavía?",
      "En la sección Juegos ves cuáles los tienen y en qué punto van. Si quieres ayudar a hacer los de tu juego favorito, mira la página Contribuir."],
    ["¿Se pierden mis logros si no tengo internet?",
      "No. Se guardan en tu tarjeta SD. Cuando vuelvas a tener internet, la app los sube a tu cuenta."],
    ["¿Necesito una cuenta?",
      "No para jugar. La cuenta es para tener tu perfil en la web, salir en el ranking, comentar juegos y seguir a otros jugadores."],
    ["¿Qué se hace con mis datos?",
      "Solo lo necesario para que funcione tu perfil: tu usuario, tu correo para entrar y los logros que subes. No hay anuncios y no vendemos nada. Lo explicamos en la página de privacidad, y puedes borrar tu cuenta cuando quieras."],
    ["¿Cómo puedo ayudar?",
      "Probando la app, buscando los valores de los juegos para crear logros, con arte o música, o con una donación. Todo está en la página Contribuir."],
  ];

  const FAQ_EN = [
    ["What is Greeny?", "A fan-made achievement system for the Switch. While you play, Greeny watches what you do (coins, lives, stars…) and when you complete a challenge it lets you know with a pop-up and a sound, like on other consoles."],
    ["Is Greeny affiliated with Nintendo?", "No. It is a free, non-profit fan project. It is not affiliated with or endorsed by Nintendo, and it uses nothing from Nintendo: the art, the mascot and the sounds are our own."],
    ["What do I need to use it?", "A Switch with Atmosphère (the most used custom firmware), an SD card and the game. You only need internet to upload your achievements to the website, see the ranking or download achievements for new games."],
    ["Does Greeny give me games or change them?", "No. Greeny does not include or change games: it only watches, without touching anything, and writes down when you earn something. You need your own games."],
    ["Can I use it while playing online?", "We recommend you don't. Play offline when using a console with custom firmware. Greeny does not cheat, but custom firmware itself can cause trouble online."],
    ["What if a game has no achievements yet?", "The Games section shows which ones have them and how far along they are. If you want to help make achievements for your favorite game, check the Contribute page."],
    ["Do I lose my achievements without internet?", "No. They are saved on your SD card. When you are back online, the app uploads them to your account."],
    ["Do I need an account?", "Not to play. The account is for having your profile on the website, appearing in the ranking, commenting on games and following other players."],
    ["What happens with my data?", "Only what is needed for your profile to work: your username, your email to sign in and the achievements you upload. There are no ads and we sell nothing. It is all explained on the privacy page, and you can delete your account at any time."],
    ["How can I help?", "By testing the app, finding game values to create achievements, with art or music, or with a donation. It is all on the Contribute page."],
  ];

  function faq(h, limit) {
    const list = isEn() ? FAQ_EN : FAQ;
    const items = (limit ? list.slice(0, limit) : list).map(([q, a]) =>
      h("details", { class: "faq-item" }, h("summary", {}, h("span", {}, q), h("span", { class: "faq-plus", "aria-hidden": "true" }, "+")),
        h("p", {}, a)));
    return h("section", { class: "card faq reveal", id: "preguntas" }, h("h2", {}, "Preguntas frecuentes"), ...items,
      limit ? h("p", { class: "faq-more" }, h("a", { href: "#/preguntas" }, "Ver todas las preguntas")) : null);
  }

  // ---------- como funciona ----------
  function howItWorks(h, cfg) {
    return [
      h("h1", {}, "Cómo funciona"),
      h("p", { class: "lead muted" }, "Todo lo que hace Greeny, explicado sin palabras raras. Son tres partes que trabajan juntas: algo que vigila tus juegos, una app en tu Switch y esta página web."),

      h("section", { class: "how-parts" },
        h("div", { class: "card how-part reveal" }, h("img", { src: "img/m_buscando.png", alt: "", width: 96, height: 96, loading: "lazy" }),
          h("h3", {}, "El vigilante"), h("p", { class: "muted" }, "Trabaja escondido mientras juegas. Cuando cumples un reto, te avisa arriba a la derecha y suena.")),
        h("div", { class: "card how-part reveal" }, h("img", { src: "img/m_celebrando.png", alt: "", width: 96, height: 96, loading: "lazy" }),
          h("h3", {}, "La app"), h("p", { class: "muted" }, "Se abre desde el menú de homebrew. Ahí ves todos tus juegos, tus logros, tu progreso y tu perfil.")),
        h("div", { class: "card how-part reveal" }, h("img", { src: "img/m_saludando.png", alt: "", width: 96, height: 96, loading: "lazy" }),
          h("h3", {}, "La web"), h("p", { class: "muted" }, "Tu perfil público, el ranking, los comentarios de cada juego y la cuenta para guardar tus logros."))),

      h("h2", {}, "1. Instalarlo"),
      step(h, 1, "Lo que necesitas",
        p(h, "Una Switch con Atmosphère, una tarjeta SD y una computadora o celular para copiar archivos. Si ya usas homebrew, ya tienes casi todo.")),
      step(h, 2, "Copia Greeny a la tarjeta",
        p(h, "Descarga ", h("a", { href: cfg.repoUrl, rel: "noopener" }, "greeny.zip"), " y descomprímelo en la raíz de la tarjeta SD (que las carpetas ", h("code", {}, "atmosphere"), " y ", h("code", {}, "switch"), " se junten con las que ya tienes).")),
      step(h, 3, "Reinicia la consola",
        p(h, "Apágala y préndela de nuevo (no basta con suspenderla). Así arranca el vigilante.")),
      step(h, 4, "Abre la app una vez",
        p(h, "Busca Greeny en el menú de homebrew. La primera vez te enseña cómo se usa y revisa que todo esté bien. Si algo falla, en Opciones → Ayuda → Diagnóstico te dice qué es y cómo arreglarlo.")),

      h("h2", {}, "2. Jugar"),
      step(h, 1, "Abre un juego que tenga logros",
        p(h, "Arriba a la derecha sale \"Logros activos\" con cuántos llevas. Si te molesta ese aviso, lo puedes quitar en Opciones → En el juego.")),
      step(h, 2, "Juega normal",
        p(h, "Cuando consigues un logro sale un aviso con su medalla (bronce, plata u oro) y un sonido. No tienes que hacer nada más: queda guardado en la tarjeta, con o sin internet."),
        p(h, "Greeny solo mira: no cambia nada del juego ni de tus partidas.")),
      step(h, 3, "¿Quieres verlos sin salir del juego?",
        p(h, "Si tienes Tesla instalado (un menú que se abre encima del juego), activa \"Ver logros en el juego\" en Opciones → En el juego. Luego, jugando, abres Tesla y eliges Greeny. Es opcional: sin Tesla todo lo demás funciona igual.")),

      h("h2", {}, "3. La app en tu Switch"),
      h("div", { class: "card reveal" },
        h("ul", { class: "how-list" },
          li(h, h("strong", {}, "Mis juegos: "), "tus juegos con su carátula, cuántos logros llevas y tus puntos. Con − cambias entre lista y cuadrícula."),
          li(h, h("strong", {}, "Dentro de un juego: "), "cada logro, cómo se consigue y, si vas por la mitad, cuánto te falta (por ejemplo 40 de 99 monedas). Con X filtras los que tienes o te faltan."),
          li(h, h("strong", {}, "Buscar (L): "), "todos los juegos con logros en Greeny, aunque no los tengas. Ahí ves cuándo se actualizaron sus logros."),
          li(h, h("strong", {}, "Comentarios (Y): "), "lo que opina la gente de cada juego. Puedes dar me gusta y escribir el tuyo."),
          li(h, h("strong", {}, "Perfil (R): "), "tu nivel, tus puntos, tus medallas, tus seguidores y el ranking."),
          li(h, h("strong", {}, "Avisos (ZL o ZR): "), "cuando alguien te sigue, le da me gusta a tu comentario o consigues una medalla. Se abren desde cualquier pantalla."),
          li(h, h("strong", {}, "Opciones (X): "), "idioma, tema claro u oscuro, sonidos, música, tu cuenta y la ayuda."))),

      h("h2", {}, "4. Tu cuenta y la web"),
      step(h, 1, "Crea tu cuenta",
        p(h, "Aquí en la web, con tu correo o con Discord. Es gratis.")),
      step(h, 2, "Vincula tu consola",
        p(h, "En la app: Opciones → Cuenta → Vincular. La consola te muestra un código de 6 letras; lo escribes en la web, en Ajustes, y listo. También puedes entrar con tu correo y contraseña desde la consola.")),
      step(h, 3, "Tus logros suben solos",
        p(h, "Cada vez que abres la app con internet, sube los logros nuevos. Tu perfil se pone al día y apareces en el ranking.")),
      step(h, 4, "Comparte y compite",
        p(h, "Cada perfil tiene un código de amigo (GR-123456). Con él sigues a tus amigos y los ves en el ranking de amigos. También hay ranking mundial y por país.")),

      h("h2", {}, "5. Puntos, niveles y medallas"),
      h("div", { class: "card reveal" },
        h("ul", { class: "how-list" },
          li(h, h("strong", {}, "Puntos: "), "cada logro da puntos según lo difícil que es. Bronce los fáciles, plata los medianos y oro los difíciles."),
          li(h, h("strong", {}, "Platino: "), "cuando consigues todos los logros de un juego."),
          li(h, h("strong", {}, "Niveles: "), "con los puntos subes de nivel. Cada nivel pide un poco más que el anterior."),
          li(h, h("strong", {}, "Medallas: "), "premios por metas: tu primer logro, 10 logros, tu primer platino, jugar 3 juegos distintos…"),
          li(h, h("strong", {}, "Me gusta: "), "cada me gusta que recibe un comentario tuyo te da 2 puntos (hasta 20 por comentario)."))),

      h("h2", {}, "6. ¿En qué punto va cada juego?"),
      h("div", { class: "card reveal" },
        h("ul", { class: "how-list" },
          li(h, h("strong", {}, "Sin empezar: "), "todavía no tiene logros útiles."),
          li(h, h("strong", {}, "En prueba: "), "tiene logros, pero pueden fallar o cambiar."),
          li(h, h("strong", {}, "Jugable: "), "funciona bien; faltan logros por añadir."),
          li(h, h("strong", {}, "Perfecto: "), "lista completa y probada.")),
        p(h, "Los logros sirven para una versión concreta del juego. Si tu juego tiene otra versión, Greeny te lo dice en vez de darte logros por error.")),

      faq(h, 5),
    ];
  }

  // ---------- terminos ----------
  function terms(h, cfg) {
    if (isEn()) return termsEn(h, cfg);
    return [
      h("h1", {}, "Términos del servicio"),
      h("p", { class: "muted" }, "Última actualización: " + UPDATED),
      h("div", { class: "card legal" },
        h("h2", {}, "1. Qué es Greeny"),
        p(h, "Greeny es un proyecto de fans, gratuito y de código abierto, que agrega un sistema de logros a la Nintendo Switch con Atmosphère. No está afiliado, patrocinado ni avalado por Nintendo. \"Nintendo Switch\" es una marca de Nintendo. Al usar Greeny aceptas estos términos."),
        h("h2", {}, "2. Uso bajo tu responsabilidad"),
        p(h, "El software se entrega \"tal cual\", sin garantías. Usar un sistema personalizado en tu consola es decisión tuya. No nos hacemos responsables de daños, pérdida de datos ni sanciones en servicios en línea. Te recomendamos jugar sin conexión."),
        h("h2", {}, "3. Nada de piratería"),
        p(h, "Greeny no trae ni comparte juegos, claves ni archivos de Nintendo. Está prohibido usar la web, los comentarios o la comunidad para compartir o pedir copias ilegales."),
        h("h2", {}, "4. Tu cuenta"),
        p(h, "Eres responsable de tu cuenta y de lo que publicas con ella. Escoge un nombre de usuario respetuoso. Puedes borrar tu cuenta cuando quieras desde Ajustes → Seguridad."),
        h("h2", {}, "5. Juego limpio"),
        p(h, "Desbloquear logros a mano, editar archivos o usar trucos para subir en el ranking va contra el espíritu del proyecto. Podemos quitar del ranking o suspender cuentas que hagan trampa."),
        h("h2", {}, "6. Comentarios"),
        p(h, "No se permiten insultos, acoso, odio, spam, publicidad ni datos personales de otras personas. Podemos borrar comentarios que incumplan esto."),
        h("h2", {}, "7. Donaciones"),
        p(h, "Las donaciones son voluntarias, se hacen por PayPal y no se devuelven. No dan ventajas en el juego ni en el ranking; solo la medalla de donador."),
        h("h2", {}, "8. Licencia"),
        p(h, "El código de Greeny es software libre bajo la licencia GPLv2. Puedes verlo y mejorarlo en ", h("a", { href: cfg.repoUrl, rel: "noopener" }, "GitHub"), "."),
        h("h2", {}, "9. Cambios"),
        p(h, "Podemos cambiar estos términos. La fecha de arriba dice cuándo fue el último cambio. Si el cambio es importante, lo anunciaremos en la web o en Discord."),
        h("h2", {}, "10. Contacto"),
        p(h, "Para cualquier duda, abre un tema en ", h("a", { href: cfg.repoUrl + "/issues", rel: "noopener" }, "GitHub"), " o escríbenos en Discord.")),
    ];
  }

  // ---------- privacidad ----------
  function privacy(h, cfg) {
    if (isEn()) return privacyEn(h, cfg);
    return [
      h("h1", {}, "Política de privacidad"),
      h("p", { class: "muted" }, "Última actualización: " + UPDATED),
      h("div", { class: "card legal" },
        p(h, "Greeny guarda lo mínimo para que funcione tu perfil. No hay anuncios, no vendemos datos y no usamos rastreadores de publicidad."),
        h("h2", {}, "Qué guardamos"),
        h("ul", {},
          li(h, h("strong", {}, "Tu cuenta: "), "correo, nombre de usuario y, si lo pones, nombre visible, frase, foto de perfil (de las de Greeny) y país."),
          li(h, h("strong", {}, "Si entras con Discord: "), "tu nombre de usuario de Discord, que sale en tu perfil."),
          li(h, h("strong", {}, "Tus logros: "), "qué logros tienes, de qué juego y cuándo los conseguiste."),
          li(h, h("strong", {}, "Tus consolas: "), "el nombre de cada consola que vinculas y cuándo se conectó por última vez. La consola guarda una llave propia, no tu contraseña."),
          li(h, h("strong", {}, "Lo que haces en la web: "), "tus comentarios, tus me gusta, a quién sigues y tus avisos.")),
        h("h2", {}, "Qué es público"),
        p(h, "Tu perfil (usuario, nombre, foto, país, frase, código de amigo, logros, medallas y seguidores), tus comentarios y tu puesto en el ranking. Tu correo nunca es público."),
        h("h2", {}, "Lo que se queda en tu consola"),
        p(h, "Tu progreso, tus opciones y el registro de errores se guardan en tu tarjeta SD. Solo se suben los logros, y solo si vinculas tu consola."),
        h("h2", {}, "Servicios que usamos"),
        h("ul", {},
          li(h, h("strong", {}, "Supabase: "), "guarda las cuentas y la base de datos."),
          li(h, h("strong", {}, "GitHub Pages: "), "sirve esta web."),
          li(h, h("strong", {}, "Cloudflare Turnstile: "), "la casilla de \"no soy un robot\" al entrar o registrarte."),
          li(h, h("strong", {}, "Discord: "), "solo si eliges entrar con Discord."),
          li(h, h("strong", {}, "PayPal: "), "procesa las donaciones. Con el correo que te da PayPal buscamos tu cuenta para darte la medalla, y no lo guardamos; solo queda marcado que donaste.")),
        h("h2", {}, "En tu navegador"),
        p(h, "La web guarda tu sesión y el idioma que elegiste en el almacenamiento del navegador. No usamos cookies de publicidad."),
        h("h2", {}, "Borrar tus datos"),
        p(h, "En Ajustes → Seguridad → Borrar mi cuenta se borra todo: tu perfil, tus logros subidos, tus consolas, comentarios, me gusta, seguidores y avisos. No se puede deshacer. Lo que hay en tu tarjeta SD no se toca."),
        h("h2", {}, "Menores"),
        p(h, "Si eres menor de edad, pide permiso a tu madre, padre o tutor antes de crear una cuenta."),
        h("h2", {}, "Contacto"),
        p(h, "Para preguntas sobre tus datos, abre un tema en ", h("a", { href: cfg.repoUrl + "/issues", rel: "noopener" }, "GitHub"), " o escríbenos en Discord.")),
    ];
  }

  function faqPage(h) {
    return [h("h1", {}, isEn() ? "Frequently asked questions" : "Preguntas frecuentes"), faq(h)];
  }

  function termsEn(h, cfg) {
    return [
      h("h1", {}, "Terms of Service"),
      h("p", { class: "muted" }, "Last updated: " + UPDATED_EN),
      h("div", { class: "card legal" },
        h("h2", {}, "1. What Greeny is"),
        p(h, "Greeny is a free, open-source fan project that adds an achievement system to the Nintendo Switch running Atmosphère. It is not affiliated with, sponsored or endorsed by Nintendo. \"Nintendo Switch\" is a trademark of Nintendo. By using Greeny you accept these terms."),
        h("h2", {}, "2. Use at your own risk"),
        p(h, "The software is provided \"as is\", without warranty of any kind. Running custom firmware on your console is your own decision. We are not responsible for damage, data loss or penalties on online services. We recommend playing offline."),
        h("h2", {}, "3. No piracy"),
        p(h, "Greeny does not include or share games, keys or Nintendo files. Using the website, the comments or the community to share or request illegal copies is forbidden."),
        h("h2", {}, "4. Your account"),
        p(h, "You are responsible for your account and for what you post with it. Choose a respectful username. You can delete your account at any time from Settings → Security."),
        h("h2", {}, "5. Fair play"),
        p(h, "Unlocking achievements by hand, editing files or using cheats to climb the ranking goes against the spirit of the project. We may remove cheating accounts from the ranking or suspend them."),
        h("h2", {}, "6. Comments"),
        p(h, "Insults, harassment, hate, spam, advertising and other people's personal data are not allowed. We may delete comments that break these rules."),
        h("h2", {}, "7. Donations"),
        p(h, "Donations are voluntary, made through PayPal and non-refundable. They give no advantage in the game or the ranking; only the supporter badge."),
        h("h2", {}, "8. License"),
        p(h, "Greeny's code is free software under the GPLv2 license. You can read and improve it on ", h("a", { href: cfg.repoUrl, rel: "noopener" }, "GitHub"), "."),
        h("h2", {}, "9. Changes"),
        p(h, "We may change these terms. The date above shows the last change. If a change is important, we will announce it on the website or on Discord."),
        h("h2", {}, "10. Contact"),
        p(h, "For any question, open an issue on ", h("a", { href: cfg.repoUrl + "/issues", rel: "noopener" }, "GitHub"), " or write to us on Discord.")),
    ];
  }

  function privacyEn(h, cfg) {
    return [
      h("h1", {}, "Privacy Policy"),
      h("p", { class: "muted" }, "Last updated: " + UPDATED_EN),
      h("div", { class: "card legal" },
        p(h, "Greeny keeps the minimum needed for your profile to work. There are no ads, we do not sell data and we do not use advertising trackers."),
        h("h2", {}, "What we store"),
        h("ul", {},
          li(h, h("strong", {}, "Your account: "), "email, username and, if you add them, display name, caption, profile picture (one of Greeny's) and country."),
          li(h, h("strong", {}, "If you sign in with Discord: "), "your Discord username, which is shown on your profile."),
          li(h, h("strong", {}, "Your achievements: "), "which achievements you have, from which game and when you earned them."),
          li(h, h("strong", {}, "Your consoles: "), "the name of each console you link and when it last connected. The console stores its own key, not your password."),
          li(h, h("strong", {}, "What you do on the website: "), "your comments, your likes, who you follow and your notifications.")),
        h("h2", {}, "What is public"),
        p(h, "Your profile (username, name, picture, country, caption, friend code, achievements, badges and followers), your comments and your place in the ranking. Your email is never public."),
        h("h2", {}, "What stays on your console"),
        p(h, "Your progress, your options and the error log are saved on your SD card. Only achievements are uploaded, and only if you link your console."),
        h("h2", {}, "Services we use"),
        h("ul", {},
          li(h, h("strong", {}, "Supabase: "), "stores the accounts and the database."),
          li(h, h("strong", {}, "GitHub Pages: "), "serves this website."),
          li(h, h("strong", {}, "Cloudflare Turnstile: "), "the \"I'm not a robot\" check when signing in or signing up."),
          li(h, h("strong", {}, "Discord: "), "only if you choose to sign in with Discord."),
          li(h, h("strong", {}, "PayPal: "), "processes donations. We use the email PayPal gives us to find your account and award the badge, and we do not keep it; only the fact that you donated is stored.")),
        h("h2", {}, "In your browser"),
        p(h, "The website stores your session and your chosen language in the browser's storage. We do not use advertising cookies."),
        h("h2", {}, "Deleting your data"),
        p(h, "Settings → Security → Delete my account erases everything: your profile, uploaded achievements, consoles, comments, likes, followers and notifications. It cannot be undone. Nothing on your SD card is touched."),
        h("h2", {}, "Minors"),
        p(h, "If you are a minor, ask your parent or guardian for permission before creating an account."),
        h("h2", {}, "Contact"),
        p(h, "For questions about your data, open an issue on ", h("a", { href: cfg.repoUrl + "/issues", rel: "noopener" }, "GitHub"), " or write to us on Discord.")),
    ];
  }

  window.GreenyPages = { howItWorks, terms, privacy, faq, faqPage };
})();
