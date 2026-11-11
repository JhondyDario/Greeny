#include "ui.h"

#include "account.h"

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

static const uint8_t *g_buttons; /* iconos de botones de la pantalla que se esta dibujando */
static const ui_assets *g_a;     /* recursos de la pantalla que se esta dibujando */
static bool g_ingame = true;
void ui_set_ingame(bool on) { g_ingame = on; }

static const ui_song *g_songs;
static int g_nsongs, g_song_busy = -1;
void ui_set_music(const ui_song *songs, int count, int busy) { g_songs = songs; g_nsongs = count; g_song_busy = busy; }
static int g_row_index; /* fila que se esta dibujando en Opciones, para su zona tactil */

static const struct { short w, h; } k_img[UI_IMG_COUNT] = {
#define UI_IMG(name, w, h) { w, h },
#include "ui_images.h"
#undef UI_IMG
};

/* Una imagen fija en su tamaño real. false si no esta cargada (quien llama dibuja otra cosa). */
static bool img(gfx_canvas *c, const ui_assets *a, int id, int x, int y) {
    if (!a || id < 0 || id >= UI_IMG_COUNT || !a->img[id]) return false;
    gfx_image(c, x, y, a->img[id], k_img[id].w, k_img[id].h);
    return true;
}

/* La misma, reducida para que quepa en size x size y centrada ahi. */
static bool img_fit(gfx_canvas *c, const ui_assets *a, int id, int x, int y, int size) {
    static uint8_t scratch[260 * 260 * 4];
    if (!a || id < 0 || id >= UI_IMG_COUNT || !a->img[id]) return false;
    int w = k_img[id].w, h = k_img[id].h;
    int big = w > h ? w : h;
    if (big <= size) { gfx_image(c, x + (size - w) / 2, y + (size - h) / 2, a->img[id], w, h); return true; }
    int dw = w * size / big, dh = h * size / big;
    if (dw < 1 || dh < 1) return false;
    gfx_scale_rgba(a->img[id], w, h, scratch, dw, dh);
    gfx_image(c, x + (size - dw) / 2, y + (size - dh) / 2, scratch, dw, dh);
    return true;
}

/* ---------- zonas tactiles ----------
 * Cada pantalla, al dibujarse, anota donde quedaron sus botones y elementos. */
#define MAX_ZONES 96
static ui_zone g_zones[MAX_ZONES];
static int g_nzones;

static void zone(int x, int y, int w, int h, ui_zone_kind kind, int value) {
    if (g_nzones < MAX_ZONES) g_zones[g_nzones++] = (ui_zone){ x, y, w, h, kind, value };
}

const ui_zone *ui_zone_at(int x, int y) {
    for (int i = g_nzones - 1; i >= 0; i--) { /* las ultimas quedan encima */
        const ui_zone *z = &g_zones[i];
        if (x >= z->x && x < z->x + z->w && y >= z->y && y < z->y + z->h) return z;
    }
    return NULL;
}

/* toda pantalla empieza por aqui */
static bool g_chip_opens_profile; /* solo en el inicio, donde R abre el perfil */

static void begin(gfx_canvas *c) {
    gfx_clear_transparent(c);
    g_nzones = 0;
    g_chip_opens_profile = false;
}

static bool g_en;
#define T(es, en) (g_en ? (en) : (es))

void ui_set_english(bool english) { g_en = english; }
void ui_set_dark(bool dark) { P = dark ? &k_dark : &k_light; }

static const uint32_t *g_bg; /* imagen de fondo, o NULL */
void ui_set_background(const uint32_t *px) { g_bg = px; }

static const ui_profile *g_profile; /* para la barra de arriba; NULL = no mostrarla */
void ui_set_profile(const ui_profile *p) { g_profile = p; }

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
    if (g_bg) {
        for (int y = 0; y < c->h && y < UI_H; y++)
            memcpy(&c->px[y * c->stride], &g_bg[y * UI_W], (size_t)(c->w < UI_W ? c->w : UI_W) * sizeof(uint32_t));
    } else {
        gfx_clear(c, P->bg);
    }
    if (t <= 0.0f) return; /* fondo animado apagado: solo la imagen */
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
        gfx_circle_alpha(c, (int)x, (int)y, (int)k[i].r, P->bubble, g_bg ? 16 : 26);
    }
}

/* ---------- piezas comunes ---------- */

static void user_chip(gfx_canvas *c);

/* cabecera: caratula del juego si la hay, si no la mascota */
static void header(gfx_canvas *c, const ui_assets *a, const uint8_t *cover, const char *title, const char *sub) {
    g_buttons = a->buttons;
    g_a = a;
    gfx_gradient(c, 0, 0, UI_W, HEADER_H, GFX_RGB(0x1F, 0x8F, 0x30), GFX_RGB(0x4C, 0xC2, 0x4A));
    gfx_gradient(c, 0, HEADER_H, UI_W, 5, GFX_RGB(0x15, 0x66, 0x22), GFX_RGB(0x2A, 0x93, 0x34));
    /* a la izquierda solo va la caratula cuando la pantalla es de un juego; el logo ya no se repite */
    int tx = MARGIN;
    if (cover) {
        gfx_round_rect(c, 62 - UI_COVER / 2 - 3, 48 - UI_COVER / 2 - 3, UI_COVER + 6, UI_COVER + 6, 13, C_WHITE);
        gfx_image(c, 62 - UI_COVER / 2, 48 - UI_COVER / 2, cover, UI_COVER, UI_COVER);
        tx = 122;
    }
    if (sub) {
        gfx_text_fit(c, tx, 12, 38, C_WHITE, title, 640);
        gfx_text_fit(c, tx + 2, 58, 21, C_GREEN_LT, sub, 760);
    } else {
        gfx_text(c, tx, 22, 46, C_WHITE, title);
    }
    user_chip(c);
}

/* primera letra de un texto UTF-8, para los avatares sin imagen */
static void first_letter(const char *text, char out[5]) {
    const unsigned char *p = (const unsigned char *)text;
    out[0] = '?';
    out[1] = 0;
    if (!*p) return;
    int n = *p < 0x80 ? 1 : (*p & 0xE0) == 0xC0 ? 2 : (*p & 0xF0) == 0xE0 ? 3 : 4, i = 0;
    for (; i < n && p[i]; i++) out[i] = (char)p[i];
    out[i] = 0;
}

static void avatar(gfx_canvas *c, int cx, int cy, int r, const char *name, int pic) {
    /* la foto llena el circulo entero, sin aro blanco alrededor */
    if (pic >= 1 && pic <= UI_AVATAR_COUNT && img_fit(c, g_a, UI_IMG_avatar_01 + pic - 1, cx - r - 3, cy - r - 3, r * 2 + 6)) return;
    char letter[5];
    first_letter(name, letter);
    gfx_circle(c, cx, cy, r + 3, C_WHITE);
    gfx_circle(c, cx, cy, r, C_GREEN_DK);
    int fs = r * 11 / 10;
    gfx_text(c, cx - gfx_text_width(fs, letter) / 2, cy - fs * 6 / 10, fs, C_WHITE, letter);
}

/* arriba a la derecha: icono del usuario, nombre y nivel */
static void user_chip(gfx_canvas *c) {
    if (!g_profile) return;
    const int cx = UI_W - MARGIN - 32, cy = 48;
    if (g_chip_opens_profile) zone(UI_W - 300, 0, 300, HEADER_H, UI_ZONE_BUTTON, UI_BTN_R); /* tocar tu nombre = R */
    avatar(c, cx, cy, 30, g_profile->name, g_profile->avatar);
    int right = cx - 30 - 16;
    gfx_text_right(c, right, 16, 25, C_WHITE, g_profile->name);
    char buf[32];
    snprintf(buf, sizeof(buf), T("Nivel %d", "Level %d"), gr_level(g_profile->stats.points, NULL, NULL));
    int w = gfx_text_width(18, buf) + 24;
    gfx_round_rect(c, right - w, 52, w, 28, 14, C_YELLOW);
    gfx_text(c, right - w + 12, 56, 18, C_ON_YELLOW, buf);
}

