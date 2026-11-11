#include "gfx.h"

#include <math.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library g_ft;
static FT_Face g_face;

static bool ft_ready(void) {
    return g_ft || FT_Init_FreeType(&g_ft) == 0;
}

bool gfx_font_from_memory(const void *data, size_t size) {
    return ft_ready() && FT_New_Memory_Face(g_ft, data, (FT_Long)size, 0, &g_face) == 0;
}

bool gfx_font_from_file(const char *path) {
    return ft_ready() && FT_New_Face(g_ft, path, 0, &g_face) == 0;
}

/* mezcla `col` sobre el pixel con cobertura a (0..255). El destino puede ser
 * transparente (capa): se compone "encima" conservando el alfa resultante. */
static inline void blend(gfx_canvas *c, int x, int y, gfx_color col, unsigned a) {
    if (x < 0 || y < 0 || x >= c->w || y >= c->h || a == 0) return;
    uint32_t *d = &c->px[y * c->stride + x];
    if (a >= 255) { *d = col | 0xFF000000u; return; }
    unsigned da = *d >> 24;
    unsigned sr = col & 0xFF, sg = (col >> 8) & 0xFF, sb = (col >> 16) & 0xFF;
    if (da == 0) { *d = sr | (sg << 8) | (sb << 16) | (a << 24); return; }
    unsigned dr = *d & 0xFF, dg = (*d >> 8) & 0xFF, db = (*d >> 16) & 0xFF;
    unsigned dw = da * (255 - a) / 255;   /* peso del destino */
    unsigned oa = a + dw;                 /* alfa resultante, 1..255 */
    dr = (sr * a + dr * dw) / oa;
    dg = (sg * a + dg * dw) / oa;
    db = (sb * a + db * dw) / oa;
    *d = dr | (dg << 8) | (db << 16) | (oa << 24);
}

void gfx_clear_transparent(gfx_canvas *c) {
    for (int y = 0; y < c->h; y++) memset(&c->px[y * c->stride], 0, (size_t)c->w * sizeof(uint32_t));
}

void gfx_compose(gfx_canvas *dst, const gfx_canvas *layer) {
    int w = dst->w < layer->w ? dst->w : layer->w, h = dst->h < layer->h ? dst->h : layer->h;
    for (int y = 0; y < h; y++) {
        const uint32_t *s = &layer->px[y * layer->stride];
        uint32_t *d = &dst->px[y * dst->stride];
        for (int x = 0; x < w; x++) {
            uint32_t p = s[x];
            unsigned a = p >> 24;
            if (a == 255) d[x] = p;
            else if (a) {
                uint32_t q = d[x];
                unsigned r = ((p & 0xFF) * a + (q & 0xFF) * (255 - a)) / 255;
                unsigned g = (((p >> 8) & 0xFF) * a + ((q >> 8) & 0xFF) * (255 - a)) / 255;
                unsigned b = (((p >> 16) & 0xFF) * a + ((q >> 16) & 0xFF) * (255 - a)) / 255;
                d[x] = r | (g << 8) | (b << 16) | 0xFF000000u;
            }
        }
    }
}

void gfx_clear(gfx_canvas *c, gfx_color col) {
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++) c->px[y * c->stride + x] = col | 0xFF000000u;
}

void gfx_rect(gfx_canvas *c, int x, int y, int w, int h, gfx_color col) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > c->w ? c->w : x + w, y1 = y + h > c->h ? c->h : y + h;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) c->px[yy * c->stride + xx] = col | 0xFF000000u;
}

void gfx_compose_fx(gfx_canvas *dst, const gfx_canvas *layer, int dy, unsigned alpha) {
    int w = dst->w < layer->w ? dst->w : layer->w, h = dst->h < layer->h ? dst->h : layer->h;
    if (alpha > 256) alpha = 256;
    for (int y = dy > 0 ? dy : 0; y < h; y++) {
        int sy = y - dy;
        if (sy < 0 || sy >= h) continue;
        const uint32_t *s = &layer->px[sy * layer->stride];
        uint32_t *d = &dst->px[y * dst->stride];
        for (int x = 0; x < w; x++) {
            uint32_t p = s[x];
            unsigned a = ((p >> 24) * alpha) >> 8;
            if (!a) continue;
            uint32_t q = d[x];
            unsigned r = ((p & 0xFF) * a + (q & 0xFF) * (255 - a)) / 255;
            unsigned g = (((p >> 8) & 0xFF) * a + ((q >> 8) & 0xFF) * (255 - a)) / 255;
            unsigned b = (((p >> 16) & 0xFF) * a + ((q >> 16) & 0xFF) * (255 - a)) / 255;
            d[x] = r | (g << 8) | (b << 16) | 0xFF000000u;
        }
    }
}

