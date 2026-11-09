/*
 * Greeny - visor de logros para el menu de homebrew.
 * Lee lo mismo que escribe el sysmodule: sdmc:/config/greeny/
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <switch.h>

#include "config.h"
#include "gfx.h"
#include "model.h"
#include "ui.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "third_party/stb_image.h"

#include "mascot_big_bin.h"
#include "mascot_small_bin.h"

#define ROOT        "sdmc:/config/greeny"
#define CONFIG_PATH ROOT "/config.ini"

/* tamaños con los que se generaron los .bin (scripts/assets.py) */
#define BIG_W 220
#define BIG_H 330
#define SMALL_W 50
#define SMALL_H 75

typedef enum { SCREEN_HOME, SCREEN_DETAIL, SCREEN_SETTINGS } screen_t;

static gr_model g_model;
static gr_config g_config;
static ui_assets g_assets;
static uint8_t *g_covers[GR_MAX_GAMES];
static bool g_system_spanish = true;
static bool g_ns_ok;

static void apply_language(void) {
    bool en = g_config.lang == GR_LANG_EN || (g_config.lang == GR_LANG_AUTO && !g_system_spanish);
    ui_set_english(en);
}

/* Carga la caratula de cada juego desde la propia consola (el icono del juego instalado). */
static void load_covers(void) {
    static NsApplicationControlData ctrl;
    for (int i = 0; i < GR_MAX_GAMES; i++) {
        free(g_covers[i]);
        g_covers[i] = NULL;
        g_assets.covers[i] = NULL;
    }
    if (!g_ns_ok) return;

    for (int i = 0; i < g_model.ngames; i++) {
        u64 tid = strtoull(g_model.games[i].tid, NULL, 16);
        u64 size = 0;
        if (R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, &ctrl, sizeof(ctrl), &size)))
            continue; /* el juego no esta instalado en esta consola */
        if (size <= sizeof(ctrl.nacp)) continue;

        int w, h, n;
        uint8_t *img = stbi_load_from_memory(ctrl.icon, (int)(size - sizeof(ctrl.nacp)), &w, &h, &n, 4);
        if (!img) continue;
        uint8_t *small = malloc(UI_COVER * UI_COVER * 4);
        if (small) {
            gfx_scale_rgba(img, w, h, small, UI_COVER, UI_COVER);
            gfx_round_corners(small, UI_COVER, UI_COVER, 12);
            g_covers[i] = small;
            g_assets.covers[i] = small;
        }
        stbi_image_free(img);
    }
}

