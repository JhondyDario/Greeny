#include "config.h"

#include <stdio.h>
#include <string.h>

static const char *k_lang[GR_LANG_COUNT] = { "auto", "es", "en" };
static const char *k_theme[GR_THEME_COUNT] = { "auto", "light", "dark" };
static const char *k_view[GR_VIEW_COUNT] = { "list", "grid" };

void gr_config_defaults(gr_config *c) {
    c->lang = GR_LANG_AUTO;
    c->theme = GR_THEME_AUTO;
    c->games_view = GR_VIEW_LIST;
    c->ach_view = GR_VIEW_LIST;
    c->sound = true;
    c->popup = true;
    c->animated_bg = true;
}

/* deja *out intacto si el valor no es uno de los nombres validos */
static void pick(const char *val, const char *const *names, int count, int *out) {
    for (int i = 0; i < count; i++)
        if (!strcmp(val, names[i])) *out = i;
}

static void pick_bool(const char *val, bool *out) {
    if (!strcmp(val, "0") || !strcmp(val, "1")) *out = val[0] == '1';
}

void gr_config_load(gr_config *c, const char *path) {
    gr_config_defaults(c);
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[96];
    while (fgets(line, sizeof(line), f)) {
        char key[32], val[32];
        if (line[0] == '#' || sscanf(line, " %31[^= \t] = %31s", key, val) != 2) continue;
        if (!strcmp(key, "lang")) pick(val, k_lang, GR_LANG_COUNT, &c->lang);
        else if (!strcmp(key, "theme")) pick(val, k_theme, GR_THEME_COUNT, &c->theme);
        else if (!strcmp(key, "games_view")) pick(val, k_view, GR_VIEW_COUNT, &c->games_view);
        else if (!strcmp(key, "ach_view")) pick(val, k_view, GR_VIEW_COUNT, &c->ach_view);
        else if (!strcmp(key, "sound")) pick_bool(val, &c->sound);
        else if (!strcmp(key, "popup")) pick_bool(val, &c->popup);
        else if (!strcmp(key, "animated_bg")) pick_bool(val, &c->animated_bg);
    }
    fclose(f);
}

static const char *name_of(const char *const *names, int count, int v) {
    return names[v >= 0 && v < count ? v : 0];
}

int gr_config_save(const gr_config *c, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    int ok = fprintf(f, "# Configuracion de Greeny. La app la escribe; el sysmodule la lee al abrir un juego.\n"
                        "lang = %s\ntheme = %s\ngames_view = %s\nach_view = %s\nsound = %d\npopup = %d\nanimated_bg = %d\n",
                     name_of(k_lang, GR_LANG_COUNT, c->lang), name_of(k_theme, GR_THEME_COUNT, c->theme),
                     name_of(k_view, GR_VIEW_COUNT, c->games_view), name_of(k_view, GR_VIEW_COUNT, c->ach_view),
                     c->sound ? 1 : 0, c->popup ? 1 : 0, c->animated_bg ? 1 : 0) > 0;
    if (fclose(f) != 0) ok = 0;
    return ok ? 0 : -1;
}
