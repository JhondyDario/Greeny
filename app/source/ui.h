/* Pantallas de la app. Portable: se renderizan igual en PC para revisarlas. */
#pragma once
#include "config.h"
#include "diag.h"
#include "gfx.h"
#include "icons.h"
#include "model.h"
#include "progress.h"

#define UI_W 1280
#define UI_H 720
#define UI_COVER      76   /* caratula en la lista y en la cabecera */
#define UI_COVER_BIG  150  /* caratula en la cuadricula */

#define UI_APP_NAME    "Greeny"
#define UI_APP_VERSION "0.0.17"

/* opciones, agrupadas en pestañas que se cambian con L y R */
enum {
    UI_OPT_LANG, UI_OPT_THEME, UI_OPT_ANIM, UI_OPT_GAMES_VIEW, UI_OPT_ACH_VIEW, UI_OPT_SOUND, UI_OPT_POPUP, UI_OPT_START, UI_OPT_SCOPE, UI_OPT_SHOW, UI_OPT_INGAME, UI_OPT_MUSIC, UI_OPT_SONG,
    UI_OPT_DIAG, UI_OPT_ABOUT, UI_OPT_SEARCH,
    /* pestaña Cuenta: unas u otras segun si la consola esta vinculada */
    UI_OPT_ACC_LINK, UI_OPT_ACC_LOGIN, UI_OPT_ACC_INFO, UI_OPT_ACC_SYNC, UI_OPT_ACC_UNLINK,
    UI_OPT_COUNT
};
#define UI_TABS 6
#define UI_TAB_MUSIC 3
#define UI_TAB_ACCOUNT 4
int ui_tab_count(int tab, bool linked);            /* cuantas opciones tiene la pestaña */
int ui_tab_option(int tab, int row, bool linked);  /* cual UI_OPT_* esta en esa fila */

/* lo que la pestaña Cuenta necesita saber */
typedef struct {
    bool linked;
    const char *username;   /* "" si no se sabe */
    const char *caption;
    const char *sync_es, *sync_en;  /* estado de la sincronizacion, ya redactado */
} ui_account;

/* Imagenes PNG que la app lleva dentro; la lista la genera scripts/assets.py. */
#define UI_IMG(name, w, h) UI_IMG_##name,
typedef enum {
#include "ui_images.h"
    UI_IMG_COUNT
} ui_img;
#undef UI_IMG

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
    /* iconos de los logros del juego abierto (GR_ICON_SIZE de lado), en el orden de model->cur.ach;
     * NULL = no hay icono guardado. El gris es para los que aun no se tienen. */
    const uint8_t *ach_icons[LG_MAX_ACH];
    const uint8_t *ach_icons_gray[LG_MAX_ACH];
    /* los mismos al doble (GR_ICON_BIG), para el logro al detalle */
    const uint8_t *ach_big[LG_MAX_ACH];
    const uint8_t *ach_big_gray[LG_MAX_ACH];
    /* imagenes fijas, RGBA en el tamaño que dice ui_images.h; NULL = no se pudo cargar */
    const uint8_t *img[UI_IMG_COUNT];
} ui_assets;

/* columnas y filas visibles de una pantalla, para que la navegacion sea la misma en lista y cuadricula */
typedef struct { int cols, rows; } ui_layout;

/* Zonas tactiles de la pantalla que se acaba de dibujar. */
typedef enum { UI_ZONE_BUTTON, UI_ZONE_ITEM, UI_ZONE_TAB } ui_zone_kind;
typedef struct {
    int x, y, w, h;
    ui_zone_kind kind;
    int value;  /* UI_BTN_* para un boton; indice para un elemento o una pestaña */
} ui_zone;
/* La zona que hay en ese punto, o NULL. */
const ui_zone *ui_zone_at(int x, int y);

/* Canciones de la musica de fondo, para la pestaña Musica de Opciones. */
typedef struct {
    const char *id, *title;
    int kb;          /* lo que pesa la descarga */
    bool have;       /* ya esta en la SD */
} ui_song;
/* `busy` = fila que se esta descargando ahora, o -1. Se guarda el puntero: debe seguir vivo. */
void ui_set_music(const ui_song *songs, int count, int busy);

