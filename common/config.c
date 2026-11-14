#include "config.h"

#include <stdio.h>
#include <string.h>

static const char *k_lang[GR_LANG_COUNT] = { "auto", "es", "en" };
static const char *k_theme[GR_THEME_COUNT] = { "auto", "light", "dark" };
static const char *k_view[GR_VIEW_COUNT] = { "list", "grid" };
static const char *k_scope[GR_SCOPE_COUNT] = { "active", "played" };
static const char *k_show[GR_SHOW_COUNT] = { "all", "pending", "done" };
static const char *k_start[GR_START_COUNT] = { "trophies", "profile" };
static const char *k_tpos[GR_TOAST_POS_COUNT] = { "right", "left", "center" };
static const char *k_ttime[GR_TOAST_TIME_COUNT] = { "short", "medium", "long" };

int gr_toast_ms(int t) { return t == GR_TOAST_SHORT ? 3000 : t == GR_TOAST_LONG ? 8000 : 5000; }

void gr_config_defaults(gr_config *c) {
    c->lang = GR_LANG_AUTO;
    c->theme = GR_THEME_AUTO;
    c->games_view = GR_VIEW_LIST;
    c->ach_view = GR_VIEW_LIST;
    c->sound = true;
    c->popup = true;
    c->start_popup = true;
    c->overlay = false;
    c->splash = true;
    c->activity = true;
    c->grid_version = false;
    c->toast_pos = GR_TOAST_RIGHT;
    c->toast_time = GR_TOAST_MEDIUM;
    c->animated_bg = true;
    c->intro_done = false;
    c->start_page = GR_START_TROPHIES;
    c->games_scope = GR_SCOPE_ACTIVE;
    c->games_show = GR_SHOW_ALL;
    c->app_sound = true;
    c->music = false;
    c->music_track[0] = 0;
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
        else if (!strcmp(key, "intro_done")) pick_bool(val, &c->intro_done);
        else if (!strcmp(key, "start_page")) pick(val, k_start, GR_START_COUNT, &c->start_page);
        else if (!strcmp(key, "games_scope")) pick(val, k_scope, GR_SCOPE_COUNT, &c->games_scope);
        else if (!strcmp(key, "games_show")) pick(val, k_show, GR_SHOW_COUNT, &c->games_show);
        else if (!strcmp(key, "music")) pick_bool(val, &c->music);
        else if (!strcmp(key, "app_sound")) pick_bool(val, &c->app_sound);
        else if (!strcmp(key, "start_popup")) pick_bool(val, &c->start_popup);
        else if (!strcmp(key, "overlay")) pick_bool(val, &c->overlay);
        else if (!strcmp(key, "splash")) pick_bool(val, &c->splash);
        else if (!strcmp(key, "activity")) pick_bool(val, &c->activity);
        else if (!strcmp(key, "grid_version")) pick_bool(val, &c->grid_version);
        else if (!strcmp(key, "toast_pos")) pick(val, k_tpos, GR_TOAST_POS_COUNT, &c->toast_pos);
        else if (!strcmp(key, "toast_time")) pick(val, k_ttime, GR_TOAST_TIME_COUNT, &c->toast_time);
        else if (!strcmp(key, "music_track")) {
            /* solo letras, numeros y _: es parte del nombre de un archivo */
            bool ok = true;
            for (const char *p = val; *p; p++)
                if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) ok = false;
            if (ok) snprintf(c->music_track, sizeof(c->music_track), "%s", val);
        }
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
                        "lang = %s\ntheme = %s\ngames_view = %s\nach_view = %s\nsound = %d\npopup = %d\nanimated_bg = %d\nintro_done = %d\nstart_page = %s\ngames_scope = %s\ngames_show = %s\nmusic = %d\napp_sound = %d\n",
                     name_of(k_lang, GR_LANG_COUNT, c->lang), name_of(k_theme, GR_THEME_COUNT, c->theme),
                     name_of(k_view, GR_VIEW_COUNT, c->games_view), name_of(k_view, GR_VIEW_COUNT, c->ach_view),
                     c->sound ? 1 : 0, c->popup ? 1 : 0, c->animated_bg ? 1 : 0, c->intro_done ? 1 : 0, name_of(k_start, GR_START_COUNT, c->start_page),
                     name_of(k_scope, GR_SCOPE_COUNT, c->games_scope), name_of(k_show, GR_SHOW_COUNT, c->games_show), c->music ? 1 : 0, c->app_sound ? 1 : 0) > 0;
    if (ok) ok = fprintf(f, "activity = %d\ngrid_version = %d\n", c->activity ? 1 : 0, c->grid_version ? 1 : 0) > 0;
    if (ok) ok = fprintf(f, "splash = %d\nstart_popup = %d\noverlay = %d\ntoast_pos = %s\ntoast_time = %s\n", c->splash ? 1 : 0, c->start_popup ? 1 : 0, c->overlay ? 1 : 0,
                         name_of(k_tpos, GR_TOAST_POS_COUNT, c->toast_pos), name_of(k_ttime, GR_TOAST_TIME_COUNT, c->toast_time)) > 0;
    if (ok && c->music_track[0]) ok = fprintf(f, "music_track = %s\n", c->music_track) > 0;
    if (fclose(f) != 0) ok = 0;
    return ok ? 0 : -1;
}
