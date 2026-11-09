/* Pantallas de la app. Portable: se renderizan igual en PC para revisarlas. */
#pragma once
#include "config.h"
#include "gfx.h"
#include "model.h"

#define UI_W 1280
#define UI_H 720
#define UI_HOME_ROWS   5
#define UI_DETAIL_ROWS 6
#define UI_COVER       76  /* lado de la caratula en pixeles */

#define UI_APP_NAME    "Greeny"
#define UI_APP_VERSION "0.3.0"

/* filas de la pantalla de opciones */
enum { UI_OPT_LANG, UI_OPT_SOUND, UI_OPT_POPUP, UI_OPT_COUNT };

typedef struct {
    const uint8_t *mascot_big;   int big_w, big_h;     /* RGBA */
    const uint8_t *mascot_small; int small_w, small_h; /* RGBA */
    /* caratulas UI_COVER x UI_COVER RGBA, en el mismo orden que model->games; NULL = no hay */
    const uint8_t *covers[GR_MAX_GAMES];
} ui_assets;

/* Idioma de la interfaz: false = español, true = ingles. */
void ui_set_english(bool english);

void ui_home(gfx_canvas *c, const ui_assets *a, const gr_model *m, int cursor, int top);
void ui_detail(gfx_canvas *c, const ui_assets *a, const gr_model *m, int game, int cursor, int top);
void ui_settings(gfx_canvas *c, const ui_assets *a, const gr_config *cfg, int cursor, bool system_spanish);
