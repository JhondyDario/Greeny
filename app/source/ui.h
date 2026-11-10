/* Pantallas de la app. Portable: se renderizan igual en PC para revisarlas. */
#pragma once
#include "config.h"
#include "diag.h"
#include "gfx.h"
#include "model.h"

#define UI_W 1280
#define UI_H 720
#define UI_COVER      76   /* caratula en la lista y en la cabecera */
#define UI_COVER_BIG  150  /* caratula en la cuadricula */

#define UI_APP_NAME    "Greeny"
#define UI_APP_VERSION "0.0.4"

/* filas de la pantalla de opciones */
enum {
    UI_OPT_LANG, UI_OPT_THEME, UI_OPT_ANIM, UI_OPT_GAMES_VIEW, UI_OPT_ACH_VIEW, UI_OPT_SOUND, UI_OPT_POPUP, UI_OPT_DIAG,
    UI_OPT_COUNT
};

typedef struct {
    const uint8_t *mascot_small; int small_w, small_h; /* RGBA */
    /* caratulas RGBA en el mismo orden que model->games; NULL = no hay */
    const uint8_t *covers[GR_MAX_GAMES];      /* UI_COVER x UI_COVER */
    const uint8_t *covers_big[GR_MAX_GAMES];  /* UI_COVER_BIG x UI_COVER_BIG */
} ui_assets;

/* columnas y filas visibles de una pantalla, para que la navegacion sea la misma en lista y cuadricula */
typedef struct { int cols, rows; } ui_layout;

void ui_set_english(bool english);
void ui_set_dark(bool dark);

ui_layout ui_home_layout(const gr_config *cfg);
ui_layout ui_detail_layout(const gr_config *cfg);

/* Fondo de todas las pantallas. `t` son segundos transcurridos; con 0 queda quieto. */
void ui_background(gfx_canvas *c, float t);

/* Las pantallas se dibujan sobre una capa transparente: primero ui_background()
 * en el destino y luego gfx_compose() de la capa encima.
 * `top` es la primera fila visible (en filas, no en elementos). */
void ui_home(gfx_canvas *c, const ui_assets *a, const gr_model *m, const gr_config *cfg, int cursor, int top);
void ui_detail(gfx_canvas *c, const ui_assets *a, const gr_model *m, const gr_config *cfg, int game, int cursor, int top);
void ui_settings(gfx_canvas *c, const ui_assets *a, const gr_config *cfg, int cursor, bool system_spanish,
                 bool system_dark);
void ui_diag(gfx_canvas *c, const ui_assets *a, const gr_diag_item *items, int count);