/* pista de boton: icono del boton (o circulo con la letra si no hay) + texto. Devuelve la x siguiente. */
static int hint(gfx_canvas *c, int x, const char *btn, const char *label) {
    static const char *const order[UI_BTN_COUNT] = { "A", "B", "X", "Y", "+", "−", "L", "R" };
    int cy = FOOTER_Y + 30, icon = -1;
    for (int i = 0; i < UI_BTN_COUNT; i++)
        if (!strcmp(btn, order[i])) icon = i;

    if (g_buttons && icon >= 0) {
        gfx_image(c, x, cy - UI_BTN_SIZE / 2, g_buttons + (size_t)icon * UI_BTN_SIZE * UI_BTN_SIZE * 4, UI_BTN_SIZE, UI_BTN_SIZE);
    } else {
        gfx_circle(c, x + 17, cy, 17, P->ink);
        gfx_text(c, x + 17 - gfx_text_width(22, btn) / 2, cy - 14, 22, P->footer, btn);
    }
    gfx_text(c, x + 46, cy - 14, 22, P->ink, label);
    int next = x + 46 + gfx_text_width(22, label) + 28;
    if (icon >= 0) zone(x - 8, FOOTER_Y, next - x - 4, UI_H - FOOTER_Y, UI_ZONE_BUTTON, icon);
    /* "+" es siempre la ultima pista: la version va a la derecha solo si queda sitio */
    if (icon == UI_BTN_PLUS && next + gfx_text_width(18, UI_APP_NAME " " UI_APP_VERSION) < UI_W - MARGIN)
        gfx_text_right(c, UI_W - MARGIN, FOOTER_Y + 20, 18, P->gray, UI_APP_NAME " " UI_APP_VERSION);
    return next;
}

static void footer_bar(gfx_canvas *c) {
    gfx_rect(c, 0, FOOTER_Y, UI_W, UI_H - FOOTER_Y, P->footer);
    gfx_rect(c, 0, FOOTER_Y, UI_W, 2, P->gray_lt);
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
    if (img_fit(c, g_a, UI_IMG_cover_fill, x, y, size)) return;
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
        zone(x, y, w, h, UI_ZONE_ITEM, i);
        cover_or_placeholder(c, x + 10, y + 8, UI_COVER, a->covers[i], g->name);
        if (!g->broken && g->total > 0 && g->unlocked == g->total) img_fit(c, a, UI_IMG_trophy, x + 10 + UI_COVER - 30, y + 8 + UI_COVER - 34, 44);

        int tx = x + 104;
        gfx_text_fit(c, tx, y + 12, 30, P->ink, g->name, 640);
        if (g->broken) {
            gfx_text_fit(c, tx, y + 54, 20, C_RED,
                         T("El archivo de logros tiene un error. Pulsa A para verlo.",
                           "The achievement file has an error. Press A to see it."), 900);
            continue;
        }
        char buf[80];
        if (g->last_played) {
            char when[32] = "";
            time_t t = (time_t)g->last_played;
            struct tm tm;
            if (localtime_r(&t, &tm)) strftime(when, sizeof(when), "%d/%m/%Y", &tm);
            snprintf(buf, sizeof(buf), T("Última vez jugado: %s", "Last played: %s"), when);
        } else snprintf(buf, sizeof(buf), "%s", T("Aún no lo has jugado con Greeny", "Not played with Greeny yet"));
        gfx_text_fit(c, tx, y + 54, 20, P->gray, buf, 560);

        snprintf(buf, sizeof(buf), "%d / %d", g->unlocked, g->total);
        gfx_text_right(c, x + w - 28, y + 12, 28, g->unlocked == g->total ? P->accent : P->ink, buf);
        progress(c, x + w - 28 - 340, y + 24, 230, g->unlocked, g->total);
        snprintf(buf, sizeof(buf), "%d / %d pts  ·  %d%%", g->points_got, g->points, g->total > 0 ? g->unlocked * 100 / g->total : 0);
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
        zone(x, y, w, h, UI_ZONE_ITEM, i);
        int cx = x + w / 2;
        cover_or_placeholder(c, cx - UI_COVER_BIG / 2, y + 12, UI_COVER_BIG, a->covers_big[i], g->name);
        if (!g->broken && g->total > 0 && g->unlocked == g->total) img_fit(c, a, UI_IMG_trophy, cx - UI_COVER_BIG / 2 - 14, y + 2, 60);
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
        snprintf(buf, sizeof(buf), "%d / %d  ·  %d pts  ·  %d%%", g->unlocked, g->total, g->points_got, g->total > 0 ? g->unlocked * 100 / g->total : 0);
        text_center(c, cx, y + 220, 17, P->gray, buf, w - 20);
    }
    scrollbar(c, 124, l.rows * (h + gy) - gy, top, l.rows, (m->ngames + l.cols - 1) / l.cols);
}

