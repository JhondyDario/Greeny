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
#define DIR_PROGRESS DIR_ROOT "/progress" /* cuanto falta para cada logro bloqueado; lo lee la app */
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
/* El aviso es oscuro, como los de la consola; solo sale claro si el tema claro esta elegido a mano. */
#define g_cfg_dark (g_config.theme != GR_THEME_LIGHT)

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

/* Botones para el buscador de valores. Solo se abre el servicio de mandos si existe un plan de
 * busqueda (lo deja la app, y solo a un administrador): un usuario normal nunca pasa por aqui. */
static bool g_pad_ready, g_combo_held;
static PadState g_pad;

static void watch_mark(u64 now);

static void combo_init(void) {
    if (g_pad_ready || (!search_has_plan() && !g_game.nwatch)) return;
    if (R_FAILED(smInitialize())) return;
    Result rc = hidInitialize();
    smExit();
    if (R_FAILED(rc)) { log_line("buscador: no se pudo abrir el servicio de mandos (rc=0x%X)", rc); return; }
    padConfigureInput(8, HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadSystemExt);
    padInitializeAny(&g_pad);
    g_pad_ready = true;
    if (search_has_plan()) log_line("buscador: plan encontrado; L+R+ZL+ZR con la cruceta (arriba, derecha, abajo) lo maneja");
}

/* L+R+ZL+ZR mantenidos y una direccion de la cruceta: arriba = paso siguiente (o "cambio"),
 * izquierda = "no cambio", derecha = punteros, abajo = comprobar. */
static void combo_poll(u64 now) {
    if (!g_pad_ready) return;
    padUpdate(&g_pad);
    u64 b = padGetButtons(&g_pad);
    /* vigilante: pulsar las dos palancas a la vez deja una marca numerada en watch.txt */
    const u64 sticks = HidNpadButton_StickL | HidNpadButton_StickR;
    static bool mark_held;
    if (g_game.nwatch && (b & sticks) == sticks) {
        if (!mark_held) {
            mark_held = true;
            watch_mark(now);
        }
    } else mark_held = false;

    const u64 hold = HidNpadButton_L | HidNpadButton_R | HidNpadButton_ZL | HidNpadButton_ZR;
    int action = -1;
    if ((b & hold) == hold) {
        if (b & HidNpadButton_Up) action = SEARCH_NEXT;
        else if (b & HidNpadButton_Right) action = SEARCH_PTR;
        else if (b & HidNpadButton_Down) action = SEARCH_CHECK;
        else if (b & HidNpadButton_Left) action = SEARCH_SAME;
    }
    if (action < 0) { g_combo_held = false; return; }
    if (g_combo_held) return; /* una vez por pulsacion */
    g_combo_held = true;

    /* avisar antes, porque mientras busca el sysmodule no hace nada mas */
    toast_push(TOAST_INFO, 0, -1, g_cfg_dark, "BUSCADOR DE VALORES", action == SEARCH_NEXT ? "Buscando..." : action == SEARCH_PTR ? "Buscando punteros..." : "Comprobando...",
               "Puede tardar. Sigue jugando cuando salga el resultado.");
    toast_update(now);
    char l1[64], l2[96];
    search_button((search_action)action, g_bases.main_base, g_main_size, g_bases.heap_base, g_space_end, l1, sizeof(l1), l2, sizeof(l2));
    log_line("buscador: %s | %s", l1, l2);
    toast_push(TOAST_INFO, 0, -1, g_cfg_dark, "BUSCADOR DE VALORES", l1, l2);
}
static bool g_loaded;
static char g_unlocked_path[96];
static char g_progress_path[96];

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

/* ---------- vigilante: copia trozos de memoria a la SD cuando cambian ----------
 * Para investigar un juego sin el buscador: se juega normal y luego se mira en watch.txt que
 * byte cambio en cada momento (entrar a un nivel, recibir daño, tocar la meta...). */
#define WATCH_PATH      DIR_ROOT "/watch.txt"
#define WATCH_PERIOD_MS 500
#define WATCH_MAX_FILE  (8 * 1024 * 1024)
static u8 g_watch_prev[LG_MAX_WATCH][LG_WATCH_MAX];
static u8 g_watch_cur[LG_WATCH_MAX];
static u64 g_watch_addr[LG_MAX_WATCH];
static u8 g_watch_state[LG_MAX_WATCH]; /* 0 = sin leer aun, 1 = leido, 2 = no se pudo leer, 3 = callado por ruidoso */
#define WATCH_MAX_FULL 40                /* un trozo que se reescribe entero tantas veces es ruido: llenaria el archivo */
static u8 g_watch_fulls[LG_MAX_WATCH];
static u64 g_watch_t0;
static int g_watch_marks;
static bool g_watch_full;

