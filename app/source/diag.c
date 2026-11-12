#include "diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SYSMODULE_DIR "atmosphere/contents/4200000000010690"
#define LOG_TAIL (16 * 1024)

static bool exists(const char *fmt, ...) {
    char path[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(path, sizeof(path), fmt, ap);
    va_end(ap);
    struct stat st;
    return stat(path, &st) == 0;
}

static gr_diag_item *add(gr_diag_item *out, int *n, int max, gr_diag_state st, const char *t_es, const char *t_en) {
    static gr_diag_item spare;
    gr_diag_item *it = *n < max ? &out[(*n)++] : &spare;
    memset(it, 0, sizeof(*it));
    it->state = st;
    snprintf(it->title_es, sizeof(it->title_es), "%s", t_es);
    snprintf(it->title_en, sizeof(it->title_en), "%s", t_en);
    return it;
}

#define DETAIL(it, es, en) do { \
    snprintf((it)->detail_es, sizeof((it)->detail_es), "%s", (es)); \
    snprintf((it)->detail_en, sizeof((it)->detail_en), "%s", (en)); } while (0)

/* Lee el final del log del sysmodule. Devuelve NULL si no existe. El que llama libera. */
static char *read_log_tail(const char *root) {
    char path[192];
    snprintf(path, sizeof(path), "%s/log.txt", root);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = malloc(LOG_TAIL + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_END) == 0) {
        long size = ftell(f);
        fseek(f, size > LOG_TAIL ? size - LOG_TAIL : 0, SEEK_SET);
    }
    size_t n = fread(buf, 1, LOG_TAIL, f);
    fclose(f);
    buf[n] = 0;
    return buf;
}

/* ultima aparicion de `needle` en `hay`, o NULL */
static const char *last_of(const char *hay, const char *needle) {
    const char *last = NULL;
    for (const char *p = hay; (p = strstr(p, needle)); p++) last = p;
    return last;
}

bool gr_tesla_installed(const char *sd) {
    /* nx-ovlloader (lo que carga los overlays) y el menu de Tesla o Ultrahand */
    return exists("%s/atmosphere/contents/420000000007E51A/exefs.nsp", sd) && exists("%s/switch/.overlays/ovlmenu.ovl", sd);
}