void ui_home(gfx_canvas *c, const ui_assets *a, const gr_model *m, const gr_config *cfg, int cursor, int top) {
    begin(c);
    g_chip_opens_profile = true;
    header(c, a, NULL, T("Mis juegos", "My games"),
           cfg->games_scope == GR_SCOPE_PLAYED ? T("Logros de todos los juegos que has jugado", "Achievements for every game you have played")
                                               : T("Logros de tus juegos activos", "Achievements for your active games"));

    if (m->ngames == 0 && m->hidden > 0) {
        card(c, MARGIN, 190, CONTENT_W, 300, false);
        img(c, a, UI_IMG_m_sorprendido, MARGIN + 30, 210);
        gfx_text(c, MARGIN + 320, 262, 34, P->ink, T("No hay juegos con este filtro", "No games match this filter"));
        char line[160];
        snprintf(line, sizeof(line), T("Hay %d juego(s) oculto(s).", "%d game(s) hidden."), m->hidden);
        gfx_text(c, MARGIN + 320, 322, 23, P->gray, line);
        gfx_text(c, MARGIN + 320, 358, 23, P->gray, T("Cámbialo en Opciones (X), pestaña Vista.", "Change it in Options (X), View tab."));
    } else if (m->ngames == 0) {
        card(c, MARGIN, 170, CONTENT_W, 340, false);
        img(c, a, UI_IMG_m_durmiendo, MARGIN + 30, 210);
        gfx_text(c, MARGIN + 320, 214, 34, P->ink, T("Todavía no hay juegos con logros", "No games with achievements yet"));
        gfx_text(c, MARGIN + 320, 274, 22, P->gray,
                 T("Con internet, Greeny baja solo los logros de tus juegos.", "With internet, Greeny downloads achievements for your games."));
        gfx_text(c, MARGIN + 320, 320, 22, P->gray,
                 T("También puedes copiar un archivo por juego en la SD:", "You can also copy one file per game to the SD card:"));
        gfx_text(c, MARGIN + 320, 354, 22, P->accent, "/config/greeny/games/<TITLE ID>.txt");
        gfx_text(c, MARGIN + 320, 406, 20, P->gray, T("Luego pulsa Y para recargar.", "Then press Y to reload."));
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
    hx = hint(c, hx, "L", T("Buscar", "Find"));
    hx = hint(c, hx, "R", T("Perfil", "Profile"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- detalle: logros de un juego ---------- */

/* El logro de relleno (para los que no tienen icono), en color y en gris, en los dos tamaños. */
static const uint8_t *g_fill_small, *g_fill_small_gray, *g_fill_big, *g_fill_big_gray;
static void prepare_fill(const ui_assets *a) {
    static uint8_t small[GR_ICON_BYTES], small_gray[GR_ICON_BYTES], big[GR_ICON_BIG_BYTES], big_gray[GR_ICON_BIG_BYTES];
    static const uint8_t *done_for;
    const uint8_t *src = a->img[UI_IMG_ach_fill];
    if (!src || src == done_for) return;
    memcpy(big, src, GR_ICON_BIG_BYTES);
    gfx_round_corners(big, GR_ICON_BIG, GR_ICON_BIG, 22);
    gfx_scale_rgba(big, GR_ICON_BIG, GR_ICON_BIG, small, GR_ICON_SIZE, GR_ICON_SIZE);
    gr_icon_gray(big, big_gray, GR_ICON_BIG);
    gr_icon_gray(small, small_gray, GR_ICON_SIZE);
    g_fill_small = small; g_fill_small_gray = small_gray; g_fill_big = big; g_fill_big_gray = big_gray;
    done_for = src;
}

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

/* Icono del logro con su marco: bronce, plata u oro si ya lo tienes, gris si no. Si no hay
 * icono guardado, el circulo con check o candado de siempre. */
static void ach_icon(gfx_canvas *c, const ui_assets *a, int index, int cx, int cy, bool unlocked, gfx_color frame) {
    const uint8_t *pic = unlocked ? a->ach_icons[index] : a->ach_icons_gray[index];
    if (!pic) pic = unlocked ? g_fill_small : g_fill_small_gray; /* el logro de relleno */
    if (!pic) {
        if (unlocked) icon_unlocked(c, cx, cy);
        else icon_locked(c, cx, cy);
        return;
    }
    const int s = GR_ICON_SIZE, x = cx - s / 2, y = cy - s / 2;
    gfx_round_rect(c, x - 5, y - 5, s + 10, s + 10, 16, P->shadow);
    gfx_round_rect(c, x - 4, y - 4, s + 8, s + 8, 15, unlocked ? frame : P->gray_lt);
    gfx_image(c, x, y, pic, s, s);
}

/* colores de bronce, plata y oro: fondo y texto encima */
static gfx_color tier_bg(const lg_ach *ac) {
    switch (lg_ach_tier(ac)) {
        case LG_TIER_GOLD:   return GFX_RGB(0xF7, 0xC9, 0x1E);
        case LG_TIER_SILVER: return GFX_RGB(0xB4, 0xBC, 0xC6);
        default:             return GFX_RGB(0xB5, 0x6A, 0x2C);
    }
}
static const char *tier_name(const lg_ach *ac) {
    switch (lg_ach_tier(ac)) {
        case LG_TIER_GOLD:   return T("Oro", "Gold");
        case LG_TIER_SILVER: return T("Plata", "Silver");
        default:             return T("Bronce", "Bronze");
    }
}

static void unlock_date(const lg_ach *ac, char *buf, size_t cap) {
    buf[0] = 0;
    if (!ac->unlocked_at) return;
    time_t t = (time_t)ac->unlocked_at;
    struct tm tm;
    if (localtime_r(&t, &tm)) strftime(buf, cap, "%d/%m/%Y  %H:%M", &tm);
}

/* a la derecha de cada logro: los puntos y un check o un candado */
static void status_pills(gfx_canvas *c, const ui_assets *a, int right, int cy, const lg_ach *ac) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%u pts", ac->points);
    if (img(c, a, ac->unlocked ? UI_IMG_check : UI_IMG_lock, right - 44, cy - 22)) {
        pill(c, right - 44 - 12, cy - 14, C_YELLOW, C_ON_YELLOW, buf);
        return;
    }
    int w = ac->unlocked ? pill(c, right, cy - 14, C_GREEN, C_WHITE, T("DESBLOQUEADO", "UNLOCKED"))
                         : pill(c, right, cy - 14, C_RED, C_WHITE, T("BLOQUEADO", "LOCKED"));
    pill(c, right - w - 10, cy - 14, C_YELLOW, C_ON_YELLOW, buf);
}

/* La lista solo trae lo esencial: estado, titulo, puntos y como se consigue.
 * La fecha y la dificultad con nombre salen al abrir el logro (ui_achievement). */
static void detail_list(gfx_canvas *c, const ui_assets *a, const lg_game *gm, int cursor, int top, int rows) {
    const int x = MARGIN, w = CONTENT_W - 24, h = 82;
    for (int i = top; i < gm->nach && i < top + rows; i++) {
        const lg_ach *ac = &gm->ach[i];
        int y = 116 + (i - top) * (h + 8);
        card(c, x, y, w, h, i == cursor);
        zone(x, y, w, h, UI_ZONE_ITEM, i);
        ach_icon(c, a, i, x + 48, y + h / 2, ac->unlocked, tier_bg(ac));

        int tx = x + 96, tw = w - 96 - 220;
        const char *desc = lg_ach_desc(ac, g_en);
        gfx_text_fit(c, tx, y + 11, 26, ac->unlocked ? P->ink : P->gray, lg_ach_name(ac, g_en), tw);
        gfx_text_fit(c, tx, y + 46, 20, P->gray, desc[0] ? desc : T("Sin descripción", "No description"), tw);
        status_pills(c, a, x + w - 22, y + h / 2, ac);
    }
    scrollbar(c, 116, rows * (h + 8) - 8, top, rows, gm->nach);
}

static void detail_grid(gfx_canvas *c, const ui_assets *a, const lg_game *gm, int cursor, int top, ui_layout l) {
    const int w = 380, h = 168, gx = 18, gy = 10;
    for (int i = top * l.cols; i < gm->nach && i < (top + l.rows) * l.cols; i++) {
        const lg_ach *ac = &gm->ach[i];
        int x = MARGIN + (i % l.cols) * (w + gx), y = 116 + (i / l.cols - top) * (h + gy);
        card(c, x, y, w, h, i == cursor);
        zone(x, y, w, h, UI_ZONE_ITEM, i);

        const char *desc = lg_ach_desc(ac, g_en);
        if (!desc[0]) desc = T("Sin descripción", "No description");
        /* con la descripcion en una sola linea todo baja un poco, para que no quede pegado arriba */
        int dy = gfx_text_width(19, desc) <= w - 36 ? 14 : 0;
        ach_icon(c, a, i, x + 46, y + 46 + dy, ac->unlocked, tier_bg(ac));
        gfx_text_fit(c, x + 94, y + 14 + dy, 23, ac->unlocked ? P->ink : P->gray, lg_ach_name(ac, g_en), w - 94 - 14);
        status_pills(c, a, x + w - 14, y + 62 + dy, ac);
        text_2lines(c, x + 18, y + 96 + dy, 19, P->gray, desc, w - 36, 26);
    }
    scrollbar(c, 116, l.rows * (h + gy) - gy, top, l.rows, (gm->nach + l.cols - 1) / l.cols);
}

/* ---------- un logro al detalle ---------- */

/* Amplia un icono cuadrado al doble, suavizando entre pixeles. */
static void icon_double(const uint8_t *src, uint8_t *dst, int s) {
    const int d = s * 2;
    for (int y = 0; y < d; y++) {
        int fy = y * 2 > 1 ? (y * 2 - 1) : 0; /* posicion en cuartos de pixel de origen */
        int y0 = fy / 4, wy = fy % 4, y1 = y0 + 1 < s ? y0 + 1 : y0;
        for (int x = 0; x < d; x++) {
            int fx = x * 2 > 1 ? (x * 2 - 1) : 0;
            int x0 = fx / 4, wx = fx % 4, x1 = x0 + 1 < s ? x0 + 1 : x0;
            for (int k = 0; k < 4; k++) {
                int p00 = src[(y0 * s + x0) * 4 + k], p01 = src[(y0 * s + x1) * 4 + k];
                int p10 = src[(y1 * s + x0) * 4 + k], p11 = src[(y1 * s + x1) * 4 + k];
                int topv = p00 * (4 - wx) + p01 * wx, bot = p10 * (4 - wx) + p11 * wx;
                dst[(y * d + x) * 4 + k] = (uint8_t)((topv * (4 - wy) + bot * wy + 8) / 16);
            }
        }
    }
}

/* un dato con su dibujo a la izquierda: etiqueta pequeña arriba y el valor debajo */
static void fact(gfx_canvas *c, const ui_assets *a, int x, int y, int icon, const char *label, const char *value, gfx_color col) {
    int tx = x;
    if (icon >= 0 && img_fit(c, a, icon, x, y, 56)) tx = x + 70;
    gfx_text(c, tx, y + 2, 19, P->gray, label);
    gfx_text(c, tx, y + 26, 26, col, value);
}

void ui_achievement(gfx_canvas *c, const ui_assets *a, const gr_model *m, int game, int index) {
    const gr_game_entry *g = &m->games[game];
    const lg_ach *ac = &m->cur.ach[index];
    begin(c);
    char sub[96];
    snprintf(sub, sizeof(sub), T("Logro %d de %d", "Achievement %d of %d"), index + 1, m->cur.nach);
    header(c, a, a->covers[game], g->name, sub);
    prepare_fill(a);

    /* flechas a los lados: lo mismo que L y R */
    const int x = MARGIN + 70, y = 130, w = CONTENT_W - 140, h = 500;
    if (index > 0) {
        img(c, a, UI_IMG_arrow_l, MARGIN - 14, y + h / 2 - 36);
        zone(0, y, x, h, UI_ZONE_BUTTON, UI_BTN_L);
    }
    if (index + 1 < m->cur.nach) {
        img(c, a, UI_IMG_arrow_r, UI_W - MARGIN - 58, y + h / 2 - 36);
        zone(x + w, y, UI_W - x - w, h, UI_ZONE_BUTTON, UI_BTN_R);
    }
    card(c, x, y, w, h, false);

    /* a la derecha, la imagen en grande con el marco de su dificultad */
    const int big = GR_ICON_BIG * 2, ix = x + w - 56 - big, iy = y + 56;
    const uint8_t *pic = ac->unlocked ? a->ach_big[index] : a->ach_big_gray[index];
    if (!pic) pic = ac->unlocked ? g_fill_big : g_fill_big_gray;
    gfx_color frame = ac->unlocked ? tier_bg(ac) : P->gray_lt;
    gfx_round_rect(c, ix - 13, iy - 13, big + 26, big + 26, 52, P->shadow);
    gfx_round_rect(c, ix - 12, iy - 12, big + 24, big + 24, 51, frame);
    if (pic) {
        static uint8_t zoom[GR_ICON_BIG * 2 * GR_ICON_BIG * 2 * 4];
        icon_double(pic, zoom, GR_ICON_BIG);
        gfx_round_corners(zoom, big, big, 42);
        gfx_image(c, ix, iy, zoom, big, big);
    } else {
        gfx_round_rect(c, ix, iy, big, big, 42, P->card);
    }
    /* debajo, si ya lo tienes */
    const char *state = ac->unlocked ? T("Desbloqueado", "Unlocked") : T("Bloqueado", "Locked");
    int sw = gfx_text_width(26, state) + 54, sx = ix + big / 2 - sw / 2, sy = iy + big + 40;
    if (!img(c, a, ac->unlocked ? UI_IMG_check : UI_IMG_lock, sx, sy)) sx -= 54;
    gfx_text(c, sx + 54, sy + 5, 26, ac->unlocked ? P->accent : P->gray, state);

    /* a la izquierda, el texto */
    const int tx = x + 52, tw = ix - 12 - tx - 40;
    const char *desc = lg_ach_desc(ac, g_en);
    gfx_text_fit(c, tx, y + 52, 44, P->ink, lg_ach_name(ac, g_en), tw);
    gfx_rect(c, tx, y + 116, 72, 5, frame);
    gfx_text(c, tx, y + 144, 20, P->gray, T("CÓMO SE CONSIGUE", "HOW TO GET IT"));
    text_2lines(c, tx, y + 174, 27, P->ink, desc[0] ? desc : T("Sin descripción", "No description"), tw, 37);

    char buf[64];
    const int fy = y + 290;
    snprintf(buf, sizeof(buf), T("%u puntos", "%u points"), ac->points);
    fact(c, a, tx, fy, UI_IMG_stat_puntos, T("Puntos", "Points"), buf, P->ink);
    int tier = lg_ach_tier(ac);
    fact(c, a, tx + 300, fy, tier == LG_TIER_GOLD ? UI_IMG_medal_oro : tier == LG_TIER_SILVER ? UI_IMG_medal_plata : UI_IMG_medal_bronce,
         T("Dificultad", "Difficulty"), tier_name(ac), P->ink);
    if (ac->unlocked) {
        unlock_date(ac, buf, sizeof(buf));
        fact(c, a, tx, fy + 96, UI_IMG_stat_completados, T("Lo conseguiste el", "Earned on"),
             buf[0] ? buf : T("Fecha sin registrar", "Date not recorded"), P->ink);
    } else {
        fact(c, a, tx, fy + 96, UI_IMG_stat_logros, T("Estado", "Status"), T("Aún no lo tienes", "Not earned yet"), P->gray);
    }

    footer_bar(c);
    int hx = hint(c, MARGIN, "B", T("Volver", "Back"));
    if (m->cur.nach > 1) {
        hx = hint(c, hx, "L", T("Anterior", "Previous"));
        hx = hint(c, hx, "R", T("Siguiente", "Next"));
    }
    hint(c, hx, "+", T("Salir", "Exit"));
}

void ui_detail(gfx_canvas *c, const ui_assets *a, const gr_model *m, const gr_config *cfg, int game, int cursor, int top) {
    const gr_game_entry *g = &m->games[game];
    begin(c);

    char sub[160], ver[48];
    if (g->version[0]) snprintf(ver, sizeof(ver), T("Versión %s", "Version %s"), g->version);
    else snprintf(ver, sizeof(ver), "%s", T("Versión sin indicar", "Version not specified"));
    if (g->broken) snprintf(sub, sizeof(sub), "%s", T("Archivo de logros con error", "Achievement file has an error"));
    else snprintf(sub, sizeof(sub), T("%s  ·  %d de %d logros  ·  %d / %d pts", "%s  ·  %d of %d achievements  ·  %d / %d pts"),
                  ver, g->unlocked, g->total, g->points_got, g->points);
    header(c, a, a->covers[game], g->name, sub);
    prepare_fill(a);

    if (g->broken) {
        img(c, a, UI_IMG_m_triste, UI_W / 2 - 130, 350);
        card(c, MARGIN, 150, CONTENT_W, 170, false);
        gfx_text(c, MARGIN + 32, 172, 28, C_RED,
                 T("El archivo de logros de este juego tiene un error", "This game's achievement file has an error"));
        gfx_text_fit(c, MARGIN + 32, 222, 22, P->ink, g->err, CONTENT_W - 64);
        gfx_text(c, MARGIN + 32, 264, 20, P->gray,
                 T("Corrígelo en la tarjeta SD y pulsa Y en la pantalla de inicio.",
                   "Fix it on the SD card and press Y on the home screen."));
    } else {
        ui_layout l = ui_detail_layout(cfg);
        if (l.cols > 1) detail_grid(c, a, &m->cur, cursor, top, l);
        else detail_list(c, a, &m->cur, cursor, top, l.rows);
    }

    footer_bar(c);
    int hx = MARGIN;
    if (!g->broken && m->cur.nach > 0) hx = hint(c, hx, "A", T("Ver detalle", "View details"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    if (!g->broken) hx = hint(c, hx, "−", cfg->ach_view == GR_VIEW_GRID ? T("Lista", "List") : T("Cuadrícula", "Grid"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- buscador de juegos ---------- */

static int status_img(const char *status) {
    if (!strcmp(status, "perfecto")) return UI_IMG_st_perfecto;
    if (!strcmp(status, "jugable")) return UI_IMG_st_jugable;
    if (!strcmp(status, "prueba")) return UI_IMG_st_prueba;
    return UI_IMG_st_sin_empezar;
}

static const char *status_name(const char *status) {
    if (!strcmp(status, "perfecto")) return T("Perfecto", "Perfect");
    if (!strcmp(status, "jugable")) return T("Jugable", "Playable");
    if (!strcmp(status, "prueba")) return T("En prueba", "Testing");
    return T("Sin empezar", "Not started");
}

void ui_browse(gfx_canvas *c, const ui_assets *a, const gr_cat_game *games, const int *shown, int nshown, int total,
               const bool *have, const char *query, ui_browse_state state, int cursor, int top) {
    (void)query;
    begin(c);
    g_buttons = a->buttons; /* sin barra arriba, igual que el perfil */
    g_a = a;

    gfx_text(c, MARGIN, 26, 42, P->ink, T("Buscar juegos", "Find games"));
    char sub[96];
    if (state == UI_BROWSE_OK) snprintf(sub, sizeof(sub), T("%d juegos con logros, de la A a la Z", "%d games with achievements, A to Z"), total);
    else snprintf(sub, sizeof(sub), "%s", T("Catálogo de Greeny", "Greeny catalogue"));
    gfx_text(c, MARGIN + 2, 78, 20, P->gray, sub);

    if (state != UI_BROWSE_OK || nshown == 0) {
        card(c, MARGIN, 180, CONTENT_W, 320, false);
        int mascot = state == UI_BROWSE_OFFLINE ? UI_IMG_m_triste : UI_IMG_m_buscando;
        img(c, a, mascot, MARGIN + 30, 210);
        const char *t1, *t2;
        if (state == UI_BROWSE_LOADING) { t1 = T("Buscando en el catálogo…", "Searching the catalogue…"); t2 = T("Un momento.", "One moment."); }
        else if (state == UI_BROWSE_OFFLINE) {
            t1 = T("No hay conexión", "No connection");
            t2 = T("Conecta la consola a internet y pulsa Y para intentarlo de nuevo.", "Connect the console to the internet and press Y to try again.");
        } else { t1 = T("El catálogo está vacío", "The catalogue is empty"); t2 = T("Todavía no se ha publicado ningún juego.", "No games have been published yet."); }
        gfx_text(c, MARGIN + 320, 290, 36, P->ink, t1);
        gfx_text_fit(c, MARGIN + 320, 350, 23, P->gray, t2, CONTENT_W - 360);
    } else {
        /* a la derecha del titulo, la letra por la que va la lista */
        char letter[5];
        first_letter(games[shown[cursor]].name, letter);
        if (letter[0] >= 'a' && letter[0] <= 'z') letter[0] = (char)(letter[0] - 32);
        gfx_circle(c, UI_W - MARGIN - 34, 60, 34, C_GREEN);
        gfx_text(c, UI_W - MARGIN - 34 - gfx_text_width(38, letter) / 2, 37, 38, C_WHITE, letter);

        const int x = MARGIN, w = CONTENT_W - 24, h = 78;
        for (int i = top; i < nshown && i < top + UI_BROWSE_ROWS; i++) {
            const gr_cat_game *g = &games[shown[i]];
            int y = 122 + (i - top) * (h + 10);
            card(c, x, y, w, h, i == cursor);
            zone(x, y, w, h, UI_ZONE_ITEM, i);
            img(c, a, status_img(g->status), x + 22, y + h / 2 - 20);
            gfx_text_fit(c, x + 82, y + 10, 27, P->ink, g->name, w - 82 - 380);
            char line[96];
            if (g->version[0]) snprintf(line, sizeof(line), T("%s  ·  versión %s", "%s  ·  version %s"), status_name(g->status), g->version);
            else snprintf(line, sizeof(line), "%s", status_name(g->status));
            gfx_text_fit(c, x + 82, y + 45, 19, P->gray, line, w - 82 - 380);
            if (have[shown[i]]) pill(c, x + w - 22, y + h / 2 - 14, C_GREEN, C_WHITE, T("EN TU CONSOLA", "ON YOUR CONSOLE"));
        }
        scrollbar(c, 122, UI_BROWSE_ROWS * (h + 10) - 10, top, UI_BROWSE_ROWS, nshown);
    }

    footer_bar(c);
    int hx = MARGIN;
    if (state == UI_BROWSE_OK && nshown > 0) hx = hint(c, hx, "A", T("Ver logros", "View achievements"));
    hx = hint(c, hx, "R", T("Mis juegos", "My games"));
    if (state == UI_BROWSE_OK && nshown > UI_BROWSE_ROWS) hx = hint(c, hx, "X", T("Siguiente letra", "Next letter"));
    hx = hint(c, hx, "Y", T("Recargar", "Reload"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

void ui_browse_game(gfx_canvas *c, const ui_assets *a, const gr_cat_game *game, const lg_game *def, int cursor, int top) {
    begin(c);
    int points = 0;
    for (int i = 0; i < def->nach; i++) points += def->ach[i].points;
    char sub[160];
    snprintf(sub, sizeof(sub), T("%s  ·  %d logros  ·  %d pts  ·  no está en tu consola", "%s  ·  %d achievements  ·  %d pts  ·  not on your console"),
             status_name(game->status), def->nach, points);
    header(c, a, NULL, game->name, sub);
    prepare_fill(a);
    static ui_assets bare; /* sin iconos propios: todos con el de relleno */
    bare = *a;
    memset(bare.ach_icons, 0, sizeof(bare.ach_icons));
    memset(bare.ach_icons_gray, 0, sizeof(bare.ach_icons_gray));
    detail_list(c, &bare, def, cursor, top, 6);
    footer_bar(c);
    int hx = hint(c, MARGIN, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- opciones ---------- */

static const struct { const char *es, *en; int n; int opt[4]; } k_tabs[UI_TABS] = {
    { "General",     "General",  4, { UI_OPT_LANG, UI_OPT_THEME, UI_OPT_ANIM, UI_OPT_START } },
    { "Vista",       "View",     4, { UI_OPT_SCOPE, UI_OPT_SHOW, UI_OPT_GAMES_VIEW, UI_OPT_ACH_VIEW } },
    { "En el juego", "In-game",  3, { UI_OPT_INGAME, UI_OPT_SOUND, UI_OPT_POPUP } },
    { "Música",      "Music",    0, { 0 } },  /* una fila para encender y una por cancion */
    { "Cuenta",      "Account",  0, { 0 } },  /* sus filas dependen de si la consola esta vinculada */
    { "Ayuda",       "Help",     2, { UI_OPT_DIAG, UI_OPT_ABOUT } },
};

int ui_tab_count(int tab, bool linked) {
    if (tab == UI_TAB_MUSIC) return 1 + g_nsongs;
    if (tab == UI_TAB_ACCOUNT) return linked ? 3 : 2;
    return k_tabs[tab].n;
}

int ui_tab_option(int tab, int row, bool linked) {
    static const int with[3] = { UI_OPT_ACC_INFO, UI_OPT_ACC_SYNC, UI_OPT_ACC_UNLINK };
    static const int without[2] = { UI_OPT_ACC_LINK, UI_OPT_ACC_LOGIN };
    if (tab == UI_TAB_MUSIC) return row == 0 ? UI_OPT_MUSIC : UI_OPT_SONG;
    if (tab == UI_TAB_ACCOUNT) return linked ? with[row] : without[row];
    return k_tabs[tab].opt[row];
}

/* valores especiales de option_row: dibujan la palanquita en vez de texto */
static const char k_on[] = "on", k_off[] = "off";

static void option_row(gfx_canvas *c, int y, bool selected, const char *label, const char *help, const char *value,
                       gfx_color value_bg) {
    const int x = MARGIN, w = CONTENT_W, h = 88;
    card(c, x, y, w, h, selected);
    zone(x, y, w, h, UI_ZONE_ITEM, g_row_index);
    gfx_text(c, x + 28, y + 12, 27, P->ink, label);
    gfx_text_fit(c, x + 28, y + 51, 19, P->gray, help, w - 56 - 300);
    if (value == k_on || value == k_off) { /* encendido o apagado: la palanquita */
        if (img(c, g_a, value == k_on ? UI_IMG_toggle_on : UI_IMG_toggle_off, x + w - 28 - 92, y + 22)) return;
        value = value == k_on ? T("Activado", "On") : T("Desactivado", "Off");
    }
    int vw = gfx_text_width(21, value) + 42;
    gfx_round_rect(c, x + w - 28 - vw, y + 23, vw, 42, 21, value_bg);
    gfx_text(c, x + w - 28 - vw + 21, y + 31, 21, C_WHITE, value);
}

/* boton L o R junto a las pestañas: el icono, o un recuadro con la letra si no hay */
static void shoulder(gfx_canvas *c, int x, int y, int btn, const char *letter) {
    zone(x - 6, y - 12, 58, 58, UI_ZONE_BUTTON, btn);
    if (g_buttons) {
        gfx_image(c, x + 4, y - 2, g_buttons + (size_t)btn * UI_BTN_SIZE * UI_BTN_SIZE * 4, UI_BTN_SIZE, UI_BTN_SIZE);
        return;
    }
    gfx_round_rect(c, x, y, 46, 34, 10, P->ink);
    gfx_text(c, x + 23 - gfx_text_width(21, letter) / 2, y + 5, 21, P->footer, letter);
}

void ui_settings(gfx_canvas *c, const ui_assets *a, const gr_config *cfg, int tab, int row, bool system_spanish,
                 bool system_dark, const ui_account *acc) {
    begin(c);
    header(c, a, NULL, T("Opciones", "Options"), T("Se guardan en la tarjeta SD", "Saved to the SD card"));

    /* pestañas */
    int tx = MARGIN + 60;
    shoulder(c, MARGIN, 121, UI_BTN_L, "L");
    for (int i = 0; i < UI_TABS; i++) {
        const char *name = T(k_tabs[i].es, k_tabs[i].en);
        int w = gfx_text_width(23, name) + 44;
        if (i == tab) gfx_round_rect(c, tx, 116, w, 44, 22, C_GREEN);
        else gfx_round_rect(c, tx, 116, w, 44, 22, P->card);
        zone(tx, 108, w, 60, UI_ZONE_TAB, i);
        gfx_text(c, tx + 22, 124, 23, i == tab ? C_WHITE : P->gray, name);
        tx += w + 10;
    }
    shoulder(c, tx + 4, 121, UI_BTN_R, "R");

    char lang[48], theme[48];
    if (cfg->lang == GR_LANG_ES) snprintf(lang, sizeof(lang), "Español");
    else if (cfg->lang == GR_LANG_EN) snprintf(lang, sizeof(lang), "English");
    else snprintf(lang, sizeof(lang), "Auto (%s)", system_spanish ? "Español" : "English");

    if (cfg->theme == GR_THEME_LIGHT) snprintf(theme, sizeof(theme), "%s", T("Claro", "Light"));
    else if (cfg->theme == GR_THEME_DARK) snprintf(theme, sizeof(theme), "%s", T("Oscuro", "Dark"));
    else snprintf(theme, sizeof(theme), "Auto (%s)", system_dark ? T("Oscuro", "Dark") : T("Claro", "Light"));

    const char *on = k_on, *off = k_off;
    const char *list = T("Lista", "List"), *grid = T("Cuadrícula", "Grid"), *open = T("Abrir", "Open");

    char who[64];
    snprintf(who, sizeof(who), "@%s", acc->username[0] ? acc->username : "?");

    int count = ui_tab_count(tab, acc->linked);
    for (int i = 0; i < count; i++) {
        int y = tab == UI_TAB_MUSIC ? 176 + i * 94 : 184 + i * 104; /* en Musica caben cinco filas */
        bool sel = i == row;
        g_row_index = i;
        switch (ui_tab_option(tab, i, acc->linked)) {
            case UI_OPT_MUSIC:
                option_row(c, y, sel, T("Música de fondo", "Background music"),
                           T("Suena mientras usas la app. Elige abajo la canción; se descargan una sola vez.",
                             "Plays while you use the app. Pick a song below; each one downloads only once."),
                           cfg->music ? on : off, C_GREEN);
                break;
            case UI_OPT_SONG: {
                const ui_song *sg = &g_songs[i - 1];
                bool current = !strcmp(cfg->music_track, sg->id);
                char help[96];
                if (g_song_busy == i) snprintf(help, sizeof(help), "%s", T("Descargando, un momento…", "Downloading, one moment…"));
                else if (sg->have) snprintf(help, sizeof(help), "%s", current ? T("Es la canción elegida. Y la borra de la SD.", "This is the chosen song. Y removes it from the SD card.")
                                                                              : T("Descargada. A la pone; Y la borra de la SD.", "Downloaded. A plays it; Y removes it from the SD card."));
                else snprintf(help, sizeof(help), T("Sin descargar · %d.%d MB", "Not downloaded · %d.%d MB"), sg->kb / 1024, sg->kb % 1024 * 10 / 1024);
                option_row(c, y, sel, sg->title, help,
                           g_song_busy == i ? T("Descargando…", "Downloading…") : !sg->have ? T("Descargar", "Download")
                           : current ? (cfg->music ? T("Sonando", "Playing") : T("Elegida", "Chosen")) : T("Usar", "Use"),
                           sg->have && current ? C_GREEN : C_NEUTRAL);
                break;
            }
            case UI_OPT_ACC_LINK:
                option_row(c, y, sel, T("Vincular con un código", "Link with a code"),
                           T("Te da un código para escribir en la web. Sirve también si entras con Discord.",
                             "Gives you a code to type on the website. Also works if you sign in with Discord."), open, C_GREEN);
                break;
            case UI_OPT_ACC_LOGIN:
                option_row(c, y, sel, T("Entrar con correo y contraseña", "Sign in with email and password"),
                           T("Escribes los datos de tu cuenta con el teclado de la consola.",
                             "Type your account details with the console keyboard."), open, C_GREEN);
                break;
            case UI_OPT_ACC_INFO:
                option_row(c, y, sel, who, acc->caption[0] ? acc->caption : T("Esta consola sube tus logros a esta cuenta.", "This console uploads your achievements to this account."),
                           T("Vinculada", "Linked"), C_GREEN);
                break;
            case UI_OPT_ACC_SYNC:
                option_row(c, y, sel, T("Sincronizar ahora", "Sync now"), T(acc->sync_es, acc->sync_en), T("Subir", "Upload"), C_GREEN);
                break;
            case UI_OPT_ACC_UNLINK:
                option_row(c, y, sel, T("Desvincular esta consola", "Unlink this console"),
                           T("Deja de subir logros. Los que ya subiste se quedan en tu cuenta.",
                             "Stops uploading. Achievements already uploaded stay on your account."),
                           T("Desvincular", "Unlink"), C_NEUTRAL);
                break;
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
            case UI_OPT_INGAME:
                option_row(c, y, sel, T("Logros en los juegos", "Achievements in games"),
                           T("Greeny vigila el juego y desbloquea logros. Al cambiarlo hay que reiniciar la consola.",
                             "Greeny watches the game and unlocks achievements. Restart the console after changing it."),
                           g_ingame ? on : off, C_GREEN);
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
            case UI_OPT_SCOPE:
                option_row(c, y, sel, T("Juegos en el inicio", "Games on the home screen"),
                           T("Activos: los instalados en esta consola. Jugados: además, los que ya tienen logros tuyos.",
                             "Active: installed on this console. Played: also the ones where you already have achievements."),
                           cfg->games_scope == GR_SCOPE_PLAYED ? T("Todos los jugados", "All played") : T("Solo activos", "Active only"), C_GREEN);
                break;
            case UI_OPT_SHOW:
                option_row(c, y, sel, T("Mostrar", "Show"),
                           T("Filtra los juegos según te falten logros o ya los tengas todos.",
                             "Filters games by whether you still have achievements left."),
                           cfg->games_show == GR_SHOW_PENDING ? T("Por completar", "In progress")
                           : cfg->games_show == GR_SHOW_DONE ? T("Completados", "Completed") : T("Todos", "All"), C_GREEN);
                break;
            case UI_OPT_START:
                option_row(c, y, sel, T("Página de inicio", "Start page"),
                           T("Qué pantalla se abre al entrar a la app.", "Which screen opens when you start the app."),
                           cfg->start_page == GR_START_PROFILE ? T("Mi perfil", "My profile") : T("Mis trofeos", "My trophies"), C_GREEN);
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

    if (tab == UI_TAB_ACCOUNT)
        gfx_text(c, MARGIN + 4, 184 + count * 104 + 6, 19, P->gray,
                 acc->linked ? T("La cuenta es opcional: sin internet la app y los logros funcionan igual.",
                                 "The account is optional: without internet the app and achievements work the same.")
                             : T("Opcional. Crea tu cuenta en " GR_WEB_URL " y vincula esta consola para ver tu progreso en la web.",
                                 "Optional. Create your account at " GR_WEB_URL " and link this console to see your progress online."));

    footer_bar(c);
    int cur = ui_tab_option(tab, row, acc->linked);
    int hx = hint(c, MARGIN, "A", cur >= UI_OPT_DIAG ? T("Abrir", "Open") : T("Cambiar", "Change"));
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
    begin(c);

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
    begin(c);
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

    int side = x + w + (UI_W - MARGIN - x - w) / 2 + 10; /* centro de la columna derecha */
    img(c, a, UI_IMG_logo, side - 150, 132);
    if (a->mascot_big) gfx_image(c, side - a->big_w / 2, 320, a->mascot_big, a->big_w, a->big_h);

    footer_bar(c);
    int hx = hint(c, MARGIN, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- introduccion ---------- */

static void intro_dots(gfx_canvas *c, int page) {
    for (int i = 0; i < UI_INTRO_PAGES; i++)
        gfx_circle(c, UI_W / 2 + i * 34 - (UI_INTRO_PAGES - 1) * 17, 632, i == page ? 9 : 6, i == page ? C_GREEN : P->gray_lt);
}

void ui_intro(gfx_canvas *c, const ui_assets *a, int page, const gr_diag_item *items, int count,
              bool sysmodule_running) {
    begin(c);
    char sub[48];
    snprintf(sub, sizeof(sub), T("Paso %d de %d", "Step %d of %d"), page + 1, UI_INTRO_PAGES);
    header(c, a, NULL, T("Primeros pasos", "Getting started"), sub);

    int fails = 0;
    for (int i = 0; i < count; i++) fails += items[i].state == GR_DIAG_FAIL;

    if (page == 0) {
        if (!img(c, a, UI_IMG_m_saludando, 90, 200) && a->mascot_big) gfx_image(c, 100, 210, a->mascot_big, a->big_w, a->big_h);
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
    } else if (page == 3) {
        const int x = 190, w = 900;
        card(c, x, 170, w, 380, false);
        int tx = x + 44;
        gfx_text(c, tx, 196, 38, P->ink, T("Tu cuenta (opcional)", "Your account (optional)"));
        gfx_text(c, tx, 266, 22, P->ink, T("Con una cuenta, tus logros aparecen en tu perfil de la web.", "With an account, your achievements show on your web profile."));
        gfx_text(c, tx, 300, 22, P->ink, T("Créala en:", "Create it at:"));
        gfx_text(c, tx + gfx_text_width(22, T("Créala en:", "Create it at:")) + 12, 300, 22, P->accent, GR_WEB_URL);
        gfx_text(c, tx, 356, 22, P->ink, T("Luego, aquí en la app: Opciones → Cuenta → Vincular.", "Then, here in the app: Options → Account → Link."));
        gfx_text_fit(c, tx, 430, 20, P->gray,
                     T("No es obligatoria: sin cuenta y sin internet, Greeny funciona igual.",
                       "It is not required: without an account or internet, Greeny works the same."), w - 88);
    } else {
        const int x = 190, w = 900;
        card(c, x, 170, w, 380, false);
        int tx = x + 44;
        img(c, a, sysmodule_running && !fails ? UI_IMG_m_celebrando : UI_IMG_m_diagnostico, x + w - 250, 40);
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

/* ---------- cuenta: codigo de vinculacion y mensajes ---------- */

void ui_link(gfx_canvas *c, const ui_assets *a, const char *code, int seconds_left) {
    begin(c);
    header(c, a, NULL, T("Vincular esta consola", "Link this console"), T("Paso único, desde tu teléfono o PC", "One-time step, from your phone or PC"));

    const int x = 190, w = 900;
    card(c, x, 130, w, 500, false);
    int tx = x + 48;
    gfx_text(c, tx, 152, 22, P->gray, T("1. Entra a esta página e inicia sesión:", "1. Open this page and sign in:"));
    gfx_text(c, tx, 184, 30, P->accent, GR_WEB_URL);
    gfx_text(c, tx, 244, 22, P->gray, T("2. En Cuenta → Vincular una consola, escribe este código:", "2. Under Account → Link a console, type this code:"));

    /* el codigo, letra por letra en recuadros */
    int n = (int)strlen(code), bw = 96, gap = 14, total = n * bw + (n - 1) * gap, bx = x + (w - total) / 2;
    for (int i = 0; i < n; i++) {
        char ch[2] = { code[i], 0 };
        gfx_round_rect(c, bx + i * (bw + gap), 296, bw, 120, 16, C_GREEN);
        gfx_text(c, bx + i * (bw + gap) + bw / 2 - gfx_text_width(76, ch) / 2, 312, 76, C_WHITE, ch);
    }

    char buf[96];
    if (seconds_left > 0) {
        snprintf(buf, sizeof(buf), T("Esperando… el código vence en %d:%02d", "Waiting… the code expires in %d:%02d"), seconds_left / 60, seconds_left % 60);
        text_center(c, x + w / 2, 446, 22, P->ink, buf, w - 80);
    } else {
        text_center(c, x + w / 2, 446, 22, C_RED, T("El código venció. Pulsa B y pide otro.", "The code expired. Press B and get a new one."), w - 80);
    }
    text_center(c, x + w / 2, 500, 19, P->gray,
                T("Esta pantalla se cierra sola cuando la web acepte el código.", "This screen closes by itself once the website accepts the code."), w - 80);
    text_center(c, x + w / 2, 560, 19, P->gray,
                T("La consola no guarda tu contraseña.", "The console does not store your password."), w - 80);

    footer_bar(c);
    int hx = hint(c, MARGIN, "B", T("Cancelar", "Cancel"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

void ui_message(gfx_canvas *c, const ui_assets *a, const char *title, const char *line1, const char *line2, bool ok) {
    begin(c);
    header(c, a, NULL, T("Cuenta", "Account"), NULL);

    const int x = 150, w = 980;
    card(c, x, 190, w, 320, false);
    int tx = x + 44;
    bool pic = img(c, a, ok ? UI_IMG_m_celebrando : UI_IMG_m_error, x + w - 44 - 240, 222);
    int room = pic ? w - 88 - 250 : w - 88;
    if (ok) {
        gfx_circle(c, tx + 34, 270, 34, C_GREEN);
        gfx_line(c, tx + 18, 272, tx + 29, 284, 8, C_WHITE);
        gfx_line(c, tx + 29, 284, tx + 51, 258, 8, C_WHITE);
    } else {
        gfx_circle(c, tx + 34, 270, 34, C_YELLOW);
        gfx_text(c, tx + 34 - gfx_text_width(44, "!") / 2, 242, 44, C_ON_YELLOW, "!");
    }
    gfx_text_fit(c, tx + 92, 246, 36, P->ink, title, room - 92);
    if (line1) text_2lines(c, tx, 334, 23, P->ink, line1, room, 30);
    if (line2) text_2lines(c, tx, 410, 21, P->gray, line2, room, 28);

    footer_bar(c);
    int hx = hint(c, MARGIN, "A", T("Aceptar", "OK"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- perfil y medallas ---------- */

/* estrella de cinco puntas hecha con lineas */
static void star(gfx_canvas *c, int cx, int cy, int r, int thick, gfx_color col) {
    int px[5], py[5];
    for (int i = 0; i < 5; i++) {
        float ang = -1.5707963f + (float)i * 1.2566371f;
        px[i] = cx + (int)(cosf(ang) * (float)r);
        py[i] = cy + (int)(sinf(ang) * (float)r);
    }
    for (int i = 0; i < 5; i++) gfx_line(c, px[i], py[i], px[(i + 2) % 5], py[(i + 2) % 5], thick, col);
}

/* Medalla provisional: el color dice de que tipo es. Cuando haya dibujos, van aqui. */
static void medal(gfx_canvas *c, int cx, int cy, int r, const gr_badge *b, bool earned) {
    /* los dibujos van en el mismo orden que gr_badges; el ultimo es la medalla bloqueada */
    int idx = (int)(b - gr_badges);
    if (idx >= 0 && idx < 11 && img_fit(c, g_a, earned ? UI_IMG_badge_primer_logro + idx : UI_IMG_badge_bloqueada, cx - r, cy - r, r * 2)) return;
    static const gfx_color kind_col[] = { GFX_RGB(0x2F, 0xA6, 0x3B), GFX_RGB(0xF7, 0xC9, 0x1E), GFX_RGB(0x2B, 0x8F, 0xD9), GFX_RGB(0xE5, 0x48, 0x4D) };
    if (!earned) {
        gfx_circle(c, cx, cy, r, P->gray_lt);
        gfx_circle(c, cx, cy - r / 5, r / 3, P->gray);
        gfx_circle(c, cx, cy - r / 5, r / 5, P->gray_lt);
        gfx_round_rect(c, cx - r * 4 / 9, cy - r / 8, r * 8 / 9, r * 2 / 3, 4, P->gray);
        return;
    }
    gfx_circle(c, cx, cy, r, P->ink);
    gfx_circle(c, cx, cy, r - 3, kind_col[b->kind]);
    star(c, cx, cy + 1, r * 5 / 9, r > 24 ? 5 : 3, b->kind == GR_BADGE_PLATINUMS ? C_ON_YELLOW : C_WHITE);
}

static void stat_tile(gfx_canvas *c, const ui_assets *a, int x, int y, int w, int icon, const char *value, const char *label) {
    card(c, x, y, w, 104, false);
    img(c, a, icon, x + w - 72, y + 24);
    gfx_text(c, x + 22, y + 12, 40, P->ink, value);
    gfx_text_fit(c, x + 22, y + 66, 19, P->gray, label, w - 100);
}

void ui_profile_screen(gfx_canvas *c, const ui_assets *a, const ui_profile *p) {
    begin(c);
    g_buttons = a->buttons; /* esta pantalla no usa la cabecera, que es la que suele dejarlos listos */
    g_a = a;

    int into, span, level = gr_level(p->stats.points, &into, &span);
    char buf[96];

    /* tarjeta del usuario */
    const int lx = MARGIN, lw = 400, ly = 34, lh = 600;
    card(c, lx, ly, lw, lh, false);
    int cx = lx + lw / 2;
    if (p->avatar < 1 || p->avatar > UI_AVATAR_COUNT) gfx_circle(c, cx, ly + 110, 84, C_YELLOW);
    avatar(c, cx, ly + 110, 77, p->name, p->avatar);
    text_center(c, cx, ly + 214, 34, P->ink, p->name, lw - 40);
    if (p->linked && p->username[0]) {
        snprintf(buf, sizeof(buf), "@%s", p->username);
        text_center(c, cx, ly + 258, 20, P->gray, buf, lw - 40);
    } else {
        text_center(c, cx, ly + 258, 19, P->gray, T("Sin cuenta vinculada", "No account linked"), lw - 40);
    }
    if (p->caption && p->caption[0]) {
        snprintf(buf, sizeof(buf), "“%s”", p->caption);
        text_center(c, cx, ly + 290, 21, P->ink, buf, lw - 40);
    }
    if (p->linked && p->friend_code && p->friend_code[0]) {
        text_center(c, cx, ly + 528, 16, P->gray, T("CÓDIGO DE AMIGO", "FRIEND CODE"), lw - 40);
        text_center(c, cx, ly + 550, 26, P->accent, p->friend_code, lw - 40);
    }

    snprintf(buf, sizeof(buf), T("Nivel %d", "Level %d"), level);
    int pw = gfx_text_width(26, buf) + 44;
    gfx_round_rect(c, cx - pw / 2, ly + 340, pw, 46, 23, C_YELLOW);
    gfx_text(c, cx - pw / 2 + 22, ly + 348, 26, C_ON_YELLOW, buf);
    gfx_round_rect(c, lx + 40, ly + 406, lw - 80, 18, 9, P->gray_lt);
    if (into > 0) gfx_round_rect(c, lx + 40, ly + 406, 18 + (lw - 98) * into / span, 18, 9, C_GREEN);
    snprintf(buf, sizeof(buf), T("%d / %d pts para el nivel %d", "%d / %d pts to level %d"), into, span, level + 1);
    text_center(c, cx, ly + 432, 18, P->gray, buf, lw - 40);
    snprintf(buf, sizeof(buf), T("%d puntos en total", "%d points in total"), p->stats.points);
    text_center(c, cx, ly + 470, 21, P->points, buf, lw - 40);

    /* numeros */
    const int rx = lx + lw + 30, tw = 240, gap = 20;
    char v[6][16];
    snprintf(v[0], 16, "%d", p->stats.games);
    snprintf(v[1], 16, "%d", p->stats.achievements);
    snprintf(v[2], 16, "%d", p->stats.platinums);
    snprintf(v[3], 16, "%d", p->stats.points);
    if (p->linked && p->social_known) { snprintf(v[4], 16, "%d", p->followers); snprintf(v[5], 16, "%d", p->following); }
    else { snprintf(v[4], 16, "–"); snprintf(v[5], 16, "–"); }
    const char *labels[6] = { T("Mis juegos", "My games"), T("Logros", "Achievements"), T("Platinados", "Platinum"),
                              T("Puntos", "Points"), T("Seguidores", "Followers"), T("Siguiendo", "Following") };
    static const int icons[6] = { UI_IMG_stat_juegos, UI_IMG_stat_logros, UI_IMG_stat_completados, UI_IMG_stat_puntos,
                                  UI_IMG_stat_seguidores, UI_IMG_stat_siguiendo };
    for (int i = 0; i < 6; i++) stat_tile(c, a, rx + (i % 3) * (tw + gap), ly + (i / 3) * 124, tw, icons[i], v[i], labels[i]);
    if (!p->linked)
        gfx_text(c, rx + 4, ly + 252, 18, P->gray,
                 T("Seguidores y siguiendo aparecen al vincular tu cuenta (Opciones → Cuenta).",
                   "Followers and following appear once you link your account (Options → Account)."));

    /* medallas */
    const int by = ly + 296, bw = 3 * tw + 2 * gap;
    card(c, rx, by, bw, 304, false);
    snprintf(buf, sizeof(buf), T("Medallas  ·  %d de %d", "Badges  ·  %d of %d"), gr_badges_earned(&p->stats), gr_badge_count);
    gfx_text(c, rx + 24, by + 16, 26, P->ink, buf);
    const int per_row = 6, r = 40, cell = (bw - 48) / per_row;
    for (int i = 0; i < gr_badge_count; i++) {
        int mx = rx + 24 + (i % per_row) * cell + cell / 2, my = by + 104 + (i / per_row) * 116;
        bool earned = gr_badge_earned(&gr_badges[i], &p->stats);
        medal(c, mx, my, r, &gr_badges[i], earned);
        text_center(c, mx, my + r + 6, 15, earned ? P->ink : P->gray, T(gr_badges[i].name_es, gr_badges[i].name_en), cell - 6);
    }

    footer_bar(c);
    int hx = hint(c, MARGIN, "L", T("Mis trofeos", "My trophies"));
    hx = hint(c, hx, "X", T("Ver medallas", "View badges"));
    if (p->linked) hx = hint(c, hx, "Y", T("Seguir con código", "Follow with code"));
    if (p->linked) hx = hint(c, hx, "A", T("Cambiar foto", "Change picture"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

void ui_avatars(gfx_canvas *c, const ui_assets *a, const ui_profile *p, int cursor) {
    begin(c);
    header(c, a, NULL, T("Foto de perfil", "Profile picture"), T("Elige una; se ve en la app y en la web", "Pick one; it shows in the app and on the website"));
    const int cell = 128, size = 108, rows = (UI_AVATAR_COUNT + UI_AVATAR_COLS - 1) / UI_AVATAR_COLS;
    const int x0 = (UI_W - UI_AVATAR_COLS * cell) / 2, y0 = 124 + (520 - rows * cell) / 2;
    for (int i = 0; i < UI_AVATAR_COUNT; i++) {
        int cx = x0 + (i % UI_AVATAR_COLS) * cell + cell / 2, cy = y0 + (i / UI_AVATAR_COLS) * cell + cell / 2;
        if (i == cursor) gfx_circle(c, cx, cy, size / 2 + 9, C_YELLOW);
        else if (i + 1 == p->avatar) gfx_circle(c, cx, cy, size / 2 + 6, C_GREEN);
        img_fit(c, a, UI_IMG_avatar_01 + i, cx - size / 2, cy - size / 2, size);
        zone(cx - cell / 2, cy - cell / 2, cell, cell, UI_ZONE_ITEM, i);
    }
    footer_bar(c);
    int hx = hint(c, MARGIN, "A", T("Usar esta", "Use this one"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

void ui_badges(gfx_canvas *c, const ui_assets *a, const ui_profile *p) {
    begin(c);
    char sub[96];
    snprintf(sub, sizeof(sub), T("%d de %d conseguidas", "%d of %d earned"), gr_badges_earned(&p->stats), gr_badge_count);
    header(c, a, NULL, T("Medallas", "Badges"), sub);

    const int x = MARGIN, w = CONTENT_W, h = 44;
    for (int i = 0; i < gr_badge_count; i++) {
        const gr_badge *b = &gr_badges[i];
        int y = 112 + i * 49, value = gr_badge_value(b, &p->stats);
        bool earned = value >= b->goal;
        gfx_round_rect(c, x, y, w, h, 12, P->card);
        medal(c, x + 28, y + h / 2, 17, b, earned);
        gfx_text_fit(c, x + 58, y + 9, 21, earned ? P->ink : P->gray, T(b->name_es, b->name_en), 250);
        gfx_text_fit(c, x + 320, y + 11, 18, P->gray, T(b->how_es, b->how_en), w - 320 - 250);
        if (earned) {
            pill(c, x + w - 16, y + 8, C_GREEN, C_WHITE, T("CONSEGUIDA", "EARNED"));
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d / %d", value, b->goal);
            gfx_text_right(c, x + w - 20, y + 11, 18, P->ink, buf);
            int bx = x + w - 230;
            gfx_round_rect(c, bx, y + 17, 110, 10, 5, P->gray_lt);
            if (value > 0) gfx_round_rect(c, bx, y + 17, 10 + 100 * value / b->goal, 10, 5, C_GREEN);
        }
    }

    footer_bar(c);
    int hx = hint(c, MARGIN, "B", T("Volver al perfil", "Back to profile"));
    hint(c, hx, "+", T("Salir", "Exit"));
}
