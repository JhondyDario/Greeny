/*
 * Greeny - sysmodule de logros para Atmosphère.
 *
 * Detecta el juego en ejecucion, carga /config/greeny/games/<TITLEID>.txt,
 * lee la memoria del juego via dmnt:cht y desbloquea logros.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <switch.h>

#include "config.h"
#include "dmntcht.h"
#include "engine.h"
#include "sound.h"
#include "search.h"
#include "toast.h"

#define DIR_ROOT     "sdmc:/config/greeny"
#define DIR_OLD      "sdmc:/config/logros" /* nombre de la carpeta antes de llamarse Greeny */
#define CONFIG_PATH  DIR_ROOT "/config.ini"
#define DIR_GAMES    DIR_ROOT "/games"
#define DIR_UNLOCKED DIR_ROOT "/unlocked"
#define LOG_PATH     DIR_ROOT "/log.txt"
#define LOG_MAX      (64 * 1024)
#define DEFS_MAX     (64 * 1024)

#define TICK_NS          250000000ull
#define ATTACH_DELAY_MS  8000   /* dejar que el juego arranque antes de engancharse */
#define ATTACH_RETRY_MS  3000
#define ATTACH_RETRIES   10

#define INNER_HEAP_SIZE 0xC0000

u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;
u32 __nx_nv_transfermem_size = 0x40000;
ViLayerFlags __nx_vi_stray_layer_flags = (ViLayerFlags)0;

void __libnx_init_time(void);

static bool g_has_time;
static bool g_system_spanish = true;
static gr_config g_config;
/* El aviso sale oscuro solo si el tema oscuro esta elegido a mano: el sysmodule no consulta el tema de la consola. */
#define g_cfg_dark (g_config.theme == GR_THEME_DARK)

static bool use_english(void) {
    if (g_config.lang == GR_LANG_EN) return true;
    if (g_config.lang == GR_LANG_ES) return false;
    return !g_system_spanish;
}

void __libnx_initheap(void) {
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void *fake_heap_start;
    extern void *fake_heap_end;
    fake_heap_start = inner_heap;
    fake_heap_end = inner_heap + sizeof(inner_heap);
}

void __appInit(void) {
    Result rc = smInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc)) hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    /* idioma de la consola, para cuando la configuracion dice "auto" */
    if (R_SUCCEEDED(setInitialize())) {
        u64 code = 0;
        SetLanguage lang = SetLanguage_ENUS;
        if (R_SUCCEEDED(setGetSystemLanguage(&code)) && R_SUCCEEDED(setMakeLanguage(code, &lang)))
            g_system_spanish = lang == SetLanguage_ES || lang == SetLanguage_ES419;
        setExit();
    }

    rc = fsInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));
    fsdevMountSdmc();

    /* la hora solo se usa para la fecha del desbloqueo; si falla se guarda 0 */
    if (R_SUCCEEDED(timeInitialize())) {
        __libnx_init_time();
        g_has_time = true;
    }

    rc = pmdmntInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);
    rc = pminfoInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);

    smExit();
}

void __appExit(void) {
    dmntchtExit();
    pminfoExit();
    pmdmntExit();
    if (g_has_time) timeExit();
    fsdevUnmountAll();
    fsExit();
}

/* ---------- log ---------- */