void gfx_gradient(gfx_canvas *c, int x, int y, int w, int h, gfx_color left, gfx_color right) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > c->w ? c->w : x + w, y1 = y + h > c->h ? c->h : y + h;
    for (int xx = x0; xx < x1; xx++) {
        unsigned t = w > 1 ? (unsigned)((xx - x) * 256 / (w - 1)) : 0;
        gfx_color col = 0xFF000000u;
        for (int k = 0; k < 24; k += 8) {
            unsigned a = (left >> k) & 255, b = (right >> k) & 255;
            col |= ((a * (256 - t) + b * t) >> 8) << k;
        }
        for (int yy = y0; yy < y1; yy++) c->px[yy * c->stride + xx] = col;
    }
}

/* cobertura de un pixel respecto a un circulo: 255 dentro, borde suavizado */
static inline unsigned disc_cov(float dx, float dy, float r) {
    float d = sqrtf(dx * dx + dy * dy);
    float a = r + 0.5f - d;
    if (a <= 0) return 0;
    if (a >= 1) return 255;
    return (unsigned)(a * 255);
}

void gfx_circle(gfx_canvas *c, int cx, int cy, int r, gfx_color col) {
    for (int y = -r - 1; y <= r + 1; y++)
        for (int x = -r - 1; x <= r + 1; x++)
            blend(c, cx + x, cy + y, col, disc_cov((float)x, (float)y, (float)r));
}

void gfx_circle_alpha(gfx_canvas *c, int cx, int cy, int r, gfx_color col, unsigned alpha) {
    for (int y = -r - 1; y <= r + 1; y++) {
        if (cy + y < 0 || cy + y >= c->h) continue;
        for (int x = -r - 1; x <= r + 1; x++)
            blend(c, cx + x, cy + y, col, disc_cov((float)x, (float)y, (float)r) * alpha / 255);
    }
}

void gfx_round_rect(gfx_canvas *c, int x, int y, int w, int h, int r, gfx_color col) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    if (r <= 0) { gfx_rect(c, x, y, w, h, col); return; }
    gfx_rect(c, x + r, y, w - 2 * r, h, col);
    gfx_rect(c, x, y + r, r, h - 2 * r, col);
    gfx_rect(c, x + w - r, y + r, r, h - 2 * r, col);
    for (int yy = 0; yy < r; yy++) {
        for (int xx = 0; xx < r; xx++) {
            unsigned a = disc_cov((float)(r - xx) - 0.5f, (float)(r - yy) - 0.5f, (float)r - 0.5f);
            blend(c, x + xx, y + yy, col, a);
            blend(c, x + w - 1 - xx, y + yy, col, a);
            blend(c, x + xx, y + h - 1 - yy, col, a);
            blend(c, x + w - 1 - xx, y + h - 1 - yy, col, a);
        }
    }
}

void gfx_line(gfx_canvas *c, int x0, int y0, int x1, int y1, int thick, gfx_color col) {
    int dx = x1 - x0, dy = y1 - y0;
    int steps = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    if (steps == 0) steps = 1;
    for (int i = 0; i <= steps; i++) gfx_circle(c, x0 + dx * i / steps, y0 + dy * i / steps, thick / 2, col);
}

/* ---------- texto ---------- */

static uint32_t utf8_next(const unsigned char **pp) {
    const unsigned char *p = *pp;
    uint32_t cp;
    int extra;
    if (*p < 0x80) { cp = *p; extra = 0; }
    else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; extra = 1; }
    else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; extra = 2; }
    else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; extra = 3; }
    else { *pp = p + 1; return '?'; }
    p++;
    for (int i = 0; i < extra; i++) {
        if ((*p & 0xC0) != 0x80) { *pp = p; return '?'; }
        cp = (cp << 6) | (*p++ & 0x3F);
    }
    *pp = p;
    return cp;
}

static bool set_size(int size) {
    return g_face && FT_Set_Pixel_Sizes(g_face, 0, (FT_UInt)size) == 0;
}

int gfx_text_width(int size, const char *s) {
    if (!set_size(size)) return 0;
    int w = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        uint32_t cp = utf8_next(&p);
        if (FT_Load_Char(g_face, cp, FT_LOAD_DEFAULT) == 0) w += (int)(g_face->glyph->advance.x >> 6);
    }
    return w;
}

