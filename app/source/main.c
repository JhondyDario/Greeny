/*
 * Greeny - visor de logros para el menu de homebrew.
 * Lee lo mismo que escribe el sysmodule: sdmc:/config/greeny/
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <switch.h>

#include "account.h"
#include "catalog.h"
#include "config.h"
#include "diag.h"
#include "gfx.h"
#include "icons.h"
#include "model.h"
#include "net.h"
#include "sfx.h"
#include "ui.h"

#include "third_party/stb_image.h" /* la implementacion esta en stb_impl.c */

#include "bg_claro_bin.h"
#include "bg_oscuro_bin.h"
#include "buttons_bin.h"
#include "snd_cargado_bin.h"
#include "snd_logro_bin.h"
#include "sysmodule_bin.h"
#include "mascot_big_bin.h"
#include "mascot_small_bin.h"

#define SD          "sdmc:"
#define ROOT        SD "/config/greeny"
#define CONFIG_PATH ROOT "/config.ini"
#define TRACE_PATH  ROOT "/app-log.txt"
#define ACCOUNT_PATH ROOT "/account.ini"
#define LINK_SECONDS 600  /* lo que dura un codigo de vinculacion */
#define LINK_POLL_MS 3000
#define SYSMODULE_TID 0x4200000000010690ull
#define TRACE_DONE  "salida normal"

/* tamaños con los que se generaron los .bin (scripts/assets.py) */
#define BIG_W 240
#define BIG_H 240
#define SMALL_W 66
#define SMALL_H 66

typedef enum {
    SCREEN_HOME, SCREEN_DETAIL, SCREEN_SETTINGS, SCREEN_DIAG, SCREEN_ABOUT, SCREEN_INTRO, SCREEN_LINK, SCREEN_MSG,
    SCREEN_PROFILE, SCREEN_BADGES
} screen_t;

static gr_model g_model;
static gr_config g_config;
static ui_assets g_assets;
static uint8_t *g_covers[GR_MAX_GAMES], *g_covers_big[GR_MAX_GAMES];
static bool g_system_spanish = true, g_system_dark;
static bool g_ns_ok;
static gr_diag_env g_env;
static gr_diag_item g_diag[GR_DIAG_MAX];
static int g_ndiag;

/* cuenta y sincronizacion */
typedef enum { SYNC_IDLE, SYNC_RUNNING, SYNC_OK, SYNC_OFFLINE, SYNC_FAILED, SYNC_UNLINKED } sync_state;
static gr_account g_account;          /* la cuenta que usa la interfaz */
static gr_account g_sync_account;     /* copia con la que trabaja el hilo de sincronizacion */
static volatile int g_sync = SYNC_IDLE;
static int g_sync_sent, g_sync_added;
static char g_sync_err[160];
static Thread g_sync_thread;
static bool g_sync_thread_live;
static char g_nick[40];               /* apodo del usuario de la consola */
static ui_profile g_profile;          /* lo que muestran la barra de arriba y la pantalla de perfil */

/* mensaje de la pantalla SCREEN_MSG */
static char g_msg_title[80], g_msg_line1[160], g_msg_line2[160];
static bool g_msg_ok;
static screen_t g_msg_return = SCREEN_SETTINGS; /* a donde vuelve el mensaje al aceptarlo */

/* iconos de los logros del juego abierto */
static uint8_t *g_icons[LG_MAX_ACH], *g_icons_gray[LG_MAX_ACH];
static Thread g_icon_thread;
static bool g_icon_thread_live;
static volatile int g_icon_state;     /* 0 nada, 1 bajando, 2 termino */
static int g_icon_downloaded;
static char g_icon_tid[17];           /* juego cuyos iconos se estan bajando */

/* juegos instalados en la consola, para pedir al catalogo solo los suyos */
#define MAX_TITLES 512
static char g_titles[MAX_TITLES][17];
static int g_ntitles;
static int g_cat_downloaded;          /* archivos de logros que bajo el catalogo esta vez */
static volatile bool g_bg_done;       /* el hilo de fondo termino */
static bool g_sysmodule_updated;      /* esta vez se escribio un sysmodule nuevo en la SD */

static uint32_t g_bg_px[UI_W * UI_H]; /* fondo del tema actual, ya decodificado */
static int g_bg_loaded = -1;          /* 0 claro, 1 oscuro, -1 ninguno */

/* ---------- rastro de arranque ----------
 * Cada paso se anota en app-log.txt antes de hacerlo. Si la app se congela, el
 * archivo dice hasta donde llego, y el diagnostico lo muestra la proxima vez. */

static void trace(const char *fmt, ...) {
    FILE *f = fopen(TRACE_PATH, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

/* Lee como termino la ejecucion anterior y empieza un rastro nuevo. */
static void trace_begin(void) {
    FILE *f = fopen(TRACE_PATH, "r");
    if (f) {
        char line[96], last[96] = "";
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (line[0]) snprintf(last, sizeof(last), "%s", line);
        }
        fclose(f);
        g_env.prev_run_known = last[0] != 0;
        g_env.prev_run_clean = !strcmp(last, TRACE_DONE);
        snprintf(g_env.prev_last_step, sizeof(g_env.prev_last_step), "%s", last);
    }
    mkdir(SD "/config", 0777);
    mkdir(ROOT, 0777);
    remove(TRACE_PATH);
    trace("inicio de Greeny " UI_APP_VERSION);
}

/* ---------- caratulas ---------- */

static uint8_t *make_cover(const uint8_t *img, int w, int h, int size) {
    uint8_t *out = malloc((size_t)size * size * 4);
    if (!out) return NULL;
    gfx_scale_rgba(img, w, h, out, size, size);
    gfx_round_corners(out, size, size, size / 6);
    return out;
}

/* Carga la caratula de cada juego desde la propia consola (el icono del juego instalado). */
static void load_covers(void) {
    static NsApplicationControlData ctrl;
    for (int i = 0; i < GR_MAX_GAMES; i++) {
        free(g_covers[i]);
        free(g_covers_big[i]);
        g_covers[i] = g_covers_big[i] = NULL;
        g_assets.covers[i] = g_assets.covers_big[i] = NULL;
    }
    if (!g_ns_ok) return;

    for (int i = 0; i < g_model.ngames; i++) {
        trace("caratula de %s", g_model.games[i].tid);
        u64 tid = strtoull(g_model.games[i].tid, NULL, 16);
        u64 size = 0;
        if (R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, &ctrl, sizeof(ctrl), &size)))
            continue; /* el juego no esta instalado en esta consola */
        if (size <= sizeof(ctrl.nacp)) continue;

        int w, h, n;
        uint8_t *img = stbi_load_from_memory(ctrl.icon, (int)(size - sizeof(ctrl.nacp)), &w, &h, &n, 4);
        if (!img) continue;
        g_assets.covers[i] = g_covers[i] = make_cover(img, w, h, UI_COVER);
        g_assets.covers_big[i] = g_covers_big[i] = make_cover(img, w, h, UI_COVER_BIG);
        stbi_image_free(img);
    }
}

