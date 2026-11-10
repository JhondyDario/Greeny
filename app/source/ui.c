#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ---------- paleta: verde, blanco, rojo y amarillo, en claro y oscuro ---------- */

typedef struct {
    gfx_color bg, card, card_sel, ink, gray, gray_lt, shadow, footer, accent, points;
} palette;

static const palette k_light = {
    .bg = GFX_RGB(0xF3, 0xF7, 0xEC), .card = GFX_RGB(0xFF, 0xFF, 0xFF), .card_sel = GFX_RGB(0xE3, 0xF4, 0xDF),
    .ink = GFX_RGB(0x1E, 0x2A, 0x1F), .gray = GFX_RGB(0x6B, 0x7A, 0x6C), .gray_lt = GFX_RGB(0xD9, 0xE2, 0xD4),
    .shadow = GFX_RGB(0xD3, 0xDD, 0xCB), .footer = GFX_RGB(0xFF, 0xFF, 0xFF), .accent = GFX_RGB(0x1D, 0x7A, 0x2A),
    .points = GFX_RGB(0x8A, 0x65, 0x00),
};
static const palette k_dark = {
    .bg = GFX_RGB(0x12, 0x1A, 0x14), .card = GFX_RGB(0x1F, 0x2B, 0x22), .card_sel = GFX_RGB(0x2A, 0x4A, 0x30),
    .ink = GFX_RGB(0xEE, 0xF4, 0xEA), .gray = GFX_RGB(0xA3, 0xB1, 0xA4), .gray_lt = GFX_RGB(0x3A, 0x4A, 0x3E),
    .shadow = GFX_RGB(0x0A, 0x0F, 0x0B), .footer = GFX_RGB(0x18, 0x22, 0x1B), .accent = GFX_RGB(0x6F, 0xD6, 0x7B),
    .points = GFX_RGB(0xF7, 0xC9, 0x1E),
};
static const palette *P = &k_light;

/* iguales en los dos temas */
#define C_GREEN     GFX_RGB(0x2F, 0xA6, 0x3B)
#define C_GREEN_DK  GFX_RGB(0x1D, 0x7A, 0x2A)
#define C_GREEN_LT  GFX_RGB(0xE3, 0xF4, 0xDF)
#define C_WHITE     GFX_RGB(0xFF, 0xFF, 0xFF)
#define C_RED       GFX_RGB(0xE5, 0x48, 0x4D)
#define C_YELLOW    GFX_RGB(0xF7, 0xC9, 0x1E)
#define C_ON_YELLOW GFX_RGB(0x1E, 0x2A, 0x1F)
#define C_NEUTRAL   GFX_RGB(0x6B, 0x7A, 0x6C)

#define HEADER_H 96
#define FOOTER_Y 660
#define MARGIN   40
#define CONTENT_W (UI_W - 2 * MARGIN)

static bool g_en;
#define T(es, en) (g_en ? (en) : (es))

void ui_set_english(bool english) { g_en = english; }
void ui_set_dark(bool dark) { P = dark ? &k_dark : &k_light; }

ui_layout ui_home_layout(const gr_config *cfg) {
    return cfg->games_view == GR_VIEW_GRID ? (ui_layout){ 5, 2 } : (ui_layout){ 1, 5 };
}

ui_layout ui_detail_layout(const gr_config *cfg) {
    return cfg->ach_view == GR_VIEW_GRID ? (ui_layout){ 3, 3 } : (ui_layout){ 1, 6 };
}

/* ---------- piezas comunes ---------- */

/* cabecera: caratula del juego si la hay, si no la mascota */
static void header(gfx_canvas *c, const ui_assets *a, const uint8_t *cover, const char *title, const char *sub) {
    gfx_rect(c, 0, 0, UI_W, HEADER_H, C_GREEN);
    gfx_rect(c, 0, HEADER_H, UI_W, 5, C_GREEN_DK);
    if (cover) {
        gfx_round_rect(c, 62 - UI_COVER / 2 - 3, 48 - UI_COVER / 2 - 3, UI_COVER + 6, UI_COVER + 6, 13, C_WHITE);
        gfx_image(c, 62 - UI_COVER / 2, 48 - UI_COVER / 2, cover, UI_COVER, UI_COVER);
    } else {
        gfx_circle(c, 62, 48, 41, C_WHITE);
        if (a->mascot_small)
            gfx_image(c, 62 - a->small_w / 2, 48 - a->small_h / 2, a->mascot_small, a->small_w, a->small_h);
    }
    if (sub) {
        gfx_text_fit(c, 122, 12, 38, C_WHITE, title, 640);
        gfx_text_fit(c, 124, 58, 21, C_GREEN_LT, sub, 640);
    } else {
        gfx_text(c, 122, 22, 46, C_WHITE, title);
    }
}

