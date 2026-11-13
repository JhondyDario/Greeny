/*
 * Greeny - visor de logros para el menu de homebrew.
 * Lee lo mismo que escribe el sysmodule: sdmc:/config/greeny/
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
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
#include "toast_aviso_bin.h"
#include "toast_bronce_bin.h"
#include "toast_ok_bin.h"
#include "toast_oro_bin.h"
#include "toast_plata_bin.h"
#include "sysmodule_bin.h"
#include "overlay_bin.h"
#include "update.h"
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
    SCREEN_PROFILE, SCREEN_BADGES, SCREEN_ACH, SCREEN_AVATARS, SCREEN_BROWSE, SCREEN_BROWSE_GAME, SCREEN_SEARCH, SCREEN_RANKING, SCREEN_USER, SCREEN_COUNTRY, SCREEN_CAPTION, SCREEN_REVIEWS, SCREEN_NOTIF, SCREEN_UPDATE
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
static int g_msg_tab = UI_TAB_ACCOUNT;          /* y en que pestaña, si vuelve a Opciones */

/* iconos de los logros del juego abierto */
static uint8_t *g_icons[LG_MAX_ACH], *g_icons_gray[LG_MAX_ACH], *g_icons_big[LG_MAX_ACH], *g_icons_big_gray[LG_MAX_ACH];
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

/* La caratula que el juego tiene en el catalogo, si ya se bajo a la SD (catalog.c). */
static uint8_t *catalog_cover(const char *tid, int *w, int *h) {
    char path[160];
    snprintf(path, sizeof(path), ROOT "/cache/%s/cover.img", tid);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *raw = len > 16 && len <= 1024 * 1024 ? malloc((size_t)len) : NULL, *img = NULL;
    int n;
    if (raw && fread(raw, 1, (size_t)len, f) == (size_t)len) img = stbi_load_from_memory(raw, (int)len, w, h, &n, 4);
    free(raw);
    fclose(f);
    return img;
}

/* Carga la caratula de cada juego: primero la del catalogo; si no hay (sin internet
 * todavia, o el juego no tiene), el icono del juego instalado en la consola. */
static void load_covers(void) {
    static NsApplicationControlData ctrl;
    for (int i = 0; i < GR_MAX_GAMES; i++) {
        free(g_covers[i]);
        free(g_covers_big[i]);
        g_covers[i] = g_covers_big[i] = NULL;
        g_assets.covers[i] = g_assets.covers_big[i] = NULL;
    }

    for (int i = 0; i < g_model.ngames; i++) {
        trace("caratula de %s", g_model.games[i].tid);
        int w, h, n;
        uint8_t *img = catalog_cover(g_model.games[i].tid, &w, &h);
        if (!img && g_ns_ok) {
            u64 tid = strtoull(g_model.games[i].tid, NULL, 16);
            u64 size = 0;
            if (R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, &ctrl, sizeof(ctrl), &size)) &&
                size > sizeof(ctrl.nacp))
                img = stbi_load_from_memory(ctrl.icon, (int)(size - sizeof(ctrl.nacp)), &w, &h, &n, 4);
        }
        if (!img) continue;
        if (w > 0 && h > 0 && w <= 2048 && h <= 2048) {
            g_assets.covers[i] = g_covers[i] = make_cover(img, w, h, UI_COVER);
            g_assets.covers_big[i] = g_covers_big[i] = make_cover(img, w, h, UI_COVER_BIG);
        }
        stbi_image_free(img);
    }
}

static void refresh_greeting(void);

