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
#define UI_APP_VERSION "0.2.1"

/* opciones, agrupadas en pestañas que se cambian con L y R */
enum {
    UI_OPT_LANG, UI_OPT_THEME, UI_OPT_ANIM, UI_OPT_GAMES_VIEW, UI_OPT_ACH_VIEW, UI_OPT_SOUND, UI_OPT_POPUP, UI_OPT_START, UI_OPT_SCOPE, UI_OPT_SHOW, UI_OPT_INGAME, UI_OPT_MUSIC, UI_OPT_SONG, UI_OPT_APPSOUND, UI_OPT_STARTPOP, UI_OPT_OVERLAY, UI_OPT_TOAST_POS, UI_OPT_TOAST_TIME, UI_OPT_SPLASH, UI_OPT_ACTIVITY,
    UI_OPT_DIAG, UI_OPT_ABOUT, UI_OPT_SEARCH, UI_OPT_UPDATE,
    /* pestaña Cuenta: unas u otras segun si la consola esta vinculada */
    UI_OPT_ACC_LINK, UI_OPT_ACC_LOGIN, UI_OPT_ACC_INFO, UI_OPT_ACC_SYNC, UI_OPT_ACC_UNLINK, UI_OPT_ACC_PHOTO, UI_OPT_ACC_COUNTRY,
    UI_OPT_NOTIFY_LIKE, UI_OPT_NOTIFY_FOLLOW, UI_OPT_NOTIFY_ACH, UI_OPT_NOTIFY_BADGE, UI_OPT_NOTIFY_FRIEND, UI_OPT_NOTIFY_RANKING, UI_OPT_NOTIFY_ALMOST, UI_OPT_NOTIFY_GAME,
    UI_OPT_COUNT
};
#define UI_TABS 7
#define UI_SETTINGS_ROWS 5 /* filas de Opciones que caben a la vez */
#define UI_RANK_ROWS 9     /* filas del ranking que caben a la vez */
#define UI_TAB_MUSIC 3
#define UI_TAB_ACCOUNT 4
#define UI_TAB_INGAME 2
#define UI_TAB_NOTIFY 5 /* que avisos recibir; Ayuda queda la ultima */
int ui_tab_count(int tab, bool linked);            /* cuantas opciones tiene la pestaña */
int ui_tab_option(int tab, int row, bool linked);  /* cual UI_OPT_* esta en esa fila */

