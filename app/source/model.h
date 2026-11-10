/* Datos que muestra la app: lee lo mismo que escribe el sysmodule. Portable. */
#pragma once
#include <stdbool.h>

#include "engine.h"

#define GR_MAX_GAMES 64

typedef struct {
    char tid[17];
    char name[LG_NAME_LEN];
    char version[16];
    int  total, unlocked;
    int  points, points_got;
    bool broken;
    char err[96];
} gr_game_entry;

typedef struct {
    gr_game_entry games[GR_MAX_GAMES];
    int ngames;
    int total, unlocked, points, points_got; /* sumas de todos los juegos */
    int games_played, platinums;             /* juegos con algun logro / con todos */
    lg_game cur;                             /* juego abierto en el detalle */
} gr_model;

/* `root` es la carpeta de datos, ej. "sdmc:/config/logros". */
void gr_scan(gr_model *m, const char *root);
/* Carga el juego idx en m->cur. false si el archivo tiene un error. */
bool gr_open(gr_model *m, int idx, const char *root);