static void refresh_greeting(void);

static void rescan(void) {
    trace("leyendo archivos de logros");
    gr_scan(&g_model, ROOT);
    load_covers();
    refresh_greeting(); /* los numeros del perfil salen de aqui */
}

/* Decodifica el fondo del tema (viene como JPEG dentro de la app). Si falla, queda el color liso. */
static void load_background(bool dark) {
    if (g_bg_loaded == (int)dark) return;
    int w, h, n;
    uint8_t *img = dark ? stbi_load_from_memory(bg_oscuro_bin, (int)bg_oscuro_bin_size, &w, &h, &n, 4)
                        : stbi_load_from_memory(bg_claro_bin, (int)bg_claro_bin_size, &w, &h, &n, 4);
    if (img && w == UI_W && h == UI_H) {
        memcpy(g_bg_px, img, sizeof(g_bg_px));
        ui_set_background(g_bg_px);
        g_bg_loaded = (int)dark;
    } else {
        ui_set_background(NULL);
        g_bg_loaded = -1;
    }
    stbi_image_free(img);
}

static void apply_config(void) {
    bool dark = g_config.theme == GR_THEME_DARK || (g_config.theme == GR_THEME_AUTO && g_system_dark);
    ui_set_english(g_config.lang == GR_LANG_EN || (g_config.lang == GR_LANG_AUTO && !g_system_spanish));
    ui_set_dark(dark);
    load_background(dark);
}

static void save_config(void) {
    mkdir(SD "/config", 0777);
    mkdir(ROOT, 0777);
    gr_config_save(&g_config, CONFIG_PATH);
    apply_config();
}

static void run_diag(void) {
    u64 pid = 0;
    g_env.sysmodule_running = false;
    if (R_SUCCEEDED(pmdmntInitialize())) {
        g_env.sysmodule_running = R_SUCCEEDED(pmdmntGetProcessId(&pid, SYSMODULE_TID));
        pmdmntExit();
    }
    g_ndiag = gr_diag_run(g_diag, GR_DIAG_MAX, &g_env, &g_model);
}

/* Apodo del usuario de la consola, para el saludo. Deja "" si no se puede saber. */
static void console_nickname(char *out, size_t cap) {
    out[0] = 0;
    if (R_FAILED(accountInitialize(AccountServiceType_Application)) &&
        R_FAILED(accountInitialize(AccountServiceType_System)))
        return;
    AccountUid uid;
    if (R_SUCCEEDED(accountGetPreselectedUser(&uid)) || R_SUCCEEDED(accountGetLastOpenedUser(&uid))) {
        AccountProfile profile;
        if (accountUidIsValid(&uid) && R_SUCCEEDED(accountGetProfile(&profile, uid))) {
            AccountProfileBase base;
            if (R_SUCCEEDED(accountProfileGet(&profile, NULL, &base))) snprintf(out, cap, "%s", base.nickname);
            accountProfileClose(&profile);
        }
    }
    accountExit();
}

/* ---------- instalacion ----------
 * La app trae dentro el sysmodule y sus sonidos, y los pone en la SD si faltan
 * o si son de otra version. Asi basta con copiar greeny.nro para instalar todo. */

/* Escribe el archivo solo si no existe o es distinto. Devuelve true si lo escribio. */
static bool install_file(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "rb");
    if (f) {
        bool same = false;
        fseek(f, 0, SEEK_END);
        if ((size_t)ftell(f) == size) {
            fseek(f, 0, SEEK_SET);
            void *have = malloc(size ? size : 1);
            same = have && fread(have, 1, size, f) == size && !memcmp(have, data, size);
            free(have);
        }
        fclose(f);
        if (same) return false;
    }
    /* se escribe con otro nombre y se cambia al final: si se corta la luz no queda un archivo a medias */
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    if (fclose(f) != 0) ok = false;
    if (!ok) { remove(tmp); return false; }
    remove(path);
    return rename(tmp, path) == 0;
}

static void install_sysmodule(void) {
    static const char toolbox[] = "{\n    \"name\": \"Greeny\",\n    \"tid\": \"4200000000010690\",\n    \"requires_reboot\": true\n}\n";
#define SYS_DIR SD "/atmosphere/contents/4200000000010690"
    mkdir(SD "/atmosphere", 0777);
    mkdir(SD "/atmosphere/contents", 0777);
    mkdir(SYS_DIR, 0777);
    mkdir(SYS_DIR "/flags", 0777);
    mkdir(SD "/config", 0777);
    mkdir(ROOT, 0777);
    mkdir(ROOT "/games", 0777);
    mkdir(ROOT "/sounds", 0777);

    g_sysmodule_updated = install_file(SYS_DIR "/exefs.nsp", sysmodule_bin, sysmodule_bin_size);
    install_file(SYS_DIR "/toolbox.json", toolbox, sizeof(toolbox) - 1);
    install_file(SYS_DIR "/flags/boot2.flag", "", 0);
    install_file(ROOT "/sounds/cargado.pcm", snd_cargado_bin, snd_cargado_bin_size);
    install_file(ROOT "/sounds/logro.pcm", snd_logro_bin, snd_logro_bin_size);
    trace("instalacion revisada: sysmodule %s", g_sysmodule_updated ? "escrito (hay que reiniciar)" : "al dia");
#undef SYS_DIR
}

