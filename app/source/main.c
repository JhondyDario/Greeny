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

#include "config.h"
#include "diag.h"
#include "gfx.h"
#include "model.h"
#include "sfx.h"
#include "ui.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "third_party/stb_image.h"

#include "mascot_small_bin.h"

#define SD          "sdmc:"
#define ROOT        SD "/config/greeny"
#define CONFIG_PATH ROOT "/config.ini"
#define TRACE_PATH  ROOT "/app-log.txt"
#define SYSMODULE_TID 0x4200000000010690ull
#define TRACE_DONE  "salida normal"

/* tamaño con el que se genero el .bin (scripts/assets.py) */
#define SMALL_W 50
#define SMALL_H 75

typedef enum { SCREEN_HOME, SCREEN_DETAIL, SCREEN_SETTINGS, SCREEN_DIAG } screen_t;

static gr_model g_model;
static gr_config g_config;
static ui_assets g_assets;
static uint8_t *g_covers[GR_MAX_GAMES], *g_covers_big[GR_MAX_GAMES];
static bool g_system_spanish = true, g_system_dark;
static bool g_ns_ok;
static gr_diag_env g_env;
static gr_diag_item g_diag[GR_DIAG_MAX];
static int g_ndiag;

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

static void rescan(void) {
    trace("leyendo archivos de logros");
    gr_scan(&g_model, ROOT);
    load_covers();
}

static void apply_config(void) {
    ui_set_english(g_config.lang == GR_LANG_EN || (g_config.lang == GR_LANG_AUTO && !g_system_spanish));
    ui_set_dark(g_config.theme == GR_THEME_DARK || (g_config.theme == GR_THEME_AUTO && g_system_dark));
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

    AppletType at = appletGetAppletType();
    g_env.atmosphere = hosversionIsAtmosphere();
    g_env.covers_ok = g_ns_ok;
    g_env.applet_mode = at != AppletType_Application && at != AppletType_SystemApplication;

    g_assets.mascot_small = mascot_small_bin;
    g_assets.small_w = SMALL_W;
    g_assets.small_h = SMALL_H;

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

    int cursor = 0, top = 0, dcursor = 0, dtop = 0, scursor = 0;
    screen_t screen = SCREEN_HOME;
    bool dirty = true;
    const u64 dirs = HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight;

    while (appletMainLoop()) {
        padUpdate(&pad);
        padRepeaterUpdate(&rep, padGetButtons(&pad) & dirs);
        u64 down = padGetButtonsDown(&pad);
        u64 nav = (down | padRepeaterGetButtons(&rep)) & dirs;
        if (down & HidNpadButton_Plus) break;

        bool moved = false, accepted = false, back = false;

        if (screen == SCREEN_HOME) {
            ui_layout l = ui_home_layout(&g_config);
            if (g_model.ngames > 0) moved = move_cursor(&cursor, g_model.ngames, l.cols, nav);
            if (down & HidNpadButton_Y) {
                rescan();
                if (cursor >= g_model.ngames) cursor = g_model.ngames ? g_model.ngames - 1 : 0;
                top = 0;
                accepted = true;
            }
            follow(cursor, &top, l);
            if ((down & HidNpadButton_A) && g_model.ngames > 0) {
                gr_open(&g_model, cursor, ROOT);
                screen = SCREEN_DETAIL;
                dcursor = dtop = 0;
                accepted = true;
            }
            if (down & HidNpadButton_X) { screen = SCREEN_SETTINGS; scursor = 0; accepted = true; }
        } else if (screen == SCREEN_DETAIL) {
            ui_layout l = ui_detail_layout(&g_config);
            int n = g_model.games[cursor].broken ? 0 : g_model.cur.nach;
            if (n > 0) moved = move_cursor(&dcursor, n, l.cols, nav);
            follow(dcursor, &dtop, l);
            if (down & HidNpadButton_B) { screen = SCREEN_HOME; back = true; }
        } else if (screen == SCREEN_SETTINGS) {
            moved = move_cursor(&scursor, UI_OPT_COUNT, 1, nav);
            if (down & HidNpadButton_A) {
                accepted = true;
                switch (scursor) {
                    case UI_OPT_LANG:       g_config.lang = (g_config.lang + 1) % GR_LANG_COUNT; break;
                    case UI_OPT_THEME:      g_config.theme = (g_config.theme + 1) % GR_THEME_COUNT; break;
                    case UI_OPT_ANIM:       g_config.animated_bg = !g_config.animated_bg; break;
                    case UI_OPT_GAMES_VIEW: g_config.games_view = (g_config.games_view + 1) % GR_VIEW_COUNT; break;
                    case UI_OPT_ACH_VIEW:   g_config.ach_view = (g_config.ach_view + 1) % GR_VIEW_COUNT; break;
                    case UI_OPT_SOUND:      g_config.sound = !g_config.sound; break;
                    case UI_OPT_POPUP:      g_config.popup = !g_config.popup; break;
                    default:                run_diag(); screen = SCREEN_DIAG; break;
                }
                if (scursor != UI_OPT_DIAG) {
                    save_config();
                    top = 0; /* la vista pudo cambiar de forma */
                    follow(cursor, &top, ui_home_layout(&g_config));
                }
            }
            if (down & HidNpadButton_B) { screen = SCREEN_HOME; back = true; }
        } else {
            if (down & HidNpadButton_Y) { rescan(); run_diag(); accepted = true; }
            if (down & HidNpadButton_B) { screen = SCREEN_SETTINGS; back = true; }
        }

        if (back) sfx_play(SFX_BACK);
        else if (accepted) sfx_play(SFX_ACCEPT);
        else if (moved) sfx_play(SFX_NAV);
        if (moved || accepted || back) dirty = true;
        sfx_update();

        /* La pantalla (texto, tarjetas) solo se redibuja en su capa cuando algo cambia.
         * Con el fondo animado, cada cuadro se pinta el fondo y se pone la capa encima. */
        bool redraw = dirty;
        if (dirty) {
            if (screen == SCREEN_DETAIL) ui_detail(&layer, &g_assets, &g_model, &g_config, cursor, dcursor, dtop);
            else if (screen == SCREEN_SETTINGS) ui_settings(&layer, &g_assets, &g_config, scursor, g_system_spanish, g_system_dark);
            else if (screen == SCREEN_DIAG) ui_diag(&layer, &g_assets, g_diag, g_ndiag);
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

    trace("cerrando");
    sfx_exit();
    if (g_ns_ok) nsExit();
    framebufferClose(&fb);
    plExit();
    trace(TRACE_DONE);
    return 0;
}
