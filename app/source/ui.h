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
#define UI_APP_VERSION "0.0.6"

/* opciones, agrupadas en pestañas que se cambian con L y R */
enum {
    UI_OPT_LANG, UI_OPT_THEME, UI_OPT_ANIM, UI_OPT_GAMES_VIEW, UI_OPT_ACH_VIEW, UI_OPT_SOUND, UI_OPT_POPUP, UI_OPT_DIAG,
    UI_OPT_ABOUT,
    /* pestaña Cuenta: unas u otras segun si la consola esta vinculada */
    UI_OPT_ACC_LINK, UI_OPT_ACC_LOGIN, UI_OPT_ACC_INFO, UI_OPT_ACC_SYNC, UI_OPT_ACC_UNLINK,
    UI_OPT_COUNT
};
#define UI_TABS 5
#define UI_TAB_ACCOUNT 3
int ui_tab_count(int tab, bool linked);            /* cuantas opciones tiene la pestaña */
int ui_tab_option(int tab, int row, bool linked);  /* cual UI_OPT_* esta en esa fila */

/* lo que la pestaña Cuenta necesita saber */
typedef struct {
    bool linked;
    const char *username;   /* "" si no se sabe */
    const char *caption;
    const char *sync_es, *sync_en;  /* estado de la sincronizacion, ya redactado */
} ui_account;

/* iconos de botones, en el orden en que estan guardados en buttons.bin */
enum { UI_BTN_A, UI_BTN_B, UI_BTN_X, UI_BTN_Y, UI_BTN_PLUS, UI_BTN_MINUS, UI_BTN_L, UI_BTN_R, UI_BTN_COUNT };
#define UI_BTN_SIZE 38

typedef struct {
    const uint8_t *mascot_big;   int big_w, big_h;     /* RGBA; introduccion y Acerca de */
    const uint8_t *mascot_small; int small_w, small_h; /* RGBA */
    const uint8_t *buttons;      /* UI_BTN_COUNT iconos RGBA de UI_BTN_SIZE, seguidos; NULL = dibujar circulos */
    /* caratulas RGBA en el mismo orden que model->games; NULL = no hay */
    const uint8_t *covers[GR_MAX_GAMES];      /* UI_COVER x UI_COVER */
    const uint8_t *covers_big[GR_MAX_GAMES];  /* UI_COVER_BIG x UI_COVER_BIG */
} ui_assets;

/* columnas y filas visibles de una pantalla, para que la navegacion sea la misma en lista y cuadricula */
typedef struct { int cols, rows; } ui_layout;

void ui_set_english(bool english);
void ui_set_dark(bool dark);
/* Nombre para el saludo del inicio ("Bienvenido de vuelta, ..."); NULL o "" = sin saludo. */
void ui_set_user(const char *name);
/* Imagen de fondo UI_W x UI_H en RGBA; NULL = color liso del tema. */
void ui_set_background(const uint32_t *px);

ui_layout ui_home_layout(const gr_config *cfg);
ui_layout ui_detail_layout(const gr_config *cfg);

/* Fondo de todas las pantallas. `t` son segundos transcurridos; con 0 queda quieto. */
void ui_background(gfx_canvas *c, float t);

/* Las pantallas se dibujan sobre una capa transparente: primero ui_background()
 * en el destino y luego gfx_compose() de la capa encima.
 * `top` es la primera fila visible (en filas, no en elementos). */
void ui_home(gfx_canvas *c, const ui_assets *a, const gr_model *m, const gr_config *cfg, int cursor, int top);
void ui_detail(gfx_canvas *c, const ui_assets *a, const gr_model *m, const gr_config *cfg, int game, int cursor, int top);
void ui_settings(gfx_canvas *c, const ui_assets *a, const gr_config *cfg, int tab, int row, bool system_spanish,
                 bool system_dark, const ui_account *acc);
/* Codigo de vinculacion en grande, con las instrucciones. */
void ui_link(gfx_canvas *c, const ui_assets *a, const char *code, int seconds_left);
/* Mensaje simple: un titulo y hasta dos lineas. `ok` elige el icono. */
void ui_message(gfx_canvas *c, const ui_assets *a, const char *title, const char *line1, const char *line2, bool ok);
void ui_diag(gfx_canvas *c, const ui_assets *a, const gr_diag_item *items, int count);
void ui_about(gfx_canvas *c, const ui_assets *a);

/* Introduccion de la primera vez: bienvenida, revision de la instalacion y que hacer despues. */
#define UI_INTRO_PAGES 4
void ui_intro(gfx_canvas *c, const ui_assets *a, int page, const gr_diag_item *items, int count,
              bool sysmodule_running);