int gr_diag_run(gr_diag_item *out, int max, const gr_diag_env *env, const gr_model *m) {
    int n = 0;
    gr_diag_item *it;

    it = add(out, &n, max, env->atmosphere ? GR_DIAG_OK : GR_DIAG_FAIL, "Atmosphère", "Atmosphère");
    if (env->atmosphere) DETAIL(it, "Detectado.", "Detected.");
    else DETAIL(it, "No se detectó. Greeny solo funciona con Atmosphère.", "Not detected. Greeny only works on Atmosphère.");

    bool nsp = exists("%s/" SYSMODULE_DIR "/exefs.nsp", env->sd);
    it = add(out, &n, max, nsp ? GR_DIAG_OK : GR_DIAG_FAIL, "Sysmodule instalado", "Sysmodule installed");
    if (nsp) DETAIL(it, "exefs.nsp está en su carpeta.", "exefs.nsp is in place.");
    else DETAIL(it, "Falta exefs.nsp. Vuelve a copiar greeny.zip a la SD.", "exefs.nsp is missing. Copy greeny.zip to the SD again.");

    bool flag = exists("%s/" SYSMODULE_DIR "/flags/boot2.flag", env->sd);
    it = add(out, &n, max, flag ? GR_DIAG_OK : GR_DIAG_FAIL, "Arranque automático", "Auto start");
    if (flag) DETAIL(it, "boot2.flag está en su carpeta.", "boot2.flag is in place.");
    else DETAIL(it, "Falta flags/boot2.flag (un archivo vacío; por MTP a veces no se copia).",
                "flags/boot2.flag is missing (an empty file; MTP sometimes skips it).");

    it = add(out, &n, max, env->sysmodule_running ? GR_DIAG_OK : GR_DIAG_FAIL, "Sysmodule en ejecución",
             "Sysmodule running");
    if (env->sysmodule_running) DETAIL(it, "Está corriendo en segundo plano.", "It is running in the background.");
    else DETAIL(it, "No está corriendo. Apaga y enciende la consola (no basta con suspender).",
                "Not running. Power the console off and on (sleep is not enough).");

    int broken = 0;
    for (int i = 0; i < m->ngames; i++) broken += m->games[i].broken;
    it = add(out, &n, max, m->ngames == 0 || broken ? GR_DIAG_WARN : GR_DIAG_OK, "Archivos de logros",
             "Achievement files");
    if (m->ngames == 0) DETAIL(it, "No hay ninguno en config/greeny/games.", "None found in config/greeny/games.");
    else if (broken) {
        snprintf(it->detail_es, sizeof(it->detail_es), "%d juego(s), %d con error. Ábrelo en la lista para ver la línea.",
                 m->ngames, broken);
        snprintf(it->detail_en, sizeof(it->detail_en), "%d game(s), %d with errors. Open it in the list to see the line.",
                 m->ngames, broken);
    } else {
        snprintf(it->detail_es, sizeof(it->detail_es), "%d juego(s), %d logros, sin errores.", m->ngames, m->total);
        snprintf(it->detail_en, sizeof(it->detail_en), "%d game(s), %d achievements, no errors.", m->ngames, m->total);
    }

    /* lo que cuenta el registro del sysmodule desde su ultimo arranque */
    char *log = read_log_tail(env->root);
    const char *session = log ? last_of(log, "iniciado") : NULL;
    if (!session) {
        it = add(out, &n, max, GR_DIAG_INFO, "Registro del sysmodule", "Sysmodule log");
        DETAIL(it, "Todavía no hay registro. Aparece cuando el sysmodule arranca.",
               "No log yet. It appears once the sysmodule starts.");
    } else {
        bool popup_bad = strstr(session, "no se pudo crear la capa") != NULL;
        it = add(out, &n, max, popup_bad ? GR_DIAG_FAIL : GR_DIAG_OK, "Aviso en pantalla", "On-screen pop-up");
        if (popup_bad) DETAIL(it, "La consola no dejó crear el aviso. Los logros se guardan igual.",
                              "The console refused the pop-up layer. Achievements are still saved.");
        else DETAIL(it, "Sin errores en el registro.", "No errors in the log.");

        bool sound_bad = strstr(session, "no se pudo abrir el audio") != NULL;
        it = add(out, &n, max, sound_bad ? GR_DIAG_WARN : GR_DIAG_OK, "Sonido de logro", "Achievement sound");
        if (sound_bad) DETAIL(it, "Una vez no se pudo abrir el audio; el logro se guardó igual y el sonido se reintenta en el próximo.",
                              "Audio could not be opened once; the achievement was still saved and sound is retried next time.");
        else DETAIL(it, "Sin errores en el registro.", "No errors in the log.");

        bool mismatch = strstr(session, "no coincide") != NULL;
        bool read_bad = strstr(session, "me rindo") != NULL;
        it = add(out, &n, max, read_bad ? GR_DIAG_FAIL : mismatch ? GR_DIAG_WARN : GR_DIAG_OK, "Lectura de juegos",
                 "Reading games");
        if (read_bad) DETAIL(it, "No se pudo leer la memoria de un juego. Mira log.txt.",
                             "Could not read a game's memory. Check log.txt.");
        else if (mismatch) DETAIL(it, "Un juego tiene otra versión que la de su archivo de logros.",
                                  "A game's version differs from its achievement file.");
        else if (strstr(session, "enganchado")) DETAIL(it, "El último juego se leyó bien.", "The last game was read fine.");
        else DETAIL(it, "Aún no se ha abierto un juego con logros.", "No game with achievements opened yet.");
    }
    free(log);

    /* Tesla es opcional: solo hace falta para ver los logros dentro del juego */
    bool tesla = gr_tesla_installed(env->sd);
    it = add(out, &n, max, tesla ? GR_DIAG_OK : GR_DIAG_INFO, "Tesla (opcional)", "Tesla (optional)");
    if (tesla) DETAIL(it, "Instalado. Puedes activar los logros en el juego en Opciones.",
                      "Installed. You can turn on in-game achievements in Options.");
    else DETAIL(it, "No está. Solo hace falta para ver los logros dentro del juego.",
                "Not found. Only needed to see achievements inside the game.");

    it = add(out, &n, max, env->covers_ok ? GR_DIAG_OK : GR_DIAG_WARN, "Carátulas", "Covers");
    if (env->covers_ok) DETAIL(it, "Se leen de los juegos instalados.", "Read from installed games.");
    else DETAIL(it, "No se pudieron leer; se muestra la inicial del juego.", "Could not be read; the game's initial is shown.");

    it = add(out, &n, max, env->applet_mode ? GR_DIAG_WARN : GR_DIAG_INFO, "Modo de la app", "App mode");
    if (env->applet_mode) DETAIL(it, "Modo applet (abierta desde el álbum): menos memoria disponible.",
                                 "Applet mode (opened from the album): less memory available.");
    else DETAIL(it, "Modo aplicación.", "Application mode.");

    if (env->prev_run_known) {
        it = add(out, &n, max, env->prev_run_clean ? GR_DIAG_OK : GR_DIAG_WARN, "Última vez que abriste la app",
                 "Last time you opened the app");
        if (env->prev_run_clean) DETAIL(it, "Cerró bien.", "It closed properly.");
        else {
            snprintf(it->detail_es, sizeof(it->detail_es), "No cerró bien. Último paso: %s", env->prev_last_step);
            snprintf(it->detail_en, sizeof(it->detail_en), "It did not close properly. Last step: %s", env->prev_last_step);
        }
    }
    return n;
}