/* true = la cuenta vinculada es de un administrador: Opciones muestra el buscador de valores. */
void ui_set_admin(bool admin);
/* ¿Estan encendidos los logros dentro de los juegos (existe boot2.flag)? Lo muestra Opciones. */
void ui_set_ingame(bool on);
void ui_set_english(bool english);
void ui_set_dark(bool dark);
/* Nombre para el saludo del inicio ("Bienvenido de vuelta, ..."); NULL o "" = sin saludo. */
void ui_set_user(const char *name);

/* El usuario: lo usan la barra de arriba (icono, nombre y nivel) y la pantalla de perfil. */
typedef struct {
    const char *name;       /* nombre visible, o el apodo de la consola si no hay cuenta */
    const char *username;   /* "" si la consola no esta vinculada */
    const char *caption;
    const char *friend_code; /* "" si aun no se sabe */
    int avatar;             /* foto de perfil elegida, de 1 a UI_AVATAR_COUNT; 0 = la inicial del nombre */
    bool linked;
    bool social_known;      /* false = todavia no se sabe cuantos seguidores tiene */
    int followers, following;
    gr_stats stats;
} ui_profile;
/* Deja el perfil para la barra de arriba. Se guarda el puntero: debe seguir vivo. */
void ui_set_profile(const ui_profile *p);
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
/* Buscador de juegos del catalogo. `shown` son los indices de `games` que pasan el filtro. */
#include "catalog.h"
typedef enum { UI_BROWSE_LOADING, UI_BROWSE_OK, UI_BROWSE_OFFLINE } ui_browse_state;
#define UI_BROWSE_ROWS 6
void ui_browse(gfx_canvas *c, const ui_assets *a, const gr_cat_game *games, const int *shown, int nshown, int total,
               const bool *have, const char *query, ui_browse_state state, int cursor, int top);
/* Los logros de un juego del catalogo que no esta en la consola (solo para mirar). */
void ui_browse_game(gfx_canvas *c, const ui_assets *a, const gr_cat_game *game, const lg_game *def, int cursor, int top);

/* Buscador de valores (para quien crea logros). `row`: 0 valor, 1 tipo, 2 cuanto cambia cada vez.
 * `lines` es lo que respondio el sysmodule (result.txt), ya partido en lineas. */
#define UI_SEARCH_LINES 15
void ui_search(gfx_canvas *c, const ui_assets *a, unsigned value, int type_bits, int step, int row,
               const char lines[][128], int nlines, bool has_plan);

/* Un logro al detalle: imagen grande, puntos, dificultad y fecha. */
void ui_achievement(gfx_canvas *c, const ui_assets *a, const gr_model *m, int game, int index);
void ui_settings(gfx_canvas *c, const ui_assets *a, const gr_config *cfg, int tab, int row, bool system_spanish,
                 bool system_dark, const ui_account *acc);
/* Codigo de vinculacion en grande, con las instrucciones. */
void ui_link(gfx_canvas *c, const ui_assets *a, const char *code, int seconds_left);
/* Mensaje simple: un titulo y hasta dos lineas. `ok` elige el icono. */
void ui_message(gfx_canvas *c, const ui_assets *a, const char *title, const char *line1, const char *line2, bool ok);
void ui_diag(gfx_canvas *c, const ui_assets *a, const gr_diag_item *items, int count);
void ui_about(gfx_canvas *c, const ui_assets *a);
/* Perfil a pantalla completa (sin la barra de arriba) y detalle de las medallas. */
void ui_profile_screen(gfx_canvas *c, const ui_assets *a, const ui_profile *p);
/* Elegir foto de perfil: cuadricula con todas; `cursor` es la que esta marcada (0..UI_AVATAR_COUNT-1). */
#define UI_AVATAR_COLS 9
void ui_avatars(gfx_canvas *c, const ui_assets *a, const ui_profile *p, int cursor);
void ui_badges(gfx_canvas *c, const ui_assets *a, const ui_profile *p);

/* Introduccion de la primera vez: bienvenida, revision de la instalacion y que hacer despues. */
#define UI_INTRO_PAGES 4
void ui_intro(gfx_canvas *c, const ui_assets *a, int page, const gr_diag_item *items, int count,
              bool sysmodule_running);
