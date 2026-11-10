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

  const fmtDate = (iso) => new Date(iso).toLocaleDateString("es", { day: "numeric", month: "short", year: "numeric" });
  const fmtNum = (n) => Number(n || 0).toLocaleString("es");
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
      [/rate limit|too many/i, "Demasiados intentos. Espera un momento y prueba de nuevo."],
      [/failed to fetch|networkerror/i, "No se pudo conectar. Revisa tu conexión."],
      // mensajes de nuestras funciones de la base de datos (van sin acentos)
      [/codigo incorrecto o vencido/i, "Código incorrecto o vencido. Pide uno nuevo en la consola."],
      [/hay que iniciar sesion/i, "Tienes que iniciar sesión."],
      [/row-level security|permission denied/i, "No tienes permiso para hacer eso."],
    ];
    for (const [re, text] of known) if (re.test(m)) return text;
    return m; // los de nuestras funciones ya vienen en español
  }

  function must(res) {
    if (res.error) throw res.error;
    return res.data;
  }

  function avatar(p, size) {
    const el = h("span", { class: "avatar " + (size || ""), "aria-hidden": "true" });
    if (p && p.avatar_url) el.append(h("img", { src: p.avatar_url, alt: "", loading: "lazy", referrerpolicy: "no-referrer" }));
    else el.textContent = shownName(p).charAt(0).toUpperCase();
    return el;
  }

  function achIcon(ach, done) {
    const url = imgUrl("icons", ach.icon_path);
    const el = h("span", { class: "icon" + (done ? " done" : ""), "aria-hidden": "true" });
    if (url) el.append(h("img", { src: url, alt: "", loading: "lazy" }));
    else el.textContent = done ? "✓" : "?";
    return el;
  }

  function cover(game, small) {
    const url = imgUrl("covers", game.cover_path);
    const el = h("div", { class: "cover" + (small ? " small" : ""), "aria-hidden": "true" });
    if (url) el.append(h("img", { src: url, alt: "", loading: "lazy" }));
    else el.textContent = (game.name || "?").charAt(0).toUpperCase();
    return el;
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

  function renderNav() {
    const slot = document.getElementById("nav-user");
    if (session) {
      slot.replaceChildren(...[
        me ? h("a", { href: "#/u/" + encodeURIComponent(me.username) }, "Mi perfil") : null,
        h("a", { href: "#/cuenta", "data-nav": "cuenta" }, "Cuenta"),
        admin ? h("a", { href: "#/admin", "data-nav": "admin" }, "Publicar") : null,
        h("button", { type: "button", onclick: async () => { await sb.auth.signOut(); location.hash = "#/"; } }, "Salir"),
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
    const [players, unlocks, games, recent] = await Promise.all([
      count("profiles"), count("unlocks"), count("games"),
      sb.from("unlocks")
        .select("unlocked_at,title_id,ach_id,profiles(username,display_name,avatar_url),achievements(name_es,points,icon_path,games(name))")
        .order("unlocked_at", { ascending: false }).limit(10),
    ]);

    const hero = h("section", { class: "hero" },
      h("div", {},
        h("h1", {}, "Logros para tu Switch"),
        h("p", { class: "lead" }, "Greeny corre en segundo plano mientras juegas, te avisa cuando desbloqueas un logro y guarda tu progreso. Aquí lo ves, lo compartes y lo comparas."),
        h("div", { class: "actions" },
          session ? h("a", { class: "btn", href: "#/cuenta" }, "Vincular mi consola")
                  : h("a", { class: "btn", href: "#/registro" }, "Crear cuenta"),
          h("a", { class: "btn alt", href: "#/juegos" }, "Ver juegos"),
          h("a", { class: "btn alt", href: cfg.repoUrl, rel: "noopener" }, "Descargar en GitHub"))),
      h("img", { src: "img/mascot.png", alt: "Greeny, la mascota: una plantita verde saltando", width: 240, height: 320 }));

    const stats = h("section", { class: "stats", "aria-label": "Números de Greeny" },
      h("div", { class: "card stat" }, h("b", {}, fmtNum(players.count)), h("span", {}, "jugadores")),
      h("div", { class: "card stat" }, h("b", {}, fmtNum(unlocks.count)), h("span", {}, "logros desbloqueados")),
      h("div", { class: "card stat" }, h("b", {}, fmtNum(games.count)), h("span", {}, "juegos con logros")));

    const rows = (recent.data || []).filter((u) => u.profiles && u.achievements).map((u) =>
      h("div", { class: "card row" },
        achIcon(u.achievements, true),
        h("div", { class: "grow" },
          h("div", { class: "clip" }, h("strong", {}, u.achievements.name_es), " ", h("span", { class: "pill pts" }, u.achievements.points + " pts")),
          h("div", { class: "muted small clip" },
            h("a", { href: "#/u/" + encodeURIComponent(u.profiles.username) }, shownName(u.profiles)),
            " en ", h("a", { href: "#/juego/" + u.title_id }, (u.achievements.games && u.achievements.games.name) || u.title_id))),
        h("span", { class: "muted small nowrap" }, fmtDate(u.unlocked_at))));

    return [
      hero,
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
    ];
  }

  // ---------- juegos ----------

  async function viewGames() {
    const [games, achs] = await Promise.all([
      sb.from("games").select("title_id,name,version,cover_path,published").order("name"),
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
      h("section", { class: "row" }, cover(game, true),
        h("div", { class: "grow" },
          h("h1", { style: "margin-bottom:4px" }, game.name),
          h("p", { class: "muted", style: "margin:0" },
            game.version ? "Logros para la versión " + game.version + " del juego" : "Versión sin indicar",
            " · " + achs.length + " logros · " + totalPts + " pts · " + fmtNum(players) + (players === 1 ? " jugador" : " jugadores")))),
      session
        ? h("div", { class: "card", style: "margin-top:18px" },
            h("div", { class: "row" }, h("strong", { class: "grow" }, "Tu progreso"),
              h("span", {}, got.length + " de " + achs.length + " · " + got.reduce((s, a) => s + a.points, 0) + " pts")),
            h("div", { style: "margin-top:8px" }, bar(got.length, achs.length)))
        : null,
      h("h2", {}, "Logros"),
      h("div", { class: "stack" }, achs.map((a) => {
        const st = stats[a.ach_id] || { unlocked_by: 0, players: 0 };
        const pct = st.players > 0 ? Math.round((st.unlocked_by / st.players) * 100) : null;
        const done = !!mine[a.ach_id];
        return h("div", { class: "card row" + (session && !done ? " locked" : "") },
          achIcon(a, done),
          h("div", { class: "grow" },
            h("div", {}, h("strong", { class: "ach-name" }, a.name_es), " ", h("span", { class: "pill pts" }, a.points + " pts")),
            h("div", { class: "muted small" }, a.desc_es || "Sin descripción")),
          h("div", { class: "right small" },
            session ? h("div", {}, done ? h("span", { class: "pill ok" }, "Desbloqueado") : h("span", { class: "pill no" }, "Bloqueado")) : null,
            h("div", { class: "muted", title: "Porcentaje de jugadores de este juego que lo tienen" },
              pct == null ? "Nadie lo tiene aún" : pct + "% lo tiene")));
      })),
    ];
  }

  // ---------- perfil ----------

  async function viewProfile(username) {
    const exact = username.replace(/[\\%_]/g, "\\$&"); // ilike sin comodines: solo ignora mayusculas
    const profile = must(await sb.from("profiles").select("*").ilike("username", exact).maybeSingle());
    if (!profile) return notFound("No existe ningún jugador con ese nombre.");

    const [statRes, unlockRes] = await Promise.all([
      sb.from("profile_stats").select("*").eq("id", profile.id).maybeSingle(),
      sb.from("unlocks").select("title_id,ach_id,unlocked_at,achievements(name_es,points,icon_path)")
        .eq("user_id", profile.id).order("unlocked_at", { ascending: false }),
    ]);
    const stats = statRes.data || { achievements: 0, points: 0, games: 0 };
    const unlocks = (unlockRes.data || []).filter((u) => u.achievements);
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
    return [
      h("section", { class: "card row" }, avatar(profile, "big"),
        h("div", { class: "grow" },
          h("h1", { style: "margin-bottom:2px" }, shownName(profile)),
          h("div", { class: "muted" }, "@" + profile.username, profile.discord_username ? " · Discord: " + profile.discord_username : ""),
          profile.caption ? h("p", { style: "margin:8px 0 0;font-size:1.1rem" }, "“" + profile.caption + "”") : null,
          h("div", { class: "muted small", style: "margin-top:6px" }, "En Greeny desde " + fmtDate(profile.created_at))),
        isMe ? h("a", { class: "btn alt", href: "#/cuenta" }, "Editar") : null),
      h("section", { class: "stats", style: "margin-top:14px" },
        h("div", { class: "card stat" }, h("b", {}, fmtNum(stats.points)), h("span", {}, "puntos")),
        h("div", { class: "card stat" }, h("b", {}, fmtNum(stats.achievements)), h("span", {}, "logros")),
        h("div", { class: "card stat" }, h("b", {}, fmtNum(stats.games)), h("span", {}, "juegos"))),
      h("h2", {}, "Juegos"),
      titles.length
        ? h("div", { class: "stack" }, titles.map((tid) => {
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
          }))
        : h("p", { class: "card muted" }, isMe ? "Todavía no has subido logros. Vincula tu consola en Cuenta." : "Este jugador todavía no ha subido logros."),
      unlocks.length ? h("h2", {}, "Últimos logros") : null,
      unlocks.length
        ? h("div", { class: "stack" }, unlocks.slice(0, 12).map((u) =>
            h("div", { class: "card row" }, achIcon(u.achievements, true),
              h("div", { class: "grow" },
                h("div", { class: "clip" }, h("strong", {}, u.achievements.name_es), " ", h("span", { class: "pill pts" }, u.achievements.points + " pts")),
                h("div", { class: "muted small clip" }, (games[u.title_id] || {}).name || u.title_id)),
              h("span", { class: "muted small nowrap" }, fmtDate(u.unlocked_at)))))
        : null,
    ];
  }

  // ---------- entrar y registrarse ----------

  function discordButton() {
    return h("button", { class: "btn discord", type: "button", onclick: async () => {
      const { error } = await sb.auth.signInWithOAuth({ provider: "discord", options: { redirectTo: here } });
      if (error) alert(explain(error));
    } }, "Continuar con Discord");
  }

  function viewLogin() {
    if (session) { location.hash = "#/cuenta"; return []; }
    return [
      h("h1", {}, "Entrar"),
      form([
        h("label", {}, "Correo", h("input", { name: "email", type: "email", autocomplete: "email", required: true })),
        h("label", {}, "Contraseña", h("input", { name: "password", type: "password", autocomplete: "current-password", required: true })),
      ], "Entrar", async (fd) => {
        must(await sb.auth.signInWithPassword({ email: fd.get("email"), password: fd.get("password") }));
        location.hash = "#/cuenta";
      }),
      h("p", { class: "or", style: "margin:18px 0" }, "o"),
      discordButton(),
      h("p", { class: "muted", style: "margin-top:22px" }, "¿No tienes cuenta? ", h("a", { href: "#/registro" }, "Créala aquí"), "."),
    ];
  }

  function viewRegister() {
    if (session) { location.hash = "#/cuenta"; return []; }
    return [
      h("h1", {}, "Crear cuenta"),
      form([
        h("label", {}, "Nombre de usuario", h("span", { class: "hint" }, "Letras, números y _ (de 3 a 20). Es el de tu perfil público."),
          h("input", { name: "username", required: true, minlength: 3, maxlength: 20, pattern: "[A-Za-z0-9_]{3,20}", autocomplete: "username", autocapitalize: "none", spellcheck: "false" })),
        h("label", {}, "Correo", h("input", { name: "email", type: "email", autocomplete: "email", required: true })),
        h("label", {}, "Contraseña", h("span", { class: "hint" }, "Mínimo 8 caracteres."),
          h("input", { name: "password", type: "password", autocomplete: "new-password", required: true, minlength: 8 })),
      ], "Crear cuenta", async (fd, el) => {
        const data = must(await sb.auth.signUp({
          email: fd.get("email"), password: fd.get("password"),
          options: { data: { username: fd.get("username") }, emailRedirectTo: here },
        }));
        if (data.session) { location.hash = "#/cuenta"; return null; }
        el.reset();
        return "Te enviamos un correo para confirmar tu cuenta. Ábrelo y sigue el enlace.";
      }),
      h("p", { class: "or", style: "margin:18px 0" }, "o"),
      discordButton(),
      h("p", { class: "muted", style: "margin-top:22px" }, "¿Ya tienes cuenta? ", h("a", { href: "#/entrar" }, "Entra aquí"), "."),
    ];
  }

  // ---------- cuenta ----------

  async function viewAccount() {
    if (!session) { location.hash = "#/entrar"; return []; }
    if (!me) return [h("h1", {}, "Cuenta"), h("p", { class: "msg err" }, "No se encontró tu perfil. Cierra sesión y vuelve a entrar.")];

    const [presetRes, deviceRes] = await Promise.all([
      sb.from("caption_presets").select("text_es").order("sort"),
      sb.from("devices").select("id,name,created_at,last_seen_at").order("created_at"),
    ]);
    const presets = (presetRes.data || []).map((p) => p.text_es);
    const CUSTOM = "__custom__";
    const isPreset = !me.caption || presets.includes(me.caption);

    const customInput = h("input", { name: "caption_custom", maxlength: 60, placeholder: "Escribe tu frase", value: isPreset ? "" : me.caption, "aria-label": "Frase personalizada" });
    const customWrap = h("div", { hidden: isPreset }, customInput);
    const select = h("select", { name: "caption_choice", onchange: (ev) => { customWrap.hidden = ev.target.value !== CUSTOM; if (!customWrap.hidden) customInput.focus(); } },
      h("option", { value: "" }, "Sin frase"),
      presets.map((p) => h("option", { value: p, selected: me.caption === p }, p)),
      h("option", { value: CUSTOM, selected: !isPreset }, "Escribir la mía…"));

    const profileForm = form([
      h("label", {}, "Nombre de usuario", h("span", { class: "hint" }, "Si lo cambias, cambia el enlace de tu perfil."),
        h("input", { name: "username", required: true, minlength: 3, maxlength: 20, pattern: "[A-Za-z0-9_]{3,20}", value: me.username, autocapitalize: "none", spellcheck: "false" })),
      h("label", {}, "Nombre visible", h("span", { class: "hint" }, "Opcional. Hasta 32 caracteres."),
        h("input", { name: "display_name", maxlength: 32, value: me.display_name || "" })),
      h("label", {}, "Frase de tu perfil", h("span", { class: "hint" }, "Elige una o escribe la tuya (hasta 60 caracteres)."), select),
      customWrap,
    ], "Guardar perfil", async (fd) => {
      const choice = fd.get("caption_choice");
      const caption = (choice === CUSTOM ? String(fd.get("caption_custom")).trim() : choice) || null;
      must(await sb.from("profiles").update({
        username: fd.get("username"), display_name: String(fd.get("display_name")).trim() || null, caption,
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
      h("h1", {}, "Cuenta"),
      h("p", { class: "muted" }, "Tu perfil público: ", h("a", { href: "#/u/" + encodeURIComponent(me.username) }, "@" + me.username)),
      h("h2", {}, "Perfil"), h("div", { class: "card" }, profileForm),
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
        : h("p", { class: "card muted" }, "No tienes ninguna consola vinculada."),
    ];
  }

  // ---------- administrador: publicar juegos e imagenes ----------

  async function viewAdmin() {
    if (!session) { location.hash = "#/entrar"; return []; }
    if (!admin) return notFound("Esta página es solo para administradores.");
    const games = (await sb.from("games").select("title_id,name").order("name")).data || [];
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
      const text = await file.text();
      const def = window.GreenyDefs.parseDefinition(text);
      must(await sb.from("games").upsert({
        title_id: tid, name: def.name || tid, version: def.version || null, build_id: def.build || null,
        definition: text, published: fd.get("published") === "on",
      }));
      // upsert: no toca icon_path de los logros que ya existian
      must(await sb.from("achievements").upsert(def.achievements.map((a, i) => ({
        title_id: tid, ach_id: a.id, name_es: a.name, desc_es: a.desc, name_en: a.name_en, desc_en: a.desc_en, points: a.points, sort: i,
      }))));
      setTimeout(render, 2000);
      return "Publicado: " + (def.name || tid) + " con " + def.achievements.length + " logros.";
    });

    const icons = form([
      h("label", {}, "Juego", gameSelect()),
      h("label", {}, "Iconos de logros", h("span", { class: "hint" }, "PNG cuadrados (128×128). Cada archivo se llama como el id del logro: monedas_5.png"),
        h("input", { name: "files", type: "file", accept: "image/png", multiple: true, required: true })),
    ], "Subir iconos", async (fd) => {
      const tid = fd.get("title_id");
      const known = new Set(((await sb.from("achievements").select("ach_id").eq("title_id", tid)).data || []).map((a) => a.ach_id));
      const done = [], skipped = [];
      for (const file of fd.getAll("files")) {
        const id = file.name.replace(/\.png$/i, "");
        if (!known.has(id)) { skipped.push(file.name); continue; }
        const path = tid + "/" + id + ".png";
        must(await sb.storage.from("icons").upload(path, file, { upsert: true, contentType: "image/png", cacheControl: "86400" }));
        must(await sb.from("achievements").update({ icon_path: path }).eq("title_id", tid).eq("ach_id", id));
        done.push(id);
      }
      if (!done.length) throw new Error("Ningún archivo coincide con un id de logro de ese juego: " + skipped.join(", "));
      return done.length + " icono(s) subido(s)." + (skipped.length ? " Sin logro con ese nombre: " + skipped.join(", ") + "." : "");
    });

    const coverForm = form([
      h("label", {}, "Juego", gameSelect()),
      h("label", {}, "Carátula", h("span", { class: "hint" }, "JPG o PNG cuadrado (512×512). Solo se usa en la web."),
        h("input", { name: "file", type: "file", accept: "image/png,image/jpeg", required: true })),
    ], "Subir carátula", async (fd) => {
      const tid = fd.get("title_id"), file = fd.get("file");
      const path = tid + (file.type === "image/png" ? ".png" : ".jpg");
      must(await sb.storage.from("covers").upload(path, file, { upsert: true, contentType: file.type, cacheControl: "86400" }));
      must(await sb.from("games").update({ cover_path: path }).eq("title_id", tid));
      return "Carátula subida.";
    });

    return [
      h("h1", {}, "Publicar"),
      h("p", { class: "muted" }, "Publicar de nuevo un juego actualiza sus textos y puntos. No borra logros que ya no estén en el archivo, porque eso borraría el progreso de la gente."),
      h("h2", {}, "1. Juego y logros"), h("div", { class: "card" }, publish),
      h("h2", {}, "2. Iconos de logros"), h("div", { class: "card" }, icons),
      h("h2", {}, "3. Carátula"), h("div", { class: "card" }, coverForm),
    ];
  }

  function notFound(text) {
    return [h("h1", {}, "No encontrado"), h("p", { class: "card" }, text || "Esa página no existe."), h("p", {}, h("a", { href: "#/" }, "Volver al inicio"))];
  }

  // ---------- rutas ----------

  let renderId = 0;
  async function render() {
    const id = ++renderId;
    const parts = location.hash.replace(/^#\/?/, "").split("/").map(decodeURIComponent);
    const routes = { "": viewHome, juegos: viewGames, juego: viewGame, u: viewProfile, entrar: viewLogin, registro: viewRegister, cuenta: viewAccount, admin: viewAdmin };
    const view = routes[parts[0]];
    renderNav();
    try {
      const nodes = view ? await view(parts[1] || "") : notFound();
      if (id !== renderId) return; // el usuario ya navego a otra parte
      main.replaceChildren(...nodes.filter(Boolean));
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

  document.getElementById("repo-link").href = cfg.repoUrl;

  let signedInAs = undefined; // para no redibujar cuando Supabase solo refresca el token
  sb.auth.onAuthStateChange((_event, s) => {
    session = s;
    const who = s ? s.user.id : null;
    if (who === signedInAs) return;
    signedInAs = who;
    // fuera del callback: supabase-js no deja hacer consultas dentro de el
    setTimeout(async () => { await loadMe(); render(); }, 0);
  });
})();
