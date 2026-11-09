/*
 * La creacion de la capa sigue el mismo procedimiento que libtesla (WerWolv, GPLv2):
 * capa administrada de vi + framebuffer RGBA4444 block-linear de 448 px de ancho.
 * La capa se crea con el primer popup y se queda viva (transparente) despues.
 */
#include "toast.h"

#include <string.h>
#include <switch.h>

#include "font.h"

#define FB_W 448
#define FB_H 128
/* vi trabaja en coordenadas de 1920x1080; 1.5x igual que Tesla */
#define LAYER_W 672
#define LAYER_H 192
#define LAYER_X (1920 - LAYER_W - 32)
#define LAYER_Y 32

#define SHOW_MS 5000
#define QUEUE_LEN 4
#define LINE_CHARS 26

extern u64 __nx_vi_layer_id;

typedef struct {
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

/* UTF-8 -> Latin-1 (lo que cubre la fuente). Devuelve cuantos caracteres escribio. */
static int to_latin1(const char *s, u8 *out, int cap) {
    int n = 0;
    const u8 *p = (const u8 *)s;
    while (*p && n < cap) {
        u32 cp;
        if (*p < 0x80) cp = *p++;
        else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { cp = ((*p & 0x1Fu) << 6) | (p[1] & 0x3Fu); p += 2; }
        else {
            /* fuera de Latin-1: saltar la secuencia completa */
            p++;
            while ((*p & 0xC0) == 0x80) p++;
            cp = '?';
        }
        out[n++] = cp < 256 ? (u8)cp : '?';
    }
    return n;
}

/* texto a doble tamaño (16x16 por caracter) */
static void draw_chars(int x, int y, const u8 *s, int n, u16 c) {
    for (int i = 0; i < n; i++) {
        const unsigned char *gl = lg_font[s[i]];
        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++)
                if (gl[row] & (1 << col))
                    fill(x + i * 16 + col * 2, y + row * 2, 2, 2, c);
    }
}

/* Dibuja hasta `max_lines` lineas cortando por palabras. */
static void draw_wrapped(int x, int y, const char *text, int max_lines, u16 c) {
    u8 buf[128];
    int n = to_latin1(text, buf, sizeof(buf));
    int pos = 0;
    for (int line = 0; line < max_lines && pos < n; line++) {
        while (pos < n && buf[pos] == ' ') pos++;
        int len = n - pos;
        if (len > LINE_CHARS) {
            len = LINE_CHARS;
            if (line < max_lines - 1) {
                int cut = len;
                while (cut > 0 && buf[pos + cut] != ' ') cut--;
                if (cut > 0) len = cut;
            } else {
                buf[pos + len - 1] = '.';
                buf[pos + len - 2] = '.';
            }
        }
        draw_chars(x, y + line * 20, buf + pos, len, c);
        pos += len;
    }
}

static void present(const toast_msg *m) {
    g_px = (u16 *)framebufferBegin(&g_fb, NULL);
    if (!g_px) return;

    fill(0, 0, FB_W, FB_H, 0); /* transparente */
    if (m) {
        const u16 bg = rgba4(1, 1, 2, 14), gold = rgba4(15, 12, 2, 15);
        fill(0, 0, FB_W, FB_H, bg);
        fill(0, 0, 6, FB_H, gold);
        draw_wrapped(18, 12, m->title, 1, gold);
        draw_wrapped(18, 40, m->name, 1, rgba4(15, 15, 15, 15));
        draw_wrapped(18, 68, m->desc, 2, rgba4(11, 11, 12, 15));
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

void toast_push(const char *title, const char *name, const char *desc) {
    if (g_failed || g_count >= QUEUE_LEN) return;
    toast_msg *m = &g_queue[(g_head + g_count) % QUEUE_LEN];
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