static void header_stats(gfx_canvas *c, int unlocked, int total, int pts, int pts_total) {
    char buf[64];
    snprintf(buf, sizeof(buf), T("%d de %d logros", "%d of %d achievements"), unlocked, total);
    gfx_text_right(c, UI_W - MARGIN, 14, 26, C_WHITE, buf);
    snprintf(buf, sizeof(buf), "%d / %d pts", pts, pts_total);
    gfx_text_right(c, UI_W - MARGIN, 46, 21, C_YELLOW, buf);
    gfx_round_rect(c, UI_W - 300, 76, 260, 10, 5, C_GREEN_DK);
    if (total > 0 && unlocked > 0) gfx_round_rect(c, UI_W - 300, 76, 10 + 250 * unlocked / total, 10, 5, C_YELLOW);
}

/* pista de boton: circulo con la letra + texto. Devuelve la x siguiente. */
static int hint(gfx_canvas *c, int x, const char *btn, const char *label) {
    int cy = FOOTER_Y + 30;
    gfx_circle(c, x + 17, cy, 17, P->ink);
    gfx_text(c, x + 17 - gfx_text_width(22, btn) / 2, cy - 14, 22, P->footer, btn);
    gfx_text(c, x + 44, cy - 14, 22, P->ink, label);
    return x + 44 + gfx_text_width(22, label) + 30;
}

static void footer_bar(gfx_canvas *c) {
    gfx_rect(c, 0, FOOTER_Y, UI_W, UI_H - FOOTER_Y, P->footer);
    gfx_rect(c, 0, FOOTER_Y, UI_W, 2, P->gray_lt);
    gfx_text_right(c, UI_W - MARGIN, FOOTER_Y + 20, 18, P->gray, UI_APP_NAME " " UI_APP_VERSION);
}

static void progress(gfx_canvas *c, int x, int y, int w, int done, int total) {
    gfx_round_rect(c, x, y, w, 14, 7, P->gray_lt);
    if (total > 0 && done > 0) gfx_round_rect(c, x, y, 14 + (w - 14) * done / total, 14, 7, C_GREEN);
}

static void card(gfx_canvas *c, int x, int y, int w, int h, bool selected) {
    if (selected) {
        gfx_round_rect(c, x - 5, y - 5, w + 10, h + 10, 19, C_YELLOW);
        gfx_round_rect(c, x, y, w, h, 14, P->card_sel);
    } else {
        gfx_round_rect(c, x, y + 3, w, h, 14, P->shadow);
        gfx_round_rect(c, x, y, w, h, 14, P->card);
    }
}

/* etiqueta redondeada alineada a la derecha; devuelve su ancho */
static int pill(gfx_canvas *c, int right, int y, gfx_color bg, gfx_color fg, const char *text) {
    int w = gfx_text_width(17, text) + 26;
    gfx_round_rect(c, right - w, y, w, 28, 14, bg);
    gfx_text(c, right - w + 13, y + 4, 17, fg, text);
    return w;
}

static void text_center(gfx_canvas *c, int cx, int y, int size, gfx_color col, const char *s, int max_w) {
    int w = gfx_text_width(size, s);
    if (w > max_w) gfx_text_fit(c, cx - max_w / 2, y, size, col, s, max_w);
    else gfx_text(c, cx - w / 2, y, size, col, s);
}

