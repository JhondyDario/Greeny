/* Configuracion compartida entre la app y el sysmodule: sdmc:/config/greeny/config.ini */
#pragma once
#include <stdbool.h>

typedef enum { GR_LANG_AUTO, GR_LANG_ES, GR_LANG_EN, GR_LANG_COUNT } gr_lang;

typedef struct {
    int  lang;   /* gr_lang */
    bool sound;  /* sonido al desbloquear un logro */
    bool popup;  /* aviso en pantalla al desbloquear un logro */
} gr_config;

void gr_config_defaults(gr_config *c);
/* Si el archivo no existe o trae valores raros, quedan los valores por defecto. */
void gr_config_load(gr_config *c, const char *path);
int  gr_config_save(const gr_config *c, const char *path); /* 0 = ok */
