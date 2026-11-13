/* Renderiza las pantallas de la app en PC a archivos PPM para revisarlas.
 *   ./ui_host <fuente.ttf> <raiz de SD falsa> <carpeta de salida> <carpeta app/data>
 * La raiz de SD falsa debe tener config/greeny/ y, si se quiere, atmosphere/contents/... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../app/source/diag.h"
#include "../../app/source/gfx.h"
#include "../../app/source/model.h"
#include "../../app/source/ui.h"

#include "../../app/source/third_party/stb_image.h" /* implementacion: app/source/stb_impl.c */

static uint8_t *slurp(const char *dir, const char *name, size_t expect) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    uint8_t *buf = malloc(expect);
    if (fread(buf, 1, expect, f) != expect) { fprintf(stderr, "%s: tamaño inesperado\n", path); exit(1); }
    fclose(f);
    return buf;
}

static float g_time; /* instante del fondo animado con el que se guarda la captura */

/* `layer` es la capa transparente de la pantalla: se compone sobre el fondo como hace la app */
static void save(const gfx_canvas *layer, const char *dir, const char *name, const char *sfx) {
    static uint32_t *final_px;
    if (!final_px) final_px = malloc(UI_W * UI_H * 4);
    gfx_canvas out = { final_px, UI_W, UI_H, UI_W };
    ui_background(&out, g_time);
    gfx_compose(&out, layer);
    const gfx_canvas *c = &out;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s%s.ppm", dir, name, sfx);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6 %d %d 255\n", c->w, c->h);
    for (int i = 0; i < c->w * c->h; i++) fwrite(&c->px[i], 1, 3, f);
    fclose(f);
}

/* caratula falsa (degradado 256x256) pasada por el mismo camino que la real */
static uint8_t *fake_cover(int size) {
    uint8_t *big = malloc(256 * 256 * 4), *small = malloc((size_t)size * size * 4);
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            uint8_t *p = big + (y * 256 + x) * 4;
            p[0] = 230; p[1] = (uint8_t)(60 + x / 2); p[2] = (uint8_t)(40 + y / 2); p[3] = 255;
        }
    gfx_scale_rgba(big, 256, 256, small, size, size);
    gfx_round_corners(small, size, size, size / 6);
    free(big);
    return small;
}

static gr_model m;