static void watch_reset(u64 tid, u64 now) {
    memset(g_watch_state, 0, sizeof(g_watch_state));
    memset(g_watch_fulls, 0, sizeof(g_watch_fulls));
    g_watch_marks = 0;
    g_watch_t0 = now;
    g_watch_full = false;
    if (!g_game.nwatch) return;
    FILE *f = fopen(WATCH_PATH, "a");
    if (!f) return;
    fprintf(f, "=== sesion %016lX main=0x%lX ===\n", tid, g_bases.main_base);
    fclose(f);
}

static void watch_report(u64 now) {
    FILE *f = NULL;
    for (int i = 0; i < g_game.nwatch && !g_watch_full; i++) {
        const lg_watch *w = &g_game.watch[i];
        if (g_watch_state[i] == 3) continue;
        u64 addr = 0;
        bool ok = lg_resolve(&w->addr, &g_bases, mem_read, NULL, &addr) && mem_read(NULL, addr, g_watch_cur, w->size);
        if (!ok && g_watch_state[i] == 2) continue;
        bool moved = ok && (g_watch_state[i] != 1 || addr != g_watch_addr[i]);
        if (ok && !moved && !memcmp(g_watch_cur, g_watch_prev[i], w->size)) continue;

        if (!f) {
            f = fopen(WATCH_PATH, "a");
            if (!f) return;
            fseek(f, 0, SEEK_END);
            if (ftell(f) > WATCH_MAX_FILE) { fprintf(f, "=== limite de tamaño: no anoto mas ===\n"); g_watch_full = true; break; }
        }
        fprintf(f, "t=%lu w%d", now - g_watch_t0, i);
        if (!ok) {
            fprintf(f, " NULL\n");
            g_watch_state[i] = 2;
            continue;
        }
        int changed = 0;
        for (u32 k = 0; k < w->size; k++) changed += g_watch_cur[k] != g_watch_prev[i][k];
        if (moved || changed > 96) { /* entero: primera vez, se mudo, o cambio casi todo */
            fprintf(f, " @0x%lX FULL ", addr);
            for (u32 k = 0; k < w->size; k++) fprintf(f, "%02X", g_watch_cur[k]);
            if (++g_watch_fulls[i] >= WATCH_MAX_FULL) {
                fprintf(f, "\nt=%lu w%d CALLADO: cambia entero demasiadas veces", now - g_watch_t0, i);
                g_watch_state[i] = 3;
                fputc('\n', f);
                continue;
            }
        } else { /* solo lo que cambio: +desplazamiento:bytes nuevos */
            for (u32 k = 0; k < w->size; k++) {
                if (g_watch_cur[k] == g_watch_prev[i][k]) continue;
                fprintf(f, " +%X:", k);
                for (; k < w->size && g_watch_cur[k] != g_watch_prev[i][k]; k++) fprintf(f, "%02X", g_watch_cur[k]);
            }
        }
        fputc('\n', f);
        memcpy(g_watch_prev[i], g_watch_cur, w->size);
        g_watch_addr[i] = addr;
        g_watch_state[i] = 1;
    }
    if (f) fclose(f);
}