/* texto en hasta dos lineas, cortando por palabras; la segunda se recorta con "..." */
static void text_2lines(gfx_canvas *c, int x, int y, int size, gfx_color col, const char *s, int max_w, int line_h) {
    if (gfx_text_width(size, s) <= max_w) { gfx_text(c, x, y, size, col, s); return; }
    char first[128];
    size_t best = 0, len = strlen(s);
    for (size_t i = 1; i <= len && i < sizeof(first); i++) {
        if (s[i] != ' ' && s[i] != 0) continue;
        memcpy(first, s, i);
        first[i] = 0;
        if (gfx_text_width(size, first) > max_w) break;
        best = i;
    }
    if (best == 0) { gfx_text_fit(c, x, y, size, col, s, max_w); return; } /* una palabra larguisima */
    memcpy(first, s, best);
    first[best] = 0;
    gfx_text(c, x, y, size, col, first);
    const char *rest = s + best;
    while (*rest == ' ') rest++;
    gfx_text_fit(c, x, y + line_h, size, col, rest, max_w);
}

static void scrollbar(gfx_canvas *c, int y, int h, int top, int visible, int total) {
    if (total <= visible) return;
    int bar_h = h * visible / total;
    int bar_y = y + (h - bar_h) * top / (total - visible);
    gfx_round_rect(c, UI_W - 26, y, 8, h, 4, P->gray_lt);
    gfx_round_rect(c, UI_W - 26, bar_y, 8, bar_h, 4, C_GREEN);
}

static void cover_or_placeholder(gfx_canvas *c, int x, int y, int size, const uint8_t *cover, const char *name) {
    if (cover) { gfx_image(c, x, y, cover, size, size); return; }
    gfx_round_rect(c, x, y, size, size, size / 6, C_GREEN);
    char letter[5] = "?";
    const unsigned char *p = (const unsigned char *)name;
    if (*p) {
        int n = *p < 0x80 ? 1 : (*p & 0xE0) == 0xC0 ? 2 : (*p & 0xF0) == 0xE0 ? 3 : 4, i = 0;
        for (; i < n && p[i]; i++) letter[i] = (char)p[i];
        letter[i] = 0;
    }
    int fs = size / 2;
    gfx_text(c, x + size / 2 - gfx_text_width(fs, letter) / 2, y + size / 5, fs, C_WHITE, letter);
}

/* ---------- inicio: juegos ---------- */

static void home_list(gfx_canvas *c, const ui_assets *a, const gr_model *m, int cursor, int top, int rows) {
    const int x = MARGIN, w = CONTENT_W - 24, h = 92;
    for (int i = top; i < m->ngames && i < top + rows; i++) {
        const gr_game_entry *g = &m->games[i];
        int y = 124 + (i - top) * (h + 13);
        card(c, x, y, w, h, i == cursor);
        cover_or_placeholder(c, x + 10, y + 8, UI_COVER, a->covers[i], g->name);

        int tx = x + 104;
        gfx_text_fit(c, tx, y + 12, 30, P->ink, g->name, 640);
        if (g->broken) {
            gfx_text_fit(c, tx, y + 54, 20, C_RED,
                         T("El archivo de logros tiene un error. Pulsa A para verlo.",
                           "The achievement file has an error. Press A to see it."), 900);
            continue;
        }
        char buf[80];
        if (g->version[0])
            snprintf(buf, sizeof(buf), T("Compatible con la versión %s del juego", "Works with game version %s"), g->version);
        else snprintf(buf, sizeof(buf), "%s", T("Versión compatible sin indicar", "Supported version not specified"));
        gfx_text_fit(c, tx, y + 54, 20, P->gray, buf, 640);

        snprintf(buf, sizeof(buf), "%d / %d", g->unlocked, g->total);
        gfx_text_right(c, x + w - 28, y + 12, 28, g->unlocked == g->total ? P->accent : P->ink, buf);
        progress(c, x + w - 28 - 340, y + 24, 230, g->unlocked, g->total);
        snprintf(buf, sizeof(buf), "%d / %d pts", g->points_got, g->points);
        gfx_text_right(c, x + w - 28, y + 54, 20, P->points, buf);
    }
    scrollbar(c, 124, rows * (h + 13) - 13, top, rows, m->ngames);
}