int main(int argc, char **argv) {
    if (argc != 5) { fprintf(stderr, "uso: %s <ttf> <sd> <salida> <app/data>\n", argv[0]); return 2; }
    if (!gfx_font_from_file(argv[1])) { fprintf(stderr, "no se pudo abrir la fuente\n"); return 1; }
    const char *sd = argv[2], *out = argv[3];
    char root[512];
    snprintf(root, sizeof(root), "%s/config/greeny", sd);

    static ui_assets a;
    a.mascot_big = slurp(argv[4], "mascot_big.bin", 240 * 240 * 4);   a.big_w = 240;  a.big_h = 240;
    a.buttons = slurp(argv[4], "buttons.bin", UI_BTN_COUNT * UI_BTN_SIZE * UI_BTN_SIZE * 4);
    /* fondos: JPEG, decodificados igual que en la app */
    uint32_t *bg[2] = { NULL, NULL };
    for (int i = 0; i < 2; i++) {
        char path[512];
        int w, h, n;
        snprintf(path, sizeof(path), "%s/%s", argv[4], i ? "bg_oscuro.bin" : "bg_claro.bin");
        FILE *bf = fopen(path, "rb");
        static uint8_t jpg[400 * 1024];
        size_t jl = bf ? fread(jpg, 1, sizeof(jpg), bf) : 0;
        if (bf) fclose(bf);
        bg[i] = (uint32_t *)stbi_load_from_memory(jpg, (int)jl, &w, &h, &n, 4);
        if (!bg[i] || w != UI_W || h != UI_H) { fprintf(stderr, "fondo %d invalido\n", i); return 1; }
    }
    a.mascot_small = slurp(argv[4], "mascot_small.bin", 66 * 66 * 4); a.small_w = 66; a.small_h = 66;
    {
        static const struct { const char *name; int w, h; } imgs[UI_IMG_COUNT] = {
#define UI_IMG(name, w, h) { #name, w, h },
#include "../../app/source/ui_images.h"
#undef UI_IMG
        };
        for (int i = 0; i < UI_IMG_COUNT; i++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/img_%s.bin", argv[4], imgs[i].name);
            int w, h, n;
            static uint8_t file[512 * 1024];
            FILE *f = fopen(path, "rb");
            size_t len = f ? fread(file, 1, sizeof(file), f) : 0;
            if (f) fclose(f);
            uint8_t *px = len ? stbi_load_from_memory(file, (int)len, &w, &h, &n, 4) : NULL;
            if (!px || w != imgs[i].w || h != imgs[i].h) { fprintf(stderr, "imagen %s invalida\n", imgs[i].name); return 1; }
            a.img[i] = px;
        }
    }
    gfx_canvas c = { malloc(UI_W * UI_H * 4), UI_W, UI_H, UI_W };

    gr_scan(&m, root);
    /* la ultima entrada queda con caratula; las demas muestran el recuadro con la inicial */
    if (m.ngames > 0) {
        a.covers[m.ngames - 1] = fake_cover(UI_COVER);
        a.covers_big[m.ngames - 1] = fake_cover(UI_COVER_BIG);
    }

    gr_diag_env env = { .sd = sd, .root = root, .atmosphere = true, .sysmodule_running = false, .covers_ok = true,
                        .applet_mode = true, .prev_run_known = true, .prev_run_clean = false };
    snprintf(env.prev_last_step, sizeof(env.prev_last_step), "caratulas");
    static gr_diag_item items[GR_DIAG_MAX];
    int nitems = gr_diag_run(items, GR_DIAG_MAX, &env, &m);

    /* variantes: tema x idioma x vista */
    for (int v = 0; v < 4; v++) {
        bool dark = v & 1, en = v & 2;
        char sfx[32];
        snprintf(sfx, sizeof(sfx), "%s%s", dark ? "_dark" : "", en ? "_en" : "");
        static ui_profile chip = { .name = "Jhondy", .username = "megaJHONDY", .caption = "", .linked = true, .donor = true, .avatar = 4, .country = "DO", .stats = { 1, 7, 0, 180 } };
        ui_set_profile(&chip);
        ui_set_dark(dark);
        ui_set_background(bg[dark]);
        ui_set_english(en);
        g_time = 0;

        for (int grid = 0; grid < 2; grid++) {
            gr_config cfg;
            gr_config_defaults(&cfg);
            cfg.games_view = cfg.ach_view = grid ? GR_VIEW_GRID : GR_VIEW_LIST;
            char name[64];

            ui_home(&c, &a, &m, &cfg, m.ngames > 1 ? 1 : 0, 0);
            save(&c, out, grid ? "home_grid" : "home", sfx);

            for (int g = 0; g < m.ngames; g++) {
                gr_open(&m, g, root);
                /* iconos guardados en <sd>/config/greeny/cache, si los hay */
                for (int i = 0; i < LG_MAX_ACH; i++) a.ach_icons[i] = a.ach_icons_gray[i] = a.ach_big[i] = a.ach_big_gray[i] = NULL;
                for (int i = 0; i < m.cur.nach; i++) {
                    uint8_t *big = malloc(GR_ICON_BIG_BYTES), *big_gray = malloc(GR_ICON_BIG_BYTES);
                    uint8_t *img = malloc(GR_ICON_BYTES), *gray = malloc(GR_ICON_BYTES);
                    if (i != 2 && gr_icon_load(root, m.games[g].tid, m.cur.ach[i].id, big)) { /* el tercero queda sin icono: se ve el de relleno */
                        gfx_scale_rgba(big, GR_ICON_BIG, GR_ICON_BIG, img, GR_ICON_SIZE, GR_ICON_SIZE);
                        gr_icon_gray(big, big_gray, GR_ICON_BIG);
                        gr_icon_gray(img, gray, GR_ICON_SIZE);
                        a.ach_icons[i] = img; a.ach_icons_gray[i] = gray;
                        a.ach_big[i] = big; a.ach_big_gray[i] = big_gray;
                    }
                }
                snprintf(name, sizeof(name), grid ? "detail%d_grid" : "detail%d", g);
                ui_detail(&c, &a, &m, &cfg, g, 1, 0);
                save(&c, out, name, sfx);
                if (!grid && !m.games[g].broken && m.cur.nach > 0) {
                    /* un logro al detalle: uno que ya se tiene y el ultimo */
                    snprintf(name, sizeof(name), "ach%d_a", g);
                    ui_achievement(&c, &a, &m, g, m.cur.nach > 3 ? 3 : 0);
                    save(&c, out, name, sfx);
                    snprintf(name, sizeof(name), "ach%d_b", g);
                    ui_achievement(&c, &a, &m, g, m.cur.nach - 1);
                    save(&c, out, name, sfx);
                }
                ui_layout l = ui_detail_layout(&cfg);
                int rows = (m.cur.nach + l.cols - 1) / l.cols;
                if (rows > l.rows) {
                    snprintf(name, sizeof(name), grid ? "detail%d_grid_end" : "detail%d_end", g);
                    ui_detail(&c, &a, &m, &cfg, g, m.cur.nach - 1, rows - l.rows);
                    save(&c, out, name, sfx);
                }
            }
        }

        gr_config cfg;
        gr_config_defaults(&cfg);
        cfg.popup = false;
        cfg.games_view = GR_VIEW_GRID;
        for (int t = 0; t < UI_TABS; t++) {
            char name[32];
            snprintf(name, sizeof(name), "settings%d", t);
            ui_account acc = { .linked = false, .username = "", .caption = "", .sync_es = "", .sync_en = "" };
            ui_settings(&c, &a, &cfg, t, ui_tab_count(t, false) - 1, true, dark, &acc);
            save(&c, out, name, sfx);
        }
        ui_account linked = { .linked = true, .username = "megaJHONDY", .caption = "Coleccionista de logros",
                              .sync_es = "Al día: 7 logros enviados, 2 nuevos en tu cuenta.",
                              .sync_en = "Up to date: 7 achievements sent, 2 new on your account." };
        ui_settings(&c, &a, &cfg, UI_TAB_ACCOUNT, 1, true, dark, &linked);
        save(&c, out, "settings_cuenta", sfx);
        linked.notify[0] = true; linked.notify[1] = false; linked.notify[2] = true; linked.notify[3] = true; linked.notify[4] = true; linked.notify[5] = false; linked.notify[6] = true;
        ui_settings(&c, &a, &cfg, UI_TAB_NOTIFY, 1, true, dark, &linked);
        save(&c, out, "settings_avisos", sfx);
        ui_profile prof = { .name = "Jhondy", .username = "megaJHONDY", .caption = "Coleccionista de logros", .friend_code = "GR-4821-0937-5516", .joined = "2025-04-11", .likes = 14, .country = "DO", .linked = true,
                            .social_known = true, .followers = 12, .following = 3,
                            .stats = { .games = m.games_played, .achievements = m.unlocked, .platinums = m.platinums, .points = m.points_got } };
        ui_set_profile(NULL);
        ui_profile_screen(&c, &a, &prof);
        save(&c, out, "perfil", sfx);
        ui_profile anon = { .name = "Jhondy", .username = "", .caption = "", .linked = false, .stats = { 0, 0, 0, 0 } };
        ui_profile_screen(&c, &a, &anon);
        save(&c, out, "perfil_sin_cuenta", sfx);
        ui_set_profile(&prof);
        ui_badges(&c, &a, &prof, 0);
        save(&c, out, "medallas", sfx);
        ui_link(&c, &a, "JSH6PN", 547);
        save(&c, out, "link", sfx);
        ui_link(&c, &a, "JSH6PN", 0);
        save(&c, out, "link_vencido", sfx);
        ui_message(&c, &a, "¡Consola vinculada!", "Esta consola quedó vinculada a @megaJHONDY.", "Tus logros se suben solos cuando hay internet.", true);
        save(&c, out, "msg_ok", sfx);
        ui_message(&c, &a, "No se pudo entrar", "Correo o contraseña incorrectos.", "Si entras con Discord, usa \"Vincular con un código\".", false);
        save(&c, out, "msg_error", sfx);
        ui_diag(&c, &a, items, nitems);
        save(&c, out, "diag", sfx);
        ui_about(&c, &a, "https://discord.gg/greeny");
        save(&c, out, "about", sfx);
        for (int p = 0; p < UI_INTRO_PAGES; p++) {
            char name[32];
            snprintf(name, sizeof(name), "intro%d", p);
            ui_intro(&c, &a, p, items, nitems, env.sysmodule_running);
            save(&c, out, name, sfx);
        }
        /* la ultima pagina cuando todo esta bien */
        static gr_diag_item ok_items[GR_DIAG_MAX];
        memcpy(ok_items, items, sizeof(ok_items));
        for (int i = 0; i < nitems; i++) if (ok_items[i].state == GR_DIAG_FAIL) ok_items[i].state = GR_DIAG_OK;
        ui_intro(&c, &a, 2, ok_items, nitems, true);
        save(&c, out, "intro2_ok", sfx);

        /* el mismo inicio en otros dos instantes, para ver que el fondo se mueve */
        cfg.games_view = GR_VIEW_LIST;
        ui_set_user("Jhondy");
        {
            static gr_activity act[2] = { { "luis", "Luis", 15, "Super Mario 3D World", "Medio camino", "Halfway", 10, 1794470000LL },
                                          { "ana_99", "Ana", 9, "Kirby and the Forgotten Land", "Primera partida", "First game", 5, 1794300000LL } };
            ui_set_activity(act, 2, 0, 1794477600LL);
            cfg.activity = true;
            ui_home(&c, &a, &m, &cfg, 0, 0);
            save(&c, out, "home_actividad", sfx);
            ui_set_activity(NULL, 0, 0, 0);
        }
        ui_home(&c, &a, &m, &cfg, 0, 0);
        ui_set_user(NULL);
        g_time = 12.0f; save(&c, out, "home_t12", sfx);
        g_time = 0.001f; /* resto de capturas con las burbujas en su sitio inicial */
        g_time = 300.0f; save(&c, out, "home_t300", sfx);
        g_time = 0;

        static gr_model empty;
        static ui_assets bare;
        bare = a;
        memset(bare.covers, 0, sizeof(bare.covers));
        memset(bare.covers_big, 0, sizeof(bare.covers_big));
        ui_home(&c, &bare, &empty, &cfg, 0, 0);
        save(&c, out, "home_empty", sfx);
        {
            static const ui_song songs[4] = { { "rest_garden", "Rest garden", 1171, true }, { "cold_in_summer", "Cold in summer", 1112, true },
                                              { "beyond_the_end", "Beyond the end", 1295, false }, { "excited_for_the_last_time", "Excited for the last time", 1244, false } };
            gr_config mc;
            gr_config_defaults(&mc);
            mc.music = true;
            snprintf(mc.music_track, sizeof(mc.music_track), "rest_garden");
            static const ui_account none = { .linked = false, .username = "", .caption = "", .sync_es = "", .sync_en = "" };
            ui_set_music(songs, 4, 4);
            ui_settings(&c, &a, &mc, UI_TAB_MUSIC, 2, true, dark, &none);
            save(&c, out, "musica", sfx);
            ui_set_music(NULL, 0, -1);
        }
        {
            static const char lines[6][128] = { "Filtro terminado.", "Paso 3  |  valor 12  |  tipo u32  |  3 candidata(s)", "#0  heap+0x1A2B3C40",
                                                "#1  heap+0x1A2B3D10", "#2  main+0x19F0124", "Ya son pocas: elige una y busca sus punteros." };
            ui_search(&c, &a, 10, 32, 1, 0, lines, 6, true);
            save(&c, out, "buscador_valores", sfx);
        }
        {
            static const gr_rank_row rows[10] = {
                { 1, "megaJHONDY", "Jhondy", 4, "DO", 520, 900, 41 }, { 2, "ana_99", "", 9, "MX", 480, 480, 30 }, { 3, "luis", "Luis M", 15, "ES", 310, 310, 22 },
                { 4, "player4", "", 20, "US", 250, 400, 18 }, { 5, "kira", "Kira", 28, "JP", 240, 240, 17 }, { 6, "sol", "Sol", 31, "AR", 200, 200, 15 },
                { 7, "nacho", "", 2, "", 180, 180, 12 }, { 8, "bea", "Bea", 12, "CO", 150, 150, 11 }, { 9, "tom", "Tom", 17, "GB", 120, 120, 9 }, { 10, "zed", "", 33, "BR", 100, 100, 8 } };
            static const gr_rank_row yo = { 14, "Jhondy", "Jhondy", 4, "DO", 70, 70, 7 };
            ui_ranking(&c, &a, rows, 10, &yo, 37, "Global", "Global", "Puntos totales", 2, 1);
            save(&c, out, "ranking", sfx);
            static gr_review rv[4] = {
                { 7, "Los logros de las estrellas están durísimos, pero vale la pena. El de los sellos me tomó toda una tarde buscando en cada nivel del mundo 4, y todavía me falta el de la corona.", "Ana_99", "Ana", 9, "MX", true, 310, 9, 3, true, false, false, "2026-11-10" },
                { 8, "Me encanta el de la Supercampana", "megaJHONDY", "Jhondy", 4, "DO", false, 190, 12, 1, false, true, false, "2026-11-11" },
                { 9, "Buenísimo", "luis", "", 15, "", false, 40, 2, 0, false, false, true, "2026-11-12" },
            };
            ui_reviews(&c, &a, "Super Mario 3D World", rv, 3, 0, 0, 1, true, false);
            save(&c, out, "comentarios", sfx);
            ui_reviews(&c, &a, "Luigi's Mansion 3", rv, 0, 0, 0, 1, true, true);
            save(&c, out, "comentarios_vacio", sfx);
            gr_rank_row from_rev = { 0, "Ana_99", "Ana", 9, "MX", 3, 310, 9 };
            ui_user(&c, &a, &from_rev, true);
            save(&c, out, "usuario_comentario", sfx);
            static gr_notif nt[6] = {
                { 1, GR_NOTIF_LIKE, false, "Ana_99", "Ana", 9, "010028600EBDA000", "Super Mario 3D World", "", "", "", 0, "2026-11-12" },
                { 5, GR_NOTIF_FRIEND, false, "luis", "Luis", 15, "010028600EBDA000", "Super Mario 3D World", "", "", "", 14, "2026-11-12" },
                { 6, GR_NOTIF_RANKING, false, "", "", 0, "010028600EBDA000", "Super Mario 3D World", "", "", "", 3, "2026-11-12" },
                { 7, GR_NOTIF_ALMOST, true, "", "", 0, "010028600EBDA000", "Super Mario 3D World", "", "", "", 2, "2026-11-11" },
                { 3, GR_NOTIF_BADGE, true, "", "", 0, "", "", "", "", "logros_10", 0, "2026-11-11" },
                { 4, GR_NOTIF_ACH, true, "", "", 0, "010028600EBDA000", "Super Mario 3D World", "Medio camino", "Halfway", "", 0, "2026-11-11" },
            };
            ui_set_unread(2);
            ui_notifs(&c, &a, nt, 6, 3, 0, 0, 1);
            save(&c, out, "avisos", sfx);
            ui_notifs(&c, &a, nt, 0, 0, 0, 0, 3);
            save(&c, out, "avisos_sin_cuenta", sfx);
            ui_update(&c, &a, UI_UPD_AVAILABLE, "1.0.0", "- Avisos dentro de la app (ZL o ZR)\n- Comentarios con me gusta en cada juego\n- Actualizar sin computadora", 0, 0);
            save(&c, out, "actualizar", sfx);
            ui_update(&c, &a, UI_UPD_DOWNLOADING, "1.0.0", "- Avisos dentro de la app (ZL o ZR)", 3 << 20, 7 << 20);
            save(&c, out, "actualizar_bajando", sfx);
            ui_splash(&c, &a, "Jhondy", false, 800);
            save(&c, out, "splash", sfx);
            ui_splash(&c, &a, "Jhondy", false, 30);
            save(&c, out, "splash_pop1", sfx);
            ui_splash(&c, &a, "Jhondy", false, 70);
            save(&c, out, "splash_pop2", sfx);
            ui_splash(&c, &a, "", true, 800);
            save(&c, out, "splash_primera", sfx);
            ui_ranking(&c, &a, rows, 3, &yo, 14, "Mi país", "My country", "Super Mario 3D World", 0, 1);
            save(&c, out, "ranking_pais", sfx);
            ui_ranking(&c, &a, rows, 0, &yo, 0, "Mi país", "My country", "Puntos totales", 0, 3);
            save(&c, out, "ranking_sin_pais", sfx);
            ui_user(&c, &a, &rows[2], true);
            save(&c, out, "usuario", sfx);
            ui_countries(&c, &a, 0, "DO");
            save(&c, out, "paises", sfx);
        }
        ui_avatars(&c, &a, &chip, 7);
        save(&c, out, "avatars", sfx);
        {
            static gr_cat_game cat[5] = {
                { "010028600EBDA000", "Super Mario 3D World + Bowser's Fury", "1.1.0", "jugable" },
                { "0100AAAAAAAAA000", "Otro juego de plataformas", "2.0.1", "perfecto" },
                { "0100BBBBBBBBB000", "Carreras locas", "", "prueba" },
                { "0100CCCCCCCCC000", "Aventura sin terminar", "1.0.0", "sin_empezar" },
                { "0100DDDDDDDDD000", "Un juego con un nombre larguisimo que no cabe en la fila", "1.2.3", "jugable" },
            };
            static const bool have[5] = { true, false, false, false, false };
            static const int all[5] = { 0, 1, 2, 3, 4 };
            static uint8_t fake[UI_BROWSE_COVER * UI_BROWSE_COVER * 4];
            for (int i = 0; i < UI_BROWSE_COVER * UI_BROWSE_COVER; i++) { fake[i * 4] = 230; fake[i * 4 + 1] = (uint8_t)(60 + i % UI_BROWSE_COVER * 2); fake[i * 4 + 2] = 70; fake[i * 4 + 3] = 255; }
            gfx_round_corners(fake, UI_BROWSE_COVER, UI_BROWSE_COVER, 10);
            const uint8_t *bc[5] = { fake, NULL, fake, NULL, NULL };
            ui_browse(&c, &a, cat, all, 5, 5, have, bc, 0, UI_BROWSE_OK, 1, 0);
            save(&c, out, "browse_todos", sfx);
            ui_browse(&c, &a, cat, all, 5, 5, have, bc, 3, UI_BROWSE_OK, 1, 0);
            save(&c, out, "browse", sfx);
            ui_browse(&c, &a, cat, all, 0, 5, have, NULL, 4, UI_BROWSE_OK, 0, 0);
            save(&c, out, "browse_nada", sfx);
            ui_browse(&c, &a, cat, all, 0, 0, have, NULL, 0, UI_BROWSE_OFFLINE, 0, 0);
            save(&c, out, "browse_sin_red", sfx);
            gr_open(&m, m.ngames - 1, root);
            ui_browse_game(&c, &a, &cat[0], &m.cur, 1, 0);
            save(&c, out, "browse_juego", sfx);
        }
        empty.hidden = 3; /* hay juegos, pero el filtro los deja fuera */
        ui_home(&c, &bare, &empty, &cfg, 0, 0);
        save(&c, out, "home_filtro", sfx);
        empty.hidden = 0;
    }
    printf("%d juego(s), %d comprobaciones de diagnostico\n", m.ngames, nitems);
    for (int i = 0; i < nitems; i++) printf("  [%d] %s: %s\n", items[i].state, items[i].title_es, items[i].detail_es);
    return 0;
}