static void log_line(const char *fmt, ...) {
    FILE *f = fopen(LOG_PATH, "a");
    if (!f) return;
    if (g_has_time) {
        time_t t = time(NULL);
        struct tm tm;
        if (localtime_r(&t, &tm))
            fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                    tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static u64 now_ms(void) {
    return armTicksToNs(armGetSystemTick()) / 1000000ull;
}

/* ---------- estado del juego actual ---------- */

static lg_game g_game;
static lg_bases g_bases;
static u64 g_main_size, g_space_end; /* para el buscador de valores */
static bool g_loaded;
static char g_unlocked_path[96];

static bool mem_read(void *ctx, uint64_t addr, void *out, size_t size) {
    (void)ctx;
    return R_SUCCEEDED(dmntchtReadCheatProcessMemory(addr, out, size));
}

#define DEBUG_MAX_LINES 400
static int g_debug_lines;

/* Modo debug: anota en el log cada vez que cambia el valor leido por una condicion. */
static void debug_report(u64 tid) {
    for (int i = 0; i < g_game.nach; i++) {
        lg_ach *a = &g_game.ach[i];
        for (int k = 0; k < a->ncond; k++) {
            lg_cond *c = &a->cond[k];
            if (!c->dbg_dirty) continue;
            c->dbg_dirty = false;
            if (g_debug_lines >= DEBUG_MAX_LINES) continue;
            if (++g_debug_lines == DEBUG_MAX_LINES) {
                log_line("%016lX: debug: limite de lineas alcanzado, no anoto mas en esta sesion", tid);
                continue;
            }
            if (c->dbg_ok)
                log_line("%016lX: debug %s#%d = %lu (0x%lX)", tid, a->id, k + 1, c->dbg_raw, c->dbg_raw);
            else
                log_line("%016lX: debug %s#%d = no se pudo leer (puntero nulo o direccion invalida)", tid, a->id,
                         k + 1);
        }
    }
}

#define SCAN_PERIOD_MS 10000
#define SCAN_MAX_HITS  12
#define SCAN_MAX_LINES 600
static int g_scan_lines;
static lg_scan_hit g_scan_prev[LG_MAX_SCAN][SCAN_MAX_HITS];
static int g_scan_prev_total[LG_MAX_SCAN];

/* Corre las busquedas de punteros y anota los candidatos cuando cambian. */
static void scan_report(u64 tid) {
    for (int s = 0; s < g_game.nscan; s++) {
        lg_scan_hit hits[SCAN_MAX_HITS];
        memset(hits, 0, sizeof(hits));
        int total = lg_scan_run(&g_game.scan[s], &g_bases, mem_read, NULL, hits, SCAN_MAX_HITS);
        if (total == g_scan_prev_total[s] && !memcmp(hits, g_scan_prev[s], sizeof(hits))) continue;
        g_scan_prev_total[s] = total;
        memcpy(g_scan_prev[s], hits, sizeof(hits));

        if (g_scan_lines >= SCAN_MAX_LINES) continue;
        const char *reg = g_game.scan[s].addr.region == LG_REGION_MAIN ? "main" : "heap";
        log_line("%016lX: scan%d: %d candidato(s)", tid, s + 1, total);
        g_scan_lines++;
        for (int i = 0; i < total && i < SCAN_MAX_HITS; i++, g_scan_lines++)
            log_line("%016lX: scan%d:   %s+0x%lX = %ld", tid, s + 1, reg, hits[i].offset, hits[i].value);
    }
}

static int count_unlocked(void) {
    int n = 0;
    for (int i = 0; i < g_game.nach; i++) n += g_game.ach[i].unlocked;
    return n;
}

/* Lee y parsea el archivo de definiciones. false si no hay archivo o esta malo. */
typedef enum { DEFS_NONE, DEFS_OK, DEFS_BAD } defs_result;

static defs_result load_defs(u64 tid) {
    char path[96];
    snprintf(path, sizeof(path), DIR_GAMES "/%016lX.txt", tid);

    FILE *f = fopen(path, "rb");
    if (!f) return DEFS_NONE; /* juego sin logros definidos: no se toca */

    char *buf = malloc(DEFS_MAX);
    if (!buf) { fclose(f); log_line("%016lX: sin memoria para leer definiciones", tid); return DEFS_BAD; }
    size_t n = fread(buf, 1, DEFS_MAX - 1, f);
    bool too_big = !feof(f);
    fclose(f);
    buf[n] = 0;

    char err[128];
    bool ok = false;
    if (too_big) log_line("%016lX: archivo de logros muy grande (max 64 KB)", tid);
    else if (lg_parse(&g_game, buf, err, sizeof(err)) != 0) log_line("%016lX: %s", tid, err);
    else ok = true;
    free(buf);
    return ok ? DEFS_OK : DEFS_BAD;
}

/* Aviso de que los logros de este juego no van a funcionar, y por que. */
static void warn_disabled(const char *reason_es, const char *reason_en) {
    if (!g_config.popup) return;
    bool en = use_english();
    toast_push(TOAST_WARN, 0, -1, g_cfg_dark, en ? "ACHIEVEMENTS OFF" : "LOGROS DESACTIVADOS", g_game.game[0] ? g_game.game : "Greeny",
               en ? reason_en : reason_es);
}

typedef enum { ATTACH_OK, ATTACH_RETRY, ATTACH_GIVE_UP } attach_result;

/* Se engancha al juego via dmnt:cht y valida que sea el proceso/version esperado. */
static attach_result attach(u64 pid, u64 tid) {
    Result rc = smInitialize();
    if (R_FAILED(rc)) return ATTACH_RETRY;
    rc = dmntchtInitialize();
    smExit();
    if (R_FAILED(rc)) return ATTACH_RETRY;

    DmntCheatProcessMetadata meta;
    bool has = false;
    rc = dmntchtHasCheatProcess(&has);
    if (R_SUCCEEDED(rc) && has) {
        rc = dmntchtGetCheatProcessMetadata(&meta);
        if (R_FAILED(rc) || meta.process_id != pid) has = false; /* quedo pegado a un proceso viejo */
    }
    if (!has) {
        rc = dmntchtForceOpenCheatProcess();
        if (R_FAILED(rc)) return ATTACH_RETRY;
        rc = dmntchtGetCheatProcessMetadata(&meta);
        if (R_FAILED(rc)) return ATTACH_RETRY;
    }
    if (meta.process_id != pid) return ATTACH_RETRY;

    char build[65];
    for (int i = 0; i < 0x20; i++) snprintf(build + i * 2, 3, "%02X", meta.main_nso_build_id[i]);

    if (g_game.build[0] && strncmp(build, g_game.build, strlen(g_game.build)) != 0) {
        log_line("%016lX: build id %.16s no coincide con el del archivo (%s); logros desactivados", tid, build,
                 g_game.build);
        char es[96], en[96];
        if (g_game.version[0]) {
            snprintf(es, sizeof(es), "Son para la version %s del juego y tienes otra", g_game.version);
            snprintf(en, sizeof(en), "They are for game version %s, you have another", g_game.version);
        } else {
            snprintf(es, sizeof(es), "Son para otra version del juego");
            snprintf(en, sizeof(en), "They are for a different game version");
        }
        warn_disabled(es, en);
        return ATTACH_GIVE_UP;
    }

    g_main_size = meta.main_nso_extents.size;
    g_space_end = meta.address_space_extents.base + meta.address_space_extents.size;
    g_bases.main_base = meta.main_nso_extents.base;
    g_bases.heap_base = meta.heap_extents.base;
    log_line("%016lX: enganchado, build %.16s, main=0x%lX heap=0x%lX", tid, build, g_bases.main_base,
             g_bases.heap_base);
    return ATTACH_OK;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    mkdir("sdmc:/config", 0777);
    /* instalaciones anteriores guardaban todo en config/logros: se conserva el progreso */
    struct stat st_new, st_old;
    bool migrated = stat(DIR_ROOT "/unlocked", &st_new) != 0 && stat(DIR_OLD "/unlocked", &st_old) == 0;
    if (migrated) {
        mkdir(DIR_ROOT, 0777);
        migrated = rename(DIR_OLD "/unlocked", DIR_ROOT "/unlocked") == 0;
    }
    mkdir(DIR_ROOT, 0777);
    mkdir(DIR_GAMES, 0777);
    mkdir(DIR_UNLOCKED, 0777);

    struct stat st;
    if (stat(LOG_PATH, &st) == 0 && st.st_size > LOG_MAX) remove(LOG_PATH);
    gr_config_defaults(&g_config);
    log_line("Greeny iniciado");
    if (migrated) log_line("progreso migrado desde config/logros");

    u64 cur_pid = 0, cur_tid = 0, attach_at = 0, scan_at = 0, search_at = 0;
    int retries = 0;
    bool pending = false, bad_defs = false, toast_fail_logged = false, sound_fail_logged = false;

    while (true) {
        u64 now = now_ms();

        u64 pid = 0;
        if (R_FAILED(pmdmntGetApplicationProcessId(&pid))) pid = 0;

        if (pid != cur_pid) {
            cur_pid = pid;
            cur_tid = 0;
            g_loaded = false;
            pending = false;
            bad_defs = false;
            if (pid && R_SUCCEEDED(pminfoGetProgramId(&cur_tid, pid))) {
                defs_result d = load_defs(cur_tid);
                if (d != DEFS_NONE) {
                    gr_config_load(&g_config, CONFIG_PATH); /* la app pudo cambiarla */
                    pending = d == DEFS_OK;
                    bad_defs = d == DEFS_BAD;
                    retries = 0;
                    attach_at = now + ATTACH_DELAY_MS; /* los avisos tambien esperan a que el juego arranque */
                }
            }
        }

        if (bad_defs && now >= attach_at) {
            bad_defs = false;
            g_game.game[0] = 0; /* el archivo no se pudo leer: no hay nombre fiable */
            warn_disabled("El archivo de logros tiene un error (ver log.txt)",
                          "The achievement file has an error (see log.txt)");
        }

        if (pending && now >= attach_at) {
            attach_result r = attach(cur_pid, cur_tid);
            if (r == ATTACH_RETRY && ++retries < ATTACH_RETRIES) {
                attach_at = now + ATTACH_RETRY_MS;
            } else {
                pending = false;
                if (r == ATTACH_OK) {
                    snprintf(g_unlocked_path, sizeof(g_unlocked_path), DIR_UNLOCKED "/%016lX.txt", cur_tid);
                    lg_store_load(&g_game, g_unlocked_path);
                    g_loaded = true;
                    g_debug_lines = 0;
                    g_scan_lines = 0;
                    scan_at = now + SCAN_PERIOD_MS;
                    memset(g_scan_prev, 0, sizeof(g_scan_prev));
                    for (int i = 0; i < LG_MAX_SCAN; i++) g_scan_prev_total[i] = -1;

                    if (g_has_time) { /* para que la app muestre "ultima vez jugado" */
                        char played[96];
                        mkdir(DIR_ROOT "/played", 0777);
                        snprintf(played, sizeof(played), DIR_ROOT "/played/%016lX.txt", cur_tid);
                        FILE *pf = fopen(played, "w");
                        if (pf) { fprintf(pf, "%llu\n", (unsigned long long)time(NULL)); fclose(pf); }
                    }

                    bool en = use_english();

                    char msg[64];
                    snprintf(msg, sizeof(msg), en ? "%d of %d unlocked" : "%d de %d desbloqueados", count_unlocked(),
                             g_game.nach);
                    if (g_config.popup)
                        toast_push(TOAST_INFO, 0, -1, g_cfg_dark, en ? "ACHIEVEMENTS ON" : "LOGROS ACTIVOS", g_game.game[0] ? g_game.game : "Greeny", msg);
                    if (g_config.sound) sound_play(SOUND_LOADED);
                    log_line("%016lX: %d logros cargados, %s", cur_tid, g_game.nach, msg);
                } else if (r == ATTACH_RETRY) {
                    log_line("%016lX: no se pudo abrir el proceso con dmnt:cht, me rindo", cur_tid);
                    warn_disabled("No se pudo leer la memoria del juego", "Could not read the game's memory");
                }
            }
        }

        if (g_loaded) {
            int idx[4];
            int n = lg_tick(&g_game, &g_bases, mem_read, NULL, now, idx, 4);
            for (int i = 0; i < n; i++) {
                lg_ach *a = &g_game.ach[idx[i]];
                u64 ts = g_has_time ? (u64)time(NULL) : 0;
                if (lg_store_append(g_unlocked_path, a->id, ts) != 0)
                    log_line("%016lX: no se pudo guardar el logro %s", cur_tid, a->id);
                log_line("%016lX: LOGRO %s (%s)", cur_tid, a->id, a->name);
                bool en = use_english();
                if (g_config.popup)
                    toast_push(TOAST_UNLOCK, (int)lg_ach_tier(a), a->points, g_cfg_dark, en ? "ACHIEVEMENT UNLOCKED" : "LOGRO DESBLOQUEADO", lg_ach_name(a, en), lg_ach_desc(a, en));
                if (g_config.sound) sound_play(SOUND_UNLOCK);
            }
            if (now >= search_at) { /* ordenes del buscador de valores, una vez por segundo */
                search_at = now + 1000;
                search_poll(g_bases.main_base, g_main_size, g_bases.heap_base, g_space_end);
            }
            if (g_game.debug) debug_report(cur_tid);
            if (g_game.nscan && now >= scan_at) {
                scan_report(cur_tid);
                scan_at = now_ms() + SCAN_PERIOD_MS;
            }
        }

        toast_update(now);
        if (toast_failed() && !toast_fail_logged) {
            toast_fail_logged = true;
            log_line("no se pudo crear la capa del popup (rc=0x%X); sigo sin popups", toast_fail_rc());
        }

        sound_update(now);
        if (sound_failed() && !sound_fail_logged) {
            sound_fail_logged = true;
            log_line("no se pudo abrir el audio (rc=0x%X); sigo sin sonido", sound_fail_rc());
        }

        svcSleepThread(TICK_NS);
    }

    return 0;
}
