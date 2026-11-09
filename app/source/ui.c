#include "ui.h"

#include <stdio.h>
#include <time.h>

/* paleta Greeny: verde, blanco, rojo y amarillo */
#define C_BG        GFX_RGB(0xF3, 0xF7, 0xEC)
#define C_GREEN     GFX_RGB(0x2F, 0xA6, 0x3B)
#define C_GREEN_DK  GFX_RGB(0x1D, 0x7A, 0x2A)
#define C_GREEN_LT  GFX_RGB(0xE3, 0xF4, 0xDF)
#define C_WHITE     GFX_RGB(0xFF, 0xFF, 0xFF)
#define C_RED       GFX_RGB(0xE5, 0x48, 0x4D)
#define C_YELLOW    GFX_RGB(0xF7, 0xC9, 0x1E)
#define C_YELLOW_DK GFX_RGB(0x8A, 0x65, 0x00)
#define C_INK       GFX_RGB(0x1E, 0x2A, 0x1F)
#define C_GRAY      GFX_RGB(0x6B, 0x7A, 0x6C)
#define C_GRAY_LT   GFX_RGB(0xD9, 0xE2, 0xD4)
#define C_SHADOW    GFX_RGB(0xD3, 0xDD, 0xCB)

#define HEADER_H 96
#define FOOTER_Y 660

static bool g_en;
#define T(es, en) (g_en ? (en) : (es))

void ui_set_english(bool english) { g_en = english; }

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

/* resumen a la derecha de la cabecera */
static void header_stats(gfx_canvas *c, int unlocked, int total, int pts, int pts_total) {
    char buf[64];
    snprintf(buf, sizeof(buf), T("%d de %d logros", "%d of %d achievements"), unlocked, total);
    gfx_text_right(c, UI_W - 40, 14, 26, C_WHITE, buf);
    snprintf(buf, sizeof(buf), "%d / %d pts", pts, pts_total);
    gfx_text_right(c, UI_W - 40, 46, 21, C_YELLOW, buf);
    gfx_round_rect(c, UI_W - 300, 76, 260, 10, 5, C_GREEN_DK);
    if (total > 0 && unlocked > 0) gfx_round_rect(c, UI_W - 300, 76, 10 + 250 * unlocked / total, 10, 5, C_YELLOW);
}

/* pista de boton: circulo con la letra + texto. Devuelve la x siguiente. */
static int hint(gfx_canvas *c, int x, const char *btn, const char *label) {
    int cy = FOOTER_Y + 30;
    gfx_circle(c, x + 17, cy, 17, C_INK);
    gfx_text(c, x + 17 - gfx_text_width(22, btn) / 2, cy - 14, 22, C_WHITE, btn);
    gfx_text(c, x + 44, cy - 14, 22, C_INK, label);
    return x + 44 + gfx_text_width(22, label) + 34;
}

static void footer_bar(gfx_canvas *c) {
    gfx_rect(c, 0, FOOTER_Y, UI_W, UI_H - FOOTER_Y, C_WHITE);
    gfx_rect(c, 0, FOOTER_Y, UI_W, 2, C_GRAY_LT);
    gfx_text_right(c, UI_W - 40, FOOTER_Y + 20, 18, C_GRAY, UI_APP_NAME " " UI_APP_VERSION);
}

static void progress(gfx_canvas *c, int x, int y, int w, int done, int total) {
    gfx_round_rect(c, x, y, w, 14, 7, C_GRAY_LT);
    if (total > 0 && done > 0) gfx_round_rect(c, x, y, 14 + (w - 14) * done / total, 14, 7, C_GREEN);
}

static void card(gfx_canvas *c, int x, int y, int w, int h, bool selected) {
    if (selected) {
        gfx_round_rect(c, x - 5, y - 5, w + 10, h + 10, 19, C_YELLOW);
        gfx_round_rect(c, x, y, w, h, 14, C_GREEN_LT);
    } else {
        gfx_round_rect(c, x, y + 3, w, h, 14, C_SHADOW);
        gfx_round_rect(c, x, y, w, h, 14, C_WHITE);
    }
}