/* Lista los juegos instalados en la consola. */
static void list_installed_titles(void) {
    static NsApplicationRecord records[MAX_TITLES];
    s32 count = 0;
    g_ntitles = 0;
    if (!g_ns_ok || R_FAILED(nsListApplicationRecord(records, MAX_TITLES, 0, &count))) return;
    for (s32 i = 0; i < count && g_ntitles < MAX_TITLES; i++)
        snprintf(g_titles[g_ntitles++], 17, "%016lX", records[i].application_id);
}

/* ---------- iconos de logros ---------- */

/* Carga de la SD los iconos del juego abierto (los que haya). */
static void load_icons(const char *tid) {
    for (int i = 0; i < LG_MAX_ACH; i++) {
        free(g_icons[i]);
        free(g_icons_gray[i]);
        g_icons[i] = g_icons_gray[i] = NULL;
        g_assets.ach_icons[i] = g_assets.ach_icons_gray[i] = NULL;
    }
    for (int i = 0; tid && i < g_model.cur.nach; i++) {
        uint8_t *img = malloc(GR_ICON_BYTES), *gray = malloc(GR_ICON_BYTES);
        if (img && gray && gr_icon_load(ROOT, tid, g_model.cur.ach[i].id, img)) {
            gr_icon_gray(img, gray);
            g_assets.ach_icons[i] = g_icons[i] = img;
            g_assets.ach_icons_gray[i] = g_icons_gray[i] = gray;
        } else {
            free(img);
            free(gray);
        }
    }
}

static void icon_thread(void *arg) {
    (void)arg;
    g_icon_downloaded = 0;
    gr_icons_sync(net_get, ROOT, g_icon_tid, &g_icon_downloaded);
    g_icon_state = 2;
}

/* Pone al dia los iconos de un juego en segundo plano, si hay internet. */
static void icons_start(const char *tid) {
    if (g_icon_thread_live || !net_online()) return;
    snprintf(g_icon_tid, sizeof(g_icon_tid), "%s", tid);
    g_icon_state = 1;
    if (R_SUCCEEDED(threadCreate(&g_icon_thread, icon_thread, NULL, NULL, 0x40000, 0x2C, -2)) &&
        R_SUCCEEDED(threadStart(&g_icon_thread)))
        g_icon_thread_live = true;
    else
        g_icon_state = 0;
}

/* Devuelve true si llegaron iconos nuevos del juego `open_tid` (hay que recargarlos y redibujar). */
static bool icons_collect(const char *open_tid) {
    if (!g_icon_thread_live || g_icon_state != 2) return false;
    threadWaitForExit(&g_icon_thread);
    threadClose(&g_icon_thread);
    g_icon_thread_live = false;
    g_icon_state = 0;
    trace("iconos de %s: %d bajados", g_icon_tid, g_icon_downloaded);
    return g_icon_downloaded > 0 && open_tid && !strcmp(open_tid, g_icon_tid);
}

/* ---------- cuenta ---------- */

static void show_message(bool ok, const char *title, const char *line1, const char *line2) {
    g_msg_ok = ok;
    snprintf(g_msg_title, sizeof(g_msg_title), "%s", title);
    snprintf(g_msg_line1, sizeof(g_msg_line1), "%s", line1 ? line1 : "");
    snprintf(g_msg_line2, sizeof(g_msg_line2), "%s", line2 ? line2 : "");
}

/* El saludo del inicio usa el usuario de Greeny si la consola esta vinculada; si no, el apodo de la consola. */
static void refresh_greeting(void) {
    bool linked = g_account.linked && g_account.username[0];
    ui_set_user(linked ? g_account.username : g_nick);

    /* el perfil se arma con lo que hay en la SD; de internet solo vienen seguidores y siguiendo */
    g_profile.linked = linked;
    g_profile.username = linked ? g_account.username : "";
    g_profile.name = linked ? (g_account.display[0] ? g_account.display : g_account.username) : (g_nick[0] ? g_nick : "Jugador");
    g_profile.caption = linked ? g_account.caption : "";
    g_profile.friend_code = linked ? g_account.friend_code : "";
    g_profile.social_known = g_account.social_known;
    g_profile.followers = g_account.followers;
    g_profile.following = g_account.following;
    g_profile.stats = (gr_stats){ .games = g_model.games_played, .achievements = g_model.unlocked,
                                  .platinums = g_model.platinums, .points = g_model.points_got };
    ui_set_profile(&g_profile);
}

static void device_name(char *out, size_t cap) {
    if (g_nick[0]) snprintf(out, cap, "Switch de %s", g_nick);
    else snprintf(out, cap, "Switch");
}

/* Hilo de fondo. Nunca bloquea la interfaz. Hace dos cosas, las dos opcionales:
 * baja los archivos de logros de los juegos instalados y, si hay cuenta, sube los logros. */
static void sync_thread(void *arg) {
    (void)arg;
    g_cat_downloaded = 0;
    gr_catalog_sync(net_get, ROOT, g_titles, g_ntitles, &g_cat_downloaded);

    if (g_sync == SYNC_RUNNING) {
        bool exists = false;
        gr_net_result r = gr_whoami(net_http, &g_sync_account, &exists);
        if (r == GR_NET_OFFLINE) g_sync = SYNC_OFFLINE;
        else if (r != GR_NET_OK) { snprintf(g_sync_err, sizeof(g_sync_err), "No se pudo consultar la cuenta."); g_sync = SYNC_FAILED; }
        else if (!exists || !g_sync_account.linked) g_sync = SYNC_UNLINKED;
        else {
            gr_fetch_social(net_http, &g_sync_account); /* si falla, quedan los numeros de la ultima vez */
            r = gr_push_unlocks(net_http, &g_sync_account, ROOT, &g_sync_sent, &g_sync_added, g_sync_err, sizeof(g_sync_err));
            g_sync = r == GR_NET_OK ? SYNC_OK : r == GR_NET_OFFLINE ? SYNC_OFFLINE : SYNC_FAILED;
        }
    }
    g_bg_done = true;
}