/* lo que la pestaña Cuenta necesita saber */
typedef struct {
    bool linked;
    const char *username;   /* "" si no se sabe */
    const char *caption;
    const char *sync_es, *sync_en;  /* estado de la sincronizacion, ya redactado */
    bool notify[8];                 /* avisos encendidos, en el orden de UI_OPT_NOTIFY_* */
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
/* Con el cursor externo, la tarjeta elegida no pinta su aro amarillo: solo avisa donde esta, y quien
 * llama dibuja un aro que se desliza de un elemento a otro (main.c). */
void ui_set_external_cursor(bool on);
/* El recuadro del elemento elegido en la pantalla recien dibujada. false si no hay ninguno. */
bool ui_selected_rect(int *x, int *y, int *w, int *h);

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
    const char *joined;     /* AAAA-MM-DD en que se unio; "" o NULL si no se sabe */
    int likes;              /* me gusta recibidos en sus comentarios */
    const char *country;    /* pais, dos letras; "" = sin indicar */
    bool donor;             /* lleva la medalla de donador */
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
#include "account.h"
#include "catalog.h"
typedef enum { UI_BROWSE_LOADING, UI_BROWSE_OK, UI_BROWSE_OFFLINE } ui_browse_state;
#define UI_BROWSE_ROWS 6
#define UI_BROWSE_FILTERS 5 /* todos, sin empezar, en prueba, jugable, perfecto */
#define UI_BROWSE_COVER 62 /* caratula pequeña a la izquierda de cada juego del catalogo */
void ui_browse(gfx_canvas *c, const ui_assets *a, const gr_cat_game *games, const int *shown, int nshown, int total,
               const bool *have, const uint8_t *const *covers, int status_filter, ui_browse_state state, int cursor, int top);
/* Los logros de un juego del catalogo que no esta en la consola (solo para mirar). */
void ui_browse_game(gfx_canvas *c, const ui_assets *a, const gr_cat_game *game, const lg_game *def, int cursor, int top);

/* Buscador de valores (para quien crea logros). `row`: 0 valor, 1 tipo, 2 cuanto cambia cada vez.
 * `lines` es lo que respondio el sysmodule (result.txt), ya partido en lineas. */
#define UI_SEARCH_LINES 15
void ui_search(gfx_canvas *c, const ui_assets *a, unsigned value, int type_bits, int step, int row,
               const char lines[][128], int nlines, bool has_plan);

/* Filtro rapido de la pantalla de logros de un juego (boton X). */
typedef enum { UI_ACH_ALL, UI_ACH_GOT, UI_ACH_MISSING, UI_ACH_FILTERS } ui_ach_filter;
void ui_set_ach_filter(int filter);
/* Los logros de `g` que pasan el filtro: deja sus indices en map (LG_MAX_ACH) y devuelve cuantos son. */
int ui_ach_visible(const lg_game *g, int filter, int *map);

/* Paises: indice en la lista (scripts/paises.py) a partir del codigo de dos letras, o -1. */
int ui_country_index(const char *code);
const char *ui_country_code(int index);
#define UI_COUNTRY_COLS 10
void ui_countries(gfx_canvas *c, const ui_assets *a, int cursor, const char *current);

/* Elegir la frase del perfil. La ultima fila (indice n) es "Sin frase". */
void ui_captions(gfx_canvas *c, const ui_assets *a, const gr_caption *list, int n, int cursor, int top, const char *current);
#define UI_CAPTION_ROWS 6

/* Ranking. `state`: 0 cargando, 1 listo, 2 sin conexion, 3 falta elegir pais. */
void ui_ranking(gfx_canvas *c, const ui_assets *a, const gr_rank_row *rows, int n, const gr_rank_row *me, int total,
                const char *scope_es, const char *scope_en, const char *mode, int cursor, int state);
/* El perfil de alguien del ranking, con la opcion de seguirlo. */
void ui_user(gfx_canvas *c, const ui_assets *a, const gr_rank_row *who, bool can_follow);
/* Actualizar la app. state: UI_UPD_* ; done/total en bytes mientras se descarga. */
enum { UI_UPD_UNKNOWN, UI_UPD_CHECKING, UI_UPD_CURRENT, UI_UPD_AVAILABLE, UI_UPD_DOWNLOADING, UI_UPD_NONE, UI_UPD_OFFLINE };
void ui_set_update(int state, const char *version);  /* para el valor de la opcion en Ayuda */
/* Presentacion al abrir: el logo, y "bienvenido de vuelta" o la frase de Greeny. t = 0..1000 (avance). */
void ui_splash(gfx_canvas *c, const ui_assets *a, const char *name, bool first, int t);
void ui_update(gfx_canvas *c, const ui_assets *a, int state, const char *version, const char *notes, long done, long total);

/* Avisos. state: 1 listo, 2 sin conexion, 3 sin cuenta vinculada. */
#define UI_NOTIF_ROWS 7
void ui_notifs(gfx_canvas *c, const ui_assets *a, const gr_notif *n, int count, int unread, int cursor, int top, int state);
void ui_notif_text(const gr_notif *n, char *out, size_t cap);
/* Actividad de amigos para la franja del inicio: la lista, cual se ve ahora y la hora actual (segundos
 * desde 1970). n = 0 o apagada = sin franja (y el inicio en lista vuelve a 5 juegos). */
void ui_set_activity(const gr_activity *list, int n, int index, long long now);
/* Avisos sin leer para la campana de la cabecera; -1 = no hay cuenta (sin campana). */
void ui_set_unread(int n);

/* Comentarios de un juego. state: 0 cargando, 1 listo, 2 sin conexion. */
#define UI_REVIEW_ROWS 3
void ui_reviews(gfx_canvas *c, const ui_assets *a, const char *game, const gr_review *r, int n, int cursor, int top, int state, bool linked, bool closed);

/* Un logro al detalle: imagen grande, puntos, dificultad y fecha. */
void ui_achievement(gfx_canvas *c, const ui_assets *a, const gr_model *m, int game, int index);
void ui_settings(gfx_canvas *c, const ui_assets *a, const gr_config *cfg, int tab, int row, bool system_spanish,
                 bool system_dark, const ui_account *acc);
/* Codigo de vinculacion en grande, con las instrucciones. */
void ui_link(gfx_canvas *c, const ui_assets *a, const char *code, int seconds_left);
/* Mensaje simple: un titulo y hasta dos lineas. `ok` elige el icono. */
void ui_message(gfx_canvas *c, const ui_assets *a, const char *title, const char *line1, const char *line2, bool ok);
void ui_diag(gfx_canvas *c, const ui_assets *a, const gr_diag_item *items, int count);
/* discord: invitacion a la comunidad ("" = no se muestra); sale de links.json en la web */
void ui_about(gfx_canvas *c, const ui_assets *a, const char *discord);
/* Perfil a pantalla completa (sin la barra de arriba) y detalle de las medallas. */
void ui_profile_screen(gfx_canvas *c, const ui_assets *a, const ui_profile *p);
/* Elegir foto de perfil: cuadricula con todas; `cursor` es la que esta marcada (0..UI_AVATAR_COUNT-1). */
#define UI_AVATAR_COLS 9
void ui_avatars(gfx_canvas *c, const ui_assets *a, const ui_profile *p, int cursor);
#define UI_BADGE_ROWS 8 /* medallas que caben a la vez; el resto con arriba y abajo */
void ui_badges(gfx_canvas *c, const ui_assets *a, const ui_profile *p, int top);

/* Introduccion de la primera vez: bienvenida, revision de la instalacion y que hacer despues. */
#define UI_INTRO_PAGES 4
void ui_intro(gfx_canvas *c, const ui_assets *a, int page, const gr_diag_item *items, int count,
              bool sysmodule_running);
