/* Diagnostico: revisa que Greeny este bien instalado. La parte de archivos es portable. */
#pragma once
#include <stdbool.h>

#include "model.h"

#define GR_DIAG_MAX 14

typedef enum { GR_DIAG_OK, GR_DIAG_WARN, GR_DIAG_FAIL, GR_DIAG_INFO } gr_diag_state;

typedef struct {
    gr_diag_state state;
    char title_es[48], title_en[48];
    char detail_es[112], detail_en[112];
} gr_diag_item;

/* Lo que solo puede averiguar la consola; la app lo llena antes de llamar a gr_diag_run. */
typedef struct {
    const char *sd;            /* raiz de la SD, ej. "sdmc:" */
    const char *root;          /* carpeta de datos, ej. "sdmc:/config/greeny" */
    bool atmosphere;           /* corre sobre Atmosphère */
    bool sysmodule_running;    /* el sysmodule esta en ejecucion */
    bool covers_ok;            /* se pudo abrir el servicio de caratulas */
    bool applet_mode;          /* abierta desde el album (menos memoria) */
    bool prev_run_known;       /* habia registro de una ejecucion anterior de la app */
    bool prev_run_clean;       /* esa ejecucion cerro bien */
    char prev_last_step[64];   /* ultimo paso que alcanzo si no cerro bien */
} gr_diag_env;

/* Esta nx-ovlloader con el menu de Tesla (o Ultrahand)? */
bool gr_tesla_installed(const char *sd);
int gr_diag_run(gr_diag_item *out, int max, const gr_diag_env *env, const gr_model *m);