/* dibuja hasta `end` (o hasta el final si es NULL) y devuelve la x final */
static int draw_run(gfx_canvas *c, int x, int y, int size, gfx_color col, const char *s, const char *end) {
    if (!set_size(size)) return x;
    int base = y + (int)(g_face->size->metrics.ascender >> 6);
    const unsigned char *p = (const unsigned char *)s;
    while (*p && (!end || (const char *)p < end)) {
        uint32_t cp = utf8_next(&p);
        if (FT_Load_Char(g_face, cp, FT_LOAD_RENDER) != 0) continue;
        FT_GlyphSlot gl = g_face->glyph;
        for (unsigned row = 0; row < gl->bitmap.rows; row++)
            for (unsigned i = 0; i < gl->bitmap.width; i++)
                blend(c, x + gl->bitmap_left + (int)i, base - gl->bitmap_top + (int)row, col,
                      gl->bitmap.buffer[row * (unsigned)gl->bitmap.pitch + i]);
        x += (int)(gl->advance.x >> 6);
    }
    return x;
}

void gfx_text(gfx_canvas *c, int x, int y, int size, gfx_color col, const char *s) {
    draw_run(c, x, y, size, col, s, NULL);
}

void gfx_text_right(gfx_canvas *c, int right, int y, int size, gfx_color col, const char *s) {
    draw_run(c, right - gfx_text_width(size, s), y, size, col, s, NULL);
}

void gfx_text_fit(gfx_canvas *c, int x, int y, int size, gfx_color col, const char *s, int max_w) {
    if (gfx_text_width(size, s) <= max_w) { gfx_text(c, x, y, size, col, s); return; }
    if (!set_size(size)) return;
    int dots = gfx_text_width(size, "...");
    set_size(size);
    int w = 0;
    const unsigned char *p = (const unsigned char *)s, *cut = p;
    while (*p) {
        const unsigned char *start = p;
        uint32_t cp = utf8_next(&p);
        int adv = FT_Load_Char(g_face, cp, FT_LOAD_DEFAULT) == 0 ? (int)(g_face->glyph->advance.x >> 6) : 0;
        if (w + adv + dots > max_w) { cut = start; break; }
        w += adv;
        cut = p;
    }
    int end_x = draw_run(c, x, y, size, col, s, (const char *)cut);
    draw_run(c, end_x, y, size, col, "...", NULL);
}

void gfx_image(gfx_canvas *c, int x, int y, const uint8_t *rgba, int w, int h) {
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            const uint8_t *s = rgba + (size_t)(yy * w + xx) * 4;
            blend(c, x + xx, y + yy, GFX_RGB(s[0], s[1], s[2]), s[3]);
        }
    }
}

void gfx_scale_rgba(const uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh) {
    for (int y = 0; y < dh; y++) {
        int y0 = y * sh / dh, y1 = (y + 1) * sh / dh;
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < dw; x++) {
            int x0 = x * sw / dw, x1 = (x + 1) * sw / dw;
            if (x1 <= x0) x1 = x0 + 1;
            unsigned sum[4] = {0, 0, 0, 0}, n = 0;
            for (int yy = y0; yy < y1 && yy < sh; yy++) {
                for (int xx = x0; xx < x1 && xx < sw; xx++) {
                    const uint8_t *p = src + (size_t)(yy * sw + xx) * 4;
                    sum[0] += p[0]; sum[1] += p[1]; sum[2] += p[2]; sum[3] += p[3];
                    n++;
                }
            }
            uint8_t *d = dst + (size_t)(y * dw + x) * 4;
            for (int k = 0; k < 4; k++) d[k] = n ? (uint8_t)(sum[k] / n) : 0;
        }
    }
}

void gfx_round_corners(uint8_t *rgba, int w, int h, int r) {
    for (int y = 0; y < r; y++) {
        for (int x = 0; x < r; x++) {
            unsigned a = disc_cov((float)(r - x) - 0.5f, (float)(r - y) - 0.5f, (float)r - 0.5f);
            int xs[2] = { x, w - 1 - x }, ys[2] = { y, h - 1 - y };
            for (int i = 0; i < 2; i++)
                for (int j = 0; j < 2; j++) {
                    uint8_t *p = rgba + (size_t)(ys[j] * w + xs[i]) * 4 + 3;
                    *p = (uint8_t)(*p * a / 255);
                }
        }
    }
}
