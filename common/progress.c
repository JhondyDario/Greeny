#include "progress.h"

/* Para pasar del nivel n al n+1 hacen falta 50*n puntos: 50, 100, 150...
 * Asi los primeros niveles llegan rapido y los altos cuestan mas. */
int gr_level(int points, int *into, int *span) {
    int level = 1, left = points < 0 ? 0 : points;
    while (left >= 50 * level && level < 999) {
        left -= 50 * level;
        level++;
    }
    if (into) *into = left;
    if (span) *span = 50 * level;
    return level;
}

const gr_badge gr_badges[] = {
    { "primer_logro",   GR_BADGE_ACHIEVEMENTS, 1,    "Primer brote",       "First sprout",     "Desbloquea tu primer logro.",          "Unlock your first achievement." },
    { "logros_10",      GR_BADGE_ACHIEVEMENTS, 10,   "Coleccionista",      "Collector",        "Desbloquea 10 logros.",                "Unlock 10 achievements." },
    { "logros_50",      GR_BADGE_ACHIEVEMENTS, 50,   "Cazatrofeos",        "Trophy hunter",    "Desbloquea 50 logros.",                "Unlock 50 achievements." },
    { "logros_200",     GR_BADGE_ACHIEVEMENTS, 200,  "Leyenda",            "Legend",           "Desbloquea 200 logros.",               "Unlock 200 achievements." },
    { "primer_platino", GR_BADGE_PLATINUMS,    1,    "Primer platino",     "First platinum",   "Consigue todos los logros de un juego.", "Get every achievement in a game." },
    { "platinos_5",     GR_BADGE_PLATINUMS,    5,    "Completista",        "Completionist",    "Platina 5 juegos.",                    "Platinum 5 games." },
    { "juegos_3",       GR_BADGE_GAMES,        3,    "Explorador",         "Explorer",         "Consigue logros en 3 juegos distintos.", "Earn achievements in 3 different games." },
    { "juegos_10",      GR_BADGE_GAMES,        10,   "Trotamundos",        "Globetrotter",     "Consigue logros en 10 juegos distintos.", "Earn achievements in 10 different games." },
    { "puntos_100",     GR_BADGE_POINTS,       100,  "Cien puntos",        "One hundred",      "Llega a 100 puntos.",                  "Reach 100 points." },
    { "puntos_500",     GR_BADGE_POINTS,       500,  "Quinientos",         "Five hundred",     "Llega a 500 puntos.",                  "Reach 500 points." },
    { "puntos_2000",    GR_BADGE_POINTS,       2000, "Imparable",          "Unstoppable",      "Llega a 2000 puntos.",                 "Reach 2000 points." },
    { "likes_3",        GR_BADGE_LIKES,        3,    "Bien recibido",      "Well received",    "Recibe 3 me gusta en tus comentarios.", "Get 3 likes on your comments." },
    { "likes_15",       GR_BADGE_LIKES,        15,   "Popular",            "Popular",          "Recibe 15 me gusta en tus comentarios.", "Get 15 likes on your comments." },
    { "likes_50",       GR_BADGE_LIKES,        50,   "Voz de la comunidad", "Community voice", "Recibe 50 me gusta en tus comentarios.", "Get 50 likes on your comments." },
};
const int gr_badge_count = (int)(sizeof(gr_badges) / sizeof(gr_badges[0]));

int gr_badge_value(const gr_badge *b, const gr_stats *s) {
    switch (b->kind) {
        case GR_BADGE_ACHIEVEMENTS: return s->achievements;
        case GR_BADGE_PLATINUMS:    return s->platinums;
        case GR_BADGE_GAMES:        return s->games;
        case GR_BADGE_LIKES:        return s->likes;
        default:                    return s->points;
    }
}

bool gr_badge_earned(const gr_badge *b, const gr_stats *s) { return gr_badge_value(b, s) >= b->goal; }

int gr_badges_earned(const gr_stats *s) {
    int n = 0;
    for (int i = 0; i < gr_badge_count; i++) n += gr_badge_earned(&gr_badges[i], s);
    return n;
}