static void sync_start(void) {
    bool linked = g_account.token[0] && g_account.linked;
    if (g_sync_thread_live) return;
    if (!net_online()) { if (linked) g_sync = SYNC_OFFLINE; return; }
    g_sync_account = g_account;
    g_sync = linked ? SYNC_RUNNING : SYNC_IDLE;
    g_bg_done = false;
    if (R_SUCCEEDED(threadCreate(&g_sync_thread, sync_thread, NULL, NULL, 0x40000, 0x2C, -2)) &&
        R_SUCCEEDED(threadStart(&g_sync_thread))) {
        g_sync_thread_live = true;
    } else if (linked) {
        snprintf(g_sync_err, sizeof(g_sync_err), "No se pudo iniciar la sincronización.");
        g_sync = SYNC_FAILED;
    }
}

static void rescan(void);

/* Recoge el resultado del hilo cuando termina. Devuelve true si hay que redibujar. */
static bool sync_collect(void) {
    if (!g_sync_thread_live || !g_bg_done) return false;
    threadWaitForExit(&g_sync_thread);
    threadClose(&g_sync_thread);
    g_sync_thread_live = false;
    trace("fondo: catalogo %d archivo(s) nuevo(s); cuenta estado %d, enviados %d, nuevos %d%s%s", g_cat_downloaded, (int)g_sync,
          g_sync_sent, g_sync_added, g_sync == SYNC_OFFLINE ? ", " : "", g_sync == SYNC_OFFLINE ? net_last_error() : "");

    if (g_sync == SYNC_UNLINKED) {
        /* la desvincularon desde la web: olvidar la llave */
        memset(&g_account, 0, sizeof(g_account));
        gr_account_save(&g_account, ACCOUNT_PATH);
    } else if (g_sync == SYNC_OK) {
        g_account = g_sync_account; /* trae el nombre y la frase al dia */
        gr_account_save(&g_account, ACCOUNT_PATH);
    }
    if (g_cat_downloaded > 0) rescan();  /* llegaron juegos nuevos: que aparezcan en la lista */
    else refresh_greeting();
    return true;
}

static void sync_text(const char **es, const char **en) {
    static char buf_es[200], buf_en[200];
    switch (g_sync) {
        case SYNC_RUNNING: *es = "Sincronizando…"; *en = "Syncing…"; return;
        case SYNC_OFFLINE: *es = "Sin conexión. Se intentará la próxima vez que abras la app."; *en = "No connection. It will retry next time you open the app."; return;
        case SYNC_FAILED:
            snprintf(buf_es, sizeof(buf_es), "No se pudo: %s", g_sync_err);
            snprintf(buf_en, sizeof(buf_en), "Failed: %s", g_sync_err);
            break;
        case SYNC_OK:
            snprintf(buf_es, sizeof(buf_es), "Al día: %d logros enviados, %d nuevos en tu cuenta.", g_sync_sent, g_sync_added);
            snprintf(buf_en, sizeof(buf_en), "Up to date: %d achievements sent, %d new on your account.", g_sync_sent, g_sync_added);
            break;
        default: *es = "Sube a tu cuenta los logros guardados en esta consola."; *en = "Uploads the achievements saved on this console to your account."; return;
    }
    *es = buf_es;
    *en = buf_en;
}

/* Teclado de la consola. Devuelve false si se cancelo o no se pudo abrir. */
static bool ask_text(const char *guide, bool password, bool numeric, char *out, size_t cap) {
    SwkbdConfig kbd;
    out[0] = 0;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return false;
    if (password) swkbdConfigMakePresetPassword(&kbd);
    else swkbdConfigMakePresetDefault(&kbd);
    if (numeric) swkbdConfigSetType(&kbd, SwkbdType_NumPad);
    swkbdConfigSetGuideText(&kbd, guide);
    swkbdConfigSetStringLenMax(&kbd, (u32)cap - 1);
    Result rc = swkbdShow(&kbd, out, cap);
    swkbdClose(&kbd);
    return R_SUCCEEDED(rc) && out[0];
}

/* Entrar con correo y contraseña. Deja listo el mensaje de resultado. */
static void login_with_password(void) {
    char email[128], password[128], name[64], err[160] = "";
    if (!net_online()) { show_message(false, "Sin conexión", "Conecta la consola a internet e inténtalo de nuevo.", NULL); return; }
    if (!ask_text("Correo de tu cuenta Greeny", false, false, email, sizeof(email))) { show_message(false, "Cancelado", "No se escribió el correo.", NULL); return; }
    if (!ask_text("Contraseña", true, false, password, sizeof(password))) { show_message(false, "Cancelado", "No se escribió la contraseña.", NULL); return; }

    device_name(name, sizeof(name));
    gr_account fresh;
    gr_net_result r = gr_login_password(net_http, email, password, name, &fresh, err, sizeof(err));
    memset(password, 0, sizeof(password));
    if (r != GR_NET_OK) {
        trace("entrar con correo: fallo %d (%s) %s", (int)r, err, net_last_error());
        show_message(false, "No se pudo entrar", err, "Si entras con Discord, usa \"Vincular con un código\".");
        return;
    }
    g_account = fresh;
    gr_account_save(&g_account, ACCOUNT_PATH);
    refresh_greeting();
    char line[160];
    snprintf(line, sizeof(line), "Esta consola quedó vinculada a @%s.", g_account.username[0] ? g_account.username : "tu cuenta");
    show_message(true, "¡Listo!", line, "Tus logros se suben solos cuando hay internet.");
    sync_start();
}

