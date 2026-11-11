/*
 * La creacion de la capa sigue el mismo procedimiento que libtesla (WerWolv, GPLv2):
 * capa administrada de vi + framebuffer RGBA4444 block-linear de 448 px de ancho.
 * La capa solo existe mientras hay un aviso en pantalla: al ocultarse se destruye,
 * para no dejar una capa ni memoria grafica ocupada mientras se juega.
 */
#include "toast.h"

#include <stdio.h>
#include <string.h>
#include <switch.h>


#define FB_W 448
#define FB_H 128
/* vi trabaja en coordenadas de 1920x1080; 1.5x igual que Tesla */
#define LAYER_W 672
#define LAYER_H 192
#define LAYER_X (1920 - LAYER_W - 32)
#define LAYER_Y 32

#define SHOW_MS 5000
#define QUEUE_LEN 4

extern u64 __nx_vi_layer_id;

typedef struct {
    toast_kind kind;
    int tier, points;
    bool dark;
    char title[32];
    char name[64];
    char desc[128];
} toast_msg;

static toast_msg g_queue[QUEUE_LEN];
static int g_head, g_count;

static bool g_init, g_failed, g_showing;
static Result g_fail_rc;
static u64 g_until;

static ViDisplay g_disp;
static ViLayer g_layer;
static NWindow g_win;
static Framebuffer g_fb;
static Event g_vsync;

bool toast_failed(void) { return g_failed; }
uint32_t toast_fail_rc(void) { return g_fail_rc; }

static Result add_to_layer_stack(ViLayer *layer, ViLayerStack stack) {
    const struct {
        u32 stack;
        u64 layerId;
    } in = { stack, layer->layer_id };
    return serviceDispatchIn(viGetSession_IManagerDisplayService(), 6000, in);
}

static Result gfx_init(void) {
    static const ViLayerStack stacks[] = {
        ViLayerStack_Default, ViLayerStack_Screenshot, ViLayerStack_Recording,
        ViLayerStack_Arbitrary, ViLayerStack_LastFrame, ViLayerStack_Null,
        ViLayerStack_ApplicationForDebug, ViLayerStack_Lcd,
    };
    int stage = 0;
    Result rc = smInitialize();
    if (R_FAILED(rc)) return rc;

    rc = viInitialize(ViServiceType_Manager);
    if (R_FAILED(rc)) goto fail;
    stage = 1;
    rc = viOpenDefaultDisplay(&g_disp);
    if (R_FAILED(rc)) goto fail;
    stage = 2;
    rc = viGetDisplayVsyncEvent(&g_disp, &g_vsync);
    if (R_FAILED(rc)) goto fail;
    stage = 3;
    rc = viCreateManagedLayer(&g_disp, (ViLayerFlags)0, 0, &__nx_vi_layer_id);
    if (R_FAILED(rc)) goto fail;
    rc = viCreateLayer(&g_disp, &g_layer);
    if (R_FAILED(rc)) goto fail;
    stage = 4;
    rc = viSetLayerScalingMode(&g_layer, ViScalingMode_FitToLayer);
    if (R_FAILED(rc)) goto fail;

    s32 z = 0;
    if (R_SUCCEEDED(viGetZOrderCountMax(&g_disp, &z)) && z > 0) {
        rc = viSetLayerZ(&g_layer, z);
        if (R_FAILED(rc)) goto fail;
    }
    for (size_t i = 0; i < sizeof(stacks) / sizeof(stacks[0]); i++) {
        rc = add_to_layer_stack(&g_layer, stacks[i]);
        if (R_FAILED(rc)) goto fail;
    }
    rc = viSetLayerSize(&g_layer, LAYER_W, LAYER_H);
    if (R_FAILED(rc)) goto fail;
    rc = viSetLayerPosition(&g_layer, LAYER_X, LAYER_Y);
    if (R_FAILED(rc)) goto fail;
    rc = nwindowCreateFromLayer(&g_win, &g_layer);
    if (R_FAILED(rc)) goto fail;
    stage = 5;
    rc = framebufferCreate(&g_fb, &g_win, FB_W, FB_H, PIXEL_FORMAT_RGBA_4444, 2);
    if (R_FAILED(rc)) goto fail;

    smExit();
    return 0;

fail:
    if (stage >= 5) nwindowClose(&g_win);
    if (stage >= 4) viDestroyManagedLayer(&g_layer);
    if (stage >= 3) eventClose(&g_vsync);
    if (stage >= 2) viCloseDisplay(&g_disp);
    if (stage >= 1) viExit();
    smExit();
    return rc;
}