static void home_grid(gfx_canvas *c, const ui_assets *a, const gr_model *m, int cursor, int top, ui_layout l) {
    const int w = 220, h = 250, gx = 18, gy = 16;
    for (int i = top * l.cols; i < m->ngames && i < (top + l.rows) * l.cols; i++) {
        const gr_game_entry *g = &m->games[i];
        int x = MARGIN + (i % l.cols) * (w + gx), y = 124 + (i / l.cols - top) * (h + gy);
        card(c, x, y, w, h, i == cursor);
        int cx = x + w / 2;
        cover_or_placeholder(c, cx - UI_COVER_BIG / 2, y + 12, UI_COVER_BIG, a->covers_big[i], g->name);
        text_center(c, cx, y + 168, 20, P->ink, g->name, w - 20);

        char buf[64];
        if (g->broken) {
            text_center(c, cx, y + 202, 18, C_RED, T("Archivo con error", "File has an error"), w - 20);
            continue;
        }
        if (g->version[0]) {
            snprintf(buf, sizeof(buf), "v%s", g->version);
            pill(c, cx + UI_COVER_BIG / 2 - 6, y + 12 + UI_COVER_BIG - 34, C_ON_YELLOW, C_WHITE, buf);
        }
        progress(c, x + 20, y + 200, w - 40, g->unlocked, g->total);
        snprintf(buf, sizeof(buf), "%d / %d  ·  %d pts", g->unlocked, g->total, g->points_got);
        text_center(c, cx, y + 220, 17, P->gray, buf, w - 20);
    }
    scrollbar(c, 124, l.rows * (h + gy) - gy, top, l.rows, (m->ngames + l.cols - 1) / l.cols);
}

void ui_home(gfx_canvas *c, const ui_assets *a, const gr_model *m, const gr_config *cfg, int cursor, int top) {
    gfx_clear(c, P->bg);
    header(c, a, NULL, UI_APP_NAME, NULL);
    gfx_text(c, 122 + gfx_text_width(46, UI_APP_NAME) + 14, 44, 22, C_GREEN_LT,
             T("Logros para tu Switch", "Achievements for your Switch"));
    header_stats(c, m->unlocked, m->total, m->points_got, m->points);

    if (m->ngames == 0) {
        card(c, MARGIN, 150, 920, 200, false);
        gfx_text(c, MARGIN + 32, 176, 30, P->ink, T("Todavía no hay juegos con logros", "No games with achievements yet"));
        gfx_text(c, MARGIN + 32, 228, 22, P->gray,
                 T("Copia un archivo de logros por juego en la tarjeta SD:", "Copy one achievement file per game to the SD card:"));
        gfx_text(c, MARGIN + 32, 262, 22, P->accent, "/config/greeny/games/<TITLE ID>.txt");
        gfx_text(c, MARGIN + 32, 300, 20, P->gray, T("Luego pulsa Y para recargar.", "Then press Y to reload."));
    } else {
        ui_layout l = ui_home_layout(cfg);
        if (l.cols > 1) home_grid(c, a, m, cursor, top, l);
        else home_list(c, a, m, cursor, top, l.rows);
    }

    footer_bar(c);
    int hx = MARGIN;
    if (m->ngames > 0) hx = hint(c, hx, "A", T("Ver logros", "View achievements"));
    hx = hint(c, hx, "X", T("Opciones", "Options"));
    hx = hint(c, hx, "Y", T("Recargar", "Reload"));
    hint(c, hx, "+", T("Salir de la app", "Exit app"));
}

/* ---------- detalle: logros de un juego ---------- */

static void icon_unlocked(gfx_canvas *c, int cx, int cy) {
    gfx_circle(c, cx, cy, 27, C_GREEN);
    gfx_line(c, cx - 12, cy + 1, cx - 4, cy + 9, 6, C_WHITE);
    gfx_line(c, cx - 4, cy + 9, cx + 12, cy - 8, 6, C_WHITE);
}

static void icon_locked(gfx_canvas *c, int cx, int cy) {
    gfx_circle(c, cx, cy, 27, P->gray_lt);
    gfx_circle(c, cx, cy - 6, 9, P->gray);         /* aro del candado */
    gfx_circle(c, cx, cy - 6, 5, P->gray_lt);
    gfx_round_rect(c, cx - 12, cy - 4, 24, 18, 4, P->gray);
}

static void unlock_date(const lg_ach *ac, char *buf, size_t cap) {
    buf[0] = 0;
    if (!ac->unlocked_at) return;
    time_t t = (time_t)ac->unlocked_at;
    struct tm tm;
    if (localtime_r(&t, &tm)) strftime(buf, cap, "%d/%m/%Y  %H:%M", &tm);
}