/* etiqueta redondeada alineada a la derecha; devuelve su ancho */
static int pill(gfx_canvas *c, int right, int y, gfx_color bg, gfx_color fg, const char *text) {
    int w = gfx_text_width(17, text) + 26;
    gfx_round_rect(c, right - w, y, w, 28, 14, bg);
    gfx_text(c, right - w + 13, y + 4, 17, fg, text);
    return w;
}

/* ---------- inicio: lista de juegos ---------- */

static void cover_or_placeholder(gfx_canvas *c, int x, int y, const uint8_t *cover, const char *name) {
    if (cover) { gfx_image(c, x, y, cover, UI_COVER, UI_COVER); return; }
    gfx_round_rect(c, x, y, UI_COVER, UI_COVER, 12, C_GREEN);
    char letter[5] = "?";
    const unsigned char *p = (const unsigned char *)name;
    if (*p) {
        int n = *p < 0x80 ? 1 : (*p & 0xE0) == 0xC0 ? 2 : (*p & 0xF0) == 0xE0 ? 3 : 4;
        int i = 0;
        for (; i < n && p[i]; i++) letter[i] = (char)p[i];
        letter[i] = 0;
    }
    gfx_text(c, x + UI_COVER / 2 - gfx_text_width(40, letter) / 2, y + 12, 40, C_WHITE, letter);
}

void ui_home(gfx_canvas *c, const ui_assets *a, const gr_model *m, int cursor, int top) {
    gfx_clear(c, C_BG);
    header(c, a, NULL, UI_APP_NAME, NULL);
    gfx_text(c, 122 + gfx_text_width(46, UI_APP_NAME) + 14, 44, 22, C_GREEN_LT,
             T("Logros para tu Switch", "Achievements for your Switch"));
    header_stats(c, m->unlocked, m->total, m->points_got, m->points);

    if (a->mascot_big) gfx_image(c, UI_W - a->big_w - 24, FOOTER_Y - a->big_h - 6, a->mascot_big, a->big_w, a->big_h);

    const int x = 40, w = 920, h = 92;
    if (m->ngames == 0) {
        card(c, x, 150, w, 200, false);
        gfx_text(c, x + 32, 176, 30, C_INK, T("Todavía no hay juegos con logros", "No games with achievements yet"));
        gfx_text(c, x + 32, 228, 22, C_GRAY,
                 T("Copia un archivo de logros por juego en la tarjeta SD:", "Copy one achievement file per game to the SD card:"));
        gfx_text(c, x + 32, 262, 22, C_GREEN_DK, "/config/greeny/games/<TITLE ID>.txt");
        gfx_text(c, x + 32, 300, 20, C_GRAY, T("Luego pulsa Y para recargar.", "Then press Y to reload."));
    }

    for (int i = top; i < m->ngames && i < top + UI_HOME_ROWS; i++) {
        const gr_game_entry *g = &m->games[i];
        int y = 124 + (i - top) * (h + 13);
        card(c, x, y, w, h, i == cursor);
        cover_or_placeholder(c, x + 10, y + 8, a->covers[i], g->name);

        int tx = x + 104;
        gfx_text_fit(c, tx, y + 12, 30, C_INK, g->name, 430);
        if (g->broken) {
            gfx_text_fit(c, tx, y + 54, 20, C_RED,
                         T("El archivo de logros tiene un error. Pulsa A para verlo.",
                           "The achievement file has an error. Press A to see it."), 760);
            continue;
        }
        char buf[80];
        if (g->version[0])
            snprintf(buf, sizeof(buf), T("Compatible con la versión %s del juego", "Works with game version %s"), g->version);
        else snprintf(buf, sizeof(buf), "%s", T("Versión compatible sin indicar", "Supported version not specified"));
        gfx_text_fit(c, tx, y + 54, 20, C_GRAY, buf, 430);

        snprintf(buf, sizeof(buf), "%d / %d", g->unlocked, g->total);
        gfx_text_right(c, x + w - 28, y + 12, 28, g->unlocked == g->total ? C_GREEN_DK : C_INK, buf);
        progress(c, x + w - 28 - 320, y + 24, 210, g->unlocked, g->total);
        snprintf(buf, sizeof(buf), "%d / %d pts", g->points_got, g->points);
        gfx_text_right(c, x + w - 28, y + 54, 20, C_YELLOW_DK, buf);
    }

    if (m->ngames > UI_HOME_ROWS) {
        char buf[32];
        snprintf(buf, sizeof(buf), T("%d de %d", "%d of %d"), cursor + 1, m->ngames);
        gfx_text(c, x + w + 16, 124, 18, C_GRAY, buf);
    }

    footer_bar(c);
    int hx = 40;
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
    gfx_circle(c, cx, cy, 27, C_GRAY_LT);
    gfx_circle(c, cx, cy - 6, 9, C_GRAY);         /* aro del candado */
    gfx_circle(c, cx, cy - 6, 5, C_GRAY_LT);
    gfx_round_rect(c, cx - 12, cy - 4, 24, 18, 4, C_GRAY);
}