static void watch_mark(u64 now) {
    FILE *f = fopen(WATCH_PATH, "a");
    if (!f) return;
    fprintf(f, "t=%lu MARCA %d\n", now - g_watch_t0, ++g_watch_marks);
    fclose(f);
    char l1[32];
    snprintf(l1, sizeof(l1), "Marca %d", g_watch_marks);
    toast_push(TOAST_INFO, 0, -1, g_cfg_dark, "GREENY", l1, "Anotada en watch.txt");
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
/* Mira que programa es de verdad, sin engancharse a el. Sirve para no ponerle un depurador encima a
 * algo que no es el juego: una app homebrew abierta "sobre" el juego (manteniendo R) tiene su mismo
 * title id, y engancharse a ella puede dejar la consola congelada al pulsar HOME.
 * Devuelve 1 si alguno de sus modulos es el build del archivo, 0 si ninguno, -1 si no se pudo saber.
 * *modules = cuantos modulos tiene (una app homebrew tiene uno solo; un juego, varios). */
static int build_matches(u64 pid, int *modules) {
    static LoaderModuleInfo info[8];
    s32 count = 0;
    *modules = 0;
    if (R_FAILED(smInitialize())) return -1;
    Result rc = ldrDmntInitialize();
    smExit();
    if (R_FAILED(rc)) return -1;
    rc = ldrDmntGetProcessModuleInfo(pid, info, 8, &count);
    ldrDmntExit();
    if (R_FAILED(rc) || count <= 0) return -1;
    *modules = count;
    for (s32 m = 0; m < count; m++) {
        char build[65];
        for (int i = 0; i < 0x20; i++) snprintf(build + i * 2, 3, "%02X", info[m].build_id[i]);
        if (!strncmp(build, g_game.build, strlen(g_game.build))) return 1;
    }
    return 0;
}

static attach_result attach(u64 pid, u64 tid) {
    if (g_game.build[0]) {
        int modules = 0, match = build_matches(pid, &modules);
        if (match == 0 && modules <= 1) {
            log_line("%016lX: lo que esta abierto no es el juego (una app sobre su title id); no me engancho", tid);
            return ATTACH_GIVE_UP;
        }
        if (match == 0) {
            log_line("%016lX: el build del juego no es %s; logros desactivados (sin engancharse)", tid, g_game.build);
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
    }

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
    mkdir(DIR_PROGRESS, 0777);

    struct stat st;
    if (stat(LOG_PATH, &st) == 0 && st.st_size > LOG_MAX) remove(LOG_PATH);
    gr_config_defaults(&g_config);
    log_line("Greeny iniciado");
    if (migrated) log_line("progreso migrado desde config/logros");

    u64 cur_pid = 0, cur_tid = 0, attach_at = 0, scan_at = 0, watch_at = 0, find_at = 0, search_at = 0, progress_at = 0;
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
                    toast_configure(g_config.toast_pos, gr_toast_ms(g_config.toast_time));
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
                    snprintf(g_progress_path, sizeof(g_progress_path), DIR_PROGRESS "/%016lX.txt", cur_tid);
                    lg_progress_load(&g_game, g_progress_path); /* lo de la ultima vez sigue valiendo hasta que haya algo nuevo */
                    g_game.prog_dirty = false;
                    progress_at = now + 5000;
                    g_loaded = true;
                    combo_init();
                    g_debug_lines = 0;
                    watch_reset(cur_tid, now);
                    watch_at = now;
                    find_at = g_game.nfind ? now + 75000 : 0; /* da tiempo a cargar la partida */
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
                    if (g_config.popup && g_config.start_popup)
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
                g_game.prog_dirty = true; /* el progreso de ese logro ya no hace falta */
                if (lg_store_append(g_unlocked_path, a->id, ts) != 0)
                    log_line("%016lX: no se pudo guardar el logro %s", cur_tid, a->id);
                log_line("%016lX: LOGRO %s (%s)", cur_tid, a->id, a->name);
                bool en = use_english();
                if (g_config.popup)
                    toast_push(TOAST_UNLOCK, (int)lg_ach_tier(a), a->points, g_cfg_dark, en ? "ACHIEVEMENT UNLOCKED" : "LOGRO DESBLOQUEADO", lg_ach_name(a, en), lg_ach_desc(a, en));
                if (g_config.sound) sound_play(SOUND_UNLOCK);
            }
            if (g_game.prog_dirty && now >= progress_at) { /* como mucho cada 5 s: la SD no es para escribir sin parar */
                progress_at = now + 5000;
                if (lg_progress_save(&g_game, g_progress_path) != 0) g_game.prog_dirty = false;
            }
            combo_poll(now);
            if (now >= search_at) { /* ordenes del buscador de valores, una vez por segundo */
                search_at = now + 1000;
                search_poll(g_bases.main_base, g_main_size, g_bases.heap_base, g_space_end);
            }
            if (g_game.debug) debug_report(cur_tid);
            if (g_game.nfind && find_at && now >= find_at) { /* una sola vez por sesion */
                find_at = 0;
                toast_push(TOAST_INFO, 0, -1, g_cfg_dark, "GREENY", "Buscando texto en la memoria...", "Puede tardar un par de minutos. Sigue en el mapa.");
                toast_update(now);
                int n = search_find_text(g_game.find, g_game.nfind, g_bases.main_base, g_main_size, g_bases.heap_base, g_space_end);
                char l2[64];
                snprintf(l2, sizeof(l2), "Encontrado %d veces. Mira find.txt.", n);
                log_line("%016lX: find: %d", cur_tid, n);
                toast_push(TOAST_INFO, 0, -1, g_cfg_dark, "GREENY", "Busqueda de texto terminada", l2);
            }
            if (g_game.nwatch && now >= watch_at) {
                watch_report(now);
                watch_at = now + WATCH_PERIOD_MS;
            }
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
