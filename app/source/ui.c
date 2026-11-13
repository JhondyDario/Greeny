#include "ui.h"
#include "qrcodegen.h"
#include <stdlib.h>

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
static bool g_admin;
void ui_set_admin(bool admin) { g_admin = admin; }

static const ui_song *g_songs;
static int g_nsongs, g_song_busy = -1;
void ui_set_music(const ui_song *songs, int count, int busy) { g_songs = songs; g_nsongs = count; g_song_busy = busy; }
static int g_row_index; /* fila que se esta dibujando en Opciones, para su zona tactil */

static const struct { short w, h; } k_img[UI_IMG_COUNT] = {
#define UI_IMG(name, w, h) { w, h },
#include "ui_images.h"
#undef UI_IMG
};

static const struct { const char *code, *es, *en; } k_countries[UI_COUNTRY_COUNT] = {
#define UI_COUNTRY(code, es, en) { code, es, en },
#include "ui_countries.h"
#undef UI_COUNTRY
};

int ui_country_index(const char *code) {
    if (!code || !code[0]) return -1;
    for (int i = 0; i < UI_COUNTRY_COUNT; i++)
        if (!strcmp(k_countries[i].code, code)) return i;
    return -1;
}
const char *ui_country_code(int index) { return index >= 0 && index < UI_COUNTRY_COUNT ? k_countries[index].code : ""; }

static int g_ach_filter;
void ui_set_ach_filter(int filter) { g_ach_filter = filter; }
int ui_ach_visible(const lg_game *g, int filter, int *map) {
    int n = 0;
    for (int i = 0; i < g->nach; i++)
        if (filter == UI_ACH_ALL || (filter == UI_ACH_GOT) == g->ach[i].unlocked) map[n++] = i;
    return n;
}

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

/* el elemento elegido de la pantalla que se esta dibujando (ver card) */
static bool g_ext_cursor, g_sel_set;
static int g_sel_x, g_sel_y, g_sel_w, g_sel_h;
void ui_set_external_cursor(bool on) { g_ext_cursor = on; }
bool ui_selected_rect(int *x, int *y, int *w, int *h) {
    if (!g_sel_set) return false;
    *x = g_sel_x; *y = g_sel_y; *w = g_sel_w; *h = g_sel_h;
    return true;
}

/* toda pantalla empieza por aqui */
static bool g_chip_opens_profile; /* solo en el inicio, donde R abre el perfil */

static void begin(gfx_canvas *c) {
    gfx_clear_transparent(c);
    g_nzones = 0;
    g_sel_set = false;
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
static int badge_order(const gr_stats *st, int *out);
static void heart(gfx_canvas *c, int cx, int cy, int size, gfx_color col);
static void text_wrap(gfx_canvas *c, int x, int y, int size, int line_h, gfx_color col, const char *s, int max_w, int max_lines);

/* Fondo de la barra de arriba: verde oscuro a la izquierda que se aclara hacia la derecha, con
 * un brillo arriba y sombra abajo, y una linea clara al pie. Como en la web. */
static void header_fill(gfx_canvas *c) {
    static const int stops[3][3] = { { 0x0E, 0x5A, 0x2C }, { 0x25, 0xA2, 0x3C }, { 0x86, 0xD9, 0x5C } };
    for (int x = 0; x < UI_W && x < c->w; x++) {
        int t = x * 512 / (UI_W - 1), k = t < 256 ? 0 : 1, f = t < 256 ? t : t - 256;
        int base[3];
        for (int i = 0; i < 3; i++) base[i] = (stops[k][i] * (256 - f) + stops[k + 1][i] * f) >> 8;
        for (int y = 0; y < HEADER_H + 5 && y < c->h; y++) {
            int col[3];
            if (y >= HEADER_H) { /* la linea del pie: un poco mas clara que la barra */
                for (int i = 0; i < 3; i++) col[i] = base[i] + (255 - base[i]) * 22 / 100;
            } else {
                /* de +14 % de luz arriba a -12 % abajo */
                int shade = 14 - y * 26 / HEADER_H;
                for (int i = 0; i < 3; i++) col[i] = shade >= 0 ? base[i] + (255 - base[i]) * shade / 100 : base[i] * (100 + shade) / 100;
            }
            c->px[y * c->stride + x] = GFX_RGB(col[0], col[1], col[2]);
        }
    }
}

/* cabecera: caratula del juego si la hay, si no la mascota */
static void header(gfx_canvas *c, const ui_assets *a, const uint8_t *cover, const char *title, const char *sub) {
    g_buttons = a->buttons;
    g_a = a;
    header_fill(c);
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
static int g_unread = -1; /* avisos sin leer; -1 = sin cuenta (no se dibuja la campana) */
void ui_set_unread(int n) { g_unread = n; }

/* Campana dibujada a mano, con el globito rojo de avisos sin leer. */
static void bell(gfx_canvas *c, int cx, int cy, int unread) {
    gfx_circle_alpha(c, cx, cy, 24, C_WHITE, 40);
    gfx_circle(c, cx, cy - 6, 9, C_WHITE);                        /* cupula */
    gfx_round_rect(c, cx - 9, cy - 6, 18, 12, 2, C_WHITE);        /* cuerpo */
    gfx_round_rect(c, cx - 13, cy + 5, 26, 4, 2, C_WHITE);        /* borde */
    gfx_circle(c, cx, cy + 12, 3, C_WHITE);                       /* badajo */
    gfx_circle(c, cx, cy - 16, 2, C_WHITE);
    if (unread > 0) {
        char n[8];
        snprintf(n, sizeof(n), unread > 99 ? "99+" : "%d", unread);
        int w = gfx_text_width(15, n) + 12;
        if (w < 22) w = 22;
        gfx_round_rect(c, cx + 6, cy - 24, w, 22, 11, GFX_RGB(0xEF, 0x44, 0x44));
        gfx_ring(c, cx + 6, cy - 24, w, 22, 11, 2, C_WHITE, 200);
        gfx_text(c, cx + 6 + (w - gfx_text_width(15, n)) / 2, cy - 22, 15, C_WHITE, n);
    }
}

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
    if (g_unread >= 0) { /* la campana, a la izquierda del nombre: ZL o ZR abren los avisos */
        int nw = gfx_text_width(25, g_profile->name);
        if (nw < w) nw = w;
        bell(c, right - nw - 40, 48, g_unread);
    }
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
        gfx_circle(c, x + 17, cy, 17, C_WHITE);
        gfx_text(c, x + 17 - gfx_text_width(22, btn) / 2, cy - 14, 22, C_GREEN_DK, btn);
    }
    gfx_text(c, x + 46, cy - 14, 22, C_WHITE, label);
    int next = x + 46 + gfx_text_width(22, label) + 28;
    if (icon >= 0) zone(x - 8, FOOTER_Y, next - x - 4, UI_H - FOOTER_Y, UI_ZONE_BUTTON, icon);
    /* "+" es siempre la ultima pista: la version va a la derecha solo si queda sitio */
    if (icon == UI_BTN_PLUS && next + gfx_text_width(18, UI_APP_NAME " " UI_APP_VERSION) < UI_W - MARGIN)
        gfx_text_right(c, UI_W - MARGIN, FOOTER_Y + 20, 18, C_GREEN_LT, UI_APP_NAME " " UI_APP_VERSION);
    return next;
}

/* Barra de abajo: el mismo verde que la de arriba, pero al reves (claro a la izquierda,
 * oscuro a la derecha) y con la luz desde abajo, para que las dos enmarquen la pantalla. */
static void footer_bar(gfx_canvas *c) {
    static const int stops[3][3] = { { 0x7C, 0xD3, 0x58 }, { 0x25, 0xA2, 0x3C }, { 0x0E, 0x5A, 0x2C } };
    const int h = UI_H - FOOTER_Y;
    for (int x = 0; x < UI_W && x < c->w; x++) {
        int t = x * 512 / (UI_W - 1), k = t < 256 ? 0 : 1, f = t < 256 ? t : t - 256;
        int base[3];
        for (int i = 0; i < 3; i++) base[i] = (stops[k][i] * (256 - f) + stops[k + 1][i] * f) >> 8;
        for (int y = 0; y < h && FOOTER_Y + y < c->h; y++) {
            int col[3];
            if (y < 3) { for (int i = 0; i < 3; i++) col[i] = base[i] + (255 - base[i]) * 22 / 100; } /* linea clara arriba */
            else {
                int shade = -12 + (y - 3) * 24 / (h - 3); /* oscuro arriba, mas claro abajo */
                for (int i = 0; i < 3; i++) col[i] = shade >= 0 ? base[i] + (255 - base[i]) * shade / 100 : base[i] * (100 + shade) / 100;
            }
            c->px[(FOOTER_Y + y) * c->stride + x] = GFX_RGB(col[0], col[1], col[2]);
        }
    }
}

static void progress(gfx_canvas *c, int x, int y, int w, int done, int total) {
    gfx_round_rect(c, x, y, w, 14, 7, P->gray_lt);
    if (total > 0 && done > 0) gfx_round_rect(c, x, y, 14 + (w - 14) * done / total, 14, 7, C_GREEN);
}

/* Tarjeta de vidrio, como en la web: semitransparente con degradado de arriba abajo,
 * un borde fino de luz y un brillo en el canto de arriba. El fondo se ve por detras. */
static void glass(gfx_canvas *c, int x, int y, int w, int h, int r, bool selected) {
    bool dark = P == &k_dark;
    if (dark) {
        if (selected) gfx_glass(c, x, y, w, h, r, GFX_RGB(0x4C, 0x8F, 0x55), GFX_RGB(0x1F, 0x4E, 0x29), 205, 215);
        else gfx_glass(c, x, y, w, h, r, GFX_RGB(0x3A, 0x55, 0x40), GFX_RGB(0x14, 0x22, 0x18), 150, 175);
        gfx_ring(c, x, y, w, h, r, 1, C_WHITE, selected ? 70 : 34);
    } else {
        if (selected) gfx_glass(c, x, y, w, h, r, GFX_RGB(0xF4, 0xFF, 0xEE), GFX_RGB(0xC9, 0xEC, 0xC0), 240, 225);
        else gfx_glass(c, x, y, w, h, r, GFX_RGB(0xFF, 0xFF, 0xFF), GFX_RGB(0xEA, 0xF6, 0xE5), 195, 155);
        gfx_ring(c, x, y, w, h, r, 1, GFX_RGB(0x9C, 0xC9, 0x92), selected ? 150 : 110);
    }
    /* brillo en el canto de arriba */
    if (w > 2 * r + 4) gfx_glass(c, x + r, y + 1, w - 2 * r, 1, 0, C_WHITE, C_WHITE, dark ? 60 : 255, dark ? 60 : 255);
}