static void rescan(void) {
    gr_scan(&g_model, ROOT);
    load_covers();
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    PadRepeater rep;
    padRepeaterInitialize(&rep, 22, 6);

    /* fuente del sistema: la misma que usa el menu de la consola */
    bool font_ok = false;
    if (R_SUCCEEDED(plInitialize(PlServiceType_User))) {
        PlFontData font;
        if (R_SUCCEEDED(plGetSharedFontByType(&font, PlSharedFontType_Standard)))
            font_ok = gfx_font_from_memory(font.address, font.size);
    }

    NWindow *win = nwindowGetDefault();
    Framebuffer fb;
    bool fb_ok = R_SUCCEEDED(framebufferCreate(&fb, win, UI_W, UI_H, PIXEL_FORMAT_RGBA_8888, 2)) &&
                 R_SUCCEEDED(framebufferMakeLinear(&fb));

    if (!font_ok || !fb_ok) {
        /* sin fuente o sin pantalla no hay nada que mostrar: salir limpio */
        if (fb_ok) framebufferClose(&fb);
        plExit();
        return 1;
    }

    if (R_SUCCEEDED(setInitialize())) {
        u64 code = 0;
        SetLanguage lang = SetLanguage_ENUS;
        if (R_SUCCEEDED(setGetSystemLanguage(&code)) && R_SUCCEEDED(setMakeLanguage(code, &lang)))
            g_system_spanish = lang == SetLanguage_ES || lang == SetLanguage_ES419;
        setExit();
    }
    g_ns_ok = R_SUCCEEDED(nsInitialize());

    g_assets.mascot_big = mascot_big_bin;     g_assets.big_w = BIG_W;     g_assets.big_h = BIG_H;
    g_assets.mascot_small = mascot_small_bin; g_assets.small_w = SMALL_W; g_assets.small_h = SMALL_H;

    gr_config_load(&g_config, CONFIG_PATH);
    apply_language();
    rescan();

    int cursor = 0, top = 0, dcursor = 0, dtop = 0, scursor = 0;
    screen_t screen = SCREEN_HOME;
    bool dirty = true;

    while (appletMainLoop()) {
        padUpdate(&pad);
        padRepeaterUpdate(&rep, padGetButtons(&pad) & (HidNpadButton_AnyUp | HidNpadButton_AnyDown));
        u64 down = padGetButtonsDown(&pad);
        u64 nav = down | padRepeaterGetButtons(&rep);
        if (down & HidNpadButton_Plus) break;

        if (screen == SCREEN_HOME) {
            int n = g_model.ngames;
            if ((nav & HidNpadButton_AnyDown) && cursor + 1 < n) { cursor++; dirty = true; }
            if ((nav & HidNpadButton_AnyUp) && cursor > 0) { cursor--; dirty = true; }
            if (down & HidNpadButton_Y) {
                rescan();
                if (cursor >= g_model.ngames) cursor = g_model.ngames ? g_model.ngames - 1 : 0;
                dirty = true;
            }
            if (cursor < top) top = cursor;
            if (cursor >= top + UI_HOME_ROWS) top = cursor - UI_HOME_ROWS + 1;
            if ((down & HidNpadButton_A) && n > 0) {
                gr_open(&g_model, cursor, ROOT);
                screen = SCREEN_DETAIL;
                dcursor = dtop = 0;
                dirty = true;
            }
            if (down & HidNpadButton_X) { screen = SCREEN_SETTINGS; scursor = 0; dirty = true; }
        } else if (screen == SCREEN_DETAIL) {
            int n = g_model.games[cursor].broken ? 0 : g_model.cur.nach;
            if ((nav & HidNpadButton_AnyDown) && dcursor + 1 < n) { dcursor++; dirty = true; }
            if ((nav & HidNpadButton_AnyUp) && dcursor > 0) { dcursor--; dirty = true; }
            if (dcursor < dtop) dtop = dcursor;
            if (dcursor >= dtop + UI_DETAIL_ROWS) dtop = dcursor - UI_DETAIL_ROWS + 1;
            if (down & HidNpadButton_B) { screen = SCREEN_HOME; dirty = true; }
        } else {
            if ((nav & HidNpadButton_AnyDown) && scursor + 1 < UI_OPT_COUNT) { scursor++; dirty = true; }
            if ((nav & HidNpadButton_AnyUp) && scursor > 0) { scursor--; dirty = true; }
            if (down & HidNpadButton_A) {
                if (scursor == UI_OPT_LANG) g_config.lang = (g_config.lang + 1) % GR_LANG_COUNT;
                else if (scursor == UI_OPT_SOUND) g_config.sound = !g_config.sound;
                else g_config.popup = !g_config.popup;
                mkdir("sdmc:/config", 0777);
                mkdir(ROOT, 0777);
                gr_config_save(&g_config, CONFIG_PATH);
                apply_language();
                dirty = true;
            }
            if (down & HidNpadButton_B) { screen = SCREEN_HOME; dirty = true; }
        }

        /* El buffer lineal conserva lo ultimo dibujado, asi que solo se repinta si algo cambio. */
        u32 stride;
        u32 *px = (u32 *)framebufferBegin(&fb, &stride);
        if (dirty) {
            gfx_canvas c = { .px = px, .w = UI_W, .h = UI_H, .stride = (int)(stride / sizeof(u32)) };
            if (screen == SCREEN_DETAIL) ui_detail(&c, &g_assets, &g_model, cursor, dcursor, dtop);
            else if (screen == SCREEN_SETTINGS) ui_settings(&c, &g_assets, &g_config, scursor, g_system_spanish);
            else ui_home(&c, &g_assets, &g_model, cursor, top);
            dirty = false;
        }
        framebufferEnd(&fb);
    }

    if (g_ns_ok) nsExit();
    framebufferClose(&fb);
    plExit();
    return 0;
}
