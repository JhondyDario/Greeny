#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ---------- paleta: verde, blanco, rojo y amarillo, en claro y oscuro ---------- */

typedef struct {
    gfx_color bg, card, card_sel, ink, gray, gray_lt, shadow, footer, accent, points, bubble;
} palette;

static const palette k_light = {
    .bg = GFX_RGB(0xF3, 0xF7, 0xEC), .card = GFX_RGB(0xFF, 0xFF, 0xFF), .card_sel = GFX_RGB(0xE3, 0xF4, 0xDF),
    .ink = GFX_RGB(0x1E, 0x2A, 0x1F), .gray = GFX_RGB(0x6B, 0x7A, 0x6C), .gray_lt = GFX_RGB(0xD9, 0xE2, 0xD4),
    .shadow = GFX_RGB(0xD3, 0xDD, 0xCB), .footer = GFX_RGB(0xFF, 0xFF, 0xFF), .accent = GFX_RGB(0x1D, 0x7A, 0x2A),
    .points = GFX_RGB(0x8A, 0x65, 0x00), .bubble = GFX_RGB(0x2F, 0xA6, 0x3B),
};
static const palette k_dark = {
    .bg = GFX_RGB(0x12, 0x1A, 0x14), .card = GFX_RGB(0x1F, 0x2B, 0x22), .card_sel = GFX_RGB(0x2A, 0x4A, 0x30),
    .ink = GFX_RGB(0xEE, 0xF4, 0xEA), .gray = GFX_RGB(0xA3, 0xB1, 0xA4), .gray_lt = GFX_RGB(0x3A, 0x4A, 0x3E),
    .shadow = GFX_RGB(0x0A, 0x0F, 0x0B), .footer = GFX_RGB(0x18, 0x22, 0x1B), .accent = GFX_RGB(0x6F, 0xD6, 0x7B),
    .points = GFX_RGB(0xF7, 0xC9, 0x1E), .bubble = GFX_RGB(0x3F, 0xC0, 0x4E),
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

static char g_user[40];
void ui_set_user(const char *name) { snprintf(g_user, sizeof(g_user), "%s", name ? name : ""); }

ui_layout ui_home_layout(const gr_config *cfg) {
    return cfg->games_view == GR_VIEW_GRID ? (ui_layout){ 5, 2 } : (ui_layout){ 1, 5 };
}

ui_layout ui_detail_layout(const gr_config *cfg) {
    return cfg->ach_view == GR_VIEW_GRID ? (ui_layout){ 3, 3 } : (ui_layout){ 1, 6 };
}

/* ---------- fondo ----------
 * Las pantallas se dibujan en una capa transparente y el fondo va debajo, asi
 * se puede animar sin tener que redibujar el texto en cada cuadro. */

void ui_background(gfx_canvas *c, float t) {
    gfx_clear(c, P->bg);
    /* burbujas grandes y muy tenues que derivan despacio; t en segundos (0 = quieto) */
    static const struct { float x, y, r, vx, vy, phase; } k[] = {
        { 160, 250, 150,  7,  3, 0.0f }, { 520, 560, 110, -5,  4, 1.7f }, { 900, 300, 190,  4, -3, 3.1f },
        { 1160, 560, 130, -6, -2, 4.4f }, { 330, 640,  90,  5, -4, 0.9f }, { 720, 180, 100, -4,  5, 2.6f },
        { 1080, 180,  70,  6,  3, 5.2f },
    };
    const float span_x = UI_W + 400, span_y = UI_H + 400;
    for (unsigned i = 0; i < sizeof(k) / sizeof(k[0]); i++) {
        float x = k[i].x + k[i].vx * t + 40.0f * sinf(t * 0.21f + k[i].phase);
        float y = k[i].y + k[i].vy * t + 30.0f * cosf(t * 0.17f + k[i].phase);
        /* dar la vuelta por el borde contrario, con margen para que no aparezcan de golpe */
        x = fmodf(fmodf(x + 200, span_x) + span_x, span_x) - 200;
        y = fmodf(fmodf(y + 200, span_y) + span_y, span_y) - 200;
        gfx_circle_alpha(c, (int)x, (int)y, (int)k[i].r, P->bubble, 26);
    }
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
    gfx_clear_transparent(c);
    header(c, a, NULL, UI_APP_NAME, NULL);
    char tagline[96];
    if (g_user[0]) snprintf(tagline, sizeof(tagline), T("¡Bienvenido de vuelta, %s!", "Welcome back, %s!"), g_user);
    else snprintf(tagline, sizeof(tagline), "%s", T("Logros para tu Switch", "Achievements for your Switch"));
    gfx_text_fit(c, 122 + gfx_text_width(46, UI_APP_NAME) + 14, 44, 22, C_GREEN_LT, tagline, 520);
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
    if (m->ngames > 0) hx = hint(c, hx, "−", cfg->games_view == GR_VIEW_GRID ? T("Lista", "List") : T("Cuadrícula", "Grid"));
    hint(c, hx, "+", T("Salir", "Exit"));
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
    gfx_clear_transparent(c);

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
    if (!g->broken) hx = hint(c, hx, "−", cfg->ach_view == GR_VIEW_GRID ? T("Lista", "List") : T("Cuadrícula", "Grid"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- opciones ---------- */

static const struct { const char *es, *en; int n; int opt[3]; } k_tabs[UI_TABS] = {
    { "General",     "General",  3, { UI_OPT_LANG, UI_OPT_THEME, UI_OPT_ANIM } },
    { "Vista",       "View",     2, { UI_OPT_GAMES_VIEW, UI_OPT_ACH_VIEW } },
    { "En el juego", "In-game",  2, { UI_OPT_SOUND, UI_OPT_POPUP } },
    { "Ayuda",       "Help",     2, { UI_OPT_DIAG, UI_OPT_ABOUT } },
};

int ui_tab_count(int tab) { return k_tabs[tab].n; }
int ui_tab_option(int tab, int row) { return k_tabs[tab].opt[row]; }

static void option_row(gfx_canvas *c, int y, bool selected, const char *label, const char *help, const char *value,
                       gfx_color value_bg) {
    const int x = MARGIN, w = CONTENT_W, h = 88;
    card(c, x, y, w, h, selected);
    gfx_text(c, x + 28, y + 12, 27, P->ink, label);
    gfx_text_fit(c, x + 28, y + 51, 19, P->gray, help, w - 56 - 300);
    int vw = gfx_text_width(21, value) + 42;
    gfx_round_rect(c, x + w - 28 - vw, y + 23, vw, 42, 21, value_bg);
    gfx_text(c, x + w - 28 - vw + 21, y + 31, 21, C_WHITE, value);
}

/* boton L o R dibujado junto a las pestañas */
static void shoulder(gfx_canvas *c, int x, int y, const char *letter) {
    gfx_round_rect(c, x, y, 46, 34, 10, P->ink);
    gfx_text(c, x + 23 - gfx_text_width(21, letter) / 2, y + 5, 21, P->footer, letter);
}

void ui_settings(gfx_canvas *c, const ui_assets *a, const gr_config *cfg, int tab, int row, bool system_spanish,
                 bool system_dark) {
    gfx_clear_transparent(c);
    header(c, a, NULL, T("Opciones", "Options"), T("Se guardan en la tarjeta SD", "Saved to the SD card"));

    /* pestañas */
    int tx = MARGIN + 60;
    shoulder(c, MARGIN, 121, "L");
    for (int i = 0; i < UI_TABS; i++) {
        const char *name = T(k_tabs[i].es, k_tabs[i].en);
        int w = gfx_text_width(23, name) + 44;
        if (i == tab) gfx_round_rect(c, tx, 116, w, 44, 22, C_GREEN);
        else gfx_round_rect(c, tx, 116, w, 44, 22, P->card);
        gfx_text(c, tx + 22, 124, 23, i == tab ? C_WHITE : P->gray, name);
        tx += w + 10;
    }
    shoulder(c, tx + 4, 121, "R");

    char lang[48], theme[48];
    if (cfg->lang == GR_LANG_ES) snprintf(lang, sizeof(lang), "Español");
    else if (cfg->lang == GR_LANG_EN) snprintf(lang, sizeof(lang), "English");
    else snprintf(lang, sizeof(lang), "Auto (%s)", system_spanish ? "Español" : "English");

    if (cfg->theme == GR_THEME_LIGHT) snprintf(theme, sizeof(theme), "%s", T("Claro", "Light"));
    else if (cfg->theme == GR_THEME_DARK) snprintf(theme, sizeof(theme), "%s", T("Oscuro", "Dark"));
    else snprintf(theme, sizeof(theme), "Auto (%s)", system_dark ? T("Oscuro", "Dark") : T("Claro", "Light"));

    const char *on = T("Activado", "On"), *off = T("Desactivado", "Off");
    const char *list = T("Lista", "List"), *grid = T("Cuadrícula", "Grid"), *open = T("Abrir", "Open");

    for (int i = 0; i < k_tabs[tab].n; i++) {
        int y = 184 + i * 104;
        bool sel = i == row;
        switch (k_tabs[tab].opt[i]) {
            case UI_OPT_LANG:
                option_row(c, y, sel, T("Idioma", "Language"),
                           T("Idioma de la app y de los avisos de logros.", "Language for the app and achievement pop-ups."), lang, C_GREEN);
                break;
            case UI_OPT_THEME:
                option_row(c, y, sel, T("Tema", "Theme"),
                           T("Auto usa el tema claro u oscuro de la consola.", "Auto follows the console's light or dark theme."), theme, C_GREEN);
                break;
            case UI_OPT_ANIM:
                option_row(c, y, sel, T("Fondo animado", "Animated background"),
                           T("Burbujas que se mueven despacio detrás de las pantallas.", "Slow-moving bubbles behind the screens."),
                           cfg->animated_bg ? on : off, cfg->animated_bg ? C_GREEN : C_NEUTRAL);
                break;
            case UI_OPT_GAMES_VIEW:
                option_row(c, y, sel, T("Vista de juegos", "Games view"),
                           T("Cómo se muestran los juegos. También se cambia con − en el inicio.", "How games are shown. Also toggled with − on the home screen."),
                           cfg->games_view == GR_VIEW_GRID ? grid : list, C_GREEN);
                break;
            case UI_OPT_ACH_VIEW:
                option_row(c, y, sel, T("Vista de logros", "Achievements view"),
                           T("Cómo se muestran los logros. También se cambia con − dentro de un juego.", "How achievements are shown. Also toggled with − inside a game."),
                           cfg->ach_view == GR_VIEW_GRID ? grid : list, C_GREEN);
                break;
            case UI_OPT_SOUND:
                option_row(c, y, sel, T("Sonidos en el juego", "In-game sounds"),
                           T("Al cargar los logros y al desbloquear uno. Se aplica al abrir un juego.",
                             "When achievements load and when one unlocks. Applies when a game starts."),
                           cfg->sound ? on : off, cfg->sound ? C_GREEN : C_NEUTRAL);
                break;
            case UI_OPT_POPUP:
                option_row(c, y, sel, T("Avisos en el juego", "In-game pop-ups"),
                           T("Mensajes en pantalla mientras juegas. Se aplica al abrir un juego.",
                             "On-screen messages while you play. Applies when a game starts."),
                           cfg->popup ? on : off, cfg->popup ? C_GREEN : C_NEUTRAL);
                break;
            case UI_OPT_DIAG:
                option_row(c, y, sel, T("Diagnóstico", "Diagnostics"),
                           T("Revisa que Greeny esté bien instalado y funcionando.", "Checks that Greeny is installed and working."), open, C_NEUTRAL);
                break;
            default:
                option_row(c, y, sel, T("Acerca de", "About"),
                           T("Créditos, versión y dónde contactar.", "Credits, version and where to get in touch."), open, C_NEUTRAL);
                break;
        }
    }

    footer_bar(c);
    int hx = hint(c, MARGIN, "A", k_tabs[tab].opt[row] >= UI_OPT_DIAG ? T("Abrir", "Open") : T("Cambiar", "Change"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
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

static void diag_row(gfx_canvas *c, int y, const gr_diag_item *it) {
    const int x = MARGIN, w = CONTENT_W, h = 44;
    gfx_round_rect(c, x, y, w, h, 12, P->card);
    diag_icon(c, x + 28, y + h / 2, it->state);
    gfx_text_fit(c, x + 58, y + 9, 21, P->ink, g_en ? it->title_en : it->title_es, 330);
    gfx_text_fit(c, x + 400, y + 11, 18, it->state == GR_DIAG_FAIL ? C_RED : P->gray,
                 g_en ? it->detail_en : it->detail_es, w - 400 - 20);
}

void ui_diag(gfx_canvas *c, const ui_assets *a, const gr_diag_item *items, int count) {
    gfx_clear_transparent(c);

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

    for (int i = 0; i < count && i < GR_DIAG_MAX; i++) diag_row(c, 112 + i * 49, &items[i]);

    footer_bar(c);
    int hx = hint(c, MARGIN, "Y", T("Revisar de nuevo", "Check again"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- acerca de ---------- */

void ui_about(gfx_canvas *c, const ui_assets *a) {
    gfx_clear_transparent(c);
    header(c, a, NULL, T("Acerca de", "About"), UI_APP_NAME " " UI_APP_VERSION);

    const int x = MARGIN, w = 880;
    card(c, x, 126, w, 500, false);
    int tx = x + 36, y = 150;
    gfx_text(c, tx, y, 40, P->ink, UI_APP_NAME);
    gfx_text(c, tx + gfx_text_width(40, UI_APP_NAME) + 16, y + 16, 22, P->gray, T("versión " UI_APP_VERSION, "version " UI_APP_VERSION));
    gfx_text(c, tx, y + 60, 22, P->gray,
             T("El primer sistema de logros no oficial para la Switch.", "The first unofficial achievement system for the Switch."));

    gfx_text(c, tx, y + 118, 19, P->gray, T("CREADO POR", "CREATED BY"));
    gfx_text(c, tx, y + 142, 30, P->ink, "Jhondy");

    gfx_text(c, tx, y + 204, 19, P->gray, T("CÓDIGO, AYUDA Y CONTACTO", "SOURCE, HELP AND CONTACT"));
    gfx_text(c, tx, y + 228, 26, P->accent, "github.com/JhondyDario/Greeny");
    gfx_text(c, tx, y + 264, 18, P->gray,
             T("Reporta errores o propone logros abriendo un \"issue\" ahí.", "Report bugs or suggest achievements by opening an issue there."));

    gfx_text(c, tx, y + 322, 19, P->gray, T("LICENCIA Y CRÉDITOS", "LICENSE AND CREDITS"));
    gfx_text_fit(c, tx, y + 346, 19, P->ink,
                 T("Software libre, licencia GPLv2. Usa código de Atmosphère, libtesla, FreeType y stb.",
                   "Free software, GPLv2 license. Uses code from Atmosphère, libtesla, FreeType and stb."), w - 72);
    gfx_text_fit(c, tx, y + 400, 18, P->gray,
                 T("Proyecto de fans. No está afiliado ni avalado por Nintendo.",
                   "Fan project. Not affiliated with or endorsed by Nintendo."), w - 72);

    if (a->mascot_big) gfx_image(c, x + w + (UI_W - MARGIN - x - w - a->big_w) / 2 + 10, 210, a->mascot_big, a->big_w, a->big_h);

    footer_bar(c);
    int hx = hint(c, MARGIN, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- introduccion ---------- */

static void intro_dots(gfx_canvas *c, int page) {
    for (int i = 0; i < UI_INTRO_PAGES; i++)
        gfx_circle(c, UI_W / 2 + (i - 1) * 34, 632, i == page ? 9 : 6, i == page ? C_GREEN : P->gray_lt);
}

void ui_intro(gfx_canvas *c, const ui_assets *a, int page, const gr_diag_item *items, int count,
              bool sysmodule_running) {
    gfx_clear_transparent(c);
    char sub[48];
    snprintf(sub, sizeof(sub), T("Paso %d de %d", "Step %d of %d"), page + 1, UI_INTRO_PAGES);
    header(c, a, NULL, T("Primeros pasos", "Getting started"), sub);

    int fails = 0;
    for (int i = 0; i < count; i++) fails += items[i].state == GR_DIAG_FAIL;

    if (page == 0) {
        if (a->mascot_big) gfx_image(c, 110, 180, a->mascot_big, a->big_w, a->big_h);
        const int tx = 400;
        gfx_text(c, tx, 190, 46, P->ink, T("¡Bienvenido a Greeny!", "Welcome to Greeny!"));
        gfx_text(c, tx, 262, 25, P->gray, T("Logros para tus juegos de Switch.", "Achievements for your Switch games."));
        gfx_text(c, tx, 326, 22, P->ink, T("Greeny corre en segundo plano mientras juegas.", "Greeny runs in the background while you play."));
        gfx_text(c, tx, 360, 22, P->ink,
                 T("Cuando cumples un logro, te avisa en pantalla y lo guarda.", "When you earn an achievement, it pops up and gets saved."));
        gfx_text(c, tx, 394, 22, P->ink, T("En esta app ves tu progreso en cada juego.", "This app shows your progress in every game."));
        gfx_text(c, tx, 458, 22, P->gray,
                 T("Primero vamos a revisar que todo esté bien instalado.", "First, let's check that everything is installed correctly."));
    } else if (page == 1) {
        gfx_text(c, MARGIN, 122, 30, P->ink, T("Revisando la instalación", "Checking the installation"));
        int shown = count < 6 ? count : 6; /* las primeras son las de instalacion; el resto esta en Diagnostico */
        for (int i = 0; i < shown; i++) diag_row(c, 180 + i * 52, &items[i]);
        gfx_text(c, MARGIN, 180 + shown * 52 + 14, 20, P->gray,
                 fails ? T("Lo que está en rojo hay que arreglarlo para que los logros funcionen.",
                           "Items in red must be fixed for achievements to work.")
                       : T("Puedes repetir esta revisión cuando quieras en Opciones → Diagnóstico.",
                           "You can run this check again any time in Options → Diagnostics."));
    } else {
        const int x = 190, w = 900;
        card(c, x, 170, w, 380, false);
        int tx = x + 44;
        if (sysmodule_running && !fails) {
            gfx_circle(c, tx + 34, 240, 34, C_GREEN);
            gfx_line(c, tx + 18, 242, tx + 29, 254, 8, C_WHITE);
            gfx_line(c, tx + 29, 254, tx + 51, 228, 8, C_WHITE);
            gfx_text(c, tx + 92, 212, 40, P->ink, T("¡Todo listo!", "All set!"));
            gfx_text(c, tx, 310, 23, P->ink, T("Greeny ya está funcionando en esta consola.", "Greeny is already running on this console."));
            gfx_text(c, tx, 348, 23, P->ink,
                     T("Abre un juego que tenga logros y verás un aviso al empezar.", "Start a game that has achievements and a pop-up will appear."));
            gfx_text(c, tx, 420, 20, P->gray,
                     T("Si no sale el aviso, vuelve aquí y abre Opciones → Diagnóstico.", "If no pop-up appears, come back and open Options → Diagnostics."));
        } else {
            gfx_circle(c, tx + 34, 240, 34, C_YELLOW);
            gfx_text(c, tx + 34 - gfx_text_width(44, "!") / 2, 212, 44, C_ON_YELLOW, "!");
            gfx_text(c, tx + 92, 212, 40, P->ink, T("Reinicia la consola", "Restart the console"));
            gfx_text(c, tx, 300, 23, P->ink,
                     T("Los logros empiezan a funcionar después de reiniciar.", "Achievements start working after a restart."));
            gfx_text(c, tx, 346, 21, P->ink,
                     T("Mantén pulsado POWER, entra a Opciones de apagado y elige Reiniciar.", "Hold POWER, open Power Options and choose Restart."));
            gfx_text(c, tx, 380, 21, P->ink,
                     T("Suspender la consola no sirve: tiene que apagar y encender.", "Sleep mode is not enough: it has to power off and on."));
            gfx_text_fit(c, tx, 446, 20, fails ? C_RED : P->gray,
                         fails ? T("Antes, corrige lo que salió en rojo en el paso anterior.", "First, fix what showed up in red on the previous step.")
                               : T("Después, abre un juego con logros y verás un aviso al empezar.", "Then start a game with achievements and a pop-up will appear."),
                         w - 88);
        }
    }

    intro_dots(c, page);
    footer_bar(c);
    int hx = hint(c, MARGIN, "A", page == UI_INTRO_PAGES - 1 ? T("Empezar", "Get started") : T("Siguiente", "Next"));
    if (page > 0) hx = hint(c, hx, "B", T("Atrás", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}