/* Seguir a alguien escribiendo su codigo de amigo. Deja listo el mensaje de resultado. */
static void follow_with_code(void) {
    char code[32], who[40], err[160] = "";
    if (!net_online()) { show_message(false, "Sin conexión", "Conecta la consola a internet e inténtalo de nuevo.", NULL); return; }
    if (!ask_text("Código de amigo: 6 números", false, true, code, sizeof(code))) {
        show_message(false, "Cancelado", "No se escribió ningún código.", NULL);
        return;
    }
    gr_net_result r = gr_follow_code(net_http, &g_account, code, who, sizeof(who), err, sizeof(err));
    if (r != GR_NET_OK) {
        trace("seguir por codigo: fallo %d (%s) %s", (int)r, err, net_last_error());
        show_message(false, "No se pudo seguir", err, NULL);
        return;
    }
    char line[160];
    snprintf(line, sizeof(line), "Ahora sigues a @%s.", who[0] ? who : "ese jugador");
    show_message(true, "¡Listo!", line, NULL);
    sync_start(); /* para que el contador de \"siguiendo\" se ponga al dia */
}

/* Si no hay fuente o pantalla grafica, al menos decirlo en modo texto en vez de cerrar sin mas. */
static int fatal_screen(PadState *pad, const char *what) {
    trace("error fatal: %s", what);
    consoleInit(NULL);
    printf("\n  Greeny no pudo iniciar: %s\n\n  Greeny could not start.\n\n  Pulsa + para salir / Press + to exit.\n", what);
    while (appletMainLoop()) {
        padUpdate(pad);
        if (padGetButtonsDown(pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    trace(TRACE_DONE);
    return 1;
}

/* Mueve el cursor en una lista (cols = 1) o cuadricula. Devuelve true si cambio. */
static bool move_cursor(int *cursor, int n, int cols, u64 nav) {
    int c = *cursor;
    if ((nav & HidNpadButton_AnyRight) && c % cols != cols - 1 && c + 1 < n) c++;
    if ((nav & HidNpadButton_AnyLeft) && c % cols != 0) c--;
    if (nav & HidNpadButton_AnyDown) {
        if (c + cols < n) c += cols;
        else if (c / cols < (n - 1) / cols) c = n - 1; /* ultima fila incompleta */
    }
    if ((nav & HidNpadButton_AnyUp) && c - cols >= 0) c -= cols;
    if (c == *cursor) return false;
    *cursor = c;
    return true;
}

/* Ajusta la primera fila visible para que el cursor se vea. */
static void follow(int cursor, int *top, ui_layout l) {
    int row = cursor / l.cols;
    if (row < *top) *top = row;
    if (row >= *top + l.rows) *top = row - l.rows + 1;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    g_env.sd = SD;
    g_env.root = ROOT;
    trace_begin();

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();
    PadRepeater rep;
    padRepeaterInitialize(&rep, 22, 6);

    /* fuente del sistema: la misma que usa el menu de la consola */
    trace("fuente del sistema");
    bool font_ok = false;
    if (R_SUCCEEDED(plInitialize(PlServiceType_User))) {
        PlFontData font;
        if (R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType_Standard)))
            font_ok = gfx_font_from_memory(font.address, font.size);
    }
    if (!font_ok) return fatal_screen(&pad, "no se pudo cargar la fuente del sistema");

    trace("pantalla");
    NWindow *win = nwindowGetDefault();
    Framebuffer fb;
    if (R_FAILED(framebufferCreate(&fb, win, UI_W, UI_H, PIXEL_FORMAT_RGBA_8888, 2))) {
        plExit();
        return fatal_screen(&pad, "no se pudo crear la pantalla");
    }
    if (R_FAILED(framebufferMakeLinear(&fb))) {
        framebufferClose(&fb);
        plExit();
        return fatal_screen(&pad, "no se pudo preparar la pantalla");
    }

    trace("idioma y tema de la consola");
    if (R_SUCCEEDED(setInitialize())) {
        u64 code = 0;
        SetLanguage lang = SetLanguage_ENUS;
        if (R_SUCCEEDED(setGetSystemLanguage(&code)) && R_SUCCEEDED(setMakeLanguage(code, &lang)))
            g_system_spanish = lang == SetLanguage_ES || lang == SetLanguage_ES419;
        setExit();
    }
    if (R_SUCCEEDED(setsysInitialize())) {
        ColorSetId color = ColorSetId_Light;
        if (R_SUCCEEDED(setsysGetColorSetId(&color))) g_system_dark = color == ColorSetId_Dark;
        setsysExit();
    }

    trace("servicio de caratulas");
    g_ns_ok = R_SUCCEEDED(nsInitialize());

    trace("audio");
    bool sfx_ok = sfx_init();

    trace("red");
    net_init();

    AppletType at = appletGetAppletType();
    g_env.atmosphere = hosversionIsAtmosphere();
    g_env.covers_ok = g_ns_ok;
    g_env.applet_mode = at != AppletType_Application && at != AppletType_SystemApplication;

    g_assets.mascot_big = mascot_big_bin;
    g_assets.big_w = BIG_W;
    g_assets.big_h = BIG_H;
    g_assets.buttons = buttons_bin_size >= UI_BTN_COUNT * UI_BTN_SIZE * UI_BTN_SIZE * 4 ? buttons_bin : NULL;
    g_assets.mascot_small = mascot_small_bin;
    g_assets.small_w = SMALL_W;
    g_assets.small_h = SMALL_H;

    trace("instalacion");
    install_sysmodule();
    list_installed_titles();

    trace("configuracion");
    gr_config_load(&g_config, CONFIG_PATH);
    apply_config();
    rescan();
    /* capa donde se dibuja la pantalla; el fondo se pinta aparte en cada cuadro */
    static u32 layer_px[UI_W * UI_H];
    gfx_canvas layer = { .px = layer_px, .w = UI_W, .h = UI_H, .stride = UI_W };
    const u64 start_tick = armGetSystemTick();

    trace("listo: %d juego(s), audio %s, caratulas %s, modo %s", g_model.ngames, sfx_ok ? "si" : "no",
          g_ns_ok ? "si" : "no", g_env.applet_mode ? "applet" : "aplicacion");

    int cursor = 0, top = 0, dcursor = 0, dtop = 0, stab = 0, srow = 0, intro_page = 0;
    screen_t screen = SCREEN_HOME;

    trace("usuario y cuenta");
    console_nickname(g_nick, sizeof(g_nick));
    gr_account_load(&g_account, ACCOUNT_PATH);
    if (!g_config.intro_done) {
        /* primera vez: introduccion con la revision de la instalacion */
        trace("introduccion");
        run_diag();
        screen = SCREEN_INTRO;
    } else if (g_sysmodule_updated) {
        /* la app ya estaba en uso y trajo un sysmodule mas nuevo */
        show_message(true, "Greeny se actualizó", "Reinicia la consola para usar la versión nueva del sistema de logros.",
                     "Apagar y encender; suspender no basta.");
        g_msg_return = SCREEN_HOME;
        screen = SCREEN_MSG;
    } else if (g_config.start_page == GR_START_PROFILE) {
        screen = SCREEN_PROFILE;
    }
    refresh_greeting();
    sync_start(); /* en segundo plano y solo si hay cuenta e internet; la app no espera por esto */

    /* tactil: un toque corto es una pulsacion; un arrastre vertical mueve la lista */
    bool touching = false;
    int touch_x0 = 0, touch_y0 = 0, touch_x = 0, touch_y = 0;

    char link_code[8] = "";
    u64 link_started = 0, link_last_poll = 0;
    int link_shown_left = -1;
    bool dirty = true, plus_pressed = false;
    const u64 dirs = HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight;

    while (appletMainLoop()) {
        padUpdate(&pad);
        padRepeaterUpdate(&rep, padGetButtons(&pad) & dirs);
        u64 down = padGetButtonsDown(&pad);
        u64 nav = (down | padRepeaterGetButtons(&rep)) & dirs;

        /* ---- tactil: se traduce a los mismos botones, asi cada pantalla no necesita saber de toques ---- */
        int tap_item = -1, tap_tab = -1;
        HidTouchScreenState touch = {0};
        if (hidGetTouchScreenStates(&touch, 1) && touch.count > 0) {
            touch_x = (int)touch.touches[0].x;
            touch_y = (int)touch.touches[0].y;
            if (!touching) { touching = true; touch_x0 = touch_x; touch_y0 = touch_y; }
        } else if (touching) {
            touching = false;
            int dx = touch_x - touch_x0, dy = touch_y - touch_y0;
            if (abs(dx) < 24 && abs(dy) < 24) {
                static const u64 button_of[UI_BTN_COUNT] = { HidNpadButton_A, HidNpadButton_B, HidNpadButton_X, HidNpadButton_Y,
                                                             HidNpadButton_Plus, HidNpadButton_Minus, HidNpadButton_L, HidNpadButton_R };
                const ui_zone *z = ui_zone_at(touch_x, touch_y);
                if (z && z->kind == UI_ZONE_BUTTON && z->value >= 0 && z->value < UI_BTN_COUNT) down |= button_of[z->value];
                else if (z && z->kind == UI_ZONE_ITEM) tap_item = z->value;
                else if (z && z->kind == UI_ZONE_TAB) tap_tab = z->value;
            } else if (abs(dy) > 60 && abs(dy) > abs(dx)) {
                nav |= dy < 0 ? HidNpadButton_AnyDown : HidNpadButton_AnyUp; /* arrastrar hacia arriba baja en la lista */
            }
        }
        /* tocar un elemento lo selecciona; tocarlo otra vez (o si ya estaba seleccionado) lo abre */
#define TAP_SELECT(cursor_var, count) do { \
            if (tap_item >= 0 && tap_item < (count)) { \
                if ((cursor_var) == tap_item) down |= HidNpadButton_A; \
                else { (cursor_var) = tap_item; dirty = true; sfx_play(SFX_NAV); } \
            } } while (0)

        if (down & HidNpadButton_Plus) { plus_pressed = true; break; }

        bool moved = false, accepted = false, accepted_alt = false, opened = false, back = false;

        if (screen == SCREEN_HOME) {
            ui_layout l = ui_home_layout(&g_config);
            TAP_SELECT(cursor, g_model.ngames);
            if (g_model.ngames > 0) moved = move_cursor(&cursor, g_model.ngames, l.cols, nav);
            if (down & HidNpadButton_Y) {
                rescan();
                if (cursor >= g_model.ngames) cursor = g_model.ngames ? g_model.ngames - 1 : 0;
                top = 0;
                accepted_alt = true;
            }
            if ((down & HidNpadButton_Minus) && g_model.ngames > 0) {
                g_config.games_view = (g_config.games_view + 1) % GR_VIEW_COUNT;
                save_config();
                top = 0;
                l = ui_home_layout(&g_config);
                accepted_alt = true;
            }
            follow(cursor, &top, l);
            if ((down & HidNpadButton_A) && g_model.ngames > 0) {
                gr_open(&g_model, cursor, ROOT);
                load_icons(g_model.games[cursor].tid);   /* los que ya esten en la SD se ven de una vez */
                icons_start(g_model.games[cursor].tid);  /* y en segundo plano se buscan los nuevos */
                screen = SCREEN_DETAIL;
                dcursor = dtop = 0;
                opened = true;
            }
            if (down & HidNpadButton_X) { screen = SCREEN_SETTINGS; stab = srow = 0; opened = true; }
            if (down & HidNpadButton_R) { screen = SCREEN_PROFILE; opened = true; }
        } else if (screen == SCREEN_DETAIL) {
            ui_layout l = ui_detail_layout(&g_config);
            int n = g_model.games[cursor].broken ? 0 : g_model.cur.nach;
            if (tap_item >= 0 && tap_item < n && dcursor != tap_item) { dcursor = tap_item; dirty = true; sfx_play(SFX_NAV); }
            if (n > 0) moved = move_cursor(&dcursor, n, l.cols, nav);
            if ((down & HidNpadButton_Minus) && n > 0) {
                g_config.ach_view = (g_config.ach_view + 1) % GR_VIEW_COUNT;
                save_config();
                dtop = 0;
                l = ui_detail_layout(&g_config);
                accepted_alt = true;
            }
            follow(dcursor, &dtop, l);
            if (down & HidNpadButton_B) { screen = SCREEN_HOME; back = true; }
        } else if (screen == SCREEN_SETTINGS) {
            bool linked = g_account.linked;
            if (tap_tab >= 0 && tap_tab < UI_TABS && tap_tab != stab) { stab = tap_tab; srow = 0; accepted_alt = true; }
            TAP_SELECT(srow, ui_tab_count(stab, linked));
            if (down & (HidNpadButton_L | HidNpadButton_R)) {
                stab = (stab + ((down & HidNpadButton_R) ? 1 : UI_TABS - 1)) % UI_TABS;
                srow = 0;
                accepted_alt = true;
            }
            if (srow >= ui_tab_count(stab, linked)) srow = 0;
            moved = move_cursor(&srow, ui_tab_count(stab, linked), 1, nav);
            if (down & HidNpadButton_A) {
                int opt = ui_tab_option(stab, srow, linked);
                switch (opt) {
                    case UI_OPT_LANG:       g_config.lang = (g_config.lang + 1) % GR_LANG_COUNT; break;
                    case UI_OPT_THEME:      g_config.theme = (g_config.theme + 1) % GR_THEME_COUNT; break;
                    case UI_OPT_ANIM:       g_config.animated_bg = !g_config.animated_bg; break;
                    case UI_OPT_GAMES_VIEW: g_config.games_view = (g_config.games_view + 1) % GR_VIEW_COUNT; break;
                    case UI_OPT_ACH_VIEW:   g_config.ach_view = (g_config.ach_view + 1) % GR_VIEW_COUNT; break;
                    case UI_OPT_SOUND:      g_config.sound = !g_config.sound; break;
                    case UI_OPT_POPUP:      g_config.popup = !g_config.popup; break;
                    case UI_OPT_START:      g_config.start_page = (g_config.start_page + 1) % GR_START_COUNT; break;
                    case UI_OPT_ABOUT:      screen = SCREEN_ABOUT; break;
                    case UI_OPT_DIAG:       run_diag(); screen = SCREEN_DIAG; break;
                    case UI_OPT_ACC_LINK: {
                        char name[64], err[160] = "";
                        device_name(name, sizeof(name));
                        gr_net_result r = gr_link_start(net_http, name, &g_account, link_code, err, sizeof(err));
                        if (r == GR_NET_OK) {
                            gr_account_save(&g_account, ACCOUNT_PATH); /* por si se cierra la app antes de terminar */
                            link_started = link_last_poll = armGetSystemTick();
                            link_shown_left = -1;
                            screen = SCREEN_LINK;
                        } else {
                            trace("vincular: fallo %d (%s) %s", (int)r, err, net_last_error());
                            show_message(false, r == GR_NET_OFFLINE ? "Sin conexión" : "No se pudo pedir el código",
                                         r == GR_NET_OFFLINE ? "Conecta la consola a internet e inténtalo de nuevo." : err, NULL);
                            screen = SCREEN_MSG;
                        }
                        break;
                    }
                    case UI_OPT_ACC_LOGIN:
                        login_with_password();
                        screen = SCREEN_MSG;
                        break;
                    case UI_OPT_ACC_SYNC:
                        sync_start();
                        break;
                    case UI_OPT_ACC_UNLINK:
                        memset(&g_account, 0, sizeof(g_account));
                        gr_account_save(&g_account, ACCOUNT_PATH);
                        g_sync = SYNC_IDLE;
                        refresh_greeting();
                        srow = 0;
                        show_message(true, "Consola desvinculada", "Ya no sube logros a tu cuenta.",
                                     "Para quitarla también de la web: Cuenta → Consolas vinculadas.");
                        screen = SCREEN_MSG;
                        break;
                    default: break; /* la fila de informacion no hace nada */
                }
                if (opt < UI_OPT_DIAG) {
                    accepted = true;
                    save_config();
                    top = 0; /* la vista pudo cambiar de forma */
                    follow(cursor, &top, ui_home_layout(&g_config));
                } else if (opt == UI_OPT_ACC_SYNC) {
                    accepted_alt = true;
                } else if (opt != UI_OPT_ACC_INFO) {
                    opened = true;
                }
            }
            if (down & HidNpadButton_B) { screen = SCREEN_HOME; back = true; }
        } else if (screen == SCREEN_LINK) {
            u64 now_tick = armGetSystemTick();
            int left = LINK_SECONDS - (int)(armTicksToNs(now_tick - link_started) / 1000000000ull);
            if (left < 0) left = 0;
            if (left != link_shown_left) { link_shown_left = left; dirty = true; } /* el contador baja cada segundo */

            if (down & HidNpadButton_B) {
                memset(&g_account, 0, sizeof(g_account)); /* el servidor borra solo los codigos sin usar */
                gr_account_save(&g_account, ACCOUNT_PATH);
                screen = SCREEN_SETTINGS;
                back = true;
            } else if (left > 0 && armTicksToNs(now_tick - link_last_poll) / 1000000ull >= LINK_POLL_MS) {
                bool exists = false;
                link_last_poll = now_tick;
                if (gr_whoami(net_http, &g_account, &exists) == GR_NET_OK && exists && g_account.linked) {
                    gr_account_save(&g_account, ACCOUNT_PATH);
                    refresh_greeting();
                    char line[160];
                    snprintf(line, sizeof(line), "Esta consola quedó vinculada a @%s.", g_account.username);
                    show_message(true, "¡Consola vinculada!", line, "Tus logros se suben solos cuando hay internet.");
                    screen = SCREEN_MSG;
                    stab = UI_TAB_ACCOUNT;
                    srow = 0;
                    opened = true;
                    sync_start();
                }
            }
        } else if (screen == SCREEN_PROFILE) {
            if (down & (HidNpadButton_L | HidNpadButton_B)) { screen = SCREEN_HOME; back = true; }
            if (down & HidNpadButton_X) { screen = SCREEN_BADGES; opened = true; }
            if ((down & HidNpadButton_Y) && g_account.linked) {
                follow_with_code();
                g_msg_return = SCREEN_PROFILE;
                screen = SCREEN_MSG;
                opened = true;
            }
        } else if (screen == SCREEN_BADGES) {
            if (down & HidNpadButton_B) { screen = SCREEN_PROFILE; back = true; }
        } else if (screen == SCREEN_MSG) {
            if (down & (HidNpadButton_A | HidNpadButton_B)) {
                screen = g_msg_return;
                if (screen == SCREEN_SETTINGS) { stab = UI_TAB_ACCOUNT; srow = 0; }
                g_msg_return = SCREEN_SETTINGS;
                back = true;
            }
        } else if (screen == SCREEN_DIAG) {
            if (down & HidNpadButton_Y) { rescan(); run_diag(); accepted_alt = true; }
            if (down & HidNpadButton_B) { screen = SCREEN_SETTINGS; back = true; }
        } else if (screen == SCREEN_ABOUT) {
            if (down & HidNpadButton_B) { screen = SCREEN_SETTINGS; back = true; }
        } else {
            if (down & HidNpadButton_A) {
                accepted = true;
                if (++intro_page >= UI_INTRO_PAGES) {
                    g_config.intro_done = true;
                    save_config();
                    refresh_greeting();
                    screen = SCREEN_HOME;
                }
            }
            if ((down & HidNpadButton_B) && intro_page > 0) { intro_page--; back = true; }
        }

        if (back) sfx_play(SFX_BACK);
        else if (opened) sfx_play(SFX_OPEN);
        else if (accepted) sfx_play(SFX_ACCEPT);
        else if (accepted_alt) sfx_play(SFX_ACCEPT_ALT);
        else if (moved) sfx_play(SFX_NAV);
        if (moved || accepted || accepted_alt || opened || back) dirty = true;
        if (sync_collect()) dirty = true;
        if (icons_collect(screen == SCREEN_DETAIL ? g_model.games[cursor].tid : NULL)) {
            load_icons(g_model.games[cursor].tid);
            dirty = true;
        }
        sfx_update();

        /* La pantalla (texto, tarjetas) solo se redibuja en su capa cuando algo cambia.
         * Con el fondo animado, cada cuadro se pinta el fondo y se pone la capa encima. */
        bool redraw = dirty;
        if (dirty) {
            if (screen == SCREEN_DETAIL) ui_detail(&layer, &g_assets, &g_model, &g_config, cursor, dcursor, dtop);
            else if (screen == SCREEN_SETTINGS) {
                ui_account acc = { .linked = g_account.linked, .username = g_account.username, .caption = g_account.caption };
                sync_text(&acc.sync_es, &acc.sync_en);
                ui_settings(&layer, &g_assets, &g_config, stab, srow, g_system_spanish, g_system_dark, &acc);
            }
            else if (screen == SCREEN_PROFILE) ui_profile_screen(&layer, &g_assets, &g_profile);
            else if (screen == SCREEN_BADGES) ui_badges(&layer, &g_assets, &g_profile);
            else if (screen == SCREEN_LINK) ui_link(&layer, &g_assets, link_code, link_shown_left < 0 ? LINK_SECONDS : link_shown_left);
            else if (screen == SCREEN_MSG) ui_message(&layer, &g_assets, g_msg_title, g_msg_line1, g_msg_line2[0] ? g_msg_line2 : NULL, g_msg_ok);
            else if (screen == SCREEN_DIAG) ui_diag(&layer, &g_assets, g_diag, g_ndiag);
            else if (screen == SCREEN_ABOUT) ui_about(&layer, &g_assets);
            else if (screen == SCREEN_INTRO) ui_intro(&layer, &g_assets, intro_page, g_diag, g_ndiag, g_env.sysmodule_running);
            else ui_home(&layer, &g_assets, &g_model, &g_config, cursor, top);
            dirty = false;
        }

        u32 stride;
        u32 *px = (u32 *)framebufferBegin(&fb, &stride);
        if (redraw || g_config.animated_bg) {
            gfx_canvas c = { .px = px, .w = UI_W, .h = UI_H, .stride = (int)(stride / sizeof(u32)) };
            float t = g_config.animated_bg ? (float)(armTicksToNs(armGetSystemTick() - start_tick) / 1000000ull) / 1000.0f : 0.0f;
            ui_background(&c, t);
            gfx_compose(&c, &layer);
        }
        framebufferEnd(&fb);
    }

    /* Cada paso del cierre queda anotado: si la app se congela al salir (por
     * ejemplo con HOME), app-log.txt dice en cual. */
    trace("saliendo del bucle (%s)", plus_pressed ? "boton +" : "lo pidio el sistema: HOME o cierre");
    trace("cerrando red");
    if (g_sync_thread_live) { threadWaitForExit(&g_sync_thread); threadClose(&g_sync_thread); }
    if (g_icon_thread_live) { threadWaitForExit(&g_icon_thread); threadClose(&g_icon_thread); }
    net_exit();
    trace("cerrando audio");
    sfx_exit();
    trace("cerrando caratulas");
    if (g_ns_ok) nsExit();
    trace("cerrando pantalla");
    framebufferClose(&fb);
    trace("cerrando fuente");
    plExit();
    trace(TRACE_DONE);
    return 0;
}