/* etiquetas de estado y puntos, alineadas a la derecha */
static void status_pills(gfx_canvas *c, int right, int y, const lg_ach *ac) {
    char buf[24];
    int w = ac->unlocked ? pill(c, right, y, C_GREEN, C_WHITE, T("DESBLOQUEADO", "UNLOCKED"))
                         : pill(c, right, y, C_RED, C_WHITE, T("BLOQUEADO", "LOCKED"));
    snprintf(buf, sizeof(buf), "%u pts", ac->points);
    pill(c, right - w - 10, y, C_YELLOW, C_ON_YELLOW, buf);
}

static void detail_list(gfx_canvas *c, const lg_game *gm, int cursor, int top, int rows) {
    const int x = MARGIN, w = CONTENT_W - 24, h = 82;
    for (int i = top; i < gm->nach && i < top + rows; i++) {
        const lg_ach *ac = &gm->ach[i];
        int y = 116 + (i - top) * (h + 8);
        card(c, x, y, w, h, i == cursor);
        if (ac->unlocked) icon_unlocked(c, x + 48, y + h / 2);
        else icon_locked(c, x + 48, y + h / 2);

        int tx = x + 96, tw = w - 96 - 300;
        const char *desc = lg_ach_desc(ac, g_en);
        gfx_text_fit(c, tx, y + 11, 26, ac->unlocked ? P->ink : P->gray, lg_ach_name(ac, g_en), tw);
        gfx_text_fit(c, tx, y + 46, 20, P->gray, desc[0] ? desc : T("Sin descripción", "No description"), tw);

        int right = x + w - 24;
        status_pills(c, right, y + 12, ac);
        char buf[48];
        if (ac->unlocked) unlock_date(ac, buf, sizeof(buf));
        else snprintf(buf, sizeof(buf), "%s", T("Aún no lo tienes", "Not earned yet"));
        gfx_text_right(c, right, y + 48, 18, P->gray, buf);
    }
    scrollbar(c, 116, rows * (h + 8) - 8, top, rows, gm->nach);
}

static void detail_grid(gfx_canvas *c, const lg_game *gm, int cursor, int top, ui_layout l) {
    const int w = 380, h = 168, gx = 18, gy = 10;
    for (int i = top * l.cols; i < gm->nach && i < (top + l.rows) * l.cols; i++) {
        const lg_ach *ac = &gm->ach[i];
        int x = MARGIN + (i % l.cols) * (w + gx), y = 116 + (i / l.cols - top) * (h + gy);
        card(c, x, y, w, h, i == cursor);
        if (ac->unlocked) icon_unlocked(c, x + 42, y + 42);
        else icon_locked(c, x + 42, y + 42);

        const char *desc = lg_ach_desc(ac, g_en);
        gfx_text_fit(c, x + 82, y + 14, 23, ac->unlocked ? P->ink : P->gray, lg_ach_name(ac, g_en), w - 82 - 14);
        status_pills(c, x + w - 14, y + 46, ac);
        text_2lines(c, x + 18, y + 84, 18, P->gray, desc[0] ? desc : T("Sin descripción", "No description"), w - 36, 23);

        char buf[48];
        if (ac->unlocked) unlock_date(ac, buf, sizeof(buf));
        else snprintf(buf, sizeof(buf), "%s", T("Aún no lo tienes", "Not earned yet"));
        gfx_text(c, x + 18, y + 138, 16, P->gray, buf);
    }
    scrollbar(c, 116, l.rows * (h + gy) - gy, top, l.rows, (gm->nach + l.cols - 1) / l.cols);
}