static void rescan(void) {
    trace("leyendo archivos de logros");
    gr_scan(&g_model, ROOT);
    gr_filter(&g_model, g_config.games_scope == GR_SCOPE_ACTIVE, g_config.games_show, g_titles, g_ntitles);
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

static bool g_ui_en; /* la app esta en ingles */

static void apply_config(void) {
    bool dark = g_config.theme == GR_THEME_DARK || (g_config.theme == GR_THEME_AUTO && g_system_dark);
    g_ui_en = g_config.lang == GR_LANG_EN || (g_config.lang == GR_LANG_AUTO && !g_system_spanish);
    ui_set_english(g_ui_en);
    sfx_set_enabled(g_config.app_sound);
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

/* ---------- overlay de Tesla (opcional) ---------- */
static void show_message(bool ok, const char *title, const char *line1, const char *line2);
static bool g_overlay_msg; /* overlay_toggle dejo un mensaje para mostrar */
#define OVERLAY_PATH "sdmc:/switch/.overlays/greeny.ovl"

/* Lo pone o lo quita de la carpeta de Tesla. Sin Tesla no se activa y se explica por que. */
static void overlay_toggle(void) {
    if (g_config.overlay) {
        remove(OVERLAY_PATH);
        g_config.overlay = false;
        return;
    }
    if (!gr_tesla_installed("sdmc:")) {
        show_message(false, "Falta Tesla", "Instala nx-ovlloader y Tesla Menu (o Ultrahand) y reinicia la consola.",
                     "Mira Opciones → Ayuda → Diagnóstico: \"Tesla\" tiene que salir en verde.");
        g_msg_tab = UI_TAB_INGAME;
        g_overlay_msg = true;
        return;
    }
    mkdir("sdmc:/switch/.overlays", 0777);
    install_file(OVERLAY_PATH, overlay_bin, overlay_bin_size);
    struct stat st;
    if (stat(OVERLAY_PATH, &st) != 0) {
        show_message(false, "No se pudo instalar", "No se pudo copiar greeny.ovl a la SD.", NULL);
        g_msg_tab = UI_TAB_INGAME;
        g_overlay_msg = true;
        return;
    }
    g_config.overlay = true;
    show_message(true, "¡Listo!", "En el juego abre Tesla (L + abajo + R3) y elige Greeny.", "Se ve tu progreso sin tocar la memoria del juego.");
    g_msg_tab = UI_TAB_INGAME;
    g_overlay_msg = true;
}

/* Imagenes PNG que van dentro de la app (app/data/img_*.bin). Se decodifican una vez al abrir. */
#define UI_IMG(name, w, h) extern const unsigned char img_##name##_bin[], img_##name##_bin_end[];
#include "ui_images.h"
#undef UI_IMG

static void load_images(void) {
    static const struct { const unsigned char *start, *end; int w, h; } k[UI_IMG_COUNT] = {
#define UI_IMG(name, w, h) { img_##name##_bin, img_##name##_bin_end, w, h },
#include "ui_images.h"
#undef UI_IMG
    };
    int bad = 0;
    for (int i = 0; i < UI_IMG_COUNT; i++) {
        int w, h, n;
        uint8_t *px = stbi_load_from_memory(k[i].start, (int)(k[i].end - k[i].start), &w, &h, &n, 4);
        if (px && (w != k[i].w || h != k[i].h)) { stbi_image_free(px); px = NULL; }
        g_assets.img[i] = px; /* NULL = la pantalla dibuja la version sencilla de antes */
        if (!px) bad++;
    }
    trace("imagenes: %d de %d", UI_IMG_COUNT - bad, UI_IMG_COUNT);
}

#define BOOT2_FLAG SD "/atmosphere/contents/4200000000010690/flags/boot2.flag"
static bool ingame_enabled(void) {
    struct stat st;
    return stat(BOOT2_FLAG, &st) == 0;
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

    /* boot2.flag es lo que enciende los logros dentro de los juegos. Solo se pone en la primera
     * instalacion: si despues alguien lo quita (opcion "Logros en los juegos"), se respeta. */
    struct stat st;
    bool fresh = stat(SYS_DIR "/exefs.nsp", &st) != 0;
    g_sysmodule_updated = install_file(SYS_DIR "/exefs.nsp", sysmodule_bin, sysmodule_bin_size);
    install_file(SYS_DIR "/toolbox.json", toolbox, sizeof(toolbox) - 1);
    if (fresh) install_file(SYS_DIR "/flags/boot2.flag", "", 0);
    install_file(ROOT "/sounds/cargado.pcm", snd_cargado_bin, snd_cargado_bin_size);
    install_file(ROOT "/sounds/logro.pcm", snd_logro_bin, snd_logro_bin_size);
    mkdir(ROOT "/toast", 0777); /* dibujos del aviso dentro del juego */
    install_file(ROOT "/toast/bronce.bin", toast_bronce_bin, toast_bronce_bin_size);
    install_file(ROOT "/toast/plata.bin", toast_plata_bin, toast_plata_bin_size);
    install_file(ROOT "/toast/oro.bin", toast_oro_bin, toast_oro_bin_size);
    install_file(ROOT "/toast/ok.bin", toast_ok_bin, toast_ok_bin_size);
    install_file(ROOT "/toast/aviso.bin", toast_aviso_bin, toast_aviso_bin_size);
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
        free(g_icons_big[i]);
        free(g_icons_big_gray[i]);
        g_icons_big[i] = g_icons_big_gray[i] = NULL;
        g_assets.ach_icons[i] = g_assets.ach_icons_gray[i] = g_assets.ach_big[i] = g_assets.ach_big_gray[i] = NULL;
    }
    for (int i = 0; tid && i < g_model.cur.nach; i++) {
        uint8_t *big = malloc(GR_ICON_BIG_BYTES), *big_gray = malloc(GR_ICON_BIG_BYTES);
        uint8_t *img = malloc(GR_ICON_BYTES), *gray = malloc(GR_ICON_BYTES);
        if (big && big_gray && img && gray && gr_icon_load(ROOT, tid, g_model.cur.ach[i].id, big)) {
            gfx_scale_rgba(big, GR_ICON_BIG, GR_ICON_BIG, img, GR_ICON_SIZE, GR_ICON_SIZE); /* para la lista */
            gr_icon_gray(big, big_gray, GR_ICON_BIG);
            gr_icon_gray(img, gray, GR_ICON_SIZE);
            g_assets.ach_icons[i] = g_icons[i] = img;
            g_assets.ach_icons_gray[i] = g_icons_gray[i] = gray;
            g_assets.ach_big[i] = g_icons_big[i] = big;
            g_assets.ach_big_gray[i] = g_icons_big_gray[i] = big_gray;
        } else {
            free(big);
            free(big_gray);
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
    /* la frase sale en el idioma de la app cuando es una de las de la lista (el servidor manda su traduccion) */
    g_profile.caption = !linked ? "" : (g_ui_en && g_account.caption_en[0]) ? g_account.caption_en : g_account.caption;
    g_profile.country = linked ? g_account.country : "";
    g_profile.friend_code = linked ? g_account.friend_code : "";
    g_profile.joined = linked ? g_account.joined : "";
    g_profile.likes = linked ? g_account.likes : 0;
    g_profile.avatar = linked ? g_account.avatar : 0;
    g_profile.donor = linked && g_account.donor;
    ui_set_admin(linked && g_account.admin);
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
/* ---------- actualizar la app ---------- */
static gr_release g_rel;
static volatile int g_upd = UI_UPD_UNKNOWN;
static bool g_upd_prompt;              /* hay version nueva: mostrarla una vez al volver al inicio */
static char g_self_path[256] = "sdmc:/switch/greeny.nro"; /* donde esta esta app; sale de argv[0] */
static volatile long g_dl_done, g_dl_total;
static volatile int g_dl_result;       /* 0 bajando, 1 listo, -1 fallo */
static Thread g_dl_thread;
static bool g_dl_live;

/* Pregunta a GitHub por la ultima version. Bloquea un momento: llamarla desde el hilo o a peticion. */
static void update_check(void) {
    static char json[96 * 1024];
    size_t len = 0;
    long status = 0;
    if (!net_online()) { g_upd = UI_UPD_OFFLINE; return; }
    if (net_get(GR_UPDATE_API, json, sizeof(json) - 1, &len, &status) != 0) { g_upd = UI_UPD_OFFLINE; return; }
    json[len] = 0;
    if (status == 404) { g_upd = UI_UPD_NONE; return; } /* aun no hay ninguna release */
    gr_release r;
    if (status != 200 || !gr_parse_release(json, &r)) { g_upd = status == 200 ? UI_UPD_NONE : UI_UPD_OFFLINE; return; }
    g_rel = r;
    g_upd = gr_version_newer(r.version, UI_APP_VERSION) ? UI_UPD_AVAILABLE : UI_UPD_CURRENT;
    trace("actualizar: ultima %s, esta %s", r.version, UI_APP_VERSION);
}

static void download_thread(void *arg) {
    (void)arg;
    char tmp[280];
    snprintf(tmp, sizeof(tmp), "%s.new", g_self_path);
    int ok = net_download(g_rel.nro_url, tmp, &g_dl_done, &g_dl_total) == 0;
    if (ok) { /* que sea un NRO de verdad antes de reemplazar la app */
        unsigned char head[0x20] = {0};
        struct stat st;
        FILE *f = fopen(tmp, "rb");
        size_t n = f ? fread(head, 1, sizeof(head), f) : 0;
        if (f) fclose(f);
        ok = stat(tmp, &st) == 0 && gr_nro_valid(head, n, (long)st.st_size) && (g_rel.size <= 0 || st.st_size == g_rel.size);
    }
    if (ok) {
        char old[280];
        snprintf(old, sizeof(old), "%s.old", g_self_path);
        remove(old);
        rename(g_self_path, old);                 /* por si algo sale mal, la anterior queda guardada */
        ok = rename(tmp, g_self_path) == 0;
        if (ok) remove(old);
        else rename(old, g_self_path);
    }
    if (!ok) remove(tmp);
    g_dl_result = ok ? 1 : -1;
}

static bool download_start(void) {
    g_dl_done = g_dl_total = 0;
    g_dl_result = 0;
    if (R_SUCCEEDED(threadCreate(&g_dl_thread, download_thread, NULL, NULL, 0x40000, 0x2C, -2)) && R_SUCCEEDED(threadStart(&g_dl_thread))) {
        g_dl_live = true;
        return true;
    }
    return false;
}

/* ---------- avisos ---------- */
static gr_notif g_notifs[GR_NOTIFS_MAX], g_sync_notifs[GR_NOTIFS_MAX];
static int g_nnotif, g_unread, g_sync_nnotif, g_sync_unread;
static bool g_sync_notifs_ok;
static u64 g_last_sync_tick; /* para volver a sincronizar cada 5 minutos */

static bool g_upd_checked;
static void sync_thread(void *arg) {
    if (!g_upd_checked) { g_upd_checked = true; update_check(); if (g_upd == UI_UPD_AVAILABLE) g_upd_prompt = true; }
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
            /* despues de subir, para que ya salgan los avisos de los logros recien subidos */
            if (gr_notifications(net_http, &g_sync_account, g_sync_notifs, GR_NOTIFS_MAX, &g_sync_nnotif, &g_sync_unread) == GR_NET_OK)
                g_sync_notifs_ok = true;
        }
    }
    g_bg_done = true;
}

static void sync_start(void) {
    g_last_sync_tick = armGetSystemTick();
    g_sync_notifs_ok = false;
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
    if (g_sync_notifs_ok) {
        memcpy(g_notifs, g_sync_notifs, sizeof(g_notifs));
        g_nnotif = g_sync_nnotif;
        g_unread = g_sync_unread;
    }
    ui_set_unread(g_account.token[0] && g_account.linked ? g_unread : -1);
    ui_set_update(g_upd, g_rel.version);
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

/* ---------- buscador de valores (lo ejecuta el sysmodule; aqui solo se le dan ordenes) ---------- */
#define SEARCH_DIR ROOT "/search"
static char g_search_lines[UI_SEARCH_LINES][128];
static int g_search_nlines;

static void search_send(const char *fmt, ...) {
    mkdir(SEARCH_DIR, 0777);
    remove(SEARCH_DIR "/result.txt"); /* para no confundir la respuesta vieja con la nueva */
    FILE *f = fopen(SEARCH_DIR "/cmd.txt", "w");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
    g_search_nlines = 1;
    snprintf(g_search_lines[0], sizeof(g_search_lines[0]), "Orden enviada. Esperando al sistema de logros...");
}

/* Lee la respuesta. Devuelve true si cambio lo que hay que mostrar. */
static bool search_read(void) {
    FILE *f = fopen(SEARCH_DIR "/result.txt", "r");
    if (!f) return false;
    static char fresh[UI_SEARCH_LINES][128];
    int n = 0;
    while (n < UI_SEARCH_LINES && fgets(fresh[n], sizeof(fresh[n]), f)) {
        fresh[n][strcspn(fresh[n], "\r\n")] = 0;
        if (fresh[n][0]) n++;
    }
    fclose(f);
    if (n == 0) return false;
    bool changed = n != g_search_nlines || memcmp(fresh, g_search_lines, sizeof(fresh[0]) * (size_t)n) != 0;
    if (changed) { memcpy(g_search_lines, fresh, sizeof(fresh)); g_search_nlines = n; }
    return changed;
}

/* ---------- ranking ---------- */
static gr_rank_row g_rank[GR_RANK_TOP], g_rank_me, g_rank_view;
static int g_nrank, g_rank_total;

/* Trae el ranking. Devuelve el estado para ui_ranking: 1 listo, 2 sin conexion, 3 falta elegir pais.
 * mode: 0 = puntos totales; k > 0 = el k-esimo juego en el que ya hay algun logro. */
static int rank_load(int scope, int mode) {
    static const char *scopes[3] = { "global", "country", "friends" };
    const char *title = "";
    for (int i = 0, k = 0; i < g_model.ngames && mode > 0; i++)
        if (g_model.games[i].unlocked > 0 && ++k == mode) title = g_model.games[i].tid;
    if (scope == 1 && !g_account.country[0]) return 3;
    if (!net_online()) return 2;
    gr_net_result r = gr_ranking(net_http, &g_account, scopes[scope], scope == 1 ? g_account.country : "", title, g_rank, &g_nrank, &g_rank_me, &g_rank_total);
    if (r != GR_NET_OK) { trace("ranking: fallo %d %s", (int)r, net_last_error()); return 2; }
    return 1;
}

/* ---------- comentarios de un juego ---------- */
static gr_review g_reviews[GR_REVIEWS_MAX];
static int g_nrev, g_rev_state;
static char g_rev_tid[17], g_rev_name[LG_NAME_LEN];
static bool g_rev_closed; /* juego "sin empezar": no se comenta */
static int g_rev_return, g_user_return = 0, g_badges_return = SCREEN_PROFILE; /* a donde vuelven Comentarios y el perfil de otro (0 = ranking) */

/* 1 listo, 2 sin conexion */
static bool cat_not_started(const char *tid);
static int reviews_load(void) {
    g_nrev = 0;
    /* el estado sale del catalogo si ya se consulto; si no se sabe, decide el servidor al guardar */
    g_rev_closed = cat_not_started(g_rev_tid);
    if (!net_online()) return 2;
    if (gr_reviews(net_http, &g_account, g_rev_tid, g_reviews, GR_REVIEWS_MAX, &g_nrev) != GR_NET_OK) {
        trace("comentarios: fallo %s", net_last_error());
        return 2;
    }
    return 1;
}

/* ---------- enlaces de la comunidad ---------- */
/* Se leen de la web (links.json) para cambiarlos sin sacar otra version de la app. */
static char g_discord[160];
static bool g_links_tried;

static void links_load(void) {
    if (g_links_tried || !net_online()) return;
    g_links_tried = true;
    char json[1024], url[160] = "";
    size_t len = 0;
    long status = 0;
    if (net_get("https://jhondydario.github.io/Greeny/links.json", json, sizeof(json) - 1, &len, &status) != 0 || status != 200) return;
    json[len] = 0;
    /* solo invitaciones de Discord: nada de enlaces raros en la pantalla */
    if (gr_json_string(json, "discord", url, sizeof(url)) &&
        (!strncmp(url, "https://discord.gg/", 19) || !strncmp(url, "https://discord.com/invite/", 27)))
        snprintf(g_discord, sizeof(g_discord), "%s", url);
}

/* 1 listo, 2 sin conexion, 3 sin cuenta */
static int notifs_load(void) {
    if (!(g_account.token[0] && g_account.linked)) return 3;
    if (!net_online()) return 2;
    if (gr_notifications(net_http, &g_account, g_notifs, GR_NOTIFS_MAX, &g_nnotif, &g_unread) != GR_NET_OK) return 2;
    ui_set_unread(g_unread);
    return 1;
}

/* ---------- frases del perfil ---------- */
static gr_caption g_captions[GR_CAPTIONS_MAX];
static int g_ncaptions;

static bool captions_load(void) {
    static char json[8 * 1024];
    size_t len = 0;
    long status = 0;
    g_ncaptions = 0;
    if (!net_online() || net_get(GR_BACKEND_URL "/rest/v1/caption_presets?select=text_es,text_en&order=sort", json, sizeof(json) - 1, &len, &status) != 0 || status != 200)
        return false;
    json[len] = 0;
    g_ncaptions = gr_parse_captions(json, g_captions, GR_CAPTIONS_MAX);
    return true;
}

/* ---------- musica de fondo ---------- */
#define MUSIC_URL "https://jhondydario.github.io/Greeny/music/"
#define MUSIC_DIR ROOT "/music"
static ui_song g_songs[] = {
    { "rest_garden", "Rest garden", 1171, false },
    { "cold_in_summer", "Cold in summer", 1112, false },
    { "beyond_the_end", "Beyond the end", 1295, false },
    { "excited_for_the_last_time", "Excited for the last time", 1244, false },
};
#define NSONGS ((int)(sizeof(g_songs) / sizeof(g_songs[0])))

static int g_music_check; /* cuadros que faltan para anotar en el registro si la musica avanza */

static void song_path(int i, char *out, size_t cap) { snprintf(out, cap, MUSIC_DIR "/%s.ogg", g_songs[i].id); }

static void music_refresh(void) {
    for (int i = 0; i < NSONGS; i++) {
        char path[160];
        struct stat st;
        song_path(i, path, sizeof(path));
        g_songs[i].have = stat(path, &st) == 0 && st.st_size > 1024;
    }
}

/* Pone a sonar lo que diga la configuracion, o calla si la musica esta apagada o falta el archivo. */
static void music_apply(void) {
    sfx_music_stop();
    if (!g_config.music) return;
    for (int i = 0; i < NSONGS; i++) {
        if (strcmp(g_config.music_track, g_songs[i].id) || !g_songs[i].have) continue;
        char path[160];
        song_path(i, path, sizeof(path));
        bool ok = sfx_music_play(path);
        trace("musica: %s -> %s (%s)", g_songs[i].id, ok ? "puesta" : "no se pudo", sfx_music_note());
        g_music_check = 180; /* en 3 segundos se anota si de verdad esta sonando */
        return;
    }
}

/* Al terminar una cancion sigue la siguiente que este descargada (si solo hay una, se repite). */
static void music_next(void) {
    int now = -1;
    for (int i = 0; i < NSONGS; i++)
        if (!strcmp(g_config.music_track, g_songs[i].id)) now = i;
    for (int step = 1; step <= NSONGS; step++) {
        int i = (now + step + NSONGS) % NSONGS;
        if (!g_songs[i].have) continue;
        snprintf(g_config.music_track, sizeof(g_config.music_track), "%s", g_songs[i].id);
        break;
    }
    music_apply();
}

static bool music_download(int i) {
    const size_t cap = 4 * 1024 * 1024;
    char url[200], path[160];
    uint8_t *buf = malloc(cap);
    if (!buf) return false;
    size_t len = 0;
    long status = 0;
    snprintf(url, sizeof(url), MUSIC_URL "%s.ogg", g_songs[i].id);
    bool ok = net_online() && net_get(url, buf, cap, &len, &status) == 0 && status == 200 && len > 1024 && !memcmp(buf, "OggS", 4);
    if (ok) {
        mkdir(MUSIC_DIR, 0777);
        song_path(i, path, sizeof(path));
        ok = install_file(path, buf, len);
        struct stat st;
        if (!ok) ok = stat(path, &st) == 0 && (size_t)st.st_size == len; /* install_file devuelve false si ya estaba igual */
    } else trace("musica: descarga de %s fallo (%ld) %s", g_songs[i].id, status, net_last_error());
    free(buf);
    return ok;
}

/* ---------- buscador de juegos del catalogo ---------- */
#define CAT_MAX 300
static gr_cat_game g_cat[CAT_MAX];
static bool g_cat_have[CAT_MAX];   /* ese juego ya tiene archivo de logros en esta consola */
static int g_cat_shown[CAT_MAX], g_cat_n, g_cat_nshown;
static char g_cat_query[40];
static ui_browse_state g_cat_state = UI_BROWSE_LOADING;
static lg_game g_cat_def;          /* logros del juego que se esta mirando */
static int g_cat_open;             /* indice en g_cat */

static bool contains_nocase(const char *text, const char *word) {
    size_t n = strlen(word);
    for (; *text; text++)
        if (!strncasecmp(text, word, n)) return true;
    return n == 0;
}

/* Filtro por estado del catalogo: 0 = todos. No se guarda: al abrir la app vuelve a "todos". */
static int g_cat_status;
static const char *const k_cat_status[UI_BROWSE_FILTERS] = { "", "sin_empezar", "prueba", "jugable", "perfecto" };

/* true si el catalogo dice que ese juego esta "sin empezar" (sin logros: no se comenta) */
static bool cat_not_started(const char *tid) {
    for (int i = 0; i < g_cat_n; i++)
        if (!strcmp(g_cat[i].tid, tid)) return !g_cat[i].status[0] || !strcmp(g_cat[i].status, "sin_empezar");
    return false;
}

static void browse_filter(void) {
    g_cat_nshown = 0;
    for (int i = 0; i < g_cat_n; i++) {
        if (g_cat_status > 0 && strcmp(g_cat[i].status, k_cat_status[g_cat_status]) != 0) continue;
        if (contains_nocase(g_cat[i].name, g_cat_query) || contains_nocase(g_cat[i].tid, g_cat_query)) g_cat_shown[g_cat_nshown++] = i;
    }
}

/* ---- caratulas del catalogo: se bajan en segundo plano y se cargan solo las que se ven ---- */
static uint8_t *g_cat_cover[CAT_MAX];
static uint8_t g_cat_cover_state[CAT_MAX]; /* 0 sin probar, 1 cargada, 2 todavia no hay */
static Thread g_cov_thread;
static bool g_cov_live;
static volatile bool g_cov_done;

static void cover_thread(void *arg) {
    (void)arg;
    for (int i = 0; i < g_cat_n; i++)
        if (g_cat[i].cover_path[0]) gr_catalog_fetch_cover(net_get, ROOT, g_cat[i].tid, g_cat[i].cover_path);
    g_cov_done = true;
}

static void browse_covers_reset(void) {
    if (g_cov_live) { threadWaitForExit(&g_cov_thread); threadClose(&g_cov_thread); g_cov_live = false; }
    for (int i = 0; i < CAT_MAX; i++) { free(g_cat_cover[i]); g_cat_cover[i] = NULL; g_cat_cover_state[i] = 0; }
}

/* Carga las caratulas de las filas que se ven. Devuelve true si cargo alguna (hay que redibujar). */
static bool browse_covers_tick(int top) {
    bool changed = false;
    if (g_cov_live && g_cov_done) { /* termino de bajar: probar otra vez las que faltaban */
        threadWaitForExit(&g_cov_thread);
        threadClose(&g_cov_thread);
        g_cov_live = false;
        for (int i = 0; i < g_cat_n; i++) if (g_cat_cover_state[i] == 2) g_cat_cover_state[i] = 0;
    }
    for (int k = top; k < g_cat_nshown && k < top + UI_BROWSE_ROWS; k++) {
        int i = g_cat_shown[k];
        if (g_cat_cover_state[i] != 0) continue;
        int w, h, n;
        uint8_t *img = catalog_cover(g_cat[i].tid, &w, &h);
        if (!img && g_ns_ok) { /* sin caratula en el catalogo: el icono del juego si esta instalado */
            static NsApplicationControlData ctrl;
            u64 size = 0;
            if (R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage, strtoull(g_cat[i].tid, NULL, 16), &ctrl, sizeof(ctrl), &size)) &&
                size > sizeof(ctrl.nacp))
                img = stbi_load_from_memory(ctrl.icon, (int)(size - sizeof(ctrl.nacp)), &w, &h, &n, 4);
        }
        if (img && w > 0 && h > 0 && w <= 2048 && h <= 2048) {
            g_cat_cover[i] = make_cover(img, w, h, UI_BROWSE_COVER);
            g_cat_cover_state[i] = g_cat_cover[i] ? 1 : 2;
            changed = true;
        } else g_cat_cover_state[i] = 2;
        stbi_image_free(img);
    }
    return changed;
}

static void browse_load(void) {
    browse_covers_reset();
    g_cat_n = 0;
    if (!net_online() || gr_catalog_list(net_get, g_cat, CAT_MAX, &g_cat_n) != 0) { g_cat_state = UI_BROWSE_OFFLINE; g_cat_nshown = 0; return; }
    for (int i = 0; i < g_cat_n; i++) {
        char path[160];
        struct stat st;
        snprintf(path, sizeof(path), ROOT "/games/%.16s.txt", g_cat[i].tid);
        g_cat_have[i] = stat(path, &st) == 0;
    }
    g_cat_state = UI_BROWSE_OK;
    browse_filter();
    g_cov_done = false;
    if (R_SUCCEEDED(threadCreate(&g_cov_thread, cover_thread, NULL, NULL, 0x20000, 0x2D, -2)) && R_SUCCEEDED(threadStart(&g_cov_thread)))
        g_cov_live = true;
}

/* Baja y lee los logros de un juego del catalogo. false si no se pudo. */
static bool browse_open(int index) {
    char *text = malloc(64 * 1024);
    if (!text) return false;
    char err[128];
    bool ok = gr_catalog_definition(net_get, g_cat[index].tid, text, 64 * 1024) && lg_parse(&g_cat_def, text, err, sizeof(err)) == 0;
    free(text);
    g_cat_open = index;
    return ok;
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
    if (argc > 0 && argv[0] && !strncmp(argv[0], "sdmc:/", 6) && strstr(argv[0], ".nro"))
        snprintf(g_self_path, sizeof(g_self_path), "%s", argv[0]);
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

    trace("imagenes");
    load_images();

    trace("instalacion");
    install_sysmodule();
    ui_set_ingame(ingame_enabled());
    list_installed_titles();

    trace("configuracion");
    gr_config_load(&g_config, CONFIG_PATH);
    /* si el overlay esta activado, se pone al dia con el de esta version (solo escribe si cambio) */
    if (g_config.overlay && gr_tesla_installed("sdmc:")) install_file(OVERLAY_PATH, overlay_bin, overlay_bin_size);
    apply_config();
    music_refresh();
    ui_set_music(g_songs, NSONGS, -1);
    music_apply();
    rescan();
    /* capa donde se dibuja la pantalla; el fondo se pinta aparte en cada cuadro */
    static u32 layer_px[UI_W * UI_H];
    gfx_canvas layer = { .px = layer_px, .w = UI_W, .h = UI_H, .stride = UI_W };
    const u64 start_tick = armGetSystemTick();

    trace("listo: %d juego(s), audio %s, caratulas %s, modo %s", g_model.ngames, sfx_ok ? "si" : "no",
          g_ns_ok ? "si" : "no", g_env.applet_mode ? "applet" : "aplicacion");

    int cursor = 0, top = 0, dcursor = 0, dtop = 0, stab = 0, srow = 0, intro_page = 0, acursor = 0, bcursor = 0, btop = 0, browse_pending = 0, song_pending = 0, song_wait = 0, vcursor = 0, vtop = 0, ncursor = 0, ntop = 0, notif_state = 0;
    screen_t notif_return = SCREEN_HOME, upd_return = SCREEN_HOME;
    bool song_all = false;
    int ccursor = 0, rcursor = 0, rank_scope = 0, rank_mode = 0, rank_state = 0, kcursor = 0, ktop = 0;
    bool lang_changed = false;
    int ach_filter = UI_ACH_ALL;
    static int ach_map[LG_MAX_ACH];
    int frow = 0, ftype = 0, fcand = 1; /* fcand: cuanto cambia el numero cada vez */
    unsigned fvalue = 10;
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
    ui_set_unread(g_account.token[0] && g_account.linked ? 0 : -1); /* la campana sale si hay cuenta */
    sync_start(); /* en segundo plano y solo si hay cuenta e internet; la app no espera por esto */

    /* tactil: un toque corto es una pulsacion; un arrastre vertical mueve la lista */
    bool touching = false;
    int touch_x0 = 0, touch_y0 = 0, touch_x = 0, touch_y = 0;

    char link_code[8] = "";
    u64 link_started = 0, link_last_poll = 0;
    int link_shown_left = -1;
    bool dirty = true, plus_pressed = false;
    bool in_focus = true;
    /* Que la app siga viva al pulsar HOME en vez de quedar suspendida: asi puede callar la musica,
     * dejar de dibujar y anotar en el registro lo que pasa. */
    if (!g_env.applet_mode) appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
    const u64 dirs = HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight;

    while (appletMainLoop()) {
        /* HOME: mientras la app esta en segundo plano no dibuja ni lee botones, y la musica se calla */
        if (!g_env.applet_mode) {
            bool focus = appletGetFocusState() == AppletFocusState_InFocus;
            if (focus != in_focus) {
                in_focus = focus;
                trace(focus ? "de vuelta en primer plano" : "en segundo plano (HOME)");
                sfx_music_pause(!focus);
                dirty = true;
            }
            if (!in_focus) { sfx_update(); svcSleepThread(50000000ull); continue; }
        }
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

        if ((down & HidNpadButton_Plus) && !g_dl_live) { plus_pressed = true; break; } /* no salir a mitad de una actualizacion */

        bool moved = false, accepted = false, accepted_alt = false, opened = false, opened_rare = false, back = false;
        bool quiet = false; /* la accion ya suena sola (elegir una cancion): sin el sonido del boton encima */

        /* ZL o ZR abren los avisos desde cualquier pantalla; B vuelve a donde estabas */
        if ((down & (HidNpadButton_ZL | HidNpadButton_ZR)) && screen != SCREEN_NOTIF && screen != SCREEN_INTRO && screen != SCREEN_MSG && !g_dl_live) {
            notif_return = screen;
            notif_state = notifs_load();
            ncursor = ntop = 0;
            screen = SCREEN_NOTIF;
            opened = true;
            down &= ~(u64)(HidNpadButton_ZL | HidNpadButton_ZR);
        }
        /* cada 5 minutos, sincronizar otra vez (sube logros nuevos y trae avisos) */
        if (!g_sync_thread_live && g_account.linked && armTicksToNs(armGetSystemTick() - g_last_sync_tick) > 300000000000ULL) sync_start();

        /* hay version nueva: se ofrece una vez, al estar en el inicio */
        if (g_upd_prompt && screen == SCREEN_HOME && !g_sync_thread_live) {
            g_upd_prompt = false;
            upd_return = SCREEN_HOME;
            screen = SCREEN_UPDATE;
            dirty = true;
        }

        if (screen == SCREEN_UPDATE) {
            if (g_dl_live) {
                dirty = true; /* para que avance la barra */
                if (g_dl_result != 0) {
                    threadWaitForExit(&g_dl_thread);
                    threadClose(&g_dl_thread);
                    g_dl_live = false;
                    if (g_dl_result > 0) {
                        char l1[160];
                        snprintf(l1, sizeof(l1), "Se instaló la versión %s. Cierra Greeny con + y ábrela otra vez para usarla.", g_rel.version);
                        show_message(true, "¡Actualizada!", l1, "Si la versión trae un sysmodule nuevo, la app te pedirá reiniciar la consola.");
                        g_upd = UI_UPD_CURRENT;
                    } else {
                        show_message(false, "No se pudo actualizar", "La descarga falló o el archivo no era válido. Tu app sigue igual.", "Revisa la conexión e inténtalo de nuevo.");
                    }
                    ui_set_update(g_upd, g_rel.version);
                    g_msg_return = SCREEN_HOME;
                    screen = SCREEN_MSG;
                }
            } else {
                if ((down & HidNpadButton_A) && g_upd == UI_UPD_AVAILABLE) {
                    if (!download_start()) {
                        show_message(false, "No se pudo actualizar", "No se pudo empezar la descarga.", NULL);
                        g_msg_return = upd_return;
                        screen = SCREEN_MSG;
                    }
                    accepted = true;
                }
                if ((down & HidNpadButton_Y) && g_upd != UI_UPD_AVAILABLE) { update_check(); ui_set_update(g_upd, g_rel.version); accepted_alt = true; }
                if (down & HidNpadButton_B) { screen = upd_return; back = true; }
            }
        } else if (screen == SCREEN_NOTIF) {
            if (ncursor >= g_nnotif) ncursor = g_nnotif ? g_nnotif - 1 : 0;
            if (notif_state == 1 && g_nnotif > 0) { TAP_SELECT(ncursor, g_nnotif); moved = move_cursor(&ncursor, g_nnotif, 1, nav); }
            if (ncursor < ntop) ntop = ncursor;
            if (ncursor >= ntop + UI_NOTIF_ROWS) ntop = ncursor - UI_NOTIF_ROWS + 1;
            if ((down & HidNpadButton_X) && notif_state == 1 && g_unread > 0) {
                if (gr_notifications_read(net_http, &g_account, -1) == GR_NET_OK) {
                    for (int i = 0; i < g_nnotif; i++) g_notifs[i].read = true;
                    g_unread = 0;
                    ui_set_unread(0);
                }
                accepted_alt = true;
            }
            if ((down & HidNpadButton_A) && notif_state == 1 && g_nnotif > 0) {
                gr_notif *v = &g_notifs[ncursor];
                if (!v->read && gr_notifications_read(net_http, &g_account, v->id) == GR_NET_OK) {
                    v->read = true;
                    if (g_unread > 0) g_unread--;
                    ui_set_unread(g_unread);
                }
                /* llevar a lo que dice el aviso */
                if (v->kind == GR_NOTIF_LIKE && v->title_id[0]) {
                    snprintf(g_rev_tid, sizeof(g_rev_tid), "%s", v->title_id);
                    snprintf(g_rev_name, sizeof(g_rev_name), "%s", v->game[0] ? v->game : v->title_id);
                    g_rev_state = reviews_load();
                    g_rev_return = SCREEN_NOTIF;
                    vcursor = vtop = 0;
                    screen = SCREEN_REVIEWS;
                } else if (v->kind == GR_NOTIF_FOLLOW && v->actor[0]) {
                    memset(&g_rank_view, 0, sizeof(g_rank_view));
                    snprintf(g_rank_view.username, sizeof(g_rank_view.username), "%s", v->actor);
                    snprintf(g_rank_view.display, sizeof(g_rank_view.display), "%s", v->actor_name);
                    g_rank_view.avatar = v->actor_avatar;
                    g_rank_view.position = -1; /* sin datos de ranking: ui_user no muestra numeros */
                    g_user_return = SCREEN_NOTIF;
                    screen = SCREEN_USER;
                } else if (v->kind == GR_NOTIF_BADGE) {
                    screen = SCREEN_BADGES;
                    g_badges_return = SCREEN_NOTIF;
                } else if (v->kind == GR_NOTIF_ACH && v->title_id[0]) {
                    for (int i = 0; i < g_model.ngames; i++)
                        if (!strcmp(g_model.games[i].tid, v->title_id)) {
                            cursor = i;
                            gr_open(&g_model, cursor, ROOT);
                            load_icons(g_model.games[cursor].tid);
                            icons_start(g_model.games[cursor].tid);
                            dcursor = dtop = 0;
                            screen = SCREEN_DETAIL;
                        }
                }
                accepted = true;
            }
            if (down & HidNpadButton_B) { screen = notif_return; back = true; }
        } else if (screen == SCREEN_HOME) {
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
            if (down & HidNpadButton_X) { screen = SCREEN_SETTINGS; stab = srow = 0; accepted_alt = true; }
            if (down & HidNpadButton_R) { screen = SCREEN_PROFILE; accepted_alt = true; }
            if (down & HidNpadButton_L) {
                screen = SCREEN_BROWSE;
                bcursor = btop = 0;
                g_cat_query[0] = 0;
                browse_load(); /* se consulta antes de cambiar de pantalla: asi no parpadea un "Buscando…" de medio segundo */
                accepted_alt = true;
            }
        } else if (screen == SCREEN_BROWSE) {
            if (browse_pending && --browse_pending == 0) { browse_load(); dirty = true; }
            if (g_cat_state == UI_BROWSE_OK && browse_covers_tick(btop)) dirty = true;
            else if (browse_pending) dirty = true;
            TAP_SELECT(bcursor, g_cat_nshown);
            if (g_cat_nshown > 0) moved = move_cursor(&bcursor, g_cat_nshown, 1, nav);
            if (bcursor < btop) btop = bcursor;
            if (bcursor >= btop + UI_BROWSE_ROWS) btop = bcursor - UI_BROWSE_ROWS + 1;
            if (down & (HidNpadButton_B | HidNpadButton_R)) { screen = SCREEN_HOME; back = true; }
            if (down & HidNpadButton_Y) { browse_load(); bcursor = btop = 0; accepted_alt = true; }
            if ((down & HidNpadButton_Minus) && g_cat_state == UI_BROWSE_OK) { /* filtro: todos -> sin empezar -> en prueba -> jugable -> perfecto */
                g_cat_status = (g_cat_status + 1) % UI_BROWSE_FILTERS;
                browse_filter();
                bcursor = btop = 0;
                accepted_alt = true;
            }
            if ((down & HidNpadButton_X) && g_cat_state == UI_BROWSE_OK && g_cat_nshown > 0) {
                /* saltar al primer juego de la letra siguiente; al final vuelve al principio */
                char now = (char)toupper((unsigned char)g_cat[g_cat_shown[bcursor]].name[0]);
                int next = 0;
                for (int i = bcursor + 1; i < g_cat_nshown; i++)
                    if ((char)toupper((unsigned char)g_cat[g_cat_shown[i]].name[0]) != now) { next = i; break; }
                bcursor = next;
                if (bcursor < btop) btop = bcursor;
                if (bcursor >= btop + UI_BROWSE_ROWS) btop = bcursor - UI_BROWSE_ROWS + 1;
                accepted_alt = true;
            }
            if ((down & HidNpadButton_A) && g_cat_state == UI_BROWSE_OK && g_cat_nshown > 0) {
                int idx = g_cat_shown[bcursor], local = -1;
                for (int i = 0; i < g_model.ngames; i++)
                    if (!strcmp(g_model.games[i].tid, g_cat[idx].tid)) local = i;
                if (local >= 0) { /* ya esta en la consola: se abre con tu progreso */
                    cursor = local;
                    gr_open(&g_model, cursor, ROOT);
                    load_icons(g_model.games[cursor].tid);
                    icons_start(g_model.games[cursor].tid);
                    screen = SCREEN_DETAIL;
                    dcursor = dtop = 0;
                } else if (browse_open(idx)) {
                    screen = SCREEN_BROWSE_GAME;
                    dcursor = dtop = 0;
                } else {
                    show_message(false, "No se pudo abrir", "No se pudieron bajar los logros de ese juego.", "Revisa la conexión e inténtalo de nuevo.");
                    g_msg_return = SCREEN_BROWSE;
                    screen = SCREEN_MSG;
                }
                accepted = true;
            }
        } else if (screen == SCREEN_BROWSE_GAME) {
            moved = move_cursor(&dcursor, g_cat_def.nach, 1, nav);
            if (dcursor < dtop) dtop = dcursor;
            if (dcursor >= dtop + 6) dtop = dcursor - 5;
            if (down & HidNpadButton_B) { screen = SCREEN_BROWSE; back = true; }
            if (down & HidNpadButton_Y) {
                snprintf(g_rev_tid, sizeof(g_rev_tid), "%s", g_cat[g_cat_open].tid);
                snprintf(g_rev_name, sizeof(g_rev_name), "%s", g_cat[g_cat_open].name);
                g_rev_state = reviews_load();
                g_rev_return = SCREEN_BROWSE_GAME;
                vcursor = vtop = 0;
                screen = SCREEN_REVIEWS;
                accepted_alt = true;
            }
        } else if (screen == SCREEN_REVIEWS) {
            if (vcursor >= g_nrev) vcursor = g_nrev ? g_nrev - 1 : 0;
            if (g_rev_state == 1 && g_nrev > 0) { TAP_SELECT(vcursor, g_nrev); moved = move_cursor(&vcursor, g_nrev, 1, nav); }
            if (vcursor < vtop) vtop = vcursor;
            if (vcursor >= vtop + UI_REVIEW_ROWS) vtop = vcursor - UI_REVIEW_ROWS + 1;
            bool have = g_rev_state == 1 && g_nrev > 0;
            if ((down & HidNpadButton_A) && have && g_account.linked && !g_reviews[vcursor].mine) {
                gr_review *v = &g_reviews[vcursor];
                char err[160] = "";
                gr_net_result r = net_online() ? gr_review_like(net_http, &g_account, v->id, !v->liked, err, sizeof(err)) : GR_NET_OFFLINE;
                if (r == GR_NET_OK) { v->liked = !v->liked; v->likes += v->liked ? 1 : -1; }
                else {
                    show_message(false, "No se pudo", r == GR_NET_OFFLINE ? "No hay conexión a internet." : err, NULL);
                    g_msg_return = SCREEN_REVIEWS;
                    screen = SCREEN_MSG;
                }
                accepted = true;
            }
            if ((down & HidNpadButton_X) && have) {
                gr_review *v = &g_reviews[vcursor];
                memset(&g_rank_view, 0, sizeof(g_rank_view));
                snprintf(g_rank_view.username, sizeof(g_rank_view.username), "%s", v->username);
                snprintf(g_rank_view.display, sizeof(g_rank_view.display), "%s", v->display);
                snprintf(g_rank_view.country, sizeof(g_rank_view.country), "%s", v->country);
                g_rank_view.avatar = v->avatar;
                g_rank_view.total_points = v->total_points;
                g_rank_view.achievements = v->achievements;
                g_rank_view.points = v->likes; /* sin posicion: ui_user muestra los me gusta */
                g_user_return = SCREEN_REVIEWS;
                screen = SCREEN_USER;
                accepted = true;
            }
            if ((down & HidNpadButton_Y) && g_account.linked && g_rev_closed) {
                show_message(false, "Todavía no se puede comentar", "Este juego todavía no tiene logros (está Sin empezar).", "Se podrá comentar cuando pase a En prueba.");
                g_msg_return = SCREEN_REVIEWS;
                screen = SCREEN_MSG;
                accepted_alt = true;
            } else if ((down & HidNpadButton_Y) && g_account.linked) {
                char text[504] = "", err[160] = "";
                bool had = false;
                for (int i = 0; i < g_nrev; i++) had = had || g_reviews[i].mine;
                /* vacio y aceptado = borrar el tuyo; cancelar no cambia nada */
                SwkbdConfig kbd;
                bool ok = false;
                if (R_SUCCEEDED(swkbdCreate(&kbd, 0))) {
                    swkbdConfigMakePresetDefault(&kbd);
                    swkbdConfigSetGuideText(&kbd, had ? "Tu comentario (déjalo vacío para borrarlo)" : "¿Qué te parecen los logros de este juego?");
                    swkbdConfigSetStringLenMax(&kbd, 500);
                    for (int i = 0; i < g_nrev; i++) if (g_reviews[i].mine) swkbdConfigSetInitialText(&kbd, g_reviews[i].body);
                    ok = R_SUCCEEDED(swkbdShow(&kbd, text, sizeof(text)));
                    swkbdClose(&kbd);
                }
                if (ok && (text[0] || had)) {
                    gr_net_result r = net_online() ? gr_review_save(net_http, &g_account, g_rev_tid, text, err, sizeof(err)) : GR_NET_OFFLINE;
                    if (r == GR_NET_OK) { g_rev_state = reviews_load(); vcursor = vtop = 0; }
                    else {
                        show_message(false, "No se pudo guardar", r == GR_NET_OFFLINE ? "No hay conexión a internet." : err, NULL);
                        g_msg_return = SCREEN_REVIEWS;
                        screen = SCREEN_MSG;
                    }
                }
                accepted_alt = true;
            }
            if (down & HidNpadButton_B) { screen = g_rev_return; back = true; }
        } else if (screen == SCREEN_DETAIL) {
            ui_layout l = ui_detail_layout(&g_config);
            int n = g_model.games[cursor].broken ? 0 : ui_ach_visible(&g_model.cur, ach_filter, ach_map);
            if (dcursor >= n) dcursor = n ? n - 1 : 0;
            TAP_SELECT(dcursor, n);
            if (n > 0) moved = move_cursor(&dcursor, n, l.cols, nav);
            if ((down & HidNpadButton_X) && !g_model.games[cursor].broken && g_model.cur.nach > 0) {
                /* filtro rapido: todos -> obtenidos -> por obtener */
                ach_filter = (ach_filter + 1) % UI_ACH_FILTERS;
                ui_set_ach_filter(ach_filter);
                dcursor = dtop = 0;
                accepted_alt = true;
            }
            if ((down & HidNpadButton_Minus) && n > 0) {
                g_config.ach_view = (g_config.ach_view + 1) % GR_VIEW_COUNT;
                save_config();
                dtop = 0;
                l = ui_detail_layout(&g_config);
                accepted_alt = true;
            }
            follow(dcursor, &dtop, l);
            if ((down & HidNpadButton_A) && n > 0) { screen = SCREEN_ACH; opened = true; }
            if (down & HidNpadButton_B) { screen = SCREEN_HOME; back = true; }
            if (down & HidNpadButton_Y) {
                snprintf(g_rev_tid, sizeof(g_rev_tid), "%s", g_model.games[cursor].tid);
                snprintf(g_rev_name, sizeof(g_rev_name), "%s", g_model.games[cursor].name);
                g_rev_state = reviews_load();
                g_rev_return = SCREEN_DETAIL;
                vcursor = vtop = 0;
                screen = SCREEN_REVIEWS;
                accepted_alt = true;
            }
        } else if (screen == SCREEN_ACH) {
            int n = ui_ach_visible(&g_model.cur, ach_filter, ach_map), step = 0; /* L y R recorren lo que deja ver el filtro */
            if (dcursor >= n) dcursor = n ? n - 1 : 0;
            if (n == 0) screen = SCREEN_DETAIL;
            if (down & (HidNpadButton_R | HidNpadButton_AnyRight)) step = 1;
            if (down & (HidNpadButton_L | HidNpadButton_AnyLeft)) step = -1;
            if (step && dcursor + step >= 0 && dcursor + step < n) { dcursor += step; moved = true; }
            if (down & HidNpadButton_B) {
                follow(dcursor, &dtop, ui_detail_layout(&g_config));
                screen = SCREEN_DETAIL;
                back = true;
            }
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
            if (song_pending > 0) {
                dirty = true;
                if (--song_wait <= 0) {
                    int i = song_pending - 1;
                    bool ok = music_download(i);
                    song_pending = 0;
                    music_refresh();
                    ui_set_music(g_songs, NSONGS, -1);
                    if (ok && song_all) {
                        /* sigue con la proxima que falte; al terminar, enciende la musica si no habia ninguna elegida */
                        for (int k = 0; k < NSONGS && song_pending == 0; k++)
                            if (!g_songs[k].have) { song_pending = k + 1; song_wait = 2; ui_set_music(g_songs, NSONGS, k + 1); }
                        if (song_pending == 0) {
                            song_all = false;
                            if (!g_config.music_track[0] || !g_config.music) {
                                if (!g_config.music_track[0]) snprintf(g_config.music_track, sizeof(g_config.music_track), "%s", g_songs[0].id);
                                g_config.music = true;
                                save_config();
                                music_apply();
                            }
                        }
                    } else if (ok) {
                        snprintf(g_config.music_track, sizeof(g_config.music_track), "%s", g_songs[i].id);
                        g_config.music = true;
                        save_config();
                        music_apply();
                    } else {
                        song_all = false;
                        show_message(false, "No se pudo descargar", "Revisa la conexión a internet e inténtalo de nuevo.", NULL);
                        g_msg_return = SCREEN_SETTINGS;
                        g_msg_tab = UI_TAB_MUSIC;
                        screen = SCREEN_MSG;
                    }
                }
            }
            if ((down & HidNpadButton_X) && stab == UI_TAB_MUSIC && song_pending == 0) {
                /* descargar todas las que falten, una detras de otra */
                song_all = true;
                for (int i = 0; i < NSONGS && song_pending == 0; i++)
                    if (!g_songs[i].have) { song_pending = i + 1; song_wait = 2; ui_set_music(g_songs, NSONGS, i + 1); }
                if (song_pending == 0) song_all = false;
                accepted_alt = true;
            }
            if ((down & HidNpadButton_Y) && stab == UI_TAB_MUSIC && srow >= 1 && g_songs[srow - 1].have) {
                char path[160];
                bool current = !strcmp(g_config.music_track, g_songs[srow - 1].id);
                if (current) { sfx_music_stop(); g_config.music_track[0] = 0; g_config.music = false; save_config(); }
                song_path(srow - 1, path, sizeof(path));
                remove(path);
                music_refresh();
                accepted_alt = true;
            }
            if (down & HidNpadButton_A) {
                int opt = ui_tab_option(stab, srow, linked);
                switch (opt) {
                    case UI_OPT_LANG:       g_config.lang = (g_config.lang + 1) % GR_LANG_COUNT; lang_changed = true; break;
                    case UI_OPT_APPSOUND:   g_config.app_sound = !g_config.app_sound; sfx_set_enabled(true); break; /* que se oiga el clic al encender */
                    case UI_OPT_THEME:      g_config.theme = (g_config.theme + 1) % GR_THEME_COUNT; break;
                    case UI_OPT_ANIM:       g_config.animated_bg = !g_config.animated_bg; break;
                    case UI_OPT_GAMES_VIEW: g_config.games_view = (g_config.games_view + 1) % GR_VIEW_COUNT; break;
                    case UI_OPT_ACH_VIEW:   g_config.ach_view = (g_config.ach_view + 1) % GR_VIEW_COUNT; break;
                    case UI_OPT_INGAME:
                        /* boot2.flag es lo que hace que Atmosphere arranque el sysmodule */
                        if (ingame_enabled()) remove(BOOT2_FLAG);
                        else install_file(BOOT2_FLAG, "", 0);
                        ui_set_ingame(ingame_enabled());
                        show_message(true, ingame_enabled() ? "Logros en los juegos: activados" : "Logros en los juegos: desactivados",
                                     "Reinicia la consola para que el cambio se aplique.", "Apagar y encender; suspender no basta.");
                        g_msg_return = SCREEN_SETTINGS;
                        g_msg_tab = stab;
                        screen = SCREEN_MSG;
                        break;
                    case UI_OPT_MUSIC:
                        quiet = g_config.music == false; /* al encenderla, lo que se oye es la musica */
                        g_config.music = !g_config.music;
                        if (g_config.music && !g_config.music_track[0]) /* aun no hay cancion elegida: la primera que este descargada */
                            for (int i = 0; i < NSONGS && !g_config.music_track[0]; i++)
                                if (g_songs[i].have) snprintf(g_config.music_track, sizeof(g_config.music_track), "%s", g_songs[i].id);
                        music_apply();
                        break;
                    case UI_OPT_SONG:
                        quiet = true;
                        if (g_songs[srow - 1].have) {
                            snprintf(g_config.music_track, sizeof(g_config.music_track), "%s", g_songs[srow - 1].id);
                            g_config.music = true;
                            music_apply();
                        } else {
                            song_pending = srow; /* primero se pinta "Descargando…" */
                            song_wait = 2;
                            ui_set_music(g_songs, NSONGS, srow);
                        }
                        break;
                    case UI_OPT_SOUND:      g_config.sound = !g_config.sound; break;
                    case UI_OPT_POPUP:      g_config.popup = !g_config.popup; break;
                    case UI_OPT_STARTPOP:   g_config.start_popup = !g_config.start_popup; break;
                    case UI_OPT_OVERLAY:
                        overlay_toggle();
                        if (g_overlay_msg) { g_overlay_msg = false; g_msg_return = SCREEN_SETTINGS; screen = SCREEN_MSG; }
                        break;
                    case UI_OPT_SCOPE:      g_config.games_scope = (g_config.games_scope + 1) % GR_SCOPE_COUNT; rescan(); cursor = top = 0; break;
                    case UI_OPT_SHOW:       g_config.games_show = (g_config.games_show + 1) % GR_SHOW_COUNT; rescan(); cursor = top = 0; break;
                    case UI_OPT_START:      g_config.start_page = (g_config.start_page + 1) % GR_START_COUNT; break;
                    case UI_OPT_ABOUT:      links_load(); screen = SCREEN_ABOUT; opened_rare = true; break;
                    case UI_OPT_DIAG:       run_diag(); screen = SCREEN_DIAG; break;
                    case UI_OPT_UPDATE:
                        if (g_upd != UI_UPD_AVAILABLE) update_check();
                        ui_set_update(g_upd, g_rel.version);
                        upd_return = SCREEN_SETTINGS;
                        screen = SCREEN_UPDATE;
                        break;
                    case UI_OPT_SEARCH:     run_diag(); search_read(); frow = 0; screen = SCREEN_SEARCH; break;
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
                    case UI_OPT_ACC_PHOTO:
                        acursor = g_account.avatar > 0 && g_account.avatar <= UI_AVATAR_COUNT ? g_account.avatar - 1 : 0;
                        screen = SCREEN_AVATARS;
                        break;
                    case UI_OPT_ACC_COUNTRY:
                        ccursor = ui_country_index(g_account.country);
                        if (ccursor < 0) ccursor = 0;
                        screen = SCREEN_COUNTRY;
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
                    case UI_OPT_ACC_INFO: /* la fila con tu usuario: cambiar la frase */
                        if (captions_load()) { kcursor = ktop = 0; screen = SCREEN_CAPTION; }
                        else {
                            show_message(false, "Sin conexión", "Conecta la consola a internet para cambiar la frase.", NULL);
                            g_msg_return = SCREEN_SETTINGS;
                            g_msg_tab = UI_TAB_ACCOUNT;
                            screen = SCREEN_MSG;
                        }
                        break;
                    default: break;
                }
                if (opt < UI_OPT_DIAG) {
                    accepted = true;
                    save_config();
                    if (lang_changed) { refresh_greeting(); lang_changed = false; } /* la frase del perfil cambia de idioma */
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
            if ((down & HidNpadButton_A) && g_account.linked) {
                rank_state = rank_load(rank_scope, rank_mode); /* antes de cambiar de pantalla, para que no parpadee */
                screen = SCREEN_RANKING;
                rcursor = 0;
                accepted = true;
            }
            if ((down & HidNpadButton_Y) && g_account.linked) {
                follow_with_code();
                g_msg_return = SCREEN_PROFILE;
                screen = SCREEN_MSG;
                opened = true;
            }
        } else if (screen == SCREEN_AVATARS) {
            TAP_SELECT(acursor, UI_AVATAR_COUNT);
            moved = move_cursor(&acursor, UI_AVATAR_COUNT, UI_AVATAR_COLS, nav);
            if (down & HidNpadButton_B) { screen = SCREEN_SETTINGS; stab = UI_TAB_ACCOUNT; back = true; }
            if (down & HidNpadButton_A) {
                char err[160] = "";
                if (!net_online()) {
                    show_message(false, "Sin conexión", "Conecta la consola a internet para cambiar la foto.", NULL);
                } else if (gr_set_avatar(net_http, &g_account, acursor + 1, err, sizeof(err)) == GR_NET_OK) {
                    gr_account_save(&g_account, ACCOUNT_PATH);
                    refresh_greeting();
                    show_message(true, "Foto cambiada", "Ya se ve en la app y en la web.", NULL);
                } else {
                    trace("cambiar foto: %s %s", err, net_last_error());
                    show_message(false, "No se pudo cambiar la foto", err[0] ? err : "Inténtalo de nuevo en un momento.", NULL);
                }
                g_msg_return = SCREEN_SETTINGS;
                g_msg_tab = UI_TAB_ACCOUNT;
                screen = SCREEN_MSG;
                accepted = true;
            }
        } else if (screen == SCREEN_CAPTION) {
            TAP_SELECT(kcursor, g_ncaptions + 1);
            moved = move_cursor(&kcursor, g_ncaptions + 1, 1, nav);
            if (kcursor < ktop) ktop = kcursor;
            if (kcursor >= ktop + UI_CAPTION_ROWS) ktop = kcursor - UI_CAPTION_ROWS + 1;
            if (down & HidNpadButton_B) { screen = SCREEN_SETTINGS; stab = UI_TAB_ACCOUNT; back = true; }
            if (down & HidNpadButton_A) {
                char err[160] = "";
                const char *text = kcursor < g_ncaptions ? g_captions[kcursor].es : ""; /* se guarda en español; la traduccion la da el servidor */
                if (gr_set_caption(net_http, &g_account, text, err, sizeof(err)) == GR_NET_OK) {
                    snprintf(g_account.caption_en, sizeof(g_account.caption_en), "%s", kcursor < g_ncaptions ? g_captions[kcursor].en : "");
                    gr_account_save(&g_account, ACCOUNT_PATH);
                    refresh_greeting();
                    show_message(true, text[0] ? "Frase cambiada" : "Frase quitada", "Ya se ve en tu perfil, en la app y en la web.", NULL);
                } else {
                    trace("cambiar frase: %s %s", err, net_last_error());
                    show_message(false, "No se pudo cambiar la frase", err[0] ? err : "Inténtalo de nuevo en un momento.", NULL);
                }
                g_msg_return = SCREEN_SETTINGS;
                g_msg_tab = UI_TAB_ACCOUNT;
                screen = SCREEN_MSG;
                accepted = true;
            }
        } else if (screen == SCREEN_COUNTRY) {
            TAP_SELECT(ccursor, UI_COUNTRY_COUNT);
            moved = move_cursor(&ccursor, UI_COUNTRY_COUNT, UI_COUNTRY_COLS, nav);
            if (down & HidNpadButton_B) { screen = SCREEN_SETTINGS; stab = UI_TAB_ACCOUNT; back = true; }
            if (down & (HidNpadButton_A | HidNpadButton_X)) {
                char err[160] = "";
                const char *code = (down & HidNpadButton_X) ? "" : ui_country_code(ccursor);
                if (!net_online()) show_message(false, "Sin conexión", "Conecta la consola a internet para cambiar el país.", NULL);
                else if (gr_set_country(net_http, &g_account, code, err, sizeof(err)) == GR_NET_OK) {
                    gr_account_save(&g_account, ACCOUNT_PATH);
                    refresh_greeting();
                    show_message(true, code[0] ? "País guardado" : "País quitado", "Se ve en tu perfil y en el ranking.", NULL);
                } else {
                    trace("cambiar pais: %s %s", err, net_last_error());
                    show_message(false, "No se pudo cambiar el país", err[0] ? err : "Inténtalo de nuevo en un momento.", NULL);
                }
                g_msg_return = SCREEN_SETTINGS;
                g_msg_tab = UI_TAB_ACCOUNT;
                screen = SCREEN_MSG;
                accepted = true;
            }
        } else if (screen == SCREEN_RANKING) {
            /* juegos por los que se puede clasificar: los que ya tienen algun logro tuyo */
            int ngames = 0, gidx[GR_MAX_GAMES];
            for (int i = 0; i < g_model.ngames; i++)
                if (g_model.games[i].unlocked > 0) gidx[ngames++] = i;
            if (rank_mode > ngames) rank_mode = 0;
            if (rcursor >= g_nrank) rcursor = 0;
            if (rank_state == 1) { TAP_SELECT(rcursor, g_nrank); if (g_nrank > 0) moved = move_cursor(&rcursor, g_nrank, 1, nav); }
            if (down & HidNpadButton_X) { rank_scope = (rank_scope + 1) % 3; rank_state = rank_load(rank_scope, rank_mode); rcursor = 0; accepted_alt = true; }
            if (down & HidNpadButton_Y) { rank_mode = (rank_mode + 1) % (ngames + 1); rank_state = rank_load(rank_scope, rank_mode); rcursor = 0; accepted_alt = true; }
            if ((down & HidNpadButton_A) && rank_state == 1 && g_nrank > 0) { g_rank_view = g_rank[rcursor]; screen = SCREEN_USER; accepted = true; }
            if (down & HidNpadButton_B) { screen = SCREEN_PROFILE; back = true; }
        } else if (screen == SCREEN_USER) {
            bool other = strcasecmp(g_rank_view.username, g_account.username) != 0;
            if ((down & HidNpadButton_A) && other) {
                char err[160] = "", line[96];
                gr_net_result r = net_online() ? gr_follow_user(net_http, &g_account, g_rank_view.username, err, sizeof(err)) : GR_NET_OFFLINE;
                snprintf(line, sizeof(line), "Ahora sigues a @%s.", g_rank_view.username);
                if (r == GR_NET_OK) { show_message(true, "¡Listo!", line, NULL); sync_start(); }
                else show_message(false, "No se pudo seguir", r == GR_NET_OFFLINE ? "No hay conexión a internet." : err, NULL);
                g_msg_return = g_user_return ? g_user_return : SCREEN_RANKING;
                g_user_return = 0;
                screen = SCREEN_MSG;
                accepted = true;
            }
            if (down & HidNpadButton_B) { screen = g_user_return ? g_user_return : SCREEN_RANKING; g_user_return = 0; back = true; }
        } else if (screen == SCREEN_BADGES) {
            if (down & HidNpadButton_B) { screen = g_badges_return; g_badges_return = SCREEN_PROFILE; back = true; }
        } else if (screen == SCREEN_MSG) {
            if (down & (HidNpadButton_A | HidNpadButton_B)) {
                screen = g_msg_return;
                if (screen == SCREEN_SETTINGS) { stab = g_msg_tab; srow = 0; }
                g_msg_return = SCREEN_SETTINGS;
                g_msg_tab = UI_TAB_ACCOUNT;
                back = true;
            }
        } else if (screen == SCREEN_SEARCH) {
            static const int bits[3] = { 32, 8, 16 };
            static int tick;
            if (++tick % 30 == 0 && search_read()) dirty = true;
            if (tap_item >= 0 && tap_item < 3) { frow = tap_item; dirty = true; }
            if (down & HidNpadButton_AnyUp) { frow = (frow + 2) % 3; moved = true; }
            if (down & HidNpadButton_AnyDown) { frow = (frow + 1) % 3; moved = true; }
            int step = (nav & HidNpadButton_AnyRight) ? 1 : (nav & HidNpadButton_AnyLeft) ? -1 : 0;
            if (down & HidNpadButton_R) step = 100;
            if (down & HidNpadButton_L) step = -100;
            if (step) {
                if (frow == 0) {
                    long v = (long)fvalue + step;
                    fvalue = v < 0 ? 0 : v > 9999999 ? 9999999 : (unsigned)v;
                } else if (frow == 1) ftype = (ftype + (step > 0 ? 1 : 2)) % 3;
                else {
                    fcand += step > 0 ? 1 : -1; /* 0 = no se sabe el numero: cambio / no cambio */
                    if (fcand > 100) fcand = 100;
                    if (fcand < -100) fcand = -100;
                }
                moved = true;
            }
            if (down & HidNpadButton_A) {
                /* plan nuevo: se borra lo de la busqueda anterior para empezar limpio */
                mkdir(SEARCH_DIR, 0777);
                remove(SEARCH_DIR "/state.txt");
                remove(SEARCH_DIR "/cand.bin");
                remove(SEARCH_DIR "/chains.bin");
                remove(SEARCH_DIR "/result.txt");
                g_search_nlines = 0;
                FILE *pf = fopen(SEARCH_DIR "/plan.txt", "w");
                if (pf) { fprintf(pf, "u%d %u %d\n", bits[ftype], fvalue, fcand); fclose(pf); }
                accepted = true;
            }
            if (down & HidNpadButton_X) { remove(SEARCH_DIR "/plan.txt"); accepted_alt = true; }
            if (down & HidNpadButton_B) { screen = SCREEN_SETTINGS; back = true; }
            if (!(g_account.linked && g_account.admin)) screen = SCREEN_SETTINGS; /* por si la cuenta dejo de ser admin */
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

        if (quiet) { /* nada: ya suena la musica */ }
        else if (back) sfx_play(SFX_BACK);
        else if (opened_rare) sfx_play(SFX_OPEN); /* solo para pantallas que casi nunca se abren */
        else if (accepted || opened) sfx_play(SFX_ACCEPT);
        else if (accepted_alt) sfx_play(SFX_ACCEPT_ALT);
        else if (moved) sfx_play(SFX_NAV);
        if (moved || accepted || accepted_alt || opened || opened_rare || back) dirty = true;
        if (sync_collect()) dirty = true;
        if (icons_collect(screen == SCREEN_DETAIL || screen == SCREEN_ACH ? g_model.games[cursor].tid : NULL)) {
            load_icons(g_model.games[cursor].tid);
            dirty = true;
        }
        if (g_music_check > 0 && --g_music_check == 0)
            trace("musica: a los 3 s han sonado %u samples%s", sfx_music_played(), sfx_music_playing() ? "" : " (ya no hay cancion puesta)");
        if (g_config.music && sfx_music_finished()) {
            music_next();
            if (screen == SCREEN_SETTINGS) dirty = true; /* para que "Sonando" pase a la cancion nueva */
        }
        sfx_update();

        /* La pantalla (texto, tarjetas) solo se redibuja en su capa cuando algo cambia.
         * Con el fondo animado, cada cuadro se pinta el fondo y se pone la capa encima. */
        bool redraw = dirty;
        if (dirty) {
            if (screen == SCREEN_DETAIL) ui_detail(&layer, &g_assets, &g_model, &g_config, cursor, dcursor, dtop);
            else if (screen == SCREEN_ACH) {
                int n = ui_ach_visible(&g_model.cur, ach_filter, ach_map);
                ui_achievement(&layer, &g_assets, &g_model, cursor, ach_map[dcursor < n ? dcursor : 0]);
            }
            else if (screen == SCREEN_SETTINGS) {
                ui_account acc = { .linked = g_account.linked, .username = g_account.username, .caption = (g_ui_en && g_account.caption_en[0]) ? g_account.caption_en : g_account.caption };
                sync_text(&acc.sync_es, &acc.sync_en);
                ui_settings(&layer, &g_assets, &g_config, stab, srow, g_system_spanish, g_system_dark, &acc);
            }
            else if (screen == SCREEN_PROFILE) ui_profile_screen(&layer, &g_assets, &g_profile);
            else if (screen == SCREEN_BADGES) ui_badges(&layer, &g_assets, &g_profile);
            else if (screen == SCREEN_CAPTION) ui_captions(&layer, &g_assets, g_captions, g_ncaptions, kcursor, ktop, g_account.caption);
            else if (screen == SCREEN_COUNTRY) ui_countries(&layer, &g_assets, ccursor, g_account.country);
            else if (screen == SCREEN_RANKING) {
                static const char *sc_es[3] = { "Global", "Mi país", "Amigos" }, *sc_en[3] = { "Global", "My country", "Friends" };
                int ngames = 0, gidx[GR_MAX_GAMES];
                for (int i = 0; i < g_model.ngames; i++)
                    if (g_model.games[i].unlocked > 0) gidx[ngames++] = i;
                const char *mode = rank_mode > 0 && rank_mode <= ngames ? g_model.games[gidx[rank_mode - 1]].name : (g_ui_en ? "Total points" : "Puntos totales");
                ui_ranking(&layer, &g_assets, g_rank, g_nrank, &g_rank_me, g_rank_total, sc_es[rank_scope], sc_en[rank_scope], mode, rcursor, rank_state);
            }
            else if (screen == SCREEN_UPDATE) ui_update(&layer, &g_assets, g_dl_live ? UI_UPD_DOWNLOADING : g_upd, g_rel.version, g_rel.notes, g_dl_done, g_dl_total);
            else if (screen == SCREEN_NOTIF) ui_notifs(&layer, &g_assets, g_notifs, g_nnotif, g_unread, ncursor, ntop, notif_state);
            else if (screen == SCREEN_REVIEWS) ui_reviews(&layer, &g_assets, g_rev_name, g_reviews, g_nrev, vcursor, vtop, g_rev_state, g_account.linked, g_rev_closed);
            else if (screen == SCREEN_USER) ui_user(&layer, &g_assets, &g_rank_view, strcasecmp(g_rank_view.username, g_account.username) != 0);
            else if (screen == SCREEN_AVATARS) ui_avatars(&layer, &g_assets, &g_profile, acursor);
            else if (screen == SCREEN_BROWSE) ui_browse(&layer, &g_assets, g_cat, g_cat_shown, g_cat_nshown, g_cat_n, g_cat_have, (const uint8_t *const *)g_cat_cover, g_cat_status, g_cat_state, bcursor, btop);
            else if (screen == SCREEN_SEARCH) {
                static const int bits[3] = { 32, 8, 16 };
                struct stat pst;
                ui_search(&layer, &g_assets, fvalue, bits[ftype], fcand, frow, g_search_lines, g_search_nlines, stat(SEARCH_DIR "/plan.txt", &pst) == 0);
            }
            else if (screen == SCREEN_BROWSE_GAME) ui_browse_game(&layer, &g_assets, &g_cat[g_cat_open], &g_cat_def, dcursor, dtop);
            else if (screen == SCREEN_LINK) ui_link(&layer, &g_assets, link_code, link_shown_left < 0 ? LINK_SECONDS : link_shown_left);
            else if (screen == SCREEN_MSG) ui_message(&layer, &g_assets, g_msg_title, g_msg_line1, g_msg_line2[0] ? g_msg_line2 : NULL, g_msg_ok);
            else if (screen == SCREEN_DIAG) ui_diag(&layer, &g_assets, g_diag, g_ndiag);
            else if (screen == SCREEN_ABOUT) ui_about(&layer, &g_assets, g_discord);
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