static void card(gfx_canvas *c, int x, int y, int w, int h, bool selected) {
    if (selected) {
        g_sel_set = true;
        g_sel_x = x - 5; g_sel_y = y - 5; g_sel_w = w + 10; g_sel_h = h + 10;
        /* con animaciones, el aro amarillo lo pone main.c y se desliza; sin ellas va fijo aqui */
        if (!g_ext_cursor) gfx_ring(c, x - 5, y - 5, w + 10, h + 10, 19, 4, C_YELLOW, 256);
    }
    glass(c, x, y, w, h, 14, selected);
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
        snprintf(buf, sizeof(buf), "%d pts  ·  %d%%", g->points_got, g->total > 0 ? g->unlocked * 100 / g->total : 0);
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

/* Progreso de un logro bloqueado, si se sabe: "40 / 99" en `buf` y la fraccion de 0 a 1000. */
static bool ach_progress(const lg_ach *ac, char *buf, size_t cap, int *permil) {
    if (ac->unlocked || ac->prog_cond < 0 || !ac->prog_known || ac->prog_goal == 0) return false;
    uint64_t v = ac->prog_value < ac->prog_goal ? ac->prog_value : ac->prog_goal;
    snprintf(buf, cap, "%llu / %llu", (unsigned long long)v, (unsigned long long)ac->prog_goal);
    *permil = (int)(v * 1000 / ac->prog_goal);
    return true;
}

static void progress_bar(gfx_canvas *c, int x, int y, int w, int h, int permil) {
    gfx_round_rect(c, x, y, w, h, h / 2, P->gray_lt);
    if (permil > 0) gfx_round_rect(c, x, y, h + (w - h) * permil / 1000, h, h / 2, C_YELLOW);
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
static void detail_list(gfx_canvas *c, const ui_assets *a, const lg_game *gm, const int *map, int n, int cursor, int top, int rows) {
    const int x = MARGIN, w = CONTENT_W - 24, h = 82;
    for (int k = top; k < n && k < top + rows; k++) {
        int i = map[k];
        const lg_ach *ac = &gm->ach[i];
        int y = 116 + (k - top) * (h + 8);
        card(c, x, y, w, h, k == cursor);
        zone(x, y, w, h, UI_ZONE_ITEM, k);
        ach_icon(c, a, i, x + 48, y + h / 2, ac->unlocked, tier_bg(ac));

        int tx = x + 96, tw = w - 96 - 220;
        const char *desc = lg_ach_desc(ac, g_en);
        gfx_text_fit(c, tx, y + 11, 26, ac->unlocked ? P->ink : P->gray, lg_ach_name(ac, g_en), tw);
        gfx_text_fit(c, tx, y + 46, 20, P->gray, desc[0] ? desc : T("Sin descripción", "No description"), tw);
        char pb[48];
        int pm;
        if (ach_progress(ac, pb, sizeof(pb), &pm)) {
            /* bloqueado con progreso: arriba los puntos y el candado, abajo cuanto falta */
            status_pills(c, a, x + w - 22, y + 26, ac);
            int pw = gfx_text_width(17, pb);
            gfx_text(c, x + w - 22 - pw, y + 51, 17, P->gray, pb);
            progress_bar(c, x + w - 22 - pw - 12 - 120, y + 57, 120, 10, pm);
        } else status_pills(c, a, x + w - 22, y + h / 2, ac);
    }
    scrollbar(c, 116, rows * (h + 8) - 8, top, rows, n);
}

static void detail_grid(gfx_canvas *c, const ui_assets *a, const lg_game *gm, const int *map, int n, int cursor, int top, ui_layout l) {
    const int w = 380, h = 168, gx = 18, gy = 10;
    for (int k = top * l.cols; k < n && k < (top + l.rows) * l.cols; k++) {
        int i = map[k];
        const lg_ach *ac = &gm->ach[i];
        int x = MARGIN + (k % l.cols) * (w + gx), y = 116 + (k / l.cols - top) * (h + gy);
        card(c, x, y, w, h, k == cursor);
        zone(x, y, w, h, UI_ZONE_ITEM, k);

        /* todas las tarjetas empiezan a la misma altura: en cuadricula se ve raro si unas bajan y otras no */
        const char *desc = lg_ach_desc(ac, g_en);
        if (!desc[0]) desc = T("Sin descripción", "No description");
        ach_icon(c, a, i, x + 46, y + 46, ac->unlocked, tier_bg(ac));
        gfx_text_fit(c, x + 94, y + 14, 23, ac->unlocked ? P->ink : P->gray, lg_ach_name(ac, g_en), w - 94 - 14);
        status_pills(c, a, x + w - 14, y + 62, ac);
        text_2lines(c, x + 18, y + 96, 19, P->gray, desc, w - 36, 26);
        char pb[48];
        int pm;
        if (ach_progress(ac, pb, sizeof(pb), &pm)) {
            int pw = gfx_text_width(16, pb);
            progress_bar(c, x + 18, y + h - 20, w - 36 - pw - 12, 9, pm);
            gfx_text(c, x + w - 18 - pw, y + h - 27, 16, P->gray, pb);
        }
    }
    scrollbar(c, 116, l.rows * (h + gy) - gy, top, l.rows, (n + l.cols - 1) / l.cols);
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
    fact(c, a, tx, fy, UI_IMG_star, T("Puntos", "Points"), buf, P->ink);
    int tier = lg_ach_tier(ac);
    fact(c, a, tx + 300, fy, tier == LG_TIER_GOLD ? UI_IMG_medal_oro : tier == LG_TIER_SILVER ? UI_IMG_medal_plata : UI_IMG_medal_bronce,
         T("Dificultad", "Difficulty"), tier_name(ac), P->ink);
    if (ac->unlocked) {
        unlock_date(ac, buf, sizeof(buf));
        fact(c, a, tx, fy + 96, UI_IMG_stat_completados, T("Lo conseguiste el", "Earned on"),
             buf[0] ? buf : T("Fecha sin registrar", "Date not recorded"), P->ink);
    } else {
        char pb[48];
        int pm;
        if (ach_progress(ac, pb, sizeof(pb), &pm)) {
            fact(c, a, tx, fy + 96, UI_IMG_stat_logros, T("Tu progreso", "Your progress"), pb, P->ink);
            progress_bar(c, tx + 70, fy + 96 + 62, 300, 12, pm);
        } else fact(c, a, tx, fy + 96, UI_IMG_stat_logros, T("Estado", "Status"), T("Aún no lo tienes", "Not earned yet"), P->gray);
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
        static int map[LG_MAX_ACH];
        int n = ui_ach_visible(&m->cur, g_ach_filter, map);
        ui_layout l = ui_detail_layout(cfg);
        if (n == 0) {
            card(c, MARGIN, 190, CONTENT_W, 300, false);
            img(c, a, g_ach_filter == UI_ACH_GOT ? UI_IMG_m_durmiendo : UI_IMG_m_celebrando, MARGIN + 30, 210);
            gfx_text(c, MARGIN + 320, 290, 34, P->ink, g_ach_filter == UI_ACH_GOT ? T("Todavía no tienes ninguno", "You have none yet")
                                                                                 : T("¡No te falta ninguno!", "Nothing left to earn!"));
            gfx_text(c, MARGIN + 320, 346, 22, P->gray, T("Pulsa X para cambiar el filtro.", "Press X to change the filter."));
        } else if (l.cols > 1) detail_grid(c, a, &m->cur, map, n, cursor, top, l);
        else detail_list(c, a, &m->cur, map, n, cursor, top, l.rows);
    }

    footer_bar(c);
    int hx = MARGIN;
    if (!g->broken && m->cur.nach > 0) hx = hint(c, hx, "A", T("Ver detalle", "View details"));
    if (!g->broken && m->cur.nach > 0)
        hx = hint(c, hx, "X", g_ach_filter == UI_ACH_GOT ? T("Viendo: obtenidos", "Showing: earned")
                              : g_ach_filter == UI_ACH_MISSING ? T("Viendo: por obtener", "Showing: not earned") : T("Viendo: todos", "Showing: all"));
    hx = hint(c, hx, "Y", T("Comentarios", "Comments"));
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
               const bool *have, const uint8_t *const *covers, int status_filter, ui_browse_state state, int cursor, int top) {
    static const char *const k_filter[UI_BROWSE_FILTERS] = { "", "sin_empezar", "prueba", "jugable", "perfecto" };
    begin(c);
    char sub[128];
    if (state == UI_BROWSE_OK && status_filter > 0)
        snprintf(sub, sizeof(sub), T("Mostrando: %s  ·  %d de %d juegos", "Showing: %s  ·  %d of %d games"), status_name(k_filter[status_filter]), nshown, total);
    else if (state == UI_BROWSE_OK) snprintf(sub, sizeof(sub), T("%d juegos con logros, de la A a la Z", "%d games with achievements, A to Z"), total);
    else snprintf(sub, sizeof(sub), "%s", T("Catálogo de Greeny", "Greeny catalogue"));
    header(c, a, NULL, T("Buscar juegos", "Find games"), sub);

    if (state != UI_BROWSE_OK || nshown == 0) {
        card(c, MARGIN, 180, CONTENT_W, 320, false);
        int mascot = state == UI_BROWSE_OFFLINE ? UI_IMG_m_triste : UI_IMG_m_buscando;
        img(c, a, mascot, MARGIN + 30, 210);
        const char *t1, *t2;
        if (state == UI_BROWSE_LOADING) { t1 = T("Buscando en el catálogo…", "Searching the catalogue…"); t2 = T("Un momento.", "One moment."); }
        else if (state == UI_BROWSE_OFFLINE) {
            t1 = T("No hay conexión", "No connection");
            t2 = T("Conecta la consola a internet y pulsa Y para intentarlo de nuevo.", "Connect the console to the internet and press Y to try again.");
        } else if (status_filter > 0 && total > 0) {
            t1 = T("Ningún juego con ese estado", "No games with that status");
            t2 = T("Pulsa − para cambiar el filtro y ver los demás.", "Press − to change the filter and see the others.");
        } else { t1 = T("El catálogo está vacío", "The catalogue is empty"); t2 = T("Todavía no se ha publicado ningún juego.", "No games have been published yet."); }
        gfx_text(c, MARGIN + 320, 290, 36, P->ink, t1);
        gfx_text_fit(c, MARGIN + 320, 350, 23, P->gray, t2, CONTENT_W - 360);
    } else {
        const int x = MARGIN, w = CONTENT_W - 56, h = 78;
        for (int i = top; i < nshown && i < top + UI_BROWSE_ROWS; i++) {
            const gr_cat_game *g = &games[shown[i]];
            int y = 116 + (i - top) * (h + 10);
            card(c, x, y, w, h, i == cursor);
            zone(x, y, w, h, UI_ZONE_ITEM, i);
            /* primero la caratula pequeña; luego el nombre y, debajo, el estado con su icono */
            const int cs = UI_BROWSE_COVER, cx = x + 10, cy = y + (h - cs) / 2;
            if (covers && covers[shown[i]]) gfx_image(c, cx, cy, covers[shown[i]], cs, cs);
            else if (!img_fit(c, a, UI_IMG_cover_fill, cx, cy, cs)) gfx_round_rect(c, cx, cy, cs, cs, 10, P->gray_lt);
            const int tx = cx + cs + 16;
            gfx_text_fit(c, tx, y + 8, 27, P->ink, g->name, w - (tx - x) - 300);
            img_fit(c, a, status_img(g->status), tx, y + 44, 24);
            char line[96];
            if (g->version[0]) snprintf(line, sizeof(line), T("%s  ·  versión %s", "%s  ·  version %s"), status_name(g->status), g->version);
            else snprintf(line, sizeof(line), "%s", status_name(g->status));
            gfx_text_fit(c, tx + 32, y + 45, 19, P->gray, line, w - (tx - x) - 32 - 300);
            if (have[shown[i]]) pill(c, x + w - 22, y + h / 2 - 14, C_GREEN, C_WHITE, T("EN TU CONSOLA", "ON YOUR CONSOLE"));
        }

        /* el abecedario, a la derecha: resalta la letra por la que va la lista y apaga las que no tienen juegos */
        bool used[26] = { false };
        for (int i = 0; i < nshown; i++) {
            char ch = games[shown[i]].name[0];
            if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
            if (ch >= 'A' && ch <= 'Z') used[ch - 'A'] = true;
        }
        char now = games[shown[cursor]].name[0];
        if (now >= 'a' && now <= 'z') now = (char)(now - 32);
        const int ax = UI_W - MARGIN - 12;
        for (int i = 0; i < 26; i++) {
            char letter[2] = { (char)('A' + i), 0 };
            int ay = 116 + i * 20;
            bool cur = now == 'A' + i;
            if (cur) gfx_circle(c, ax, ay + 10, 12, C_GREEN);
            gfx_text(c, ax - gfx_text_width(15, letter) / 2, ay + 1, 15, cur ? C_WHITE : used[i] ? P->ink : P->gray_lt, letter);
        }
    }

    footer_bar(c);
    int hx = MARGIN;
    if (state == UI_BROWSE_OK && nshown > 0) hx = hint(c, hx, "A", T("Ver logros", "View achievements"));
    if (state == UI_BROWSE_OK) {
        char f[64];
        snprintf(f, sizeof(f), T("Filtro: %s", "Filter: %s"), status_filter > 0 ? status_name(k_filter[status_filter]) : T("Todos", "All"));
        hx = hint(c, hx, "−", f);
    }
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
    int k = snprintf(sub, sizeof(sub), T("%s  ·  %d logros  ·  %d pts", "%s  ·  %d achievements  ·  %d pts"),
                     status_name(game->status), def->nach, points);
    if (game->updated[0])
        snprintf(sub + k, sizeof(sub) - (size_t)k, T("  ·  Actualizado el %.2s/%.2s/%.4s", "  ·  Updated %.2s/%.2s/%.4s"),
                 T(game->updated + 8, game->updated + 5), T(game->updated + 5, game->updated + 8), game->updated);
    header(c, a, NULL, game->name, sub);
    prepare_fill(a);
    static ui_assets bare; /* sin iconos propios: todos con el de relleno */
    bare = *a;
    memset(bare.ach_icons, 0, sizeof(bare.ach_icons));
    memset(bare.ach_icons_gray, 0, sizeof(bare.ach_icons_gray));
    static int all[LG_MAX_ACH];
    for (int i = 0; i < def->nach; i++) all[i] = i;
    detail_list(c, &bare, def, all, def->nach, cursor, top, 6);
    footer_bar(c);
    int hx = hint(c, MARGIN, "Y", T("Comentarios", "Comments"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- actualizar la app ---------- */

static int g_upd_state;
static char g_upd_label[48];
void ui_set_update(int state, const char *version) {
    g_upd_state = state;
    snprintf(g_upd_label, sizeof(g_upd_label), "%s %s", T("Nueva:", "New:"), version ? version : "");
}

void ui_update(gfx_canvas *c, const ui_assets *a, int state, const char *version, const char *notes, long done, long total) {
    begin(c);
    char sub[96];
    snprintf(sub, sizeof(sub), T("Tienes la versión %s", "You have version %s"), UI_APP_VERSION);
    header(c, a, NULL, T("Actualizar Greeny", "Update Greeny"), sub);
    card(c, MARGIN, 120, CONTENT_W, 520, false);
    int pic = state == UI_UPD_AVAILABLE ? UI_IMG_m_sorprendido : state == UI_UPD_DOWNLOADING || state == UI_UPD_CHECKING ? UI_IMG_m_buscando
            : state == UI_UPD_CURRENT ? UI_IMG_m_celebrando : UI_IMG_m_triste;
    img(c, a, pic, MARGIN + 30, 150);
    const int tx = MARGIN + 320, tw = CONTENT_W - 360;
    char line[160];
    switch (state) {
        case UI_UPD_CHECKING:
            gfx_text(c, tx, 170, 34, P->ink, T("Buscando versiones…", "Checking for updates…"));
            break;
        case UI_UPD_CURRENT:
            gfx_text(c, tx, 170, 34, P->ink, T("Ya tienes la última versión", "You have the latest version"));
            gfx_text_fit(c, tx, 226, 22, P->gray, T("Cuando salga una nueva, te avisamos al abrir la app.", "When a new one comes out, the app will tell you."), tw);
            break;
        case UI_UPD_NONE:
            gfx_text(c, tx, 170, 34, P->ink, T("Todavía no hay versiones publicadas", "No versions published yet"));
            gfx_text_fit(c, tx, 226, 22, P->gray, T("Cuando se publique la primera, se podrá instalar desde aquí.", "Once the first one is out, you can install it from here."), tw);
            break;
        case UI_UPD_OFFLINE:
            gfx_text(c, tx, 170, 34, P->ink, T("No se pudo buscar", "Could not check"));
            gfx_text_fit(c, tx, 226, 22, P->gray, T("Revisa la conexión a internet e inténtalo de nuevo.", "Check the internet connection and try again."), tw);
            break;
        case UI_UPD_AVAILABLE:
        case UI_UPD_DOWNLOADING:
            snprintf(line, sizeof(line), T("Versión %s disponible", "Version %s available"), version);
            gfx_text(c, tx, 160, 34, P->ink, line);
            if (notes && notes[0]) {
                gfx_text(c, tx, 214, 18, P->gray, T("QUÉ TRAE", "WHAT'S NEW"));
                text_wrap(c, tx, 240, 20, 29, P->ink, notes, tw, state == UI_UPD_DOWNLOADING ? 7 : 9);
            }
            if (state == UI_UPD_DOWNLOADING) {
                int permil = total > 0 ? (int)(done * 1000 / total) : 0;
                gfx_round_rect(c, tx, 548, tw, 20, 10, P->gray_lt);
                if (permil > 0) gfx_round_rect(c, tx, 548, 20 + (tw - 20) * permil / 1000, 20, 10, C_GREEN);
                snprintf(line, sizeof(line), T("Descargando…  %ld de %ld MB", "Downloading…  %ld of %ld MB"), done >> 20, (total > 0 ? total : done) >> 20);
                gfx_text(c, tx, 580, 20, P->gray, line);
            } else {
                gfx_text_fit(c, tx, 572, 19, P->gray, T("Se descarga la app nueva y reemplaza a esta. Tus logros y opciones no se tocan.",
                                                       "The new app is downloaded and replaces this one. Your achievements and options stay."), tw);
            }
            break;
        default: break;
    }
    footer_bar(c);
    int hx = MARGIN;
    if (state == UI_UPD_AVAILABLE) hx = hint(c, hx, "A", T("Descargar e instalar", "Download and install"));
    if (state == UI_UPD_OFFLINE || state == UI_UPD_CURRENT || state == UI_UPD_NONE) hx = hint(c, hx, "Y", T("Buscar otra vez", "Check again"));
    if (state != UI_UPD_DOWNLOADING) hx = hint(c, hx, "B", state == UI_UPD_AVAILABLE ? T("Ahora no", "Not now") : T("Volver", "Back"));
    if (state != UI_UPD_DOWNLOADING) hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- avisos ---------- */

void ui_notif_text(const gr_notif *n, char *out, size_t cap) {
    const char *who = n->actor_name[0] ? n->actor_name : n->actor;
    switch (n->kind) {
        case GR_NOTIF_LIKE:   snprintf(out, cap, T("A %s le gustó tu comentario en %s", "%s liked your comment on %s"), who, n->game[0] ? n->game : n->title_id); break;
        case GR_NOTIF_FOLLOW: snprintf(out, cap, T("%s empezó a seguirte", "%s started following you"), who); break;
        case GR_NOTIF_ACH:    snprintf(out, cap, T("Desbloqueaste “%s” en %s", "You unlocked “%s” in %s"),
                                       g_en && n->ach_name_en[0] ? n->ach_name_en : n->ach_name, n->game[0] ? n->game : n->title_id); break;
        case GR_NOTIF_BADGE: {
            const char *name = n->badge;
            for (int i = 0; i < gr_badge_count; i++)
                if (!strcmp(gr_badges[i].id, n->badge)) name = T(gr_badges[i].name_es, gr_badges[i].name_en);
            snprintf(out, cap, T("Nueva medalla: %s", "New badge: %s"), name);
            break;
        }
        default: out[0] = 0;
    }
}

void ui_notifs(gfx_canvas *c, const ui_assets *a, const gr_notif *n, int count, int unread, int cursor, int top, int state) {
    begin(c);
    char sub[96];
    if (unread > 0) snprintf(sub, sizeof(sub), T("%d sin leer", "%d unread"), unread);
    else snprintf(sub, sizeof(sub), "%s", T("Todo leído", "All caught up"));
    header(c, a, NULL, T("Avisos", "Notifications"), sub);

    if (state != 1 || count == 0) {
        card(c, MARGIN, 180, CONTENT_W, 320, false);
        img(c, a, state == 2 ? UI_IMG_m_triste : state == 3 ? UI_IMG_m_saludando : UI_IMG_m_durmiendo, MARGIN + 30, 210);
        const char *t1, *t2;
        if (state == 2) { t1 = T("No hay conexión", "No connection"); t2 = T("Conecta la consola a internet e inténtalo de nuevo.", "Connect the console to the internet and try again."); }
        else if (state == 3) { t1 = T("Vincula tu cuenta", "Link your account"); t2 = T("Los avisos llegan a tu cuenta. Vincúlala en Opciones → Cuenta.", "Notifications go to your account. Link it in Options → Account."); }
        else { t1 = T("No tienes avisos", "No notifications"); t2 = T("Aquí sale cuando alguien te sigue, le da me gusta a tu comentario o consigues algo nuevo.", "They show up when someone follows you, likes your comment or you earn something new."); }
        gfx_text(c, MARGIN + 320, 280, 36, P->ink, t1);
        gfx_text_fit(c, MARGIN + 320, 340, 22, P->gray, t2, CONTENT_W - 360);
    } else {
        for (int i = top; i < count && i < top + UI_NOTIF_ROWS; i++) {
            const gr_notif *v = &n[i];
            const int x = MARGIN, y = 112 + (i - top) * 76, w = CONTENT_W, h = 68;
            card(c, x, y, w, h, i == cursor);
            zone(x, y, w, h, UI_ZONE_ITEM, i);
            if (!v->read) gfx_circle(c, x + 18, y + h / 2, 6, C_GREEN);
            int ix = x + 34, iy = y + 8;
            bool drawn = false;
            if (v->kind == GR_NOTIF_FOLLOW && v->actor_avatar >= 1 && v->actor_avatar <= UI_AVATAR_COUNT)
                drawn = img_fit(c, a, UI_IMG_avatar_01 + v->actor_avatar - 1, ix, iy, 52);
            else if (v->kind == GR_NOTIF_BADGE)
                for (int k = 0; k < gr_badge_count && !drawn; k++)
                    if (!strcmp(gr_badges[k].id, v->badge)) drawn = img_fit(c, a, UI_IMG_badge_primer_logro + k, ix, iy, 52);
            if (!drawn && v->kind == GR_NOTIF_LIKE) {
                gfx_circle_alpha(c, ix + 26, iy + 26, 26, GFX_RGB(0xFF, 0x6B, 0x8A), 60);
                if (!img_fit(c, a, UI_IMG_heart_lleno, ix + 8, iy + 8, 36)) heart(c, ix + 26, iy + 27, 28, GFX_RGB(0xFF, 0x6B, 0x8A));
                drawn = true;
            }
            if (!drawn) img_fit(c, a, v->kind == GR_NOTIF_FOLLOW ? UI_IMG_stat_seguidores : UI_IMG_trophy, ix, iy, 52);
            char text[200];
            ui_notif_text(v, text, sizeof(text));
            gfx_text_fit(c, x + 102, y + 9, 22, v->read ? P->gray : P->ink, text, w - 102 - 30);
            char date[16];
            snprintf(date, sizeof(date), "%.2s/%.2s/%.4s", T(v->date + 8, v->date + 5), T(v->date + 5, v->date + 8), v->date);
            gfx_text(c, x + 102, y + 39, 17, P->gray, date);
        }
        if (count > top + UI_NOTIF_ROWS) text_center(c, UI_W / 2, 640, 16, P->gray, "▼", 40);
    }

    footer_bar(c);
    int hx = MARGIN;
    if (state == 1 && count > 0) hx = hint(c, hx, "A", T("Abrir", "Open"));
    if (state == 1 && unread > 0) hx = hint(c, hx, "X", T("Marcar todo como leído", "Mark all as read"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- comentarios de un juego ---------- */

/* Un corazon de `size` px centrado en (cx, cy), hecho con dos circulos y un triangulo. */
static void heart(gfx_canvas *c, int cx, int cy, int size, gfx_color col) {
    int r = size / 4 + 1;
    gfx_circle(c, cx - r + 1, cy - r / 2, r, col);
    gfx_circle(c, cx + r - 1, cy - r / 2, r, col);
    int top = cy - r / 2, bottom = cy + size / 2, half = 2 * r;
    for (int y = top; y <= bottom; y++) {
        int hw = half * (bottom - y) / (bottom - top);
        gfx_rect(c, cx - hw, y, hw * 2 + 1, 1, col);
    }
}

/* Escribe `s` partiendo en palabras para que quepa en `max_w`; hasta `max_lines` lineas. */
static void text_wrap(gfx_canvas *c, int x, int y, int size, int line_h, gfx_color col, const char *s, int max_w, int max_lines) {
    char line[512];
    int lines = 0;
    while (*s && lines < max_lines) {
        while (*s == ' ' || *s == '\n') s++;
        size_t best = 0, i = 0;
        for (;;) {
            size_t j = i;
            while (s[j] && s[j] != ' ' && s[j] != '\n') j++;
            if (j >= sizeof(line)) break;
            memcpy(line, s, j);
            line[j] = 0;
            if (gfx_text_width(size, line) > max_w && best) break;
            best = j;
            if (!s[j] || s[j] == '\n') break;
            i = j + 1;
        }
        if (!best) break;
        memcpy(line, s, best);
        line[best] = 0;
        bool last = ++lines == max_lines && s[best] && s[best + 1];
        if (last) gfx_text_fit(c, x, y, size, col, line, max_w - gfx_text_width(size, "…"));
        else gfx_text_fit(c, x, y, size, col, line, max_w);
        if (last) gfx_text(c, x + (gfx_text_width(size, line) < max_w - 20 ? gfx_text_width(size, line) : max_w - 20), y, size, col, "…");
        y += line_h;
        s += best;
    }
}

void ui_reviews(gfx_canvas *c, const ui_assets *a, const char *game, const gr_review *r, int n, int cursor, int top, int state, bool linked, bool closed) {
    begin(c);
    char sub[160];
    bool wrote = false;
    for (int i = 0; i < n; i++) wrote = wrote || r[i].mine;
    snprintf(sub, sizeof(sub), T("Comentarios  ·  %d  ·  cada me gusta le da 2 puntos al autor", "Comments  ·  %d  ·  each like gives the author 2 points"), n);
    header(c, a, NULL, game, sub);

    if (state != 1 || n == 0) {
        card(c, MARGIN, 180, CONTENT_W, 320, false);
        img(c, a, state == 2 ? UI_IMG_m_triste : UI_IMG_m_buscando, MARGIN + 30, 210);
        const char *t1, *t2;
        if (state == 2) { t1 = T("No hay conexión", "No connection"); t2 = T("Conecta la consola a internet e inténtalo de nuevo.", "Connect the console to the internet and try again."); }
        else if (closed) { t1 = T("Todavía no se puede comentar", "Comments are not open yet");
               t2 = T("Este juego todavía no tiene logros. Se podrá comentar cuando pase a En prueba.", "This game has no achievements yet. Comments open once it reaches Testing."); }
        else { t1 = T("Nadie ha comentado todavía", "No comments yet");
               t2 = linked ? T("Pulsa Y para escribir el primero.", "Press Y to write the first one.")
                           : T("Vincula la consola en Opciones → Cuenta para comentar.", "Link the console in Options → Account to comment."); }
        gfx_text(c, MARGIN + 320, 290, 36, P->ink, t1);
        text_wrap(c, MARGIN + 320, 350, 23, 32, P->gray, t2, CONTENT_W - 360, 2);
    } else {
        for (int i = top; i < n && i < top + UI_REVIEW_ROWS; i++) {
            const gr_review *v = &r[i];
            const int x = MARGIN, y = 116 + (i - top) * 172, w = CONTENT_W, h = 160;
            card(c, x, y, w, h, i == cursor);
            zone(x, y, w, h, UI_ZONE_ITEM, i);
            if (!(v->avatar >= 1 && v->avatar <= UI_AVATAR_COUNT && img_fit(c, a, UI_IMG_avatar_01 + v->avatar - 1, x + 20, y + 18, 64)))
                gfx_circle(c, x + 52, y + 50, 32, C_GREEN_DK);
            const char *name = v->display[0] ? v->display : v->username;
            int nw = gfx_text_width(24, name);
            if (nw > 380) nw = 380;
            gfx_text_fit(c, x + 104, y + 14, 24, P->ink, name, 380);
            int fx = x + 104 + nw + 10;
            int ci = ui_country_index(v->country);
            if (ci >= 0 && img_fit(c, a, UI_IMG_flag_do + ci, fx, y + 14, 28)) fx += 36;
            if (v->donor && img_fit(c, a, UI_IMG_badge_donador, fx, y + 12, 30)) fx += 36;
            char info[96];
            snprintf(info, sizeof(info), "@%s  ·  %d pts  ·  %.2s/%.2s/%.4s%s", v->username, v->total_points,
                     T(v->date + 8, v->date + 5), T(v->date + 5, v->date + 8), v->date, v->mine ? T("  ·  tuyo", "  ·  yours") : v->friend_ ? T("  ·  amigo", "  ·  friend") : "");
            gfx_text_fit(c, fx + 6, y + 19, 18, P->gray, info, x + w - 200 - fx);
            text_wrap(c, x + 104, y + 52, 21, 30, P->ink, v->body, w - 104 - 190, 3);
            /* me gusta */
            char likes[16];
            snprintf(likes, sizeof(likes), "%d", v->likes);
            int lw = gfx_text_width(24, likes) + 74;
            int lx = x + w - 24 - lw, ly = y + h / 2 - 24;
            gfx_round_rect(c, lx, ly, lw, 48, 24, v->liked ? C_GREEN : P->card_sel);
            if (!img_fit(c, g_a, v->liked ? UI_IMG_heart_lleno : UI_IMG_heart_vacio, lx + 14, ly + 8, 32))
                heart(c, lx + 30, ly + 24, 20, v->liked ? C_WHITE : P->gray);
            gfx_text(c, lx + 52, ly + 9, 24, v->liked ? C_WHITE : P->ink, likes);
        }
        if (n > top + UI_REVIEW_ROWS) text_center(c, UI_W / 2, 632, 18, P->gray, "▼", 40);
    }

    footer_bar(c);
    int hx = MARGIN;
    bool sel = state == 1 && n > 0 && cursor < n;
    if (sel && linked && !r[cursor].mine) hx = hint(c, hx, "A", r[cursor].liked ? T("Quitar me gusta", "Unlike") : T("Me gusta", "Like"));
    if (sel) hx = hint(c, hx, "X", T("Ver perfil", "View profile"));
    if (linked && !closed) hx = hint(c, hx, "Y", wrote ? T("Editar el tuyo", "Edit yours") : T("Escribir", "Write"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- buscador de valores ---------- */

static void search_field(gfx_canvas *c, int x, int y, int w, bool sel, const char *label, const char *value) {
    card(c, x, y, w, 76, sel);
    zone(x, y, w, 76, UI_ZONE_ITEM, (y - 116) / 88);
    gfx_text(c, x + 20, y + 10, 18, P->gray, label);
    gfx_text(c, x + 20, y + 34, 28, P->ink, value);
    if (sel) gfx_text_right(c, x + w - 18, y + 26, 22, P->accent, "◀  ▶");
}

void ui_search(gfx_canvas *c, const ui_assets *a, unsigned value, int type_bits, int step, int row,
               const char lines[][128], int nlines, bool has_plan) {
    begin(c);
    header(c, a, NULL, T("Buscador de valores", "Value finder"),
           T("Prepara el plan aquí; la búsqueda se hace dentro del juego con botones", "Set the plan here; the search runs inside the game with buttons"));

    const int lx = MARGIN, lw = 330;
    char buf[48];
    snprintf(buf, sizeof(buf), "%u", value);
    search_field(c, lx, 116, lw, row == 0, T("NÚMERO QUE TIENES AHORA", "NUMBER YOU HAVE NOW"), buf);
    snprintf(buf, sizeof(buf), type_bits == 8 ? T("u8 (0 a 255)", "u8 (0 to 255)") : type_bits == 16 ? T("u16 (0 a 65535)", "u16 (0 to 65535)")
                                                                                                     : T("u32 (lo normal)", "u32 (the usual)"));
    search_field(c, lx, 204, lw, row == 1, T("TIPO", "TYPE"), buf);
    if (step == 0) snprintf(buf, sizeof(buf), "%s", T("No sé el número", "Number unknown"));
    else snprintf(buf, sizeof(buf), "%+d", step);
    search_field(c, lx, 292, lw, row == 2, T("CADA VEZ CAMBIA EN", "EACH TIME IT CHANGES BY"), buf);

    static const char *const known[9][2] = {
        { "1. A guarda el plan. Cierra Greeny.", "1. A saves the plan. Close Greeny." },
        { "2. Abre el juego. Mantén L+R+ZL+ZR", "2. Start the game. Hold L+R+ZL+ZR" },
        { "   y pulsa ARRIBA: busca el número.", "   and press UP: searches the number." },
        { "3. Haz que cambie (coge una estrella)", "3. Make it change (grab a star)" },
        { "   y repite la combinación.", "   and repeat the combo." },
        { "4. Con pocas candidatas: la misma", "4. With few candidates: same combo" },
        { "   combinación con DERECHA (punteros).", "   with RIGHT (pointers)." },
        { "5. Cierra y abre el juego: la misma", "5. Close and reopen the game: same" },
        { "   con ABAJO comprueba cuáles sirven.", "   with DOWN checks which ones work." },
    };
    /* "No sé el número": para un traje, una montura, un nivel terminado... */
    static const char *const unknown[9][2] = {
        { "1. A guarda el plan. Cierra Greeny.", "1. A saves the plan. Close Greeny." },
        { "2. En el juego, L+R+ZL+ZR + ARRIBA", "2. In the game, L+R+ZL+ZR + UP" },
        { "   guarda una foto de la memoria.", "   saves a snapshot of memory." },
        { "3. Cambia eso (ponte un traje) y", "3. Change it (put on a suit) and" },
        { "   pulsa ARRIBA: \"cambió\".", "   press UP: \"changed\"." },
        { "4. Sin cambiarlo, muévete y pulsa", "4. Without changing it, move and" },
        { "   IZQUIERDA: \"no cambió\". Repite.", "   press LEFT: \"unchanged\". Repeat." },
        { "5. DERECHA: punteros. ABAJO: qué", "5. RIGHT: pointers. DOWN: what" },
        { "   lee cada uno. Pide 3 GB en la SD.", "   each reads. Needs 3 GB on the SD." },
    };
    /* la guia cabe en la columna de la izquierda: si una linea es larga se achica, nunca se mete en el panel */
    for (int i = 0; i < 9; i++) {
        const char *ln = step == 0 ? T(unknown[i][0], unknown[i][1]) : T(known[i][0], known[i][1]);
        int sz = 17;
        while (sz > 13 && gfx_text_width(sz, ln) > lw - 4) sz--;
        gfx_text_fit(c, lx + 4, 384 + i * 26, sz, P->gray, ln, lw - 4);
    }

    const int rx = lx + lw + 20, rw = UI_W - MARGIN - rx;
    card(c, rx, 116, rw, 524, false);
    gfx_text(c, rx + 24, 132, 22, has_plan ? P->accent : P->gray,
             has_plan ? T("Hay un plan guardado. El resultado sale aquí al volver.", "A plan is saved. The result shows here when you come back.")
                      : T("No hay plan guardado.", "No plan saved."));
    if (nlines == 0)
        gfx_text_fit(c, rx + 24, 176, 20, P->gray,
                     T("Todavía no hay resultados. El juego avisa en pantalla después de cada paso.",
                       "No results yet. The game shows a pop-up after each step."), rw - 48);
    for (int i = 0; i < nlines && i < UI_SEARCH_LINES - 1; i++)
        gfx_text_fit(c, rx + 24, 172 + i * 32, 20, P->ink, lines[i], rw - 48);

    footer_bar(c);
    int hx = hint(c, MARGIN, "A", T("Guardar plan", "Save plan"));
    hx = hint(c, hx, "X", T("Borrar plan", "Delete plan"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- opciones ---------- */

static const struct { const char *es, *en; int n; int opt[5]; } k_tabs[UI_TABS] = {
    { "General",     "General",  5, { UI_OPT_ANIM, UI_OPT_APPSOUND, UI_OPT_LANG, UI_OPT_THEME, UI_OPT_START } }, /* los de encender/apagar arriba, como en Música */
    { "Vista",       "View",     4, { UI_OPT_SCOPE, UI_OPT_SHOW, UI_OPT_GAMES_VIEW, UI_OPT_ACH_VIEW } },
    { "En el juego", "In-game",  5, { UI_OPT_INGAME, UI_OPT_SOUND, UI_OPT_POPUP, UI_OPT_STARTPOP, UI_OPT_OVERLAY } },
    { "Música",      "Music",    0, { 0 } },  /* una fila para encender y una por cancion */
    { "Cuenta",      "Account",  0, { 0 } },  /* sus filas dependen de si la consola esta vinculada */
    { "Ayuda",       "Help",     4, { UI_OPT_DIAG, UI_OPT_UPDATE, UI_OPT_ABOUT, UI_OPT_SEARCH } }, /* la ultima solo la ve un administrador */
};

int ui_tab_count(int tab, bool linked) {
    if (tab == UI_TAB_MUSIC) return 1 + g_nsongs;
    if (tab == UI_TABS - 1) return g_admin ? 4 : 3;
    if (tab == UI_TAB_ACCOUNT) return linked ? 5 : 2;
    return k_tabs[tab].n;
}

int ui_tab_option(int tab, int row, bool linked) {
    static const int with[5] = { UI_OPT_ACC_INFO, UI_OPT_ACC_PHOTO, UI_OPT_ACC_COUNTRY, UI_OPT_ACC_SYNC, UI_OPT_ACC_UNLINK };
    static const int without[2] = { UI_OPT_ACC_LINK, UI_OPT_ACC_LOGIN };
    if (tab == UI_TAB_MUSIC) return row == 0 ? UI_OPT_MUSIC : UI_OPT_SONG;
    if (tab == UI_TAB_ACCOUNT) return linked ? with[row] : without[row];
    return k_tabs[tab].opt[row];
}

/* Etiqueta de imagen (verde, amarilla, gris o roja) de cualquier ancho: las puntas tal cual
 * y el medio estirado. Si la imagen no esta, el degradado dibujado de antes. */
static bool pill_img(gfx_canvas *c, int id, int x, int y, int w) {
    static uint8_t row[2048 * 4];
    if (!g_a || !g_a->img[id] || w < 44 || w > 2048) return false;
    const int iw = k_img[id].w, ih = k_img[id].h, cap = ih / 2 + 2; /* 42 de alto: puntas de 23 */
    const uint8_t *src = g_a->img[id];
    for (int yy = 0; yy < ih; yy++) {
        const uint8_t *s = src + (size_t)yy * iw * 4;
        for (int xx = 0; xx < w; xx++) {
            int sx = xx < cap ? xx : xx >= w - cap ? iw - (w - xx) : cap + (xx - cap) * (iw - 2 * cap) / (w - 2 * cap);
            memcpy(row + xx * 4, s + sx * 4, 4);
        }
        gfx_image(c, x, y + yy, row, w, 1);
    }
    return true;
}

static int g_row_h = 88;    /* alto de las filas de Opciones */
static int g_opt_icon = -1; /* icono de la fila que se dibuja ahora (UI_IMG_opt_*), -1 = sin icono */

/* valores especiales de option_row: dibujan la palanquita en vez de texto */
static const char k_on[] = "on", k_off[] = "off";

static void option_row(gfx_canvas *c, int y, bool selected, const char *label, const char *help, const char *value,
                       gfx_color value_bg) {
    const int x = MARGIN, w = CONTENT_W, h = g_row_h, ty = (h - 88) / 2; /* con seis filas van mas bajitas */
    card(c, x, y, w, h, selected);
    zone(x, y, w, h, UI_ZONE_ITEM, g_row_index);
    int lx = x + 28;
    if (g_opt_icon >= 0 && img_fit(c, g_a, g_opt_icon, x + 22, y + (h - 48) / 2, 48)) lx = x + 88;
    gfx_text(c, lx, y + 12 + ty / 2, 27, P->ink, label);
    gfx_text_fit(c, lx, y + 51 + ty, 19, P->gray, help, w - (lx - x) - 28 - 300);
    y += ty; /* lo de la derecha, centrado en el alto de la fila */
    if (value == k_on || value == k_off) { /* encendido o apagado: la palanquita */
        if (img(c, g_a, value == k_on ? UI_IMG_toggle_on : UI_IMG_toggle_off, x + w - 28 - 92, y + 22)) return;
        value = value == k_on ? T("Activado", "On") : T("Desactivado", "Off");
    }
    int vw = gfx_text_width(21, value) + 42;
    /* el valor, como los botones de la web: degradado de su color con brillo arriba */
    int px = x + w - 28 - vw;
    /* la etiqueta de su color (imagenes de assets/etiquetas); el amarillo y el gris llevan letra oscura */
    int pid = value_bg == C_YELLOW ? UI_IMG_pill_amarilla : value_bg == C_NEUTRAL ? UI_IMG_pill_gris : value_bg == C_RED ? UI_IMG_pill_roja : UI_IMG_pill_verde;
    gfx_color fg = pid == UI_IMG_pill_amarilla || pid == UI_IMG_pill_gris ? C_ON_YELLOW : C_WHITE;
    if (!pill_img(c, pid, px, y + 23, vw)) {
        gfx_color lt = 0;
        for (int k = 0; k < 24; k += 8) { unsigned v = (value_bg >> k) & 255; lt |= (v + (255 - v) * 2 / 7) << k; }
        gfx_glass(c, px, y + 23, vw, 42, 21, lt, value_bg, 255, 255);
        gfx_glass(c, px + 21, y + 24, vw - 42, 1, 0, C_WHITE, C_WHITE, 110, 110);
        fg = C_WHITE;
    }
    gfx_text(c, px + 21, y + 31, 21, fg, value);
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

/* El icono de cada opcion (assets/opciones). Las canciones no llevan, para que se lean como lista. */
static int opt_icon(int opt, int row) {
    switch (opt) {
        case UI_OPT_LANG: return UI_IMG_opt_idioma;
        case UI_OPT_THEME: return UI_IMG_opt_tema;
        case UI_OPT_START: return UI_IMG_opt_inicio;
        case UI_OPT_ANIM: return UI_IMG_opt_fondo_animado;
        case UI_OPT_APPSOUND: return UI_IMG_opt_sonido_app;
        case UI_OPT_GAMES_VIEW: return UI_IMG_opt_vista_juegos;
        case UI_OPT_ACH_VIEW: return UI_IMG_opt_vista_logros;
        case UI_OPT_SCOPE: return UI_IMG_opt_juegos_inicio;
        case UI_OPT_SHOW: return UI_IMG_opt_mostrar;
        case UI_OPT_INGAME: return UI_IMG_opt_logros_juego;
        case UI_OPT_SOUND: return UI_IMG_opt_sonido_juego;
        case UI_OPT_POPUP: return UI_IMG_opt_avisos;
        case UI_OPT_STARTPOP: return UI_IMG_opt_aviso_inicio;
        case UI_OPT_OVERLAY: return UI_IMG_opt_ver_logros_juego;
        case UI_OPT_MUSIC: return UI_IMG_opt_musica;
        case UI_OPT_SONG: return -1;
        case UI_OPT_ACC_INFO: case UI_OPT_ACC_PHOTO: case UI_OPT_ACC_LINK: case UI_OPT_ACC_LOGIN: return UI_IMG_opt_foto;
        case UI_OPT_ACC_COUNTRY: { /* la bandera del pais que elegiste; sin pais, el icono generico */
            int ci = g_profile ? ui_country_index(g_profile->country) : -1;
            return ci >= 0 ? UI_IMG_flag_do + ci : UI_IMG_opt_pais;
        }
        case UI_OPT_ACC_SYNC: case UI_OPT_UPDATE: return UI_IMG_opt_sincronizar;
        case UI_OPT_ACC_UNLINK: return UI_IMG_opt_desvincular;
        case UI_OPT_DIAG: case UI_OPT_SEARCH: return UI_IMG_opt_diagnostico;
        case UI_OPT_ABOUT: return UI_IMG_opt_acerca;
        default: (void)row; return -1;
    }
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
        if (i == tab) {
            gfx_glass(c, tx, 116, w, 44, 22, GFX_RGB(0x5C, 0xC8, 0x5A), GFX_RGB(0x1D, 0x7A, 0x2A), 255, 255);
            gfx_glass(c, tx + 22, 117, w - 44, 1, 0, C_WHITE, C_WHITE, 120, 120);
        } else glass(c, tx, 116, w, 44, 22, false);
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
        g_row_h = count >= 6 ? 74 : 88;
        int y = count >= 6 ? 172 + i * 80 : count >= 5 ? 176 + i * 94 : 184 + i * 104; /* con cinco o seis filas van mas juntas */
        bool sel = i == row;
        g_row_index = i;
        g_opt_icon = opt_icon(ui_tab_option(tab, i, acc->linked), i);
        switch (ui_tab_option(tab, i, acc->linked)) {
            case UI_OPT_MUSIC:
                option_row(c, y, sel, T("Música de fondo", "Background music"),
                           T("Al terminar una canción sigue la siguiente. Pulsa X para descargarlas todas.",
                             "When a song ends the next one follows. Press X to download them all."),
                           cfg->music ? on : off, C_GREEN);
                break;
            case UI_OPT_SONG: {
                const ui_song *sg = &g_songs[i - 1];
                bool current = !strcmp(cfg->music_track, sg->id);
                char help[96];
                if (g_song_busy == i) snprintf(help, sizeof(help), "%s", T("Descargando, un momento…", "Downloading, one moment…"));
                else if (sg->have) snprintf(help, sizeof(help), "%s", current ? T("Es la que suena ahora. Y la borra de la SD.", "This one is playing now. Y removes it from the SD card.")
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
                option_row(c, y, sel, who, acc->caption[0] ? acc->caption : T("Sin frase. Pulsa A para elegir una.", "No caption. Press A to pick one."),
                           T("Cambiar frase", "Change caption"), C_GREEN);
                break;
            case UI_OPT_APPSOUND:
                option_row(c, y, sel, T("Sonidos de la app", "App sounds"),
                           T("Los sonidos al moverte y pulsar botones. La música va aparte.", "Sounds when you move and press buttons. Music is separate."),
                           cfg->app_sound ? on : off, C_GREEN);
                break;
            case UI_OPT_ACC_PHOTO:
                option_row(c, y, sel, T("Foto de perfil", "Profile picture"),
                           T("Elige una de las fotos de Greeny.", "Pick one of Greeny's pictures."), T("Cambiar", "Change"), C_GREEN);
                break;
            case UI_OPT_ACC_COUNTRY: {
                int ci = ui_country_index(g_profile ? g_profile->country : "");
                option_row(c, y, sel, T("País", "Country"),
                           T("Sale como bandera en tu perfil y en el ranking.", "Shown as a flag on your profile and in the ranking."),
                           ci >= 0 ? T(k_countries[ci].es, k_countries[ci].en) : T("Sin indicar", "Not set"), ci >= 0 ? C_GREEN : C_NEUTRAL);
                break;
            }
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
            case UI_OPT_STARTPOP:
                option_row(c, y, sel, T("Aviso al abrir un juego", "Pop-up when a game starts"),
                           T("El de \"Logros activos\" arriba a la derecha. Los de logros desbloqueados siguen saliendo.",
                             "The \"Achievements on\" one at the top right. Unlock pop-ups still show."),
                           cfg->start_popup ? on : off, C_GREEN);
                break;
            case UI_OPT_OVERLAY:
                option_row(c, y, sel, T("Ver logros en el juego (Tesla)", "See achievements in-game (Tesla)"),
                           T("Opcional: hace falta tener Tesla. Ábrelo en el juego y elige Greeny.",
                             "Optional: needs Tesla. Open it in-game and pick Greeny."),
                           cfg->overlay ? on : off, C_GREEN);
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
            case UI_OPT_SEARCH:
                option_row(c, y, sel, T("Buscador de valores", "Value finder"),
                           T("Para crear logros: encuentra dónde guarda el juego un número.", "For making achievements: finds where the game keeps a number."),
                           open, C_NEUTRAL);
                break;
            case UI_OPT_UPDATE:
                option_row(c, y, sel, T("Actualizar Greeny", "Update Greeny"),
                           T("Busca una versión nueva y la instala sin pasar por la computadora.", "Finds a new version and installs it, no computer needed."),
                           g_upd_state == UI_UPD_AVAILABLE ? g_upd_label : g_upd_state == UI_UPD_CURRENT ? T("Al día", "Up to date") : T("Buscar", "Check"),
                           g_upd_state == UI_UPD_AVAILABLE ? C_YELLOW : g_upd_state == UI_UPD_CURRENT ? C_GREEN : C_NEUTRAL);
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

    if (tab == UI_TAB_ACCOUNT && count < 5)
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

static void diag_row(gfx_canvas *c, int y, int h, const gr_diag_item *it) {
    const int x = MARGIN, w = CONTENT_W;
    glass(c, x, y, w, h, 12, false);
    diag_icon(c, x + 28, y + h / 2, it->state);
    gfx_text_fit(c, x + 58, y + h / 2 - 13, 21, P->ink, g_en ? it->title_en : it->title_es, 330);
    gfx_text_fit(c, x + 400, y + h / 2 - 11, 18, it->state == GR_DIAG_FAIL ? C_RED : P->gray,
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

    /* con muchas filas se aprietan para que quepan sobre la barra de abajo */
    int step = count > 0 && (650 - 112) / count < 49 ? (650 - 112) / count : 49;
    for (int i = 0; i < count && i < GR_DIAG_MAX; i++) diag_row(c, 112 + i * step, step - 5, &items[i]);

    footer_bar(c);
    int hx = hint(c, MARGIN, "Y", T("Revisar de nuevo", "Check again"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* ---------- acerca de ---------- */

/* Codigo QR de `text` en un cuadro blanco de `size` px (para escanearlo con el celular). */
static bool qr_draw(gfx_canvas *c, int x, int y, int size, const char *text) {
    static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)], tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    if (!qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_MEDIUM, 1, 10, qrcodegen_Mask_AUTO, true)) return false;
    int n = qrcodegen_getSize(qr), cell = size / (n + 4);
    if (cell < 2) return false;
    int side = cell * (n + 4), ox = x + (size - side) / 2, oy = y + (size - side) / 2;
    gfx_round_rect(c, ox, oy, side, side, 12, C_WHITE);
    for (int r = 0; r < n; r++)
        for (int q = 0; q < n; q++)
            if (qrcodegen_getModule(qr, q, r)) gfx_rect(c, ox + (q + 2) * cell, oy + (r + 2) * cell, cell, cell, GFX_RGB(0x10, 0x2A, 0x1A));
    return true;
}

void ui_about(gfx_canvas *c, const ui_assets *a, const char *discord) {
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
    gfx_text_fit(c, tx, y + 346, 19, P->ink, T("Software libre, licencia GPLv2.", "Free software, GPLv2 license."), w - 72);
    gfx_text_fit(c, tx, y + 372, 19, P->ink,
                 T("Usa código de Atmosphère, libtesla, FreeType, stb y QR Code generator (Nayuki).",
                   "Uses code from Atmosphère, libtesla, FreeType, stb and QR Code generator (Nayuki)."), w - 72);
    gfx_text_fit(c, tx, y + 418, 18, P->gray,
                 T("Proyecto de fans. No está afiliado ni avalado por Nintendo.",
                   "Fan project. Not affiliated with or endorsed by Nintendo."), w - 72);

    int side = x + w + (UI_W - MARGIN - x - w) / 2 + 10; /* centro de la columna derecha */
    img(c, a, UI_IMG_logo, side - 150, 132);
    if (discord && discord[0]) {
        /* la comunidad: un QR para abrir la invitacion en el celular */
        const char *shown = strstr(discord, "://") ? strstr(discord, "://") + 3 : discord;
        qr_draw(c, side - 105, 300, 210, discord);
        text_center(c, side, 522, 22, P->ink, T("Únete al Discord", "Join the Discord"), 320);
        text_center(c, side, 554, 19, P->accent, shown, 320);
        text_center(c, side, 584, 16, P->gray, T("Escanéalo con el celular", "Scan it with your phone"), 320);
    } else if (a->mascot_big) gfx_image(c, side - a->big_w / 2, 320, a->mascot_big, a->big_w, a->big_h);

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
        for (int i = 0; i < shown; i++) diag_row(c, 180 + i * 52, 44, &items[i]);
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
    int ci = ui_country_index(p->country);
    if (ci >= 0) { /* nombre y bandera, centrados juntos */
        int nw = gfx_text_width(34, p->name);
        if (nw > lw - 100) nw = lw - 100;
        gfx_text_fit(c, cx - (nw + 50) / 2, ly + 214, 34, P->ink, p->name, nw);
        img(c, a, UI_IMG_flag_do + ci, cx - (nw + 50) / 2 + nw + 10, ly + 216);
    } else text_center(c, cx, ly + 214, 34, P->ink, p->name, lw - 40);
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
    /* abajo: desde cuando esta en Greeny y el codigo de amigo, separados por una linea fina */
    bool has_joined = p->linked && p->joined && strlen(p->joined) == 10;
    bool has_code = p->linked && p->friend_code && p->friend_code[0];
    if (has_joined || has_code) gfx_glass(c, lx + 40, ly + 470, lw - 80, 1, 0, P->gray, P->gray, 90, 90);
    if (has_joined) {
        static const char *const mes_es[12] = { "ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic" };
        static const char *const mes_en[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        int y4 = atoi(p->joined), m2 = atoi(p->joined + 5), d2 = atoi(p->joined + 8);
        if (m2 >= 1 && m2 <= 12) {
            char when[40];
            if (g_en) snprintf(when, sizeof(when), "%s %d, %d", mes_en[m2 - 1], d2, y4);
            else snprintf(when, sizeof(when), "%d %s %d", d2, mes_es[m2 - 1], y4);
            text_center(c, cx, ly + 486, 16, P->gray, T("EN GREENY DESDE", "MEMBER SINCE"), lw - 40);
            text_center(c, cx, ly + 506, 22, P->ink, when, lw - 40);
        }
    }
    if (has_code) {
        text_center(c, cx, ly + has_joined * 62 + 486, 16, P->gray, T("CÓDIGO DE AMIGO", "FRIEND CODE"), lw - 40);
        text_center(c, cx, ly + has_joined * 62 + 506, 24, P->accent, p->friend_code, lw - 40);
    }

    /* el nivel y, a su izquierda, los corazones (me gusta recibidos); los dos juntos y centrados */
    snprintf(buf, sizeof(buf), T("Nivel %d", "Level %d"), level);
    int pw = gfx_text_width(26, buf) + 44;
    char hearts[16] = "";
    int hw = 0;
    if (p->linked) {
        snprintf(hearts, sizeof(hearts), "%d", p->likes);
        hw = gfx_text_width(26, hearts) + 30 + 44;
    }
    int gx = cx - (pw + (hw ? hw + 12 : 0)) / 2;
    if (hw) {
        gfx_glass(c, gx, ly + 340, hw, 46, 23, GFX_RGB(0xFF, 0x8F, 0xA6), GFX_RGB(0xE5, 0x3E, 0x5C), 255, 255);
        gfx_glass(c, gx + 23, ly + 341, hw - 46, 1, 0, C_WHITE, C_WHITE, 120, 120);
        if (!img_fit(c, a, UI_IMG_heart_lleno, gx + 14, ly + 348, 30)) heart(c, gx + 29, ly + 363, 24, C_WHITE);
        gfx_text(c, gx + 52, ly + 348, 26, C_WHITE, hearts);
        gx += hw + 12;
    }
    gfx_round_rect(c, gx, ly + 340, pw, 46, 23, C_YELLOW);
    gfx_text(c, gx + 22, ly + 348, 26, C_ON_YELLOW, buf);
    gfx_round_rect(c, lx + 40, ly + 406, lw - 80, 18, 9, P->gray_lt);
    if (into > 0) gfx_round_rect(c, lx + 40, ly + 406, 18 + (lw - 98) * into / span, 18, 9, C_GREEN);
    snprintf(buf, sizeof(buf), T("%d / %d pts para el nivel %d", "%d / %d pts to level %d"), into, span, level + 1);
    {
        int tw = gfx_text_width(18, buf), sx = cx - (tw + 30) / 2;
        img_fit(c, a, UI_IMG_star, sx, ly + 431, 24);
        gfx_text(c, sx + 30, ly + 432, 18, P->gray, buf);
    }

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
    /* pegadas a los numeros; solo se dejan mas abajo si hace falta el aviso de vincular la cuenta */
    const int by = p->linked ? ly + 252 : ly + 296, bw = 3 * tw + 2 * gap;
    card(c, rx, by, bw, ly + 600 - by, false);
    snprintf(buf, sizeof(buf), T("Medallas  ·  %d de %d", "Badges  ·  %d of %d"), gr_badges_earned(&p->stats), gr_badge_count);
    gfx_text(c, rx + 24, by + 16, 26, P->ink, buf);
    const int per_row = 6, r = 40, cell = (bw - 48) / per_row, mdy = (ly + 600 - by - 304) / 2; /* medallas centradas en el alto que sobre */
    int order[32], no = badge_order(&p->stats, order);
    for (int i = 0; i < no; i++) {
        const gr_badge *b = &gr_badges[order[i]];
        int mx = rx + 24 + (i % per_row) * cell + cell / 2, my = by + 104 + mdy + (i / per_row) * 116;
        bool earned = gr_badge_earned(b, &p->stats);
        medal(c, mx, my, r, b, earned);
        text_center(c, mx, my + r + 6, 15, earned ? P->ink : P->gray, T(b->name_es, b->name_en), cell - 6);
    }
    if (p->donor) { /* en el hueco que queda al final de la segunda fila */
        int i = gr_badge_count, mx = rx + 24 + (i % per_row) * cell + cell / 2, my = by + 104 + mdy + (i / per_row) * 116;
        img_fit(c, a, UI_IMG_badge_donador, mx - r, my - r, r * 2);
        text_center(c, mx, my + r + 6, 15, P->ink, T("Donador", "Supporter"), cell - 6);
    }

    footer_bar(c);
    int hx = hint(c, MARGIN, "L", T("Mis trofeos", "My trophies"));
    hx = hint(c, hx, "X", T("Ver medallas", "View badges"));
    if (p->linked) hx = hint(c, hx, "Y", T("Seguir con código", "Follow with code"));
    if (p->linked) hx = hint(c, hx, "A", T("Ranking", "Ranking"));
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

void ui_captions(gfx_canvas *c, const ui_assets *a, const gr_caption *list, int n, int cursor, int top, const char *current) {
    begin(c);
    header(c, a, NULL, T("Frase del perfil", "Profile caption"), T("Se ve debajo de tu nombre, en la app y en la web", "Shown under your name, in the app and on the website"));
    const int x = MARGIN, w = CONTENT_W - 24, h = 78;
    for (int i = top; i <= n && i < top + UI_CAPTION_ROWS; i++) {
        int y = 116 + (i - top) * (h + 10);
        bool none = i == n;
        const char *text = none ? T("Sin frase", "No caption") : T(list[i].es, list[i].en);
        bool mine = none ? !current[0] : !strcmp(current, list[i].es);
        card(c, x, y, w, h, i == cursor);
        zone(x, y, w, h, UI_ZONE_ITEM, i);
        gfx_text_fit(c, x + 28, y + 22, 27, none ? P->gray : P->ink, text, w - 56 - 200);
        if (mine) pill(c, x + w - 22, y + h / 2 - 14, C_GREEN, C_WHITE, T("LA TUYA", "YOURS"));
    }
    scrollbar(c, 116, UI_CAPTION_ROWS * (h + 10) - 10, top, UI_CAPTION_ROWS, n + 1);
    footer_bar(c);
    int hx = hint(c, MARGIN, "A", T("Usar esta", "Use this one"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

void ui_countries(gfx_canvas *c, const ui_assets *a, int cursor, const char *current) {
    begin(c);
    header(c, a, NULL, T("País", "Country"), cursor >= 0 && cursor < UI_COUNTRY_COUNT ? T(k_countries[cursor].es, k_countries[cursor].en) : "");
    const int cw = 118, ch = 86, rows = (UI_COUNTRY_COUNT + UI_COUNTRY_COLS - 1) / UI_COUNTRY_COLS;
    const int x0 = (UI_W - UI_COUNTRY_COLS * cw) / 2, y0 = 116 + (536 - rows * ch) / 2;
    int cur = ui_country_index(current);
    for (int i = 0; i < UI_COUNTRY_COUNT; i++) {
        int x = x0 + (i % UI_COUNTRY_COLS) * cw, y = y0 + (i / UI_COUNTRY_COLS) * ch;
        if (i == cursor) gfx_round_rect(c, x + 3, y + 3, cw - 6, ch - 6, 14, C_YELLOW);
        glass(c, x + 7, y + 7, cw - 14, ch - 14, 11, i == cur);
        img(c, a, UI_IMG_flag_do + i, x + cw / 2 - 20, y + 12);
        gfx_text(c, x + cw / 2 - gfx_text_width(17, k_countries[i].code) / 2, y + 53, 17, i == cur ? P->accent : P->gray, k_countries[i].code);
        zone(x, y, cw, ch, UI_ZONE_ITEM, i);
    }
    footer_bar(c);
    int hx = hint(c, MARGIN, "A", T("Elegir", "Choose"));
    hx = hint(c, hx, "X", T("Sin país", "No country"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

static void rank_line(gfx_canvas *c, const ui_assets *a, int y, const gr_rank_row *r, bool sel, bool me) {
    const int x = MARGIN, w = CONTENT_W, h = 44;
    if (sel) gfx_round_rect(c, x - 4, y - 4, w + 8, h + 8, 14, C_YELLOW);
    glass(c, x, y, w, h, 10, me);
    char buf[64];
    snprintf(buf, sizeof(buf), "#%d", r->position);
    gfx_color pc = r->position == 1 ? GFX_RGB(0xC7, 0x9A, 0x00) : r->position == 2 ? GFX_RGB(0x7D, 0x88, 0x94)
                 : r->position == 3 ? GFX_RGB(0xB5, 0x6A, 0x2C) : P->gray;
    gfx_text(c, x + 18, y + 7, 25, pc, buf);
    if (!(r->avatar >= 1 && r->avatar <= UI_AVATAR_COUNT && img_fit(c, a, UI_IMG_avatar_01 + r->avatar - 1, x + 96, y + 3, 38)))
        gfx_circle(c, x + 115, y + 22, 18, C_GREEN_DK);
    const char *name = r->display[0] ? r->display : r->username;
    int nw = gfx_text_width(24, name);
    if (nw > 420) nw = 420;
    gfx_text_fit(c, x + 148, y + 8, 24, P->ink, name, 420);
    int ci = ui_country_index(r->country);
    if (ci >= 0) img_fit(c, a, UI_IMG_flag_do + ci, x + 148 + nw + 10, y + 7, 30);
    snprintf(buf, sizeof(buf), T("Nivel %d", "Level %d"), gr_level(r->total_points, NULL, NULL));
    gfx_text(c, x + 700, y + 11, 20, P->gray, buf);
    snprintf(buf, sizeof(buf), "%d pts", r->points);
    gfx_text_right(c, x + w - 20, y + 8, 25, P->points, buf);
}

void ui_ranking(gfx_canvas *c, const ui_assets *a, const gr_rank_row *rows, int n, const gr_rank_row *me, int total,
                const char *scope_es, const char *scope_en, const char *mode, int cursor, int state) {
    begin(c);
    char sub[200];
    int k = snprintf(sub, sizeof(sub), "%s  ·  %s", T(scope_es, scope_en), mode);
    if (state == 1 && me->position > 0)
        snprintf(sub + k, sizeof(sub) - (size_t)k, T("  ·  Tú: #%d de %d", "  ·  You: #%d of %d"), me->position, total);
    header(c, a, NULL, "Ranking", sub);

    if (state != 1 || n == 0) {
        card(c, MARGIN, 180, CONTENT_W, 320, false);
        img(c, a, state == 2 ? UI_IMG_m_triste : UI_IMG_m_buscando, MARGIN + 30, 210);
        const char *t1, *t2;
        if (state == 0) { t1 = T("Cargando el ranking…", "Loading the ranking…"); t2 = T("Un momento.", "One moment."); }
        else if (state == 2) { t1 = T("No hay conexión", "No connection"); t2 = T("Conecta la consola a internet e inténtalo de nuevo.", "Connect the console to the internet and try again."); }
        else if (state == 3) { t1 = T("Falta tu país", "Your country is missing"); t2 = T("Elígelo en Opciones → Cuenta → País.", "Pick it in Options → Account → Country."); }
        else { t1 = T("Todavía no hay nadie aquí", "Nobody here yet"); t2 = T("Nadie tiene puntos en esta clasificación.", "Nobody has points in this ranking."); }
        gfx_text(c, MARGIN + 320, 290, 36, P->ink, t1);
        gfx_text_fit(c, MARGIN + 320, 350, 23, P->gray, t2, CONTENT_W - 360);
    } else {
        bool in_top = false;
        for (int i = 0; i < n; i++) {
            bool mine = me->position > 0 && !strcmp(rows[i].username, me->username);
            in_top = in_top || mine;
            rank_line(c, a, 112 + i * 49, &rows[i], i == cursor, mine);
            zone(MARGIN, 112 + i * 49, CONTENT_W, 44, UI_ZONE_ITEM, i);
        }
        /* si no saliste entre los diez y queda sitio, tu fila va debajo; si no, tu posicion ya esta en la cabecera */
        if (me->position > 0 && !in_top && n < 10) rank_line(c, a, 112 + n * 49 + 6, me, false, true);
    }

    footer_bar(c);
    int hx = MARGIN;
    if (state == 1 && n > 0) hx = hint(c, hx, "A", T("Ver perfil", "View profile"));
    hx = hint(c, hx, "X", T("Global / País / Amigos", "Global / Country / Friends"));
    hx = hint(c, hx, "Y", T("Puntos / Juego", "Points / Game"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

void ui_user(gfx_canvas *c, const ui_assets *a, const gr_rank_row *who, bool can_follow) {
    begin(c);
    char sub[64];
    snprintf(sub, sizeof(sub), "@%s", who->username);
    const char *name = who->display[0] ? who->display : who->username;
    header(c, a, NULL, name, sub);

    const int w = 720, x = (UI_W - w) / 2, y = 136, h = 470;
    card(c, x, y, w, h, false);
    int cx = x + w / 2;
    if (!(who->avatar >= 1 && who->avatar <= UI_AVATAR_COUNT && img_fit(c, a, UI_IMG_avatar_01 + who->avatar - 1, cx - 76, y + 26, 152)))
        gfx_circle(c, cx, y + 102, 74, C_GREEN_DK);
    int ci = ui_country_index(who->country);
    int nw = gfx_text_width(36, name);
    if (nw > w - 140) nw = w - 140;
    int tx = cx - (nw + (ci >= 0 ? 50 : 0)) / 2;
    gfx_text_fit(c, tx, y + 194, 36, P->ink, name, nw);
    if (ci >= 0) img(c, a, UI_IMG_flag_do + ci, tx + nw + 10, y + 197);
    if (ci >= 0) text_center(c, cx, y + 244, 20, P->gray, T(k_countries[ci].es, k_countries[ci].en), w - 60);

    char buf[48];
    snprintf(buf, sizeof(buf), T("Nivel %d", "Level %d"), gr_level(who->total_points, NULL, NULL));
    int pw = gfx_text_width(26, buf) + 44;
    gfx_round_rect(c, cx - pw / 2, y + 282, pw, 46, 23, C_YELLOW);
    gfx_text(c, cx - pw / 2 + 22, y + 290, 26, C_ON_YELLOW, buf);

    char v[3][16];
    /* desde un comentario no hay posicion: se muestran sus me gusta (van en points) */
    if (who->position > 0) snprintf(v[0], 16, "#%d", who->position);
    else snprintf(v[0], 16, "%d", who->points);
    if (who->position < 0) { snprintf(v[0], 16, "—"); } /* desde un aviso: no se saben sus numeros */
    snprintf(v[1], 16, "%d", who->total_points);
    snprintf(v[2], 16, "%d", who->achievements);
    if (who->position < 0) for (int i = 0; i < 3; i++) snprintf(v[i], 16, "—");
    const char *labels[3] = { who->position > 0 ? T("En este ranking", "In this ranking") : T("Me gusta", "Likes"), T("Puntos", "Points"), T("Logros", "Achievements") };
    for (int i = 0; i < 3; i++) {
        int sx = x + 60 + i * 200;
        gfx_text(c, sx + 100 - gfx_text_width(36, v[i]) / 2, y + 356, 36, P->ink, v[i]);
        text_center(c, sx + 100, y + 404, 19, P->gray, labels[i], 190);
    }

    footer_bar(c);
    int hx = MARGIN;
    if (can_follow) hx = hint(c, hx, "A", T("Seguir", "Follow"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir", "Exit"));
}

/* Indices de las medallas: primero las conseguidas, luego las que faltan (cada grupo en su orden). */
static int badge_order(const gr_stats *st, int *out) {
    int n = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < gr_badge_count && n < 32; i++)
            if (gr_badge_earned(&gr_badges[i], st) == (pass == 0)) out[n++] = i;
    return n;
}

void ui_badges(gfx_canvas *c, const ui_assets *a, const ui_profile *p) {
    begin(c);
    char sub[96];
    snprintf(sub, sizeof(sub), T("%d de %d conseguidas", "%d of %d earned"), gr_badges_earned(&p->stats), gr_badge_count);
    header(c, a, NULL, T("Medallas", "Badges"), sub);

    const int x = MARGIN, w = CONTENT_W, h = 44;
    int order[32], no = badge_order(&p->stats, order);
    for (int i = 0; i < no; i++) {
        const gr_badge *b = &gr_badges[order[i]];
        int y = 112 + i * 49, value = gr_badge_value(b, &p->stats);
        bool earned = value >= b->goal;
        glass(c, x, y, w, h, 12, false);
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
