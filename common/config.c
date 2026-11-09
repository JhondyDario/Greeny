#include "config.h"

#include <stdio.h>
#include <string.h>

static const char *k_lang[GR_LANG_COUNT] = { "auto", "es", "en" };

void gr_config_defaults(gr_config *c) {
    c->lang = GR_LANG_AUTO;
    c->sound = true;
    c->popup = true;
}

void gr_config_load(gr_config *c, const char *path) {
    gr_config_defaults(c);
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[96];
    while (fgets(line, sizeof(line), f)) {
        char key[32], val[32];
        if (line[0] == '#' || sscanf(line, " %31[^= \t] = %31s", key, val) != 2) continue;
        if (!strcmp(key, "lang")) {
            for (int i = 0; i < GR_LANG_COUNT; i++)
                if (!strcmp(val, k_lang[i])) c->lang = i;
        } else if (!strcmp(key, "sound")) {
            if (!strcmp(val, "0") || !strcmp(val, "1")) c->sound = val[0] == '1';
        } else if (!strcmp(key, "popup")) {
            if (!strcmp(val, "0") || !strcmp(val, "1")) c->popup = val[0] == '1';
        }
    }
    fclose(f);
}

int gr_config_save(const gr_config *c, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    int lang = c->lang >= 0 && c->lang < GR_LANG_COUNT ? c->lang : GR_LANG_AUTO;
    int ok = fprintf(f, "# Configuracion de Greeny. La app la escribe; el sysmodule la lee al abrir un juego.\n"
                        "lang = %s\nsound = %d\npopup = %d\n",
                     k_lang[lang], c->sound ? 1 : 0, c->popup ? 1 : 0) > 0;
    if (fclose(f) != 0) ok = 0;
    return ok ? 0 : -1;
}
