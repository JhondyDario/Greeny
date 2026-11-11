/* Configuracion compartida entre la app y el sysmodule: sdmc:/config/greeny/config.ini */
#pragma once
#include <stdbool.h>

typedef enum { GR_LANG_AUTO, GR_LANG_ES, GR_LANG_EN, GR_LANG_COUNT } gr_lang;
typedef enum { GR_THEME_AUTO, GR_THEME_LIGHT, GR_THEME_DARK, GR_THEME_COUNT } gr_theme;
typedef enum { GR_VIEW_LIST, GR_VIEW_GRID, GR_VIEW_COUNT } gr_view;
typedef enum { GR_SCOPE_ACTIVE, GR_SCOPE_PLAYED, GR_SCOPE_COUNT } gr_scope;
typedef enum { GR_SHOW_ALL, GR_SHOW_PENDING, GR_SHOW_DONE, GR_SHOW_COUNT } gr_show;
typedef enum { GR_START_TROPHIES, GR_START_PROFILE, GR_START_COUNT } gr_start;

typedef struct {
    int  lang;        /* gr_lang */
    int  theme;       /* gr_theme: auto sigue el tema de la consola */
    int  games_view;  /* gr_view: como se muestran los juegos en la app */
    int  ach_view;    /* gr_view: como se muestran los logros en la app */
    bool sound;       /* sonido al desbloquear un logro */
    bool popup;       /* aviso en pantalla al desbloquear un logro */
    bool animated_bg; /* fondo animado en la app */
    bool intro_done;  /* ya se mostro la introduccion de la app */
    int  start_page;  /* gr_start: que pantalla abre la app */
    int  games_scope; /* gr_scope: solo los juegos instalados, o tambien los ya jugados */
    int  games_show;  /* gr_show: todos, por completar o completados */
    bool music;       /* musica de fondo en la app */
    char music_track[32]; /* cancion elegida (su id); "" = ninguna */
} gr_config;

void gr_config_defaults(gr_config *c);
/* Si el archivo no existe o trae valores raros, quedan los valores por defecto. */
void gr_config_load(gr_config *c, const char *path);
int  gr_config_save(const gr_config *c, const char *path); /* 0 = ok */