void ui_detail(gfx_canvas *c, const ui_assets *a, const gr_model *m, int game, int cursor, int top) {
    const gr_game_entry *g = &m->games[game];
    gfx_clear(c, C_BG);

    char sub[96];
    if (g->broken) snprintf(sub, sizeof(sub), "%s", T("Archivo de logros con error", "Achievement file has an error"));
    else if (g->version[0])
        snprintf(sub, sizeof(sub), T("Logros compatibles con la versión %s del juego", "Achievements for game version %s"),
                 g->version);
    else snprintf(sub, sizeof(sub), "%s", T("Versión compatible sin indicar", "Supported version not specified"));
    header(c, a, a->covers[game], g->name, sub);
    if (!g->broken) header_stats(c, g->unlocked, g->total, g->points_got, g->points);

    const int x = 40, w = UI_W - 80 - 16, h = 82;
    if (g->broken) {
        card(c, x, 150, w, 170, false);
        gfx_text(c, x + 32, 172, 28, C_RED,
                 T("El archivo de logros de este juego tiene un error", "This game's achievement file has an error"));
        gfx_text_fit(c, x + 32, 222, 22, C_INK, g->err, w - 64);
        gfx_text(c, x + 32, 264, 20, C_GRAY,
                 T("Corrígelo en la tarjeta SD y pulsa Y en la pantalla de inicio.",
                   "Fix it on the SD card and press Y on the home screen."));
    }

    const lg_game *gm = &m->cur;
    for (int i = top; !g->broken && i < gm->nach && i < top + UI_DETAIL_ROWS; i++) {
        const lg_ach *ac = &gm->ach[i];
        int y = 116 + (i - top) * (h + 8);
        card(c, x, y, w, h, i == cursor);

        int cy = y + h / 2;
        if (ac->unlocked) icon_unlocked(c, x + 48, cy);
        else icon_locked(c, x + 48, cy);

        int tx = x + 96, tw = w - 96 - 300;
        const char *desc = lg_ach_desc(ac, g_en);
        gfx_text_fit(c, tx, y + 11, 26, ac->unlocked ? C_INK : C_GRAY, lg_ach_name(ac, g_en), tw);
        gfx_text_fit(c, tx, y + 46, 20, C_GRAY, desc[0] ? desc : T("Sin descripción", "No description"), tw);

        char buf[48];
        int right = x + w - 24, status_w;
        if (ac->unlocked) {
            status_w = pill(c, right, y + 12, C_GREEN, C_WHITE, T("DESBLOQUEADO", "UNLOCKED"));
            buf[0] = 0;
            if (ac->unlocked_at) {
                time_t t = (time_t)ac->unlocked_at;
                struct tm tm;
                if (localtime_r(&t, &tm)) strftime(buf, sizeof(buf), "%d/%m/%Y  %H:%M", &tm);
            }
            gfx_text_right(c, right, y + 48, 18, C_GRAY, buf);
        } else {
            status_w = pill(c, right, y + 12, C_RED, C_WHITE, T("BLOQUEADO", "LOCKED"));
            gfx_text_right(c, right, y + 48, 18, C_GRAY, T("Aún no lo tienes", "Not earned yet"));
        }
        snprintf(buf, sizeof(buf), "%u pts", ac->points);
        pill(c, right - status_w - 10, y + 12, C_YELLOW, C_INK, buf);
    }

    /* barra de desplazamiento */
    if (!g->broken && gm->nach > UI_DETAIL_ROWS) {
        int track_y = 116, track_h = UI_DETAIL_ROWS * (h + 8) - 8;
        int bar_h = track_h * UI_DETAIL_ROWS / gm->nach;
        int bar_y = track_y + (track_h - bar_h) * top / (gm->nach - UI_DETAIL_ROWS);
        gfx_round_rect(c, UI_W - 34, track_y, 8, track_h, 4, C_GRAY_LT);
        gfx_round_rect(c, UI_W - 34, bar_y, 8, bar_h, 4, C_GREEN);
    }

    footer_bar(c);
    int hx = hint(c, 40, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir de la app", "Exit app"));
}

/* ---------- opciones ---------- */

static void option_row(gfx_canvas *c, int y, bool selected, const char *label, const char *help, const char *value,
                       bool positive) {
    const int x = 40, w = 920, h = 104;
    card(c, x, y, w, h, selected);
    gfx_text(c, x + 28, y + 16, 28, C_INK, label);
    gfx_text_fit(c, x + 28, y + 60, 19, C_GRAY, help, w - 56 - 240);
    int vw = gfx_text_width(22, value) + 44;
    gfx_round_rect(c, x + w - 28 - vw, y + 30, vw, 44, 22, positive ? C_GREEN : C_GRAY);
    gfx_text(c, x + w - 28 - vw + 22, y + 39, 22, C_WHITE, value);
}

void ui_settings(gfx_canvas *c, const ui_assets *a, const gr_config *cfg, int cursor, bool system_spanish) {
    gfx_clear(c, C_BG);
    header(c, a, NULL, T("Opciones", "Options"), T("Se guardan en la tarjeta SD", "Saved to the SD card"));
    if (a->mascot_big) gfx_image(c, UI_W - a->big_w - 24, FOOTER_Y - a->big_h - 6, a->mascot_big, a->big_w, a->big_h);

    char lang[48];
    if (cfg->lang == GR_LANG_ES) snprintf(lang, sizeof(lang), "Español");
    else if (cfg->lang == GR_LANG_EN) snprintf(lang, sizeof(lang), "English");
    else snprintf(lang, sizeof(lang), "Auto (%s)", system_spanish ? "Español" : "English");

    const char *on = T("Activado", "On"), *off = T("Desactivado", "Off");
    option_row(c, 124, cursor == UI_OPT_LANG, T("Idioma", "Language"),
               T("Idioma de la app y de los avisos de logros.", "Language for the app and achievement pop-ups."), lang, true);
    option_row(c, 244, cursor == UI_OPT_SOUND, T("Sonido de logro", "Achievement sound"),
               T("Suena al desbloquear un logro mientras juegas.", "Plays when you unlock an achievement in-game."),
               cfg->sound ? on : off, cfg->sound);
    option_row(c, 364, cursor == UI_OPT_POPUP, T("Aviso en pantalla", "On-screen pop-up"),
               T("Muestra el logro en pantalla al desbloquearlo.", "Shows the achievement on screen when unlocked."),
               cfg->popup ? on : off, cfg->popup);

    gfx_text(c, 44, 492, 19, C_GRAY,
             T("El sonido y el aviso se aplican la próxima vez que abras un juego.",
               "Sound and pop-up changes apply the next time you start a game."));

    footer_bar(c);
    int hx = hint(c, 40, "A", T("Cambiar", "Change"));
    hx = hint(c, hx, "B", T("Volver", "Back"));
    hint(c, hx, "+", T("Salir de la app", "Exit app"));
}