static void gfx_exit(void) {
    framebufferClose(&g_fb);
    nwindowClose(&g_win);
    viDestroyManagedLayer(&g_layer);
    eventClose(&g_vsync);
    viCloseDisplay(&g_disp);
    viExit();
}

/* ---------- dibujo ---------- */

static u16 *g_px;

static inline u16 rgba4(u8 r, u8 g, u8 b, u8 a) {
    return (u16)(r | (g << 4) | (b << 8) | (a << 12));
}

/* offset dentro del framebuffer block-linear (misma formula que libtesla) */
static inline u32 px_offset(u32 x, u32 y) {
    u32 pos = ((y & 127) / 16) + (x / 32 * 8) + ((y / 16 / 8) * (((FB_W / 2) / 16 * 8)));
    pos *= 16 * 16 * 4;
    pos += ((y % 16) / 8) * 512 + ((x % 32) / 16) * 256 + ((y % 8) / 2) * 64 +
           ((x % 16) / 8) * 32 + (y % 2) * 16 + (x % 8) * 2;
    return pos / 2;
}

static inline void put_px(int x, int y, u16 c) {
    if (x < 0 || y < 0 || x >= FB_W || y >= FB_H) return;
    g_px[px_offset((u32)x, (u32)y)] = c;
}

static void fill(int x0, int y0, int w, int h, u16 c) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            put_px(x, y, c);
}

/* toast_draw entrega 8 bits por componente; la capa usa 4 */
static void put_rgba8(int x, int y, unsigned r, unsigned g, unsigned b, unsigned a) {
    put_px(x, y, rgba4((u8)(r >> 4), (u8)(g >> 4), (u8)(b >> 4), (u8)(a >> 4)));
}

static void present(const toast_msg *m) {
    g_px = (u16 *)framebufferBegin(&g_fb, NULL);
    if (!g_px) return;

    if (m) {
        /* El dibujo de la medalla se lee de la SD en el momento; si no esta, sale el circulo sencillo. */
        static unsigned char badge[TOAST_BADGE * TOAST_BADGE * 4];
        const char *file = m->kind == TOAST_WARN ? "aviso" : m->kind == TOAST_INFO ? "ok"
                         : m->tier == 3 ? "oro" : m->tier == 2 ? "plata" : "bronce";
        char path[96];
        snprintf(path, sizeof(path), "sdmc:/config/greeny/toast/%s.bin", file);
        bool have = false;
        FILE *f = fopen(path, "rb");
        if (f) {
            have = fread(badge, 1, sizeof(badge), f) == sizeof(badge);
            fclose(f);
        }
        toast_view v = { m->kind, m->tier, m->points, m->dark, m->title, m->name, m->desc, have ? badge : NULL };
        toast_draw(&v, put_rgba8);
    } else {
        fill(0, 0, FB_W, FB_H, 0); /* transparente */
    }

    eventWait(&g_vsync, 100000000ull);
    framebufferEnd(&g_fb);
    g_px = NULL;
}

/* ---------- cola ---------- */

static void copy_trunc(char *dst, size_t cap, const char *src) {
    size_t n = src ? strlen(src) : 0;
    if (n >= cap) {
        n = cap - 1;
        while (n > 0 && ((u8)src[n] & 0xC0) == 0x80) n--;
    }
    if (n) memcpy(dst, src, n);
    dst[n] = 0;
}

void toast_push(toast_kind kind, int tier, int points, bool dark, const char *title, const char *name, const char *desc) {
    if (g_failed || g_count >= QUEUE_LEN) return;
    toast_msg *m = &g_queue[(g_head + g_count) % QUEUE_LEN];
    m->kind = kind;
    m->tier = tier;
    m->points = points;
    m->dark = dark;
    copy_trunc(m->title, sizeof(m->title), title);
    copy_trunc(m->name, sizeof(m->name), name);
    copy_trunc(m->desc, sizeof(m->desc), desc);
    g_count++;
}

void toast_update(uint64_t now_ms) {
    if (g_failed) return;

    if (g_showing && now_ms >= g_until) {
        present(NULL);
        g_showing = false;
        if (g_count == 0) { /* no hay otro en cola: soltar la capa */
            gfx_exit();
            g_init = false;
        }
    }
    if (g_showing || g_count == 0) return;

    if (!g_init) {
        Result rc = gfx_init();
        if (R_FAILED(rc)) {
            g_failed = true;
            g_fail_rc = rc;
            g_count = 0;
            return;
        }
        g_init = true;
    }

    present(&g_queue[g_head]);
    g_head = (g_head + 1) % QUEUE_LEN;
    g_count--;
    g_showing = true;
    g_until = now_ms + SHOW_MS;
}
