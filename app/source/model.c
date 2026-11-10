#include "model.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define DEFS_MAX (64 * 1024)

static bool load_game(lg_game *g, const char *root, const char *tid, char *err, size_t errlen) {
    char path[160];
    snprintf(path, sizeof(path), "%s/games/%s.txt", root, tid);
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, errlen, "no se pudo abrir el archivo"); return false; }

    char *buf = malloc(DEFS_MAX);
    if (!buf) { fclose(f); snprintf(err, errlen, "sin memoria"); return false; }
    size_t n = fread(buf, 1, DEFS_MAX - 1, f);
    bool too_big = !feof(f);
    fclose(f);
    buf[n] = 0;

    bool ok = false;
    if (too_big) snprintf(err, errlen, "archivo muy grande (max 64 KB)");
    else ok = lg_parse(g, buf, err, errlen) == 0;
    free(buf);
    if (!ok) return false;

    snprintf(path, sizeof(path), "%s/unlocked/%s.txt", root, tid);
    lg_store_load(g, path);
    return true;
}

static bool is_title_id(const char *name) {
    if (strlen(name) != 20 || strcmp(name + 16, ".txt")) return false;
    for (int i = 0; i < 16; i++) {
        char c = name[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

static void summarize(gr_game_entry *e, const lg_game *g) {
    snprintf(e->name, sizeof(e->name), "%s", g->game[0] ? g->game : e->tid);
    snprintf(e->version, sizeof(e->version), "%s", g->version);
    e->total = g->nach;
    e->unlocked = e->points = e->points_got = 0;
    for (int i = 0; i < g->nach; i++) {
        e->points += g->ach[i].points;
        if (g->ach[i].unlocked) { e->unlocked++; e->points_got += g->ach[i].points; }
    }
}

static int cmp_games(const void *a, const void *b) {
    return strcasecmp(((const gr_game_entry *)a)->name, ((const gr_game_entry *)b)->name);
}

void gr_scan(gr_model *m, const char *root) {
    m->ngames = m->total = m->unlocked = m->points = m->points_got = 0;
    m->games_played = m->platinums = m->hidden = 0;

    char path[160];
    snprintf(path, sizeof(path), "%s/games", root);
    DIR *d = opendir(path);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) && m->ngames < GR_MAX_GAMES) {
        if (!is_title_id(de->d_name)) continue;
        if (!strncmp(de->d_name, "0100000000000000", 16)) continue; /* la plantilla */

        gr_game_entry *e = &m->games[m->ngames++];
        memset(e, 0, sizeof(*e));
        memcpy(e->tid, de->d_name, 16);

        if (!load_game(&m->cur, root, e->tid, e->err, sizeof(e->err))) {
            e->broken = true;
            memcpy(e->name, e->tid, sizeof(e->tid));
            continue;
        }
        summarize(e, &m->cur);
        m->total += e->total;
        m->unlocked += e->unlocked;
        m->points += e->points;
        m->points_got += e->points_got;
        if (e->unlocked > 0) m->games_played++;
        if (e->total > 0 && e->unlocked == e->total) m->platinums++;
    }
    closedir(d);
    qsort(m->games, (size_t)m->ngames, sizeof(m->games[0]), cmp_games);
}

void gr_filter(gr_model *m, bool only_installed, int show, const char (*installed)[17], int ninstalled) {
    int kept = 0;
    for (int i = 0; i < m->ngames; i++) {
        const gr_game_entry *e = &m->games[i];
        bool keep = true;
        if (!e->broken) { /* un archivo con error se ve siempre, para que se note */
            bool done = e->total > 0 && e->unlocked == e->total;
            if (show == 1 && done) keep = false;
            if (show == 2 && !done) keep = false;
            if (keep && ninstalled > 0) {
                bool here = false;
                for (int k = 0; k < ninstalled && !here; k++) here = !strcasecmp(installed[k], e->tid);
                keep = here || (!only_installed && e->unlocked > 0);
            }
        }
        if (keep) m->games[kept++] = *e;
    }
    m->hidden = m->ngames - kept;
    m->ngames = kept;
}

bool gr_open(gr_model *m, int idx, const char *root) {
    gr_game_entry *e = &m->games[idx];
    if (!load_game(&m->cur, root, e->tid, e->err, sizeof(e->err))) {
        e->broken = true;
        m->cur.nach = 0;
        return false;
    }
    e->broken = false;
    summarize(e, &m->cur);
    return true;
}
