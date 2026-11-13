/* Nivel y medallas del usuario. Se calculan a partir de sus logros, asi que
 * funcionan igual sin internet. La web usa las mismas reglas (web/js/progress.js):
 * si se cambia algo aqui, hay que cambiarlo alla. */
#pragma once
#include <stdbool.h>

typedef struct {
    int games;         /* juegos con al menos un logro */
    int achievements;  /* logros desbloqueados */
    int platinums;     /* juegos con todos sus logros */
    int points;        /* puntos ganados */
    int likes;         /* me gusta recibidos en sus comentarios (solo con cuenta) */
} gr_stats;

/* Nivel segun los puntos (empieza en 1). *into = puntos ganados dentro del nivel,
 * *span = puntos que mide el nivel completo. */
int gr_level(int points, int *into, int *span);

typedef enum { GR_BADGE_ACHIEVEMENTS, GR_BADGE_PLATINUMS, GR_BADGE_GAMES, GR_BADGE_POINTS, GR_BADGE_LIKES } gr_badge_kind;

typedef struct {
    const char *id;        /* tambien es el nombre del dibujo: <id>.png */
    gr_badge_kind kind;
    int goal;
    const char *name_es, *name_en;
    const char *how_es, *how_en;
} gr_badge;

extern const gr_badge gr_badges[];
extern const int gr_badge_count;

int  gr_badge_value(const gr_badge *b, const gr_stats *s);   /* cuanto lleva el usuario hacia esa medalla */
bool gr_badge_earned(const gr_badge *b, const gr_stats *s);
int  gr_badges_earned(const gr_stats *s);
