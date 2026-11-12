// Web de Greeny. Sin paso de compilacion: JavaScript plano + supabase-js.
// Todo lo que viene de usuarios se pone con textContent, nunca como HTML.
(function () {
  "use strict";

  const cfg = window.GREENY_CONFIG;
  const sb = window.supabase.createClient(cfg.supabaseUrl, cfg.supabaseKey, {
    auth: { flowType: "pkce", detectSessionInUrl: true, persistSession: true },
  });
  const main = document.getElementById("contenido");
  const here = location.origin + location.pathname; // a donde vuelven los correos y Discord

  let session = null; // sesion de Supabase
  let me = null;      // mi fila de profiles
  let admin = false;

  // ---------- utilidades ----------

  function h(tag, attrs, ...kids) {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs || {})) {
      if (v == null || v === false) continue;
      if (k === "class") el.className = v;
      else if (k.startsWith("on")) el.addEventListener(k.slice(2), v);
      else if (k === "value") el.value = v;
      else el.setAttribute(k, v === true ? "" : v);
    }
    for (const kid of kids.flat()) {
      if (kid == null || kid === false) continue;
      el.append(kid.nodeType ? kid : document.createTextNode(String(kid)));
    }
    return el;
  }

  // En que punto va el trabajo de logros de un juego (lo elige el desarrollador).
  const STATUS = {
    sin_empezar: { label: "Sin empezar", help: "Todavía no tiene logros útiles." },
    prueba:      { label: "En prueba",   help: "Tiene logros, pero pueden fallar o cambiar." },
    jugable:     { label: "Jugable",     help: "Funciona bien; faltan logros por añadir." },
    perfecto:    { label: "Perfecto",    help: "Lista de logros completa y probada." },
  };
  function statusPill(status) {
    const st = STATUS[status];
    return st ? h("span", { class: "pill st-" + status + " with-icon", title: st.help }, h("img", { src: "img/st_" + status + ".png", alt: "", width: 18, height: 18 }), st.label) : null;
  }

  const uiLang = () => (window.GreenyI18n && window.GreenyI18n.lang) || "es";
  const fmtDate = (iso) => new Date(iso).toLocaleDateString(uiLang(), { day: "numeric", month: "short", year: "numeric" });
  const fmtNum = (n) => Number(n || 0).toLocaleString(uiLang());
  const imgUrl = (bucket, path) => (path ? sb.storage.from(bucket).getPublicUrl(path).data.publicUrl : null);
  const shownName = (p) => (p && (p.display_name || p.username)) || "?";

  // Los errores de Supabase llegan en ingles: se traducen los que un usuario puede ver.
  function explain(err) {
    const m = (err && err.message) || String(err);
    const known = [
      [/invalid login credentials/i, "Correo o contraseña incorrectos."],
      [/email not confirmed/i, "Todavía no has confirmado tu correo. Revisa tu bandeja de entrada."],
      [/user already registered/i, "Ya existe una cuenta con ese correo."],
      [/password should be at least/i, "La contraseña es muy corta."],
      [/profiles_username_lower_idx|duplicate key/i, "Ese nombre de usuario ya está ocupado."],
      [/profiles_username_format/i, "El usuario solo puede tener letras, números y _ (de 3 a 20)."],
      [/profiles_caption_len/i, "La frase puede tener hasta 60 caracteres."],
      [/new password should be different/i, "La contraseña nueva tiene que ser distinta a la anterior."],
      [/manual linking is disabled/i, "Vincular Discord a una cuenta existente todavía no está activado en el servidor."],
      [/identity is already linked/i, "Esa cuenta de Discord ya está vinculada a otra cuenta de Greeny."],
      [/captcha/i, "La verificación de \"no soy un robot\" falló o venció. Márcala de nuevo."],
      [/propio comentario/i, "No puedes darle me gusta a tu propio comentario."],
      [/rate limit|too many/i, "Demasiados intentos. Espera un momento y prueba de nuevo."],
      [/failed to fetch|networkerror/i, "No se pudo conectar. Revisa tu conexión."],
      // mensajes de nuestras funciones de la base de datos (van sin acentos)
      [/no hay nadie con ese codigo/i, "No hay nadie con ese código de amigo."],
      [/6 numeros/i, "El código de amigo tiene 6 números."],
      [/tu propio codigo/i, "Ese es tu propio código."],
      [/could not find the function/i, "El servidor todavía no tiene esta función. Hay que actualizar la base de datos."],
      [/codigo incorrecto o vencido/i, "Código incorrecto o vencido. Pide uno nuevo en la consola."],
      [/hay que iniciar sesion/i, "Tienes que iniciar sesión."],
      [/row-level security|permission denied/i, "No tienes permiso para hacer eso."],
    ];
    for (const [re, text] of known) if (re.test(m)) return text;
    return m; // los de nuestras funciones ya vienen en español
  }

  // patron para ilike que solo ignora mayusculas (sin comodines)
  const exactPattern = (text) => String(text).replace(/[\\%_]/g, "\\$&");

  // nombres que nadie puede usar, para que no se hagan pasar por el proyecto
  const RESERVED = ["greeny", "admin", "administrador", "administrator", "soporte", "support", "moderador", "moderator", "oficial", "official", "staff", "nintendo"];

  // Lanza un error si el nombre de usuario no se puede usar. `myId` es para no chocar con uno mismo.
  async function checkUsername(username, myId) {
    if (RESERVED.includes(username.toLowerCase())) throw new Error("Ese nombre de usuario está reservado. Elige otro.");
    const row = must(await sb.from("profiles").select("id").ilike("username", exactPattern(username)).maybeSingle());
    if (row && row.id !== myId) throw new Error("Ese nombre de usuario ya está ocupado. Elige otro.");
  }

  function must(res) {
    if (res.error) throw res.error;
    return res.data;
  }

  // Bronce, plata u oro: se ve en el borde del icono. Con la base de datos vieja no hay tier: sale de los puntos.
  const TIER_LABEL = { bronce: "Bronce", plata: "Plata", oro: "Oro" };
  function tierOf(a) { return TIER_LABEL[a.tier] ? a.tier : window.GreenyDefs.tierFromPoints(a.points); }
  function ptsPill(a) {
    const t = tierOf(a);
    return h("span", { class: "pill pts", title: "Dificultad: " + TIER_LABEL[t] }, a.points + " pts");
  }

  // Pestañas: recuerda la ultima abierta mientras dure la visita.
  function tabbed(key, tabs) {
    let current = tabs[0].id;
    try { current = sessionStorage.getItem(key) || current; } catch (e) { /* sin almacenamiento: empieza en la primera */ }
    if (!tabs.some((t) => t.id === current)) current = tabs[0].id;
    const panels = tabs.map((t) => h("section", { class: "dev-panel", id: "tab-" + t.id, role: "tabpanel", hidden: t.id !== current }, t.body));
    const buttons = tabs.map((t, i) => h("button", { type: "button", role: "tab", class: "tab" + (t.id === current ? " on" : ""),
      "aria-selected": String(t.id === current), "aria-controls": "tab-" + t.id, onclick: () => {
        buttons.forEach((b, k) => { b.classList.toggle("on", k === i); b.setAttribute("aria-selected", String(k === i)); });
        panels.forEach((pn, k) => { pn.hidden = k !== i; });
        try { sessionStorage.setItem(key, t.id); } catch (e) { /* da igual */ }
      } }, t.label));
    return h("div", {}, h("div", { class: "tabs", role: "tablist", "aria-label": "Secciones" }, buttons), h("div", { class: "dev" }, panels));
  }

  function copyButton(text) {
    return h("button", { type: "button", class: "btn alt small", style: "vertical-align:middle", onclick: async (ev) => {
      const b = ev.currentTarget;
      try { await navigator.clipboard.writeText(text); }
      catch (e) { // sin permiso de portapapeles: se copia con un campo temporal
        const t = h("textarea", { style: "position:fixed;opacity:0" }, text);
        document.body.append(t); t.select(); document.execCommand("copy"); t.remove();
      }
      b.textContent = "Copiado";
      setTimeout(() => { b.textContent = "Copiar"; }, 1500);
    } }, "Copiar");
  }

  // Paises que se pueden elegir (web/js/countries.js) y su bandera.
  const COUNTRIES = (window.GreenyCountries || []).slice().sort((a, b) => a.es.localeCompare(b.es, "es"));
  const countryName = (code) => { const c = COUNTRIES.find((x) => x.code === code); return c ? (uiLang() === "en" ? c.en : c.es) : code; };
  function flag(code, size) {
    if (!code || !COUNTRIES.some((c) => c.code === code)) return null;
    return h("img", { class: "flag", src: "img/flags/" + code.toLowerCase() + ".png", alt: countryName(code), title: countryName(code), width: size || 22, height: size || 22, loading: "lazy" });
  }

  // Cuadricula de banderas para elegir pais, como en la app. `withNone` añade "Sin indicar".
  function flagGrid(current, onPick, withNone) {
    const grid = h("div", { class: "flag-grid", role: "radiogroup", "aria-label": "País" });
    const paint = (cur) => grid.replaceChildren(...[
      withNone ? h("button", { type: "button", role: "radio", "aria-checked": String(!cur), class: "flag-opt none" + (!cur ? " on" : ""),
        onclick: () => { paint(""); onPick(""); } }, h("span", { class: "flag-none" }, "—"), h("span", { class: "flag-code" }, "Sin indicar")) : null,
      ...COUNTRIES.map((c) => h("button", { type: "button", role: "radio", "aria-checked": String(cur === c.code), title: countryName(c.code),
        class: "flag-opt" + (cur === c.code ? " on" : ""), onclick: () => { paint(c.code); onPick(c.code); } },
        h("img", { src: "img/flags/" + c.code.toLowerCase() + ".png", alt: "", width: 36, height: 36, loading: "lazy" }),
        h("span", { class: "flag-code" }, c.code))),
    ].filter(Boolean));
    paint(current || "");
    return grid;
  }

  // La foto de perfil es una de las que trae Greeny, por numero (profiles.avatar).
  const AVATARS = 36;
  const avatarSrc = (n) => "img/avatars/" + String(n).padStart(2, "0") + ".png";
  function avatar(p, size) {
    const el = h("span", { class: "avatar " + (size || ""), "aria-hidden": "true" });
    const n = p && Number(p.avatar);
    if (n >= 1 && n <= AVATARS) el.append(h("img", { src: avatarSrc(n), alt: "", loading: "lazy" }));
    else el.textContent = shownName(p).charAt(0).toUpperCase();
    return el;
  }

  function achIcon(ach, done) {
    const url = imgUrl("icons", ach.icon_path);
    // el borde dice la dificultad: bronce, plata u oro
    const el = h("span", { class: "icon" + (done ? " done" : "") + (ach.points != null ? " tier-" + tierOf(ach) : ""), title: ach.points != null ? TIER_LABEL[tierOf(ach)] : null });
    if (url) el.append(h("img", { src: url, alt: "", loading: "lazy" }));
    else el.append(h("img", { src: "img/ach_fill.png", alt: "", loading: "lazy" }));
    return el;
  }

  function cover(game, small) {
    const url = imgUrl("covers", game.cover_path);
    const el = h("div", { class: "cover" + (small ? " small" : ""), "aria-hidden": "true" });
    if (url) el.append(h("img", { src: url, alt: "", loading: "lazy" }));
    else el.append(h("img", { src: "img/cover_fill.png", alt: "", loading: "lazy" }));
    return el;
  }

  // Muestra los primeros `n` y un boton para ver el resto (todo de una vez).
  function more(items, n, cls) {
    const list = items.filter(Boolean);
    const box = h("div", { class: cls });
    box.append(...list.slice(0, n));
    if (list.length <= n) return box;
    const btn = h("button", { class: "btn alt more-btn", type: "button" }, "Ver todos (" + list.length + ")");
    btn.addEventListener("click", () => { box.append(...list.slice(n)); btn.remove(); });
    return h("div", {}, box, btn);
  }

  function bar(done, total) {
    const pct = total > 0 ? Math.round((done / total) * 100) : 0;
    return h("div", { class: "bar", role: "img", "aria-label": pct + "% completado" }, h("i", { style: "width:" + pct + "%" }));
  }

  // Formulario con boton que se bloquea mientras trabaja y un sitio para el mensaje.
  function form(fields, label, onSubmit) {
    const msg = h("div", { role: "status", "aria-live": "polite" });
    const btn = h("button", { class: "btn", type: "submit" }, label);
    const el = h("form", { class: "stack" }, fields, msg, h("div", {}, btn));
    el.addEventListener("submit", async (ev) => {
      ev.preventDefault();
      btn.disabled = true;
      msg.replaceChildren();
      try {
        const ok = await onSubmit(new FormData(el), el);
        if (ok) msg.replaceChildren(h("p", { class: "msg ok" }, ok));
      } catch (err) {
        msg.replaceChildren(h("p", { class: "msg err" }, explain(err)));
      } finally {
        btn.disabled = false;
      }
    });
    return el;
  }

  // Lo que se sube se convierte aqui a una imagen cuadrada del tamaño justo.
  // Asi da igual si el archivo era WebP, AVIF o JPEG con nombre .png, o si no
  // era cuadrado: la consola solo sabe abrir PNG y JPEG de verdad.
  async function squareImage(file, size, type) {
    let bitmap;
    try { bitmap = await createImageBitmap(file); }
    catch (e) { throw new Error("No se pudo abrir " + file.name + " como imagen."); }
    const side = Math.min(bitmap.width, bitmap.height);
    const canvas = document.createElement("canvas");
    canvas.width = canvas.height = size;
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingQuality = "high";
    ctx.drawImage(bitmap, (bitmap.width - side) / 2, (bitmap.height - side) / 2, side, side, 0, 0, size, size);
    const blob = await new Promise((done) => canvas.toBlob(done, type, 0.9));
    if (!blob) throw new Error("No se pudo convertir " + file.name + ".");
    return blob;
  }

  // Campo de contraseña con boton para ver lo que se escribe.
  function passwordField(label, name, autocomplete, hint) {
    const input = h("input", { name, type: "password", autocomplete, required: true, minlength: autocomplete === "new-password" ? 8 : null });
    const toggle = h("button", { type: "button", class: "peek", "aria-pressed": "false", onclick: () => {
      const show = input.type === "password";
      input.type = show ? "text" : "password";
      toggle.textContent = show ? "Ocultar" : "Ver";
      toggle.setAttribute("aria-pressed", String(show));
      toggle.setAttribute("aria-label", show ? "Ocultar contraseña" : "Ver contraseña");
    }, "aria-label": "Ver contraseña" }, "Ver");
    return h("label", {}, label, hint ? h("span", { class: "hint" }, hint) : null, h("span", { class: "pw" }, input, toggle));
  }

  // ---------- sesion ----------

  async function loadMe() {
    me = null;
    admin = false;
    if (!session) return;
    const [p, a] = await Promise.all([
      sb.from("profiles").select("*").eq("id", session.user.id).maybeSingle(),
      sb.rpc("is_admin"),
    ]);
    me = p.data || null;
    admin = a.data === true;
  }

  // ---------- avisos (notificaciones) ----------
  const BELL = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 22a2.5 2.5 0 0 0 2.45-2h-4.9A2.5 2.5 0 0 0 12 22Zm7-6V11a7 7 0 0 0-5.5-6.84V3.5a1.5 1.5 0 0 0-3 0v.66A7 7 0 0 0 5 11v5l-1.7 1.7A1 1 0 0 0 4 19.4h16a1 1 0 0 0 .7-1.7Z"/></svg>';
  const HEART = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 21s-7.5-4.6-9.6-9.2C.9 8.4 3 4.5 6.8 4.5c2.1 0 3.6 1.2 5.2 3 1.6-1.8 3.1-3 5.2-3 3.8 0 5.9 3.9 4.4 7.3C19.5 16.4 12 21 12 21Z"/></svg>';
  let notifs = { unread: 0, items: [] };
  let notifTimer = null;
  function svgEl(markup) { const t = document.createElement("template"); t.innerHTML = markup; return t.content.firstChild; } // solo SVG fijos de aqui, nunca texto de usuarios

  function bellLink() {
    const label = uiLang() === "en" ? "Notifications" : "Avisos";
    const a = h("a", { href: "#/avisos", class: "bell", "data-nav": "avisos", "data-label": label, "aria-label": label + (notifs.unread ? " (" + notifs.unread + ")" : "") }, svgEl(BELL));
    if (notifs.unread) a.append(h("span", { class: "bell-count" }, notifs.unread > 99 ? "99+" : String(notifs.unread)));
    return a;
  }
  async function loadNotifs(redraw) {
    if (!session) return;
    const res = await sb.rpc("my_notifications");
    if (res.error || !res.data) return;
    const before = notifs.unread;
    notifs = { unread: Number(res.data.unread) || 0, items: res.data.items || [] };
    if (redraw || before !== notifs.unread) renderNav();
  }
  function startNotifs() {
    clearInterval(notifTimer);
    if (!session) return;
    loadNotifs(true);
    notifTimer = setInterval(() => { if (!document.hidden) loadNotifs(false); }, 60000); // cada minuto, solo con la pestaña a la vista
  }
  const badgeName = (id) => { const b = window.GreenyProgress.badges({}).find((x) => x.id === id); return b ? b.name : id; };
  function notifText(n) {
    const en = uiLang() === "en", who = n.actor_name || n.actor || "?";
    const game = n.game || n.title_id || "";
    switch (n.kind) {
      case "like": return en ? who + " liked your comment on " + game : "A " + who + " le gustó tu comentario en " + game;
      case "follow": return en ? who + " started following you" : who + " empezó a seguirte";
      case "achievement": return en ? "You unlocked \u201c" + (n.ach_name_en || n.ach_name) + "\u201d in " + game : "Desbloqueaste \u201c" + n.ach_name + "\u201d en " + game;
      case "badge": return en ? "New badge: " + badgeName(n.badge) : "Nueva medalla: " + badgeName(n.badge);
      default: return "";
    }
  }
  function notifIcon(n) {
    const box = h("span", { class: "notif-icon", "aria-hidden": "true" });
    if (n.kind === "like") box.append(svgEl(HEART));
    else if (n.kind === "follow") box.append(n.actor_avatar ? h("img", { src: avatarSrc(n.actor_avatar), alt: "" }) : h("img", { src: "img/stat_seguidores.png", alt: "" }));
    else if (n.kind === "badge") box.append(h("img", { src: "img/badges/" + n.badge + ".png", alt: "" }));
    else box.append(h("img", { src: "img/trophy.png", alt: "" }));
    return box;
  }
  function notifTarget(n) {
    if (n.kind === "follow" && n.actor) return "#/u/" + encodeURIComponent(n.actor);
    if (n.kind === "badge" && me) return "#/u/" + encodeURIComponent(me.username);
    if (n.title_id) return "#/juego/" + n.title_id;
    return "#/avisos";
  }
  async function markRead(id) {
    const q = sb.from("notifications").update({ read_at: new Date().toISOString() }).is("read_at", null);
    await (id == null ? q.eq("user_id", session.user.id) : q.eq("id", id));
  }

  async function viewNotifications() {
    if (!session) { location.hash = "#/entrar"; return []; }
    await loadNotifs(true);
    const list = notifs.items;
    const allBtn = h("button", { class: "btn alt mini", type: "button", disabled: !notifs.unread, onclick: async () => {
      allBtn.disabled = true; await markRead(null); await loadNotifs(true); render();
    } }, "Marcar todo como leído");
    return [
      h("div", { class: "row", style: "align-items:center" }, h("h1", { class: "grow" }, "Avisos"), allBtn),
      list.length
        ? more(list.map((n) => h("a", { class: "card notif" + (n.read ? "" : " unread"), href: notifTarget(n), onclick: async (ev) => {
            if (n.read) return;
            ev.preventDefault();
            await markRead(n.id);
            await loadNotifs(true);
            location.hash = notifTarget(n);
          } }, notifIcon(n),
            h("div", { class: "grow" }, h("div", {}, notifText(n)), h("div", { class: "muted small" }, fmtDate(n.created_at))))), 15, "stack")
        : h("div", { class: "card", style: "text-align:center" }, h("img", { src: "img/m_durmiendo.png", alt: "", width: 120, height: 120 }),
            h("p", { class: "muted" }, "No tienes avisos todavía. Aquí sale cuando alguien te sigue, le da me gusta a tu comentario o consigues algo nuevo.")),
    ];
  }

  // ---------- idioma ----------
  const LANGS = [["es", "Español"], ["en", "English"]]; // para añadir uno: agregarlo aqui y en i18n.js
  function renderLang() {
    const box = document.getElementById("lang-menu");
    if (!box || !window.GreenyI18n) return;
    const cur = window.GreenyI18n.lang;
    box.replaceChildren(
      h("summary", { "aria-label": "Idioma / Language" },
        h("span", { class: "lang-short" }, cur.toUpperCase()),
        h("span", { class: "lang-long" }, "Idioma · Language"),
        h("span", { class: "caret", "aria-hidden": "true" }, "▾")),
      h("div", { class: "lang-list" }, LANGS.map(([code, name]) => h("button", { type: "button", lang: code, "aria-current": String(code === cur), onclick: () => {
        box.open = false;
        if (code === window.GreenyI18n.lang) return;
        window.GreenyI18n.set(code); renderLang(); renderNav(); renderFooter(); render();
      } }, name))));
  }

  // ---------- pie de pagina ----------
  const SOCIAL = {
    discord: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M20.3 4.4A19.6 19.6 0 0 0 15.4 3l-.6 1.3a18 18 0 0 0-5.6 0L8.6 3a19.5 19.5 0 0 0-4.9 1.5C.6 9 0 13.6.3 18.1A19.8 19.8 0 0 0 6.3 21l1.3-2a12.7 12.7 0 0 1-2-1l.5-.4a14 14 0 0 0 11.8 0l.5.4c-.6.4-1.3.7-2 1l1.3 2a19.7 19.7 0 0 0 6-3c.4-5.2-.7-9.8-3.4-13.6ZM8.5 15.3c-1.2 0-2.1-1.1-2.1-2.4s.9-2.4 2.1-2.4 2.1 1.1 2.1 2.4-.9 2.4-2.1 2.4Zm7 0c-1.2 0-2.1-1.1-2.1-2.4s.9-2.4 2.1-2.4 2.1 1.1 2.1 2.4-.9 2.4-2.1 2.4Z"/></svg>',
    github: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 .5a11.5 11.5 0 0 0-3.6 22.4c.6.1.8-.3.8-.6v-2c-3.2.7-3.9-1.5-3.9-1.5-.5-1.3-1.3-1.7-1.3-1.7-1-.7.1-.7.1-.7 1.2.1 1.8 1.2 1.8 1.2 1 1.8 2.8 1.3 3.5 1 .1-.8.4-1.3.7-1.6-2.6-.3-5.3-1.3-5.3-5.7 0-1.3.5-2.3 1.2-3.1-.1-.3-.5-1.5.1-3.1 0 0 1-.3 3.2 1.2a11 11 0 0 1 5.8 0C17.3 4.7 18.3 5 18.3 5c.6 1.6.2 2.8.1 3.1.8.8 1.2 1.8 1.2 3.1 0 4.4-2.7 5.4-5.3 5.7.4.4.8 1.1.8 2.2v3.2c0 .3.2.7.8.6A11.5 11.5 0 0 0 12 .5Z"/></svg>',
  };
  let discordUrl = "";
  function renderFooter() {
    const foot = document.getElementById("foot");
    if (!foot) return;
    const link = (href, text, ext) => h("a", ext ? { href, target: "_blank", rel: "noopener" } : { href }, text);
    foot.replaceChildren(h("div", { class: "wrap" }, h("div", { class: "foot-card" },
      h("div", { class: "foot-top" },
        h("div", {},
          h("a", { class: "foot-brand", href: "#/" }, h("img", { src: "img/icon.png", alt: "", width: 38, height: 38 }), "Greeny"),
          h("p", { class: "foot-about" }, "El primer sistema de logros no oficial para la Switch. Gratis, de fans y de código abierto.")),
        h("nav", { class: "foot-col", "aria-label": "Proyecto" }, h("h4", {}, "Proyecto"),
          link("#/como-funciona", "Cómo funciona"), link("#/juegos", "Juegos"), link("#/ranking", "Ranking"),
          link(cfg.repoUrl, "Descargar", true), link("#/contribuir", "Contribuir")),
        h("nav", { class: "foot-col", "aria-label": "Ayuda" }, h("h4", {}, "Ayuda"),
          link("#/preguntas", "Preguntas frecuentes"), discordUrl ? link(discordUrl, "Comunidad en Discord", true) : null,
          link(cfg.repoUrl + "/issues", "Reportar un problema", true), link("#/terminos", "Términos del servicio"), link("#/privacidad", "Política de privacidad")),
        h("nav", { class: "foot-col", "aria-label": "Cuenta" }, h("h4", {}, "Cuenta"),
          session ? [me ? link("#/u/" + encodeURIComponent(me.username), "Mi perfil") : null, link("#/avisos", "Avisos"), link("#/cuenta", "Ajustes")]
                  : [link("#/registro", "Crear cuenta"), link("#/entrar", "Entrar")])),
      h("div", { class: "foot-bottom" },
        h("p", { class: "foot-legal" }, "© " + new Date().getFullYear() + " Greeny. Proyecto de fans independiente, no afiliado ni avalado por Nintendo. «Nintendo Switch» es una marca de Nintendo. Greeny no proporciona juegos ni archivos de Nintendo."),
        h("div", { class: "foot-social" }, h("span", {}, "Síguenos"),
          discordUrl ? h("a", { href: discordUrl, target: "_blank", rel: "noopener", "aria-label": "Discord" }, svgEl(SOCIAL.discord)) : null,
          h("a", { href: cfg.repoUrl, target: "_blank", rel: "noopener", "aria-label": "GitHub" }, svgEl(SOCIAL.github)))))));
  }

  function renderNav() {
    const slot = document.getElementById("nav-user");
    if (session) {
      // con sesion: tu nombre, y al abrirlo tu perfil, los ajustes y salir
      const menu = h("details", { class: "user-menu" },
        h("summary", {}, avatar(me, "tiny"), h("span", { class: "user-name" }, me ? shownName(me) : "Mi cuenta"), h("span", { class: "caret", "aria-hidden": "true" }, "▾")),
        h("div", { class: "user-menu-list" },
          me ? h("a", { href: "#/u/" + encodeURIComponent(me.username) }, "Mi perfil") : null,
          h("a", { href: "#/cuenta" }, "Ajustes"),
          h("button", { type: "button", onclick: async () => { await sb.auth.signOut(); location.hash = "#/"; } }, "Salir")));
      menu.addEventListener("click", (ev) => { if (ev.target.closest(".user-menu-list")) menu.open = false; });
      document.addEventListener("click", (ev) => { if (!menu.contains(ev.target)) menu.open = false; });
      slot.replaceChildren(...[
        admin ? h("a", { href: "#/desarrollador", "data-nav": "desarrollador" }, "Desarrollador") : null,
        bellLink(),
        menu,
      ].filter(Boolean));
    } else {
      slot.replaceChildren(
        h("a", { href: "#/entrar", "data-nav": "entrar" }, "Entrar"),
        h("a", { href: "#/registro", class: "cta", "data-nav": "registro" }, "Crear cuenta")
      );
    }
    const section = (location.hash.split("/")[1] || "").split("?")[0];
    for (const a of document.querySelectorAll("[data-nav]")) {
      if (a.dataset.nav === section) a.setAttribute("aria-current", "page");
      else a.removeAttribute("aria-current");
    }
  }

  // ---------- portada ----------

  async function viewHome() {
    const count = (table) => sb.from(table).select("*", { count: "exact", head: true });
    const [players, unlocks, games, recent, live] = await Promise.all([
      count("profiles"), count("unlocks"), count("games"),
      sb.from("unlocks")
        .select("unlocked_at,title_id,ach_id,profiles(username,display_name,avatar),achievements(*,games(name))")
        .order("unlocked_at", { ascending: false }).limit(10),
      sb.rpc("activity_now"),
    ]);

    const hero = h("section", { class: "hero" },
      h("div", {},
        h("img", { class: "hero-logo", src: "img/logo.png", alt: "Greeny", width: 300, height: 152 }),
        h("h1", {}, "Logros para tu Switch"),
        h("p", { class: "lead" }, "Greeny corre en segundo plano mientras juegas, te avisa cuando desbloqueas un logro y guarda tu progreso. Aquí lo ves, lo compartes y lo comparas."),
        h("div", { class: "actions" },
          session ? h("a", { class: "btn", href: "#/cuenta" }, "Vincular mi consola")
                  : h("a", { class: "btn", href: "#/registro" }, "Crear cuenta"),
          h("a", { class: "btn alt", href: "#/juegos" }, "Ver juegos"),
          h("a", { class: "btn alt", href: cfg.repoUrl, rel: "noopener" }, "Descargar en GitHub"))),
      h("img", { class: "hero-mascot", src: "img/m_saludando.png", alt: "Greeny, la mascota: una plantita verde saludando", width: 260, height: 260 }));

    const stats = h("section", { class: "stats three", "aria-label": "Números de Greeny" },
      h("div", { class: "card stat" }, h("b", { "data-count": players.count || 0 }, "0"), h("span", {}, "jugadores")),
      h("div", { class: "card stat" }, h("b", { "data-count": unlocks.count || 0 }, "0"), h("span", {}, "logros desbloqueados")),
      h("div", { class: "card stat" }, h("b", { "data-count": games.count || 0 }, "0"), h("span", {}, "juegos con logros")));

    // en vivo: el bombillito verde que late. "Hoy" = abrieron la app o subieron logros en 24 horas.
    const act = (live && !live.error && live.data) || null;
    const liveCard = act ? h("div", { class: "card live", "aria-live": "polite" }, h("span", { class: "live-dot", "aria-hidden": "true" }),
      h("div", {}, h("b", { "data-count": act.today || 0 }, "0"), h("span", {}, "jugadores activos hoy"),
        act.online ? h("span", { class: "live-sub" }, fmtNum(act.online) + " con Greeny abierto ahora") : null)) : null;

    const rows = (recent.data || []).filter((u) => u.profiles && u.achievements).map((u) =>
      h("div", { class: "card row" },
        achIcon(u.achievements, true),
        h("div", { class: "grow" },
          h("div", { class: "clip" }, h("strong", {}, u.achievements.name_es), " ", ptsPill(u.achievements)),
          h("div", { class: "muted small clip" },
            h("a", { href: "#/u/" + encodeURIComponent(u.profiles.username) }, shownName(u.profiles)),
            " en ", h("a", { href: "#/juego/" + u.title_id }, (u.achievements.games && u.achievements.games.name) || u.title_id))),
        h("span", { class: "muted small nowrap" }, fmtDate(u.unlocked_at))));

    return [
      hero,
      liveCard ? h("div", { class: "home-top" }, liveCard, h("div", { class: "card", style: "display:flex;align-items:center;gap:14px" },
        h("img", { src: "img/m_celebrando.png", alt: "", width: 64, height: 64 }),
        h("div", {}, h("strong", {}, "¿Nuevo por aquí?"), h("div", { class: "muted small" }, "Mira en un minuto qué es Greeny y cómo empezar."),
          h("a", { href: "#/como-funciona", style: "font-weight:700" }, "Cómo funciona →")))) : null,
      h("h2", {}, "Greeny en números"), stats,
      h("h2", {}, "Últimos logros"),
      rows.length ? h("div", { class: "stack" }, rows)
                  : h("p", { class: "card muted" }, "Todavía nadie ha subido logros. ¡Sé el primero!"),
      h("h2", {}, "Cómo funciona"),
      h("div", { class: "card" }, h("ol", { class: "steps" },
        h("li", {}, "Instala Greeny en tu Switch con Atmosphère (las instrucciones están en GitHub)."),
        h("li", {}, "Juega. Los logros salen en pantalla y se guardan en la consola."),
        h("li", {}, "Crea tu cuenta aquí y vincula tu consola con un código."),
        h("li", {}, "Tu progreso aparece en tu perfil para que lo compartas."))),
      window.GreenyPages.faq(h, 5),
    ];
  }

  // ---------- juegos ----------

  async function viewGames() {
    const [games, achs] = await Promise.all([
      sb.from("games").select("*").order("name"),
      sb.from("achievements").select("title_id,points"),
    ]);
    const list = must(games);
    const totals = {};
    for (const a of achs.data || []) {
      const t = (totals[a.title_id] ||= { n: 0, pts: 0 });
      t.n++;
      t.pts += a.points;
    }
    if (!list.length) return [h("h1", {}, "Juegos"), h("p", { class: "card muted" }, "Todavía no hay juegos publicados.")];

    return [
      h("h1", {}, "Juegos"),
      h("p", { class: "muted" }, "Cada lista de logros sirve para una versión concreta del juego."),
      h("div", { class: "grid games" }, list.map((g) => {
        const t = totals[g.title_id] || { n: 0, pts: 0 };
        return h("a", { class: "card", href: "#/juego/" + g.title_id },
          cover(g),
          h("strong", { class: "clip", style: "display:block" }, g.name),
          h("div", { style: "margin:4px 0" }, statusPill(g.status)),
          h("div", { class: "muted small" }, g.version ? "Versión " + g.version : "Versión sin indicar", g.published ? "" : " · sin publicar"),
          h("div", { class: "small", style: "margin-top:6px" }, t.n + " logros · " + t.pts + " pts"));
      })),
    ];
  }

  async function viewGame(tid) {
    const [gameRes, achRes, statRes, mineRes] = await Promise.all([
      sb.from("games").select("*").eq("title_id", tid).maybeSingle(),
      sb.from("achievements").select("*").eq("title_id", tid).order("sort"),
      sb.from("achievement_stats").select("*").eq("title_id", tid),
      session ? sb.from("unlocks").select("ach_id,unlocked_at").eq("title_id", tid).eq("user_id", session.user.id) : { data: [] },
    ]);
    const game = must(gameRes);
    if (!game) return notFound("Ese juego no está en Greeny.");
    const achs = achRes.data || [];
    const stats = Object.fromEntries((statRes.data || []).map((s) => [s.ach_id, s]));
    const mine = Object.fromEntries((mineRes.data || []).map((u) => [u.ach_id, u]));
    const got = achs.filter((a) => mine[a.ach_id]);
    const totalPts = achs.reduce((s, a) => s + a.points, 0);
    const players = (statRes.data && statRes.data[0] && statRes.data[0].players) || 0;

    return [
      h("section", { class: "row game-head" }, cover(game, true),
        h("div", { class: "grow" },
          h("h1", { style: "margin-bottom:4px" }, game.name),
          STATUS[game.status] ? h("p", { style: "margin:0 0 4px" }, statusPill(game.status), " ", h("span", { class: "muted small" }, STATUS[game.status].help)) : null,
          h("p", { class: "muted", style: "margin:0" },
            game.version ? "Logros para la versión " + game.version + " del juego" : "Versión sin indicar",
            " · " + achs.length + " logros · " + totalPts + " pts · " + fmtNum(players) + (players === 1 ? " jugador" : " jugadores")),
          game.updated_at ? h("p", { class: "muted small", style: "margin:4px 0 0" }, "Logros actualizados el " + fmtDate(game.updated_at)) : null)),
      session
        ? h("div", { class: "card", style: "margin-top:18px" },
            h("div", { class: "row" }, h("strong", { class: "grow" }, "Tu progreso"),
              h("span", {}, got.length + " de " + achs.length + " · " + got.reduce((s, a) => s + a.points, 0) + " pts")),
            h("div", { style: "margin-top:8px" }, bar(got.length, achs.length)))
        : null,
      h("h2", {}, "Logros"),
      more(achs.map((a) => {
        const st = stats[a.ach_id] || { unlocked_by: 0, players: 0 };
        const pct = st.players > 0 ? Math.round((st.unlocked_by / st.players) * 100) : null;
        const done = !!mine[a.ach_id];
        return h("div", { class: "card row ach-row" + (session && !done ? " locked" : "") },
          achIcon(a, done),
          h("div", { class: "grow" },
            h("div", {}, h("strong", { class: "ach-name" }, a.name_es), " ", ptsPill(a)),
            h("div", { class: "muted small" }, a.desc_es || "Sin descripción")),
          h("div", { class: "right small" },
            session ? h("div", {}, done ? h("span", { class: "pill ok" }, "Desbloqueado") : h("span", { class: "pill no" }, "Bloqueado")) : null,
            h("div", { class: "muted", title: "Porcentaje de jugadores de este juego que lo tienen" },
              pct == null ? "Nadie lo tiene aún" : pct + "% lo tiene")));
      }), 6, "stack"),
      h("h2", {}, "Comentarios"),
      await reviewsBox(tid),
    ];
  }

  // Comentarios de un juego: uno por persona, y "me gusta" que dan 2 puntos al autor.
  async function reviewsBox(tid) {
    const box = h("div", { class: "reviews" });
    async function draw() {
      const res = await sb.rpc("game_reviews", { title: tid });
      if (res.error) { box.replaceChildren(h("p", { class: "card muted" }, "No se pudieron cargar los comentarios.")); return; }
      const list = res.data || [];
      const mine = list.find((r) => r.mine);
      box.replaceChildren(
        session ? writer(mine) : h("p", { class: "card muted" }, h("a", { href: "#/entrar" }, "Entra"), " para comentar y dar me gusta."),
        list.length ? more(list.map(card), 5, "reviews") : h("p", { class: "muted" }, "Nadie ha comentado este juego todavía. ¡Sé el primero!"));
    }
    function writer(mine) {
      const ta = h("textarea", { name: "body", maxlength: 500, required: true, placeholder: "¿Qué te parecen los logros de este juego?" });
      ta.value = mine ? mine.body : "";
      const f = form([h("label", { class: "field" }, h("span", {}, mine ? "Tu comentario" : "Escribe un comentario"), ta)],
        mine ? "Guardar" : "Publicar", async (fd) => {
          const body = String(fd.get("body") || "").trim();
          if (body.length < 2) throw new Error("Escribe un poco más.");
          const res = mine
            ? await sb.from("reviews").update({ body, updated_at: new Date().toISOString() }).eq("id", mine.id)
            : await sb.from("reviews").insert({ title_id: tid, user_id: session.user.id, body });
          if (res.error) throw res.error;
          setTimeout(draw, 0);
          return "Listo.";
        });
      f.classList.add("card", "review-form");
      if (mine) f.lastElementChild.append(" ", h("button", { type: "button", class: "btn danger mini", onclick: async () => {
        if (!confirm("¿Borrar tu comentario?")) return;
        const res = await sb.from("reviews").delete().eq("id", mine.id);
        if (res.error) alert(explain(res.error)); else draw();
      } }, "Borrar"));
      return f;
    }
    function card(r) {
      let liked = !!r.liked, likes = Number(r.likes || 0);
      const btn = h("button", { type: "button", class: "like", "aria-pressed": String(liked), disabled: !session || r.mine,
        title: r.mine ? "Es tu comentario" : !session ? "Entra para dar me gusta" : null });
      const paint = () => { btn.setAttribute("aria-pressed", String(liked)); btn.replaceChildren(liked ? "♥" : "♡", " " + likes); };
      paint();
      btn.addEventListener("click", async () => {
        btn.disabled = true;
        const res = liked
          ? await sb.from("review_likes").delete().eq("review_id", r.id).eq("user_id", session.user.id)
          : await sb.from("review_likes").insert({ review_id: r.id, user_id: session.user.id });
        btn.disabled = false;
        if (res.error) { alert(explain(res.error)); return; }
        liked = !liked; likes += liked ? 1 : -1; paint();
      });
      const edited = r.updated_at && new Date(r.updated_at) - new Date(r.created_at) > 60000;
      return h("div", { class: "card review" }, avatar(r),
        h("div", {},
          h("a", { class: "who", href: "#/u/" + encodeURIComponent(r.username) },
            h("strong", {}, shownName(r)), flag(r.country, 20),
            r.donor ? h("img", { class: "donor-mini", src: "img/badges/donador.png", alt: "Donador", title: "Donador" }) : null,
            h("span", { class: "muted small" }, "@" + r.username + " · " + fmtNum(r.total_points) + " pts · " + fmtNum(r.achievements) + " logros")),
          h("p", { class: "body" }, r.body),
          h("div", { class: "acts" }, btn,
            h("span", { class: "muted small" }, fmtDate(r.created_at) + (edited ? " · editado" : "")))));
    }
    await draw();
    return box;
  }

  // ---------- perfil ----------

  async function viewProfile(username) {
    const profile = must(await sb.from("profiles").select("*").ilike("username", exactPattern(username)).maybeSingle());
    if (!profile) return notFound("No existe ningún jugador con ese nombre.");

    const [statRes, unlockRes] = await Promise.all([
      sb.from("profile_stats").select("*").eq("id", profile.id).maybeSingle(),
      sb.from("unlocks").select("title_id,ach_id,unlocked_at,achievements(*)")
        .eq("user_id", profile.id).order("unlocked_at", { ascending: false }),
    ]);
    // platinums, followers y following solo existen con el esquema nuevo de la base de datos
    const stats = { achievements: 0, points: 0, games: 0, platinums: 0, followers: 0, following: 0, ...(statRes.data || {}) };
    const unlocks = (unlockRes.data || []).filter((u) => u.achievements);
    const lvl = window.GreenyProgress.level(stats.points);
    const badgeList = window.GreenyProgress.badges(stats).sort((a, b) => Number(b.earned) - Number(a.earned)); // las conseguidas primero
    const earned = badgeList.filter((b) => b.earned).length;
    const titles = [...new Set(unlocks.map((u) => u.title_id))];
    const [gameRes, allAch] = titles.length
      ? await Promise.all([
          sb.from("games").select("title_id,name,version,cover_path").in("title_id", titles),
          sb.from("achievements").select("title_id,points").in("title_id", titles),
        ])
      : [{ data: [] }, { data: [] }];
    const games = Object.fromEntries((gameRes.data || []).map((g) => [g.title_id, g]));
    const totals = {};
    for (const a of allAch.data || []) totals[a.title_id] = (totals[a.title_id] || 0) + 1;

    const isMe = session && session.user.id === profile.id;

    // boton de seguir: solo con sesion y en el perfil de otra persona
    let followBtn = null;
    if (session && !isMe) {
      const mine = await sb.from("follows").select("follower_id").eq("follower_id", session.user.id).eq("followed_id", profile.id).maybeSingle();
      if (!mine.error) {
        let following = !!mine.data;
        followBtn = h("button", { class: "btn" + (following ? " alt" : ""), type: "button", "aria-pressed": String(following) }, following ? "Siguiendo" : "Seguir");
        followBtn.addEventListener("click", async () => {
          followBtn.disabled = true;
          const res = following
            ? await sb.from("follows").delete().eq("follower_id", session.user.id).eq("followed_id", profile.id)
            : await sb.from("follows").insert({ follower_id: session.user.id, followed_id: profile.id });
          followBtn.disabled = false;
          if (res.error) { alert(explain(res.error)); return; }
          render(); // para que el contador de seguidores quede al dia
        });
      }
    }

    return [
      h("section", { class: "card row profile-head" }, avatar(profile, "big"),
        h("div", { class: "grow" },
          h("h1", { style: "margin-bottom:2px" }, shownName(profile), " ", flag(profile.country, 36)),
          h("div", { class: "muted" }, "@" + profile.username, profile.discord_username ? " · Discord: " + profile.discord_username : ""),
          profile.caption ? h("p", { style: "margin:8px 0 0;font-size:1.1rem" }, "“" + profile.caption + "”") : null,
          profile.friend_code ? h("div", { class: "small", style: "margin-top:8px" }, "Código de amigo: ", h("code", {}, profile.friend_code), " ", copyButton(profile.friend_code)) : null,
          h("div", { class: "muted small", style: "margin-top:6px" }, "En Greeny desde " + fmtDate(profile.created_at))),
        isMe ? h("a", { class: "btn alt", href: "#/cuenta" }, "Editar") : followBtn),
      h("section", { class: "card level", style: "margin-top:14px" },
        h("div", { class: "row" },
          h("span", { class: "pill pts lvl" }, "Nivel " + lvl.level),
          h("div", { class: "grow" }, h("div", { class: "bar", role: "img", "aria-label": lvl.into + " de " + lvl.span + " puntos para el nivel " + (lvl.level + 1) },
            h("i", { style: "width:" + Math.round((lvl.into / lvl.span) * 100) + "%" }))),
          h("span", { class: "muted small nowrap lvl-pts" }, h("img", { src: "img/estrella.png", alt: "", width: 18, height: 18 }), lvl.into + " / " + lvl.span + " pts para el nivel " + (lvl.level + 1)))),
      h("section", { class: "stats", style: "margin-top:14px" },
        h("div", { class: "card stat with-icon" }, h("img", { src: "img/stat_puntos.png", alt: "", width: 44, height: 44 }), h("div", {}, h("b", {}, fmtNum(stats.points)), h("span", {}, "puntos"))),
        h("div", { class: "card stat with-icon" }, h("img", { src: "img/stat_logros.png", alt: "", width: 44, height: 44 }), h("div", {}, h("b", {}, fmtNum(stats.achievements)), h("span", {}, "logros"))),
        h("div", { class: "card stat with-icon" }, h("img", { src: "img/stat_juegos.png", alt: "", width: 44, height: 44 }), h("div", {}, h("b", {}, fmtNum(stats.games)), h("span", {}, "mis juegos"))),
        h("div", { class: "card stat with-icon" }, h("img", { src: "img/stat_completados.png", alt: "", width: 44, height: 44 }), h("div", {}, h("b", {}, fmtNum(stats.platinums)), h("span", {}, "platinados"))),
        h("div", { class: "card stat with-icon" }, h("img", { src: "img/stat_seguidores.png", alt: "", width: 44, height: 44 }), h("div", {}, h("b", {}, fmtNum(stats.followers)), h("span", {}, "seguidores"))),
        h("div", { class: "card stat with-icon" }, h("img", { src: "img/stat_siguiendo.png", alt: "", width: 44, height: 44 }), h("div", {}, h("b", {}, fmtNum(stats.following)), h("span", {}, "siguiendo")))),
      h("h2", {}, "Medallas ", h("span", { class: "muted small" }, earned + " de " + badgeList.length)),
      more([profile.donor ? h("div", { class: "card badge earned donor" },
          h("img", { class: "medal-img", src: "img/badges/donador.png", alt: "", width: 64, height: 64, loading: "lazy" }),
          h("div", {}, h("strong", {}, "Donador"), h("div", { class: "muted small" }, "Apoya a Greeny con una donación."),
            h("span", { class: "pill ok" }, "Conseguida"))) : null,
        badgeList.map((b) =>
        h("div", { class: "card badge" + (b.earned ? " earned" : "") },
          h("img", { class: "medal-img", src: "img/badges/" + (b.earned ? b.id : "bloqueada") + ".png", alt: "", width: 64, height: 64, loading: "lazy" }),
          h("div", {},
            h("strong", {}, b.name),
            h("div", { class: "muted small" }, b.how),
            b.earned ? h("span", { class: "pill ok" }, "Conseguida")
                     : h("span", { class: "muted small" }, Math.min(b.value, b.goal) + " / " + b.goal))))].flat(), 6, "badges"),
      h("h2", {}, "Juegos"),
      titles.length
        ? more(titles.map((tid) => {
            const g = games[tid] || { name: tid };
            const mine = unlocks.filter((u) => u.title_id === tid);
            const total = totals[tid] || mine.length;
            return h("a", { class: "card row", href: "#/juego/" + tid }, cover(g, true),
              h("div", { class: "grow" },
                h("strong", { class: "clip", style: "display:block" }, g.name),
                h("div", { class: "muted small", style: "margin-bottom:6px" },
                  mine.length + " de " + total + " logros · " + mine.reduce((s, u) => s + u.achievements.points, 0) + " pts"),
                bar(mine.length, total)),
              mine.length === total ? h("span", { class: "pill ok" }, "Completado") : null);
          }), 5, "stack")
        : h("p", { class: "card muted" }, isMe ? "Todavía no has subido logros. Vincula tu consola en Ajustes." : "Este jugador todavía no ha subido logros."),
      unlocks.length ? h("h2", {}, "Últimos logros") : null,
      unlocks.length
        ? more(unlocks.slice(0, 30).map((u) =>
            h("div", { class: "card row" }, achIcon(u.achievements, true),
              h("div", { class: "grow" },
                h("div", { class: "clip" }, h("strong", {}, u.achievements.name_es), " ", ptsPill(u.achievements)),
                h("div", { class: "muted small clip" }, (games[u.title_id] || {}).name || u.title_id)),
              h("span", { class: "muted small nowrap" }, fmtDate(u.unlocked_at)))), 5, "stack")
        : null,
    ];
  }

  // ---------- entrar y registrarse ----------

  // ---------- captcha (Cloudflare Turnstile) ----------
  // Solo si config.js trae la clave del sitio (es publica). La secreta va en Supabase,
  // en Authentication → Attack Protection, nunca aqui. Sin clave, los formularios van sin captcha.
  let turnstileLoad = null;
  function loadTurnstile() {
    if (!turnstileLoad) turnstileLoad = new Promise((ok, fail) => {
      const sc = document.createElement("script");
      sc.src = "https://challenges.cloudflare.com/turnstile/v0/api.js?render=explicit";
      sc.async = true;
      sc.onload = () => ok(window.turnstile);
      sc.onerror = () => { turnstileLoad = null; fail(new Error("No se pudo cargar la verificación. Revisa tu conexión.")); };
      document.head.append(sc);
    });
    return turnstileLoad;
  }
  // Devuelve { el, token(), reset() }. token() espera a que la persona pase la verificacion.
  function captcha() {
    const key = cfg.turnstileSiteKey;
    const el = h("div", { class: "captcha" });
    if (!key) return { el: null, token: async () => undefined, reset: () => {} };
    let id = null, current = null;
    loadTurnstile().then((ts) => {
      id = ts.render(el, { sitekey: key, theme: "dark", language: uiLang(), callback: (t) => { current = t; }, "expired-callback": () => { current = null; } });
    }).catch((e) => el.replaceChildren(h("p", { class: "msg err" }, e.message)));
    return {
      el,
      token: async () => {
        if (!current) throw new Error("Marca la casilla de verificación (no soy un robot).");
        return current;
      },
      reset: () => { current = null; if (id != null && window.turnstile) window.turnstile.reset(id); },
    };
  }

  function discordButton() {
    return h("button", { class: "btn discord", type: "button", onclick: async () => {
      const { error } = await sb.auth.signInWithOAuth({ provider: "discord", options: { redirectTo: here } });
      if (error) alert(explain(error));
    } }, "Continuar con Discord");
  }

  // Entrar y crear cuenta: un panel de bienvenida a la izquierda y el formulario a la derecha.
  function authShell(title, lead, steps, nodes) {
    return h("div", { class: "auth" },
      h("section", { class: "auth-hero" },
        h("img", { src: "img/logo.png", alt: "Greeny", width: 220, height: 111 }),
        h("p", { class: "auth-title" }, title),
        h("p", { class: "auth-lead" }, lead),
        h("ol", { class: "auth-steps" }, steps.map((t, i) => h("li", { class: i === 0 ? "on" : "" }, h("span", {}, String(i + 1)), t))),
        h("img", { class: "auth-mascot", src: "img/m_saludando.png", alt: "", width: 150, height: 150 })),
      h("section", { class: "auth-form card" }, nodes));
  }

  function viewLogin() {
    if (session) { location.hash = "#/cuenta"; return []; }
    return [authShell("Hola de nuevo", "Entra para ver tus logros, tu nivel y tu puesto en el ranking.", ["Entra a tu cuenta", "Vincula tu consola", "Juega y desbloquea logros"], [
      h("h1", {}, "Entrar"),
      (() => { const cap = captcha(); return form([
        h("label", {}, "Correo", h("input", { name: "email", type: "email", autocomplete: "email", required: true })),
        passwordField("Contraseña", "password", "current-password"),
        cap.el,
      ], "Entrar", async (fd) => {
        const captchaToken = await cap.token();
        try { must(await sb.auth.signInWithPassword({ email: fd.get("email"), password: fd.get("password"), options: { captchaToken } })); }
        finally { cap.reset(); } // cada verificacion sirve una sola vez
        location.hash = "#/cuenta";
      }); })(),
      h("p", { style: "margin-top:12px" }, h("a", { href: "#/recuperar" }, "¿Olvidaste tu contraseña?")),
      h("p", { class: "or", style: "margin:18px 0" }, "o"),
      discordButton(),
      h("p", { class: "muted", style: "margin-top:22px" }, "¿No tienes cuenta? ", h("a", { href: "#/registro" }, "Créala aquí"), "."),
    ])];
  }

  function viewRegister() {
    if (session) { location.hash = "#/cuenta"; return []; }
    const regCap = captcha();
    return [authShell("Únete a Greeny", "Crea tu cuenta gratis y lleva la cuenta de tus logros de Switch.", ["Crea tu cuenta", "Vincula tu consola", "Juega y desbloquea logros"], [
      h("h1", {}, "Crear cuenta"),
      form([
        h("label", {}, "Nombre de usuario", h("span", { class: "hint" }, "Letras, números y _ (de 3 a 20). Es el de tu perfil público."),
          h("input", { name: "username", required: true, minlength: 3, maxlength: 20, pattern: "[A-Za-z0-9_]{3,20}", autocomplete: "username", autocapitalize: "none", spellcheck: "false" })),
        h("label", {}, "Correo", h("input", { name: "email", type: "email", autocomplete: "email", required: true })),
        passwordField("Contraseña", "password", "new-password", "Mínimo 8 caracteres."),
        regCap.el,
      ], "Crear cuenta", async (fd, el) => {
        await checkUsername(fd.get("username"), null); // no se crea la cuenta si el nombre ya existe
        const captchaToken = await regCap.token();
        let data;
        try {
          data = must(await sb.auth.signUp({
            email: fd.get("email"), password: fd.get("password"),
            options: { data: { username: fd.get("username") }, emailRedirectTo: here, captchaToken },
          }));
        } finally { regCap.reset(); }
        if (data.session) { location.hash = "#/cuenta"; return null; }
        el.reset();
        return "Te enviamos un correo para confirmar tu cuenta. Ábrelo y sigue el enlace.";
      }),
      h("p", { class: "or", style: "margin:18px 0" }, "o"),
      discordButton(),
      h("p", { class: "muted", style: "margin-top:22px" }, "¿Ya tienes cuenta? ", h("a", { href: "#/entrar" }, "Entra aquí"), "."),
    ])];
  }

  // Paso 1: pedir el correo con el enlace para cambiar la contraseña.
  function viewForgot() {
    const forgotCap = captcha();
    return [
      h("h1", {}, "Recuperar contraseña"),
      h("p", { class: "muted" }, "Escribe el correo de tu cuenta y te enviamos un enlace para poner una contraseña nueva."),
      form([
        h("label", {}, "Correo", h("input", { name: "email", type: "email", autocomplete: "email", required: true })),
        forgotCap.el,
      ], "Enviar enlace", async (fd, el) => {
        const captchaToken = await forgotCap.token();
        try { must(await sb.auth.resetPasswordForEmail(fd.get("email"), { redirectTo: here, captchaToken })); }
        finally { forgotCap.reset(); }
        el.reset();
        // el mismo mensaje exista o no la cuenta, para no revelar quien esta registrado
        return "Si hay una cuenta con ese correo, te llegará un enlace en unos minutos. Revisa también el correo no deseado.";
      }),
      h("p", { class: "muted", style: "margin-top:22px" }, h("a", { href: "#/entrar" }, "Volver a entrar")),
    ];
  }

  // Paso 2: al volver del enlace del correo ya hay sesion; solo falta la contraseña nueva.
  function viewNewPassword() {
    if (!session) return [h("h1", {}, "Nueva contraseña"), h("p", { class: "msg err" }, "El enlace ya no sirve o venció. Pide uno nuevo."),
      h("p", {}, h("a", { href: "#/recuperar" }, "Recuperar contraseña"))];
    return [
      h("h1", {}, "Nueva contraseña"),
      form([passwordField("Contraseña nueva", "password", "new-password", "Mínimo 8 caracteres.")], "Guardar contraseña", async (fd) => {
        must(await sb.auth.updateUser({ password: fd.get("password") }));
        setTimeout(() => { location.hash = "#/cuenta"; }, 1200);
        return "Contraseña cambiada.";
      }),
    ];
  }

  // ---------- cuenta ----------

  async function viewAccount() {
    if (!session) { location.hash = "#/entrar"; return []; }
    if (!me) return [h("h1", {}, "Ajustes"), h("p", { class: "msg err" }, "No se encontró tu perfil. Cierra sesión y vuelve a entrar.")];

    const [presetRes, deviceRes, identRes] = await Promise.all([
      sb.from("caption_presets").select("text_es").order("sort"),
      sb.from("devices").select("id,name,created_at,last_seen_at").order("created_at"),
      sb.auth.getUserIdentities(),
    ]);
    const identities = (identRes.data && identRes.data.identities) || session.user.identities || [];
    const discord = identities.find((i) => i.provider === "discord");

    // Foto de perfil: se elige una de la lista.
    const picker = h("div", { class: "avatar-picker", role: "radiogroup", "aria-label": "Foto de perfil" });
    function drawPicker() {
      picker.replaceChildren(...Array.from({ length: AVATARS }, (_, i) => {
        const n = i + 1;
        return h("button", { type: "button", role: "radio", "aria-checked": String(me.avatar === n), "aria-label": "Foto " + n,
          class: "avatar-option" + (me.avatar === n ? " on" : ""), onclick: async (ev) => {
            const b = ev.currentTarget;
            b.disabled = true;
            const res = await sb.from("profiles").update({ avatar: n }).eq("id", me.id);
            b.disabled = false;
            if (res.error) { alert(explain(res.error)); return; }
            me.avatar = n;
            drawPicker();
          } }, h("img", { src: avatarSrc(n), alt: "", loading: "lazy", width: 72, height: 72 }));
      }));
    }
    drawPicker();
    const photoRow = h("div", { class: "stack", style: "margin-bottom:18px" },
      h("div", {}, h("strong", {}, "Foto de perfil"), h("div", { class: "muted small" }, "Elige una. Es la misma en la app y en la web.")),
      picker);
    const discordName = discord && discord.identity_data &&
      (discord.identity_data.full_name || discord.identity_data.name || (discord.identity_data.custom_claims || {}).global_name);
    const presets = (presetRes.data || []).map((p) => p.text_es);
    const CUSTOM = "__custom__";
    const isPreset = !me.caption || presets.includes(me.caption);

    const customInput = h("input", { name: "caption_custom", maxlength: 60, placeholder: "Escribe tu frase", value: isPreset ? "" : me.caption, "aria-label": "Frase personalizada" });
    const customWrap = h("div", { hidden: isPreset }, customInput);
    const select = h("select", { name: "caption_choice", onchange: (ev) => { customWrap.hidden = ev.target.value !== CUSTOM; if (!customWrap.hidden) customInput.focus(); } },
      h("option", { value: "" }, "Sin frase"),
      presets.map((p) => h("option", { value: p, selected: me.caption === p }, p)),
      h("option", { value: CUSTOM, selected: !isPreset }, "Escribir la mía…"));

    const countryInput = h("input", { type: "hidden", name: "country", value: me.country || "" });
    const profileForm = form([
      h("label", {}, "Nombre de usuario", h("span", { class: "hint" }, "Si lo cambias, cambia el enlace de tu perfil."),
        h("input", { name: "username", required: true, minlength: 3, maxlength: 20, pattern: "[A-Za-z0-9_]{3,20}", value: me.username, autocapitalize: "none", spellcheck: "false" })),
      h("label", {}, "Nombre visible", h("span", { class: "hint" }, "Opcional. Hasta 32 caracteres."),
        h("input", { name: "display_name", maxlength: 32, value: me.display_name || "" })),
      h("label", {}, "Frase de tu perfil", h("span", { class: "hint" }, "Elige una o escribe la tuya (hasta 60 caracteres)."), select),
      customWrap,
      h("div", { class: "field" }, h("span", { class: "field-label" }, "País"), h("span", { class: "hint" }, "Opcional. Sale como bandera en tu perfil y en el ranking."),
        countryInput, flagGrid(me.country, (code) => { countryInput.value = code; }, true)),
    ], "Guardar perfil", async (fd) => {
      const choice = fd.get("caption_choice");
      const caption = (choice === CUSTOM ? String(fd.get("caption_custom")).trim() : choice) || null;
      await checkUsername(fd.get("username"), me.id);
      must(await sb.from("profiles").update({
        username: fd.get("username"), display_name: String(fd.get("display_name")).trim() || null, caption,
        country: fd.get("country") || null,
      }).eq("id", me.id));
      await loadMe();
      renderNav();
      return "Perfil guardado.";
    });

    const linkForm = form([
      h("label", {}, "Código que muestra tu consola",
        h("input", { name: "code", class: "code", required: true, minlength: 6, maxlength: 6, autocomplete: "off", autocapitalize: "characters", spellcheck: "false", inputmode: "text" })),
    ], "Vincular consola", async (fd, el) => {
      const name = must(await sb.rpc("device_link_claim", { code: fd.get("code") }));
      el.reset();
      setTimeout(render, 1500);
      return "Listo: “" + name + "” quedó vinculada a tu cuenta.";
    });

    const devices = deviceRes.data || [];
    return [
      h("h1", {}, "Ajustes"),
      h("p", { class: "muted" }, "Tu perfil público: ", h("a", { href: "#/u/" + encodeURIComponent(me.username) }, "@" + me.username)),
      tabbed("greeny-ajustes-tab", [
        { id: "perfil", label: "Perfil", body: [
          h("div", { class: "card" }, photoRow, profileForm)] },
        { id: "consola", label: "Consola", body: [
          h("h2", {}, "Vincular una consola"),
      h("div", { class: "card stack" },
        h("p", { class: "muted", style: "margin:0" }, "En tu Switch, abre Greeny y elige vincular cuenta. Te mostrará un código de 6 caracteres que dura 10 minutos."),
        linkForm),
          h("h2", {}, "Consolas vinculadas"),
      devices.length
        ? h("div", { class: "stack" }, devices.map((d) =>
            h("div", { class: "card row" },
              h("div", { class: "grow" }, h("strong", {}, d.name),
                h("div", { class: "muted small" }, "Vinculada el " + fmtDate(d.created_at),
                  d.last_seen_at ? " · última vez " + fmtDate(d.last_seen_at) : " · todavía no ha subido nada")),
              h("button", { class: "btn danger", type: "button", onclick: async (ev) => {
                if (!confirm("¿Desvincular “" + d.name + "”? Tus logros ya subidos no se borran.")) return;
                ev.target.disabled = true;
                const { error } = await sb.from("devices").delete().eq("id", d.id);
                if (error) { alert(explain(error)); ev.target.disabled = false; } else render();
              } }, "Desvincular"))))
        : h("p", { class: "card muted" }, "No tienes ninguna consola vinculada.")] },
        { id: "amigos", label: "Amigos", body: [
      h("div", { class: "card stack" },
        me.friend_code
          ? h("p", { style: "margin:0" }, "Tu código: ", h("code", { style: "font-size:1.15rem" }, me.friend_code), " ",
              copyButton(me.friend_code), " ",
              h("span", { class: "muted small" }, "Compártelo para que te sigan."))
          : h("p", { class: "muted", style: "margin:0" }, "Tu código aparecerá aquí cuando se actualice la base de datos (falta volver a correr schema.sql en Supabase)."),
        form([
          h("label", {}, "Seguir a alguien con su código", h("span", { class: "hint" }, "6 números. El GR y el guion son opcionales."),
            h("input", { name: "friend_code", required: true, inputmode: "numeric", autocomplete: "off", placeholder: "GR-000000", maxlength: 12, style: "max-width:200px" })),
        ], "Seguir", async (fd, el) => {
          const who = must(await sb.rpc("follow_by_code", { code: fd.get("friend_code") }));
          el.reset();
          return "Ahora sigues a @" + who + ".";
        }))] },
        { id: "seguridad", label: "Seguridad", body: [
          h("h2", {}, "Contraseña"),
      h("div", { class: "card" }, form([passwordField("Contraseña nueva", "password", "new-password", "Mínimo 8 caracteres.")],
        "Cambiar contraseña", async (fd, el) => {
          must(await sb.auth.updateUser({ password: fd.get("password") }));
          el.reset();
          return "Contraseña cambiada.";
        })),
          h("h2", {}, "Discord"),
      discord
        ? h("div", { class: "card row" },
            h("div", { class: "grow" }, h("strong", {}, discordName || "Cuenta de Discord"),
              h("div", { class: "muted small" }, "Puedes entrar a Greeny con Discord.")),
            h("span", { class: "pill ok" }, "Vinculado"))
        : h("div", { class: "card row" },
            h("div", { class: "grow" }, h("strong", {}, "Sin vincular"),
              h("div", { class: "muted small" }, "Vincúlalo para entrar con Discord a esta misma cuenta.")),
            h("button", { class: "btn discord", type: "button", onclick: async (ev) => {
              ev.target.disabled = true;
              const { error } = await sb.auth.linkIdentity({ provider: "discord", options: { redirectTo: here } });
              if (error) { alert(explain(error)); ev.target.disabled = false; }
            } }, "Vincular Discord")),
          h("h2", {}, "Borrar mi cuenta"),
      h("div", { class: "card row" },
        h("div", { class: "grow" }, h("strong", {}, "Borra todo de una vez"),
          h("div", { class: "muted small" }, "Tu perfil, tus logros subidos, consolas, comentarios, me gusta, seguidores y avisos. No se puede deshacer. Lo que está en tu tarjeta SD no se toca.")),
        h("button", { class: "btn danger", type: "button", onclick: async (ev) => {
          const name = me ? me.username : "";
          const typed = prompt("Para confirmar, escribe tu nombre de usuario: " + name);
          if (typed == null) return;
          if (typed.trim().toLowerCase() !== name.toLowerCase()) { alert("El nombre no coincide. No se borró nada."); return; }
          ev.target.disabled = true;
          const res = await sb.rpc("delete_my_account");
          if (res.error) { alert(explain(res.error)); ev.target.disabled = false; return; }
          await sb.auth.signOut();
          location.hash = "#/";
        } }, "Borrar mi cuenta"))] },
      ])
    ];
  }

  // ---------- administrador: publicar juegos e imagenes ----------

  async function viewAdmin() {
    if (!session) { location.hash = "#/entrar"; return []; }
    if (!admin) return notFound("Esta página es solo para el desarrollador.");
    const games = (await sb.from("games").select("*").order("name")).data || [];
    const presets = (await sb.from("caption_presets").select("*").order("sort")).data || [];
    const gameSelect = () => h("select", { name: "title_id", required: true },
      games.length ? games.map((g) => h("option", { value: g.title_id }, g.name + " (" + g.title_id + ")")) : h("option", { value: "" }, "Primero publica un juego"));

    const publish = form([
      h("label", {}, "Archivo de logros", h("span", { class: "hint" }, "El .txt del juego, con el Title ID como nombre: 010028600EBDA000.txt"),
        h("input", { name: "file", type: "file", accept: ".txt,text/plain", required: true })),
      h("label", { style: "display:flex;gap:8px;align-items:center" },
        h("input", { name: "published", type: "checkbox", checked: true, style: "width:auto" }), "Visible para todos"),
    ], "Publicar juego", async (fd) => {
      const file = fd.get("file");
      const tid = file.name.replace(/\.txt$/i, "").toUpperCase();
      if (!/^[0-9A-F]{16}$/.test(tid)) throw new Error("El nombre del archivo tiene que ser el Title ID (16 dígitos hexadecimales) seguido de .txt");
      // Las lineas de busqueda y de depuracion son para desarrollar: no se publican,
      // porque le gastarian CPU y llenarian el registro a todos los usuarios.
      const raw = await file.text();
      const kept = raw.split(/\r?\n/).filter((l) => !/^\s*(scan|debug)\s*=/.test(l));
      const removed = raw.split(/\r?\n/).length - kept.length;
      const text = kept.join("\n");
      const def = window.GreenyDefs.parseDefinition(text);
      must(await sb.from("games").upsert({
        title_id: tid, name: def.name || tid, version: def.version || null, build_id: def.build || null,
        definition: text, published: fd.get("published") === "on",
      }));
      // upsert: no toca icon_path de los logros que ya existian
      must(await sb.from("achievements").upsert(def.achievements.map((a, i) => ({
        title_id: tid, ach_id: a.id, name_es: a.name, desc_es: a.desc, name_en: a.name_en, desc_en: a.desc_en, points: a.points, tier: a.tier, sort: i,
      }))));
      setTimeout(render, 2000);
      return "Publicado: " + (def.name || tid) + " con " + def.achievements.length + " logros." +
        (removed ? " Se quitaron " + removed + " línea(s) de scan/debug." : "");
    });

    // Cada subida usa un nombre nuevo: si se reusara el mismo, el navegador y la
    // consola seguirian mostrando la imagen vieja que tienen guardada.
    async function uploadImage(bucket, base, file, oldPath) {
      const ext = file.type === "image/jpeg" ? ".jpg" : ".png";
      const path = base + "-" + Date.now().toString(36) + ext;
      must(await sb.storage.from(bucket).upload(path, file, { contentType: file.type, cacheControl: "31536000" }));
      return { path, cleanup: async () => { if (oldPath && oldPath !== path) await sb.storage.from(bucket).remove([oldPath]); } };
    }
    async function setIcon(tid, achId, file, oldPath) {
      const png = await squareImage(file, 128, "image/png");
      const up = await uploadImage("icons", tid + "/" + achId, png, oldPath);
      must(await sb.from("achievements").update({ icon_path: up.path }).eq("title_id", tid).eq("ach_id", achId));
      await up.cleanup();
    }

    async function setTier(tid, achId, tier) {
      const g = must(await sb.from("games").select("definition").eq("title_id", tid).single());
      const text = window.GreenyDefs.setTier(g.definition, achId, tier);
      window.GreenyDefs.parseDefinition(text); // si quedara mal, que falle antes de guardar
      must(await sb.from("achievements").update({ tier }).eq("title_id", tid).eq("ach_id", achId));
      must(await sb.from("games").update({ definition: text }).eq("title_id", tid));
    }

    // Lista de logros del juego elegido: asi se sabe como nombrar cada icono.
    const achList = h("div", { class: "ach-list", "aria-live": "polite" });
    async function showAchievements(tid) {
      if (!tid) { achList.replaceChildren(); return; }
      achList.replaceChildren(h("p", { class: "muted small" }, "Cargando logros…"));
      const rows = (await sb.from("achievements").select("*").eq("title_id", tid).order("sort")).data || [];
      const missing = rows.filter((a) => !a.icon_path).length;
      achList.replaceChildren(
        h("p", { class: "small", style: "margin:0 0 6px" },
          h("strong", {}, rows.length + " logros"), missing ? " · faltan " + missing + " iconos" : " · todos tienen icono"),
        h("table", {},
          h("thead", {}, h("tr", {}, h("th", {}, ""), h("th", {}, "Nombre del archivo"), h("th", {}, "Logro"), h("th", {}, "Dificultad"), h("th", {}, "Icono"), h("th", {}, ""))),
          h("tbody", {}, rows.map((a) => {
            // boton para poner o cambiar el icono de este logro, sin importar como se llame el archivo
            const input = h("input", { type: "file", accept: "image/*", hidden: true, "aria-label": "Icono de " + a.name_es });
            const btn = h("button", { type: "button", class: "btn alt mini", onclick: () => input.click() }, a.icon_path ? "Cambiar" : "Subir");
            input.addEventListener("change", async () => {
              if (!input.files.length) return;
              btn.disabled = true;
              btn.textContent = "Subiendo…";
              try { await setIcon(tid, a.ach_id, input.files[0], a.icon_path); await showAchievements(tid); }
              catch (err) { alert(explain(err)); btn.disabled = false; btn.textContent = a.icon_path ? "Cambiar" : "Subir"; }
            });
            // bronce, plata u oro: se guarda en el logro y en el archivo, para que la consola lo vuelva a bajar
            const tierSel = h("select", { "aria-label": "Dificultad de " + a.name_es, class: "tier-select tier-" + tierOf(a) },
              Object.keys(TIER_LABEL).map((t) => h("option", { value: t, selected: t === tierOf(a) ? "" : null }, TIER_LABEL[t])));
            tierSel.addEventListener("change", async () => {
              tierSel.disabled = true;
              try { await setTier(tid, a.ach_id, tierSel.value); await showAchievements(tid); }
              catch (err) { alert(explain(err)); tierSel.disabled = false; tierSel.value = tierOf(a); }
            });
            return h("tr", {},
              h("td", {}, achIcon(a, false)),
              h("td", {}, h("code", {}, a.ach_id + ".png")),
              h("td", {}, a.name_es),
              h("td", {}, tierSel),
              h("td", {}, a.icon_path ? h("span", { class: "pill ok" }, "Subido") : h("span", { class: "pill no" }, "Falta")),
              h("td", {}, btn, input));
          }))));
    }
    const iconGame = gameSelect();
    iconGame.addEventListener("change", () => showAchievements(iconGame.value));
    showAchievements(iconGame.value);

    const icons = form([
      h("label", {}, "Juego", iconGame),
      achList,
      h("label", {}, "Subir varios a la vez", h("span", { class: "hint" }, "Cada archivo se llama como dice la lista de arriba. Se convierten solos a PNG cuadrado de 128×128; si la imagen no es cuadrada, se recorta al centro."),
        h("input", { name: "files", type: "file", accept: "image/*", multiple: true, required: true })),
    ], "Subir iconos", async (fd) => {
      const tid = fd.get("title_id");
      const current = Object.fromEntries(((await sb.from("achievements").select("ach_id,icon_path").eq("title_id", tid)).data || []).map((a) => [a.ach_id, a.icon_path]));
      const done = [], skipped = [];
      for (const file of fd.getAll("files")) {
        const id = file.name.replace(/\.[a-z0-9]+$/i, "");
        if (!(id in current)) { skipped.push(file.name); continue; }
        await setIcon(tid, id, file, current[id]);
        done.push(id);
      }
      if (!done.length) throw new Error("Ningún archivo coincide con un id de logro de ese juego: " + skipped.join(", "));
      showAchievements(tid);
      return done.length + " icono(s) subido(s)." + (skipped.length ? " Sin logro con ese nombre: " + skipped.join(", ") + "." : "");
    });

    // Caratula actual del juego elegido, para ver como queda.
    const coverPreview = h("div", { class: "cover-preview", "aria-live": "polite" });
    async function showCover(tid) {
      if (!tid) { coverPreview.replaceChildren(); return; }
      const g = (await sb.from("games").select("name,cover_path").eq("title_id", tid).maybeSingle()).data;
      coverPreview.replaceChildren(g ? cover(g, true) : "",
        h("span", { class: "muted small" }, g && g.cover_path ? "Carátula actual. Sube otra para cambiarla." : "Este juego todavía no tiene carátula."));
    }
    const coverGame = gameSelect();
    coverGame.addEventListener("change", () => showCover(coverGame.value));
    showCover(coverGame.value);

    const coverForm = form([
      h("label", {}, "Juego", coverGame),
      coverPreview,
      h("label", {}, "Carátula", h("span", { class: "hint" }, "Cualquier imagen; se convierte a 512×512. Solo se usa en la web."),
        h("input", { name: "file", type: "file", accept: "image/*", required: true })),
    ], "Subir carátula", async (fd, el) => {
      const tid = fd.get("title_id"), file = fd.get("file");
      const jpg = await squareImage(file, 512, "image/jpeg");
      const old = (await sb.from("games").select("cover_path").eq("title_id", tid).maybeSingle()).data;
      const up = await uploadImage("covers", tid, jpg, old && old.cover_path);
      must(await sb.from("games").update({ cover_path: up.path }).eq("title_id", tid));
      await up.cleanup();
      el.querySelector("input[type=file]").value = "";
      await showCover(tid);
      return "Carátula subida.";
    });

    // Datos del juego: nombre, version, estado y si se ve.
    const infoFields = h("div", { class: "stack" });
    const infoGame = gameSelect();
    function showInfo() {
      const g = games.find((x) => x.title_id === infoGame.value);
      if (!g) { infoFields.replaceChildren(); return; }
      infoFields.replaceChildren(
        h("label", {}, "Nombre", h("input", { name: "name", required: true, maxlength: 60, value: g.name })),
        h("label", {}, "Versión del juego requerida", h("span", { class: "hint" }, "La que tienen que tener los jugadores. Ejemplo: 1.1.0"),
          h("input", { name: "version", maxlength: 16, value: g.version || "", style: "max-width:200px" })),
        h("label", {}, "Estado", h("span", { class: "hint" }, "Lo ven los usuarios en la lista de juegos."),
          h("select", { name: "status", style: "max-width:260px" },
            Object.entries(STATUS).map(([k, v]) => h("option", { value: k, selected: (g.status || "jugable") === k }, v.label + " — " + v.help)))),
        h("label", { style: "display:flex;gap:8px;align-items:center" },
          h("input", { name: "published", type: "checkbox", checked: g.published, style: "width:auto" }), "Visible para todos"),
        h("p", { class: "muted small", style: "margin:0" }, "Title ID: ", h("code", {}, g.title_id),
          " · Build: ", h("code", {}, g.build_id || "sin indicar"),
          " · Si vuelves a publicar el archivo .txt, el nombre y la versión se toman del archivo."));
    }
    infoGame.addEventListener("change", showInfo);
    showInfo();
    const infoForm = form([h("label", {}, "Juego", infoGame), infoFields], "Guardar datos", async (fd) => {
      const tid = fd.get("title_id");
      const patch = { name: String(fd.get("name")).trim(), version: String(fd.get("version")).trim() || null,
                      status: fd.get("status"), published: fd.get("published") === "on" };
      // El nombre y la version tambien viven dentro del archivo de logros: se cambian ahi,
      // asi la consola nota el cambio y vuelve a bajar el archivo.
      const cur = must(await sb.from("games").select("definition").eq("title_id", tid).single());
      let text = window.GreenyDefs.setHeader(cur.definition, "game", patch.name);
      if (patch.version) text = window.GreenyDefs.setHeader(text, "version", patch.version);
      window.GreenyDefs.parseDefinition(text);
      must(await sb.from("games").update({ ...patch, definition: text }).eq("title_id", tid));
      Object.assign(games.find((x) => x.title_id === tid), patch);
      return "Datos guardados.";
    });

    // Frases sugeridas del perfil (las del desplegable de Cuenta).
    const presetList = h("div", { class: "stack" });
    function drawPresets() {
      presetList.replaceChildren(...presets.map((p) => {
        const es = h("input", { value: p.text_es, maxlength: 60, "aria-label": "Frase en español" });
        const en = h("input", { value: p.text_en, maxlength: 60, "aria-label": "Frase en inglés" });
        const save = h("button", { type: "button", class: "btn alt mini" }, "Guardar");
        const del = h("button", { type: "button", class: "btn danger mini" }, "Quitar");
        save.addEventListener("click", async () => {
          if (!es.value.trim() || !en.value.trim()) { alert("La frase no puede quedar vacía."); return; }
          save.disabled = true;
          const { error } = await sb.from("caption_presets").update({ text_es: es.value.trim(), text_en: en.value.trim() }).eq("id", p.id);
          save.disabled = false;
          if (error) alert(explain(error)); else { p.text_es = es.value.trim(); p.text_en = en.value.trim(); save.textContent = "Guardado"; setTimeout(() => { save.textContent = "Guardar"; }, 1500); }
        });
        del.addEventListener("click", async () => {
          if (!confirm("¿Quitar la frase “" + p.text_es + "”? Quien ya la tenga puesta la conserva.")) return;
          const { error } = await sb.from("caption_presets").delete().eq("id", p.id);
          if (error) { alert(explain(error)); return; }
          presets.splice(presets.indexOf(p), 1);
          drawPresets();
        });
        return h("div", { class: "preset-row" }, es, en, save, del);
      }));
    }
    drawPresets();
    const presetForm = form([
      h("div", { class: "preset-row" },
        h("input", { name: "text_es", required: true, maxlength: 60, placeholder: "Frase nueva en español" }),
        h("input", { name: "text_en", required: true, maxlength: 60, placeholder: "La misma en inglés" })),
    ], "Añadir frase", async (fd, el) => {
      const row = { id: presets.reduce((m, p) => Math.max(m, p.id), 0) + 1, text_es: String(fd.get("text_es")).trim(),
                    text_en: String(fd.get("text_en")).trim(), sort: presets.reduce((m, p) => Math.max(m, p.sort), 0) + 10 };
      must(await sb.from("caption_presets").insert(row));
      presets.push(row);
      el.reset();
      drawPresets();
      return "Frase añadida.";
    });

    // Postulaciones al equipo y donadores.
    const teamPanel = h("div", { class: "stack" });
    async function drawTeam() {
      const apps = (await sb.from("applications").select("id,role,contact,message,status,created_at,profiles(username,display_name)").order("created_at", { ascending: false })).data || [];
      const setStatus = async (id, status) => { const r = await sb.from("applications").update({ status }).eq("id", id); if (r.error) alert(explain(r.error)); drawTeam(); };
      const remove = async (id) => { if (!confirm("¿Borrar esta postulación?")) return; const r = await sb.from("applications").delete().eq("id", id); if (r.error) alert(explain(r.error)); drawTeam(); };
      const donors = (await sb.from("profiles").select("username,display_name,avatar").eq("donor", true).order("username")).data || [];
      const setDonor = async (who, on) => { const r = await sb.rpc("set_donor", { who, is_donor: on }); if (r.error) { alert(explain(r.error)); return null; } drawTeam(); return r.data; };
      const donor = form([
        h("label", {}, "Correo o nombre de usuario", h("span", { class: "hint" }, "El correo que te dio PayPal sirve si es el mismo de su cuenta de Greeny."),
          h("input", { name: "who", required: true, autocomplete: "off", style: "max-width:320px" })),
      ], "Dar la medalla", async (fd) => {
        const v = String(fd.get("who")).trim();
        const who = must(v.includes("@") ? await sb.rpc("set_donor_email", { donor_email: v, is_donor: true }) : await sb.rpc("set_donor", { who: v, is_donor: true }));
        setTimeout(drawTeam, 1200);
        return "@" + who + " ya tiene la medalla de donador.";
      });
      const donorList = donors.length
        ? h("div", { class: "stack" }, donors.map((d) => h("div", { class: "card row" }, avatar(d, "tiny"),
            h("strong", { class: "grow" }, "@" + d.username),
            h("button", { type: "button", class: "btn danger mini", onclick: () => { if (confirm("¿Quitarle la medalla de donador a @" + d.username + "?")) setDonor(d.username, false); } }, "Quitar"))))
        : h("p", { class: "muted" }, "Todavía nadie tiene la medalla.");
      teamPanel.replaceChildren(
        h("h2", { style: "margin-top:0" }, "Postulaciones"),
        apps.length ? h("div", { class: "stack" }, apps.map((a) => h("div", { class: "card stack" },
          h("div", { class: "row" },
            h("strong", { class: "grow" }, (a.profiles ? "@" + a.profiles.username : "?") + " · " + (ROLES.find((r) => r.id === a.role) || { name: a.role }).name),
            h("span", { class: "pill " + (a.status === "aceptada" ? "ok" : "no") }, STATUS_APP[a.status] || a.status)),
          h("div", { class: "small" }, "Contacto: ", h("code", {}, a.contact), " · " + fmtDate(a.created_at)),
          a.message ? h("p", { class: "muted small", style: "margin:0;white-space:pre-wrap" }, a.message) : null,
          h("div", { class: "row", style: "flex-wrap:wrap;gap:8px" },
            h("button", { type: "button", class: "btn alt mini", onclick: () => setStatus(a.id, "aceptada") }, "Aceptar"),
            h("button", { type: "button", class: "btn alt mini", onclick: () => setStatus(a.id, "rechazada") }, "Rechazar"),
            h("button", { type: "button", class: "btn danger mini", onclick: () => remove(a.id) }, "Borrar")))))
          : h("p", { class: "card muted" }, "Todavía no hay postulaciones."),
        h("h2", {}, "Donadores"),
        h("p", { class: "muted" }, "PayPal y Greeny no están unidos: con el correo del donante, busca su usuario y dale la medalla aquí."),
        h("div", { class: "card" }, donor),
        h("h3", { style: "margin-top:18px" }, "Tienen la medalla"),
        donorList);
    }
    drawTeam();

    // Pestañas: cada tarea en la suya, para no tener todo en una sola columna larga.
    const tabs = [
      { id: "nuevo", label: "Publicar juego", body: [
        h("p", { class: "muted" }, "Sube el archivo de logros de un juego nuevo, o vuelve a subirlo para actualizarlo."),
        h("div", { class: "card" }, publish)] },
      { id: "juegos", label: "Editar juegos", body: games.length ? [
        h("h2", {}, "Datos del juego"), h("div", { class: "card" }, infoForm),
        h("h2", {}, "Logros: dificultad e iconos"), h("div", { class: "card" }, icons),
        h("h2", {}, "Carátula"), h("div", { class: "card" }, coverForm)]
        : [h("p", { class: "card muted" }, "Primero publica un juego.")] },
      { id: "equipo", label: "Equipo y donadores", body: [teamPanel] },
      { id: "frases", label: "Frases de perfil", body: [
        h("p", { class: "muted" }, "Son las que salen en el desplegable de Ajustes. Tu propia frase la cambias en Ajustes → Perfil."),
        h("div", { class: "card stack" }, presetList, presetForm)] },
    ];
    return [
      h("h1", {}, "Desarrollador"),
      h("p", { class: "muted" }, "Publicar de nuevo un juego actualiza sus textos y puntos. No borra logros que ya no estén en el archivo, porque eso borraría el progreso de la gente."),
      tabbed("greeny-dev-tab", tabs),
    ];
  }

  // ---------- contribuir: donar y unirse al equipo ----------

  const ROLES = [
    { id: "beta_tester", name: "Beta tester", hot: true,
      what: "Prueba la app y busca los valores de memoria con los que se hacen los logros. Es lo que más falta hace ahora mismo." },
    { id: "moderador", name: "Moderador de Discord", what: "Cuida la comunidad de Discord: responde dudas y mantiene el orden." },
    { id: "arte", name: "Diseño y arte", what: "Iconos de logros, medallas, mascotas y todo lo que se ve en la app y la web." },
    { id: "musica", name: "Música", what: "Canciones de fondo y sonidos para la app." },
  ];
  const STATUS_APP = { nueva: "En revisión", aceptada: "Aceptada", rechazada: "No esta vez" };

  async function viewContribute() {
    const mine = session ? (await sb.from("applications").select("role,status,created_at").eq("user_id", session.user.id)).data || [] : [];
    const roleName = (id) => (ROLES.find((r) => r.id === id) || { name: id }).name;

    const apply = session
      ? form([
          h("label", {}, "Rol", h("select", { name: "role", required: true }, ROLES.map((r) => h("option", { value: r.id }, r.name)))),
          h("label", {}, "Cómo contactarte", h("span", { class: "hint" }, "Tu usuario de Discord, o un correo si lo prefieres."),
            h("input", { name: "contact", required: true, minlength: 2, maxlength: 80, autocomplete: "off" })),
          h("label", {}, "Cuéntanos de ti", h("span", { class: "hint" }, "Opcional. Qué juegos tienes, qué sabes hacer, cuánto tiempo puedes dedicarle."),
            h("textarea", { name: "message", maxlength: 1000, rows: 4 })),
        ], "Enviar postulación", async (fd, el) => {
          must(await sb.from("applications").insert({ user_id: session.user.id, role: fd.get("role"),
            contact: String(fd.get("contact")).trim(), message: String(fd.get("message") || "").trim() }));
          el.reset();
          setTimeout(render, 1500);
          return "¡Gracias! Te escribiremos por el contacto que dejaste.";
        })
      : h("p", { class: "muted", style: "margin:0" }, "Para postularte tienes que ", h("a", { href: "#/entrar" }, "entrar"), " con tu cuenta de Greeny.");

    return [
      h("h1", {}, "Contribuir"),
      h("p", { class: "lead muted" }, "Greeny es gratis y lo va a seguir siendo. Lo hace una sola persona en su tiempo libre: hay dos formas de echar una mano."),

      h("section", { class: "contrib" },
        h("div", { class: "card contrib-card" },
          h("h2", { style: "margin-top:0" }, "Apoya a Greeny"),
          h("p", {}, "Con tu donación se puede:"),
          h("ul", { class: "contrib-list" },
            h("li", {}, "Tener una página web con dominio propio."),
            h("li", {}, "Ampliar el catálogo de juegos con logros."),
            h("li", {}, "Mejorar la comunidad de Discord."),
            h("li", {}, "Darle mantenimiento a todo el proyecto.")),
          h("a", { class: "btn donate", href: cfg.donateUrl, target: "_blank", rel: "noopener" }, "Donar con PayPal"),
          h("div", { class: "row donor-note" },
            h("img", { src: "img/badges/donador.png", alt: "", width: 72, height: 72 }),
            h("div", {},
              h("strong", {}, "Medalla de donador"),
              h("p", { class: "muted small", style: "margin:4px 0 0" }, "Quien dona recibe esta medalla en su perfil. Para que sepamos a quién dársela, escribe tu nombre de usuario de Greeny en la nota de la donación."),
              h("p", { class: "muted small", style: "margin:4px 0 0" }, "La medalla se pone a mano, así que puede tardar un poco en aparecer."))),
          h("p", { class: "muted small", style: "margin:12px 0 0" }, "Donar es opcional: no desbloquea logros ni da ventajas.")),

        h("div", { class: "card contrib-card" },
          h("h2", { style: "margin-top:0" }, "Únete al equipo"),
          h("div", { class: "row team-now" }, h("img", { src: "img/fundador.jpg", alt: "", width: 44, height: 44, style: "border-radius:50%;object-fit:cover" }),
            h("div", {}, h("strong", {}, "Jhondy"), h("div", { class: "muted small" }, "Fundador y programador"))),
          h("div", { class: "roles" }, ROLES.map((r) =>
            h("div", { class: "role" + (r.hot ? " hot" : "") },
              h("strong", {}, r.name), r.hot ? h("span", { class: "pill pts" }, "Se busca") : null,
              h("p", { class: "muted small", style: "margin:4px 0 0" }, r.what)))),
          mine.length ? h("div", { class: "stack", style: "margin:14px 0" }, h("strong", {}, "Tus postulaciones"),
            mine.map((m) => h("div", { class: "row small" }, h("span", { class: "grow" }, roleName(m.role)),
              h("span", { class: "pill " + (m.status === "aceptada" ? "ok" : "no") }, STATUS_APP[m.status] || m.status)))) : null,
          h("h3", { style: "margin-top:18px" }, "Postúlate"),
          apply)),
      discordUrl ? h("section", { class: "card discord-card", style: "margin-top:18px" },
        h("img", { src: "img/m_saludando.png", alt: "", width: 72, height: 72 }),
        h("div", { class: "grow" }, h("h2", { style: "margin:0" }, "Comunidad en Discord"),
          h("p", { class: "muted", style: "margin:4px 0 0" }, "Avisos de juegos nuevos, ayuda con la instalación, ideas de logros y encuestas.")),
        h("a", { class: "btn discord", href: discordUrl, target: "_blank", rel: "noopener" }, "Entrar al Discord")) : null,
    ];
  }

  // ---------- ranking ----------

  async function viewRanking() {
    const mine = session ? (await sb.from("profiles").select("country").eq("id", session.user.id).maybeSingle()).data : null;
    // por juego: con sesion, los juegos donde ya tienes algun logro; sin sesion, todos los publicados
    let gameList = (await sb.from("games").select("title_id,name").eq("published", true).order("name")).data || [];
    if (session) {
      const played = new Set(((await sb.from("game_points").select("title_id").eq("user_id", session.user.id)).data || []).map((g) => g.title_id));
      if (played.size) gameList = gameList.filter((g) => played.has(g.title_id));
    }
    const state = { scope: "global", country: (mine && mine.country) || "DO", title: "" };
    const out = h("div", { "aria-live": "polite" });

    const scopes = [["global", "Global"], ["country", "Por país"], ["friends", "Amigos"]];
    const scopeBtns = scopes.map(([id, label]) => h("button", { type: "button", class: "tab" + (id === state.scope ? " on" : ""), "aria-pressed": String(id === state.scope),
      onclick: () => { state.scope = id; draw(); } }, label));
    // pais del ranking: un boton con la bandera elegida que abre la cuadricula de banderas
    const countryLabel = h("span", {});
    const paintCountry = () => countryLabel.replaceChildren(flag(state.country, 26), " ", countryName(state.country));
    paintCountry();
    const countrySel = h("details", { class: "flag-drop" },
      h("summary", {}, countryLabel, h("span", { class: "caret", "aria-hidden": "true" }, "▾")),
      h("div", { class: "flag-drop-list" }, flagGrid(state.country, (code) => { state.country = code; countrySel.open = false; paintCountry(); draw(); }, false)));
    const gameSel = h("select", { "aria-label": "Clasificar por", onchange: (ev) => { state.title = ev.target.value; draw(); } },
      h("option", { value: "" }, "Puntos totales"), gameList.map((g) => h("option", { value: g.title_id }, "Juego: " + g.name)));

    function row(r, isMe) {
      const lvl = window.GreenyProgress.level(r.total_points).level;
      return h("a", { class: "card row rank-row" + (isMe ? " me" : ""), href: "#/u/" + encodeURIComponent(r.username) },
        h("span", { class: "rank-pos" + (r.position <= 3 ? " top" + r.position : "") }, "#" + r.position),
        avatar(r, ""),
        h("div", { class: "grow clip" },
          h("strong", {}, shownName(r)), " ", flag(r.country, 26),
          h("div", { class: "muted small" }, "@" + r.username + " · Nivel " + lvl)),
        h("div", { class: "right" }, h("strong", { class: "rank-pts" }, fmtNum(r.points)), h("div", { class: "muted small" }, "puntos")));
    }

    let seq = 0;
    async function draw() {
      const id = ++seq;
      scopeBtns.forEach((b, i) => { const on = scopes[i][0] === state.scope; b.classList.toggle("on", on); b.setAttribute("aria-pressed", String(on)); });
      countrySel.hidden = state.scope !== "country";
      if (state.scope === "friends" && !session) {
        out.replaceChildren(h("p", { class: "card muted" }, "Para ver el ranking de tus amigos tienes que ", h("a", { href: "#/entrar" }, "entrar"), "."));
        return;
      }
      out.replaceChildren(h("p", { class: "muted" }, "Cargando…"));
      const res = await sb.rpc("ranking", { scope: state.scope, country: state.scope === "country" ? state.country : null, title: state.title || null });
      if (id !== seq) return; // llego tarde: ya se pidio otra cosa
      if (res.error) { out.replaceChildren(h("p", { class: "msg err" }, explain(res.error))); return; }
      const data = res.data || { top: [], me: null, total: 0 };
      if (!data.top.length) {
        out.replaceChildren(h("div", { class: "card row" }, h("img", { src: "img/m_buscando.png", alt: "", width: 96, height: 96 }),
          h("p", { style: "margin:0" }, "Todavía no hay nadie con puntos aquí.")));
        return;
      }
      const inTop = data.me && data.top.some((r) => r.username === data.me.username);
      out.replaceChildren(...[
        h("div", { class: "stack" }, data.top.map((r) => row(r, data.me && r.username === data.me.username))),
        data.me && !inTop ? h("div", { style: "margin-top:16px" }, h("h2", {}, "Tu posición"), row(data.me, true)) : null,
        session && !data.me ? h("p", { class: "muted small", style: "margin-top:12px" }, "Todavía no apareces aquí: te falta tener puntos en esta clasificación.") : null,
        h("p", { class: "muted small", style: "margin-top:12px" }, fmtNum(data.total) + " jugador(es) en esta clasificación."),
      ].filter(Boolean));
    }
    draw();

    return [
      h("h1", {}, "Ranking"),
      h("p", { class: "muted" }, "Los 10 jugadores con más puntos. Toca a alguien para ver su perfil y seguirlo."),
      h("div", { class: "rank-filters" }, h("div", { class: "tabs", style: "margin:0;border:0;padding:0" }, scopeBtns), countrySel, gameSel),
      out,
    ];
  }

  function notFound(text) {
    return [h("h1", {}, "No encontrado"), h("div", { class: "card row" }, h("img", { src: "img/m_buscando.png", alt: "", width: 110, height: 110 }), h("p", { style: "margin:0" }, text || "Esa página no existe.")), h("p", {}, h("a", { href: "#/" }, "Volver al inicio"))];
  }

  // ---------- rutas ----------

  let renderId = 0;
  async function render() {
    const id = ++renderId;
    const parts = location.hash.replace(/^#\/?/, "").split("/").map(decodeURIComponent);
    const P = window.GreenyPages;
    const routes = { avisos: viewNotifications, "como-funciona": () => P.howItWorks(h, cfg), preguntas: () => P.faqPage(h),
      terminos: () => P.terms(h, cfg), privacidad: () => P.privacy(h, cfg), "": viewHome, contribuir: viewContribute, ranking: viewRanking, juegos: viewGames, juego: viewGame, u: viewProfile, entrar: viewLogin, registro: viewRegister, recuperar: viewForgot, "nueva-clave": viewNewPassword, cuenta: viewAccount, admin: viewAdmin, desarrollador: viewAdmin };
    const view = routes[parts[0]];
    renderNav();
    try {
      const nodes = view ? await view(parts[1] || "") : notFound();
      if (id !== renderId) return; // el usuario ya navego a otra parte
      main.replaceChildren(...nodes.filter(Boolean));
      if (window.GreenyFx) window.GreenyFx.scan(main);
    } catch (err) {
      if (id !== renderId) return;
      main.replaceChildren(h("h1", {}, "Algo salió mal"), h("p", { class: "msg err" }, explain(err)),
        h("p", {}, h("button", { class: "btn alt", type: "button", onclick: render }, "Reintentar")));
    }
    document.title = (main.querySelector("h1") ? main.querySelector("h1").textContent + " — " : "") + "Greeny";
  }

  window.addEventListener("hashchange", () => {
    if (!location.hash || location.hash.startsWith("#/")) { render(); main.focus({ preventScroll: true }); window.scrollTo(0, 0); }
  });

  // Invitacion de Discord: sale de links.json (la app de la consola lee el mismo archivo).
  fetch("links.json", { cache: "no-cache" }).then((r) => (r.ok ? r.json() : {})).then((j) => {
    const u = String((j && j.discord) || "");
    if (!/^https:\/\/(discord\.gg|discord\.com\/invite)\//.test(u)) return;
    discordUrl = u;
    const nav = document.getElementById("nav-discord");
    nav.href = u;
    nav.hidden = false;
    renderFooter();
    if (/^#\/contribuir/.test(location.hash)) render();
  }).catch(() => {});
  const top = document.querySelector(".top"), menuBtn = document.getElementById("menu-toggle");
  const closeMenu = () => { top.classList.remove("open"); menuBtn.setAttribute("aria-expanded", "false"); };
  menuBtn.addEventListener("click", () => {
    const open = top.classList.toggle("open");
    menuBtn.setAttribute("aria-expanded", String(open));
  });
  window.addEventListener("hashchange", closeMenu);
  document.addEventListener("click", (ev) => { if (!top.contains(ev.target)) closeMenu(); });
  renderLang();
  renderFooter();

  let signedInAs = undefined; // para no redibujar cuando Supabase solo refresca el token
  sb.auth.onAuthStateChange((event, s) => {
    session = s;
    const who = s ? s.user.id : null;
    // vino del enlace de "olvide mi contraseña": llevarlo a poner la nueva
    if (event === "PASSWORD_RECOVERY") location.hash = "#/nueva-clave";
    if (who === signedInAs) return;
    signedInAs = who;
    // fuera del callback: supabase-js no deja hacer consultas dentro de el
    setTimeout(async () => { await loadMe(); render(); renderFooter(); startNotifs(); }, 0);
  });
})();