void ui_detail(gfx_canvas *c, const ui_assets *a, const gr_model *m, const gr_config *cfg, int game, int cursor, int top) {
    const gr_game_entry *g = &m->games[game];
    gfx_clear(c, P->bg);

    char sub[96];
    if (g->broken) snprintf(sub, sizeof(sub), "%s", T("Archivo de logros con error", "Achievement file has an error"));
    else if (g->version[0])
        snprintf(sub, sizeof(sub), T("Logros compatibles con la versión %s del juego", "Achievements for game version %s"),
                 g->version);
    else snprintf(sub, sizeof(sub), "%s", T("Versión compatible sin indicar", "Supported version not specified"));
    header(c, a, a->covers[game], g->name, sub);

    if (g->broken) {
        card(c, MARGIN, 150, CONTENT_W, 170, false);
        gfx_text(c, MARGIN + 32, 172, 28, C_RED,
                 T("El archivo de logros de este juego tiene un error", "This game's achievement file has an error"));
        gfx_text_fit(c, MARGIN + 32, 222, 22, P->ink, g->err, CONTENT_W - 64);
        gfx_text(c, MARGIN + 32, 264, 20, P->gray,
                 T("Corrígelo en la tarjeta SD y pulsa Y en la pantalla de inicio.",
                   "Fix it on the SD card and press Y on the home screen."));
    } else {
        header_stats(c, g->unlocked, g->total, g->points_got, g->points);
        ui_layout l = ui_detail_layout(cfg);
        if (l.cols > 1) detail_grid(c, &m->cur, cursor, top, l);
        else detail_list(c, &m->cur, cursor, top, l.rows);
    }

    footer_bar(c);
    int hx = hint(c, MARGIN, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir de la app", "Exit app"));
}

/* ---------- opciones ---------- */

static void option_row(gfx_canvas *c, int row, bool selected, const char *label, const char *help, const char *value,
                       gfx_color value_bg) {
    const int x = MARGIN, w = CONTENT_W, h = 64, y = 118 + row * (h + 12);
    card(c, x, y, w, h, selected);
    gfx_text(c, x + 24, y + 7, 24, P->ink, label);
    gfx_text_fit(c, x + 24, y + 38, 17, P->gray, help, w - 48 - 300);
    int vw = gfx_text_width(20, value) + 40;
    gfx_round_rect(c, x + w - 24 - vw, y + 13, vw, 38, 19, value_bg);
    gfx_text(c, x + w - 24 - vw + 20, y + 20, 20, C_WHITE, value);
}

void ui_settings(gfx_canvas *c, const ui_assets *a, const gr_config *cfg, int cursor, bool system_spanish,
                 bool system_dark) {
    gfx_clear(c, P->bg);
    header(c, a, NULL, T("Opciones", "Options"), T("Se guardan en la tarjeta SD", "Saved to the SD card"));

    char lang[48], theme[48];
    if (cfg->lang == GR_LANG_ES) snprintf(lang, sizeof(lang), "Español");
    else if (cfg->lang == GR_LANG_EN) snprintf(lang, sizeof(lang), "English");
    else snprintf(lang, sizeof(lang), "Auto (%s)", system_spanish ? "Español" : "English");

    if (cfg->theme == GR_THEME_LIGHT) snprintf(theme, sizeof(theme), "%s", T("Claro", "Light"));
    else if (cfg->theme == GR_THEME_DARK) snprintf(theme, sizeof(theme), "%s", T("Oscuro", "Dark"));
    else snprintf(theme, sizeof(theme), "Auto (%s)", system_dark ? T("Oscuro", "Dark") : T("Claro", "Light"));

    const char *on = T("Activado", "On"), *off = T("Desactivado", "Off");
    const char *list = T("Lista", "List"), *grid = T("Cuadrícula", "Grid");

    option_row(c, UI_OPT_LANG, cursor == UI_OPT_LANG, T("Idioma", "Language"),
               T("Idioma de la app y de los avisos de logros.", "Language for the app and achievement pop-ups."), lang, C_GREEN);
    option_row(c, UI_OPT_THEME, cursor == UI_OPT_THEME, T("Tema", "Theme"),
               T("Auto usa el tema claro u oscuro de la consola.", "Auto follows the console's light or dark theme."),
               theme, C_GREEN);
    option_row(c, UI_OPT_GAMES_VIEW, cursor == UI_OPT_GAMES_VIEW, T("Vista de juegos", "Games view"),
               T("Cómo se muestran los juegos en el inicio.", "How games are shown on the home screen."),
               cfg->games_view == GR_VIEW_GRID ? grid : list, C_GREEN);
    option_row(c, UI_OPT_ACH_VIEW, cursor == UI_OPT_ACH_VIEW, T("Vista de logros", "Achievements view"),
               T("Cómo se muestran los logros de un juego.", "How a game's achievements are shown."),
               cfg->ach_view == GR_VIEW_GRID ? grid : list, C_GREEN);
    option_row(c, UI_OPT_SOUND, cursor == UI_OPT_SOUND, T("Sonidos en el juego", "In-game sounds"),
               T("Al cargar los logros y al desbloquear uno. Se aplica al abrir un juego.",
                 "When achievements load and when one unlocks. Applies when a game starts."),
               cfg->sound ? on : off, cfg->sound ? C_GREEN : C_NEUTRAL);
    option_row(c, UI_OPT_POPUP, cursor == UI_OPT_POPUP, T("Avisos en el juego", "In-game pop-ups"),
               T("Mensajes en pantalla mientras juegas. Se aplica al abrir un juego.",
                 "On-screen messages while you play. Applies when a game starts."),
               cfg->popup ? on : off, cfg->popup ? C_GREEN : C_NEUTRAL);
    option_row(c, UI_OPT_DIAG, cursor == UI_OPT_DIAG, T("Diagnóstico", "Diagnostics"),
               T("Revisa que Greeny esté bien instalado y funcionando.", "Checks that Greeny is installed and working."),
               T("Abrir", "Open"), C_NEUTRAL);

    footer_bar(c);
    int hx = hint(c, MARGIN, "A", cursor == UI_OPT_DIAG ? T("Abrir", "Open") : T("Cambiar", "Change"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir de la app", "Exit app"));
}

/* ---------- diagnostico ---------- */

static void diag_icon(gfx_canvas *c, int cx, int cy, gr_diag_state st) {
    if (st == GR_DIAG_OK) {
        gfx_circle(c, cx, cy, 15, C_GREEN);
        gfx_line(c, cx - 7, cy + 1, cx - 2, cy + 6, 4, C_WHITE);
        gfx_line(c, cx - 2, cy + 6, cx + 7, cy - 5, 4, C_WHITE);
    } else if (st == GR_DIAG_FAIL) {
        gfx_circle(c, cx, cy, 15, C_RED);
        gfx_line(c, cx - 6, cy - 6, cx + 6, cy + 6, 4, C_WHITE);
        gfx_line(c, cx + 6, cy - 6, cx - 6, cy + 6, 4, C_WHITE);
    } else {
        bool warn = st == GR_DIAG_WARN;
        gfx_circle(c, cx, cy, 15, warn ? C_YELLOW : C_NEUTRAL);
        const char *ch = warn ? "!" : "i";
        gfx_text(c, cx - gfx_text_width(20, ch) / 2, cy - 13, 20, warn ? C_ON_YELLOW : C_WHITE, ch);
    }
}

void ui_diag(gfx_canvas *c, const ui_assets *a, const gr_diag_item *items, int count) {
    gfx_clear(c, P->bg);

    int fails = 0, warns = 0;
    for (int i = 0; i < count; i++) {
        fails += items[i].state == GR_DIAG_FAIL;
        warns += items[i].state == GR_DIAG_WARN;
    }
    char sub[96];
    if (fails) snprintf(sub, sizeof(sub), T("%d problema(s) que hay que arreglar", "%d problem(s) to fix"), fails);
    else if (warns) snprintf(sub, sizeof(sub), T("Funciona, con %d aviso(s)", "Working, with %d warning(s)"), warns);
    else snprintf(sub, sizeof(sub), "%s", T("Todo en orden", "Everything looks good"));
    header(c, a, NULL, T("Diagnóstico", "Diagnostics"), sub);

    const int x = MARGIN, w = CONTENT_W, h = 44;
    for (int i = 0; i < count && i < GR_DIAG_MAX; i++) {
        const gr_diag_item *it = &items[i];
        int y = 112 + i * (h + 5);
        gfx_round_rect(c, x, y, w, h, 12, P->card);
        diag_icon(c, x + 28, y + h / 2, it->state);
        gfx_text_fit(c, x + 58, y + 9, 21, P->ink, g_en ? it->title_en : it->title_es, 330);
        gfx_text_fit(c, x + 400, y + 11, 18, it->state == GR_DIAG_FAIL ? C_RED : P->gray,
                     g_en ? it->detail_en : it->detail_es, w - 400 - 20);
    }

    footer_bar(c);
    int hx = hint(c, MARGIN, "Y", T("Revisar de nuevo", "Check again"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir de la app", "Exit app"));
}
