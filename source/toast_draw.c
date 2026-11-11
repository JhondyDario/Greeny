#include "toast_draw.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "toast_font.h"

typedef struct { float r, g, b; } rgb;
#define RGB(hex) ((rgb){ (float)(((hex) >> 16) & 255), (float)(((hex) >> 8) & 255), (float)((hex) & 255) })

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static rgb mix(rgb a, rgb b, float t) { return (rgb){ a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }

/* Cuanto de un pixel cae dentro de cada figura (0..1), con borde suave. */
static float in_round_rect(float px, float py, float x, float y, float w, float h, float r) {
    float qx = fabsf(px - (x + w / 2)) - (w / 2 - r), qy = fabsf(py - (y + h / 2)) - (h / 2 - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float inside = qx > qy ? qx : qy;
    float d = sqrtf(ox * ox + oy * oy) + (inside < 0 ? inside : 0) - r;
    return clamp01(0.5f - d);
}

static float in_circle(float px, float py, float cx, float cy, float r) {
    return clamp01(0.5f - (sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy)) - r));
}

static float in_segment(float px, float py, float ax, float ay, float bx, float by, float thick) {
    float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
    float t = clamp01((wx * vx + wy * vy) / (vx * vx + vy * vy));
    float dx = wx - vx * t, dy = wy - vy * t;
    return clamp01(0.5f - (sqrtf(dx * dx + dy * dy) - thick / 2));
}

/* Estrella de cinco puntas centrada en (0,0), radio 1; puntos ya calculados. */
static const float k_star[10][2] = {
    { 0.000f, -1.000f }, { 0.259f, -0.356f }, { 0.951f, -0.309f }, { 0.418f, 0.136f }, { 0.588f, 0.809f },
    { 0.000f, 0.440f }, { -0.588f, 0.809f }, { -0.418f, 0.136f }, { -0.951f, -0.309f }, { -0.259f, -0.356f },
};

static bool point_in_star(float x, float y) {
    bool in = false;
    for (int i = 0, j = 9; i < 10; j = i++) {
        float xi = k_star[i][0], yi = k_star[i][1], xj = k_star[j][0], yj = k_star[j][1];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) in = !in;
    }
    return in;
}

static float in_star(float px, float py, float cx, float cy, float r) {
    if (fabsf(px - cx) > r + 1 || fabsf(py - cy) > r + 1) return 0;
    int hits = 0;
    for (int sy = 0; sy < 4; sy++)
        for (int sx = 0; sx < 4; sx++)
            if (point_in_star((px + (sx - 1.5f) / 4 - cx) / r, (py + (sy - 1.5f) / 4 - cy) / r)) hits++;
    return hits / 16.0f;
}

/* ---------- texto ---------- */

/* UTF-8 -> Latin-1, que es lo que cubren las letras guardadas. */
static int to_latin1(const char *s, uint8_t *out, int cap) {
    int n = 0;
    const uint8_t *p = (const uint8_t *)(s ? s : "");
    while (*p && n < cap) {
        unsigned cp;
        if (*p < 0x80) cp = *p++;
        else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { cp = ((*p & 0x1Fu) << 6) | (p[1] & 0x3Fu); p += 2; }
        else {
            p++;
            while ((*p & 0xC0) == 0x80) p++;
            cp = '?';
        }
        out[n++] = cp < 256 ? (uint8_t)cp : '?';
    }
    return n;
}

static int text_width(const tf_font *f, const uint8_t *s, int n) {
    int w = 0;
    for (int i = 0; i < n; i++) w += f->glyphs[s[i]].adv;
    return w;
}

/* Pinta letras sobre un fondo de color conocido (las letras nunca cruzan dos fondos). */
static void draw_text(toast_put_fn put, const tf_font *f, int x, int y, const uint8_t *s, int n, rgb fg, rgb bg) {
    for (int i = 0; i < n; i++) {
        const tf_glyph *g = &f->glyphs[s[i]];
        for (int row = 0; row < g->h; row++) {
            for (int col = 0; col < g->w; col++) {
                int idx = row * g->w + col;
                unsigned v = (f->data[g->off + idx / 2] >> ((idx & 1) * 4)) & 15;
                if (!v) continue;
                rgb c = mix(bg, fg, v / 15.0f);
                put(x + g->x + col, y + g->y + row, (unsigned)c.r, (unsigned)c.g, (unsigned)c.b, 255);
            }
        }
        x += g->adv;
    }
}

/* Hasta max_lines lineas cortando por palabras; la ultima termina en "..." si sobra texto. */
static void draw_wrapped(toast_put_fn put, const tf_font *f, int x, int y, int max_w, int line_h, int max_lines,
                         const char *text, rgb fg, rgb bg) {
    uint8_t buf[160];
    int n = to_latin1(text, buf, sizeof(buf)), pos = 0;
    for (int line = 0; line < max_lines && pos < n; line++) {
        while (pos < n && buf[pos] == ' ') pos++;
        int len = 0, w = 0, last_space = -1;
        while (pos + len < n) {
            int adv = f->glyphs[buf[pos + len]].adv;
            if (w + adv > max_w) break;
            if (buf[pos + len] == ' ') last_space = len;
            w += adv;
            len++;
        }
        bool rest = pos + len < n;
        if (rest && line < max_lines - 1) {
            if (last_space > 0) len = last_space;
        } else if (rest) {
            static const uint8_t dots[3] = { '.', '.', '.' };
            int dw = text_width(f, dots, 3);
            while (len > 0 && text_width(f, buf + pos, len) + dw > max_w) len--;
            while (len > 0 && buf[pos + len - 1] == ' ') len--;
            draw_text(put, f, x, y + line * line_h, buf + pos, len, fg, bg);
            draw_text(put, f, x + text_width(f, buf + pos, len), y + line * line_h, dots, 3, fg, bg);
            return;
        }
        draw_text(put, f, x, y + line * line_h, buf + pos, len, fg, bg);
        pos += len;
    }
}

/* ---------- el aviso ---------- */

void toast_draw(const toast_view *v, toast_put_fn put) {
    const rgb card = v->dark ? RGB(0x1E2A22) : RGB(0xFFFFFF);
    const rgb ink = v->dark ? RGB(0xFFFFFF) : RGB(0x1E2A1F);
    const rgb soft = v->dark ? RGB(0xB8C6BB) : RGB(0x5C6B5E);
    const rgb white = RGB(0xFFFFFF), yellow = RGB(0xF7C91E), on_yellow = RGB(0x1E2A1F);

    rgb accent, head;
    switch (v->kind) {
        case TOAST_WARN: accent = RGB(0xE5484D); head = v->dark ? RGB(0xFF8A8E) : RGB(0xC62F35); break;
        case TOAST_UNLOCK:
            accent = v->tier == 3 ? RGB(0xF0B400) : v->tier == 2 ? RGB(0x93A0AE) : RGB(0xB56A2C);
            head = v->dark ? RGB(0x6FDC7C) : RGB(0x1E8A2E);
            break;
        default: accent = RGB(0x2FA63B); head = v->dark ? RGB(0x6FDC7C) : RGB(0x1E8A2E); break;
    }
    const rgb edge = mix(card, accent, 0.55f);

    /* etiqueta de puntos, arriba a la derecha */
    uint8_t pts[12];
    int npts = 0, pill_w = 0;
    if (v->points >= 0) {
        char tmp[12];
        int p = v->points > 9999 ? 9999 : v->points, len = 0;
        char digits[5];
        do { digits[len++] = (char)('0' + p % 10); p /= 10; } while (p);
        int k = 0;
        tmp[k++] = '+';
        while (len) tmp[k++] = digits[--len];
        memcpy(tmp + k, " pts", 5);
        npts = to_latin1(tmp, pts, sizeof(pts));
        pill_w = text_width(&tf_small, pts, npts) + 20;
    }
    const float pill_x = TOAST_W - 16 - pill_w, pill_y = 14, pill_h = 24; /* la etiqueta se queda arriba */

    const float bcx = 62, bcy = 64, br = 38;
    for (int y = 0; y < TOAST_H; y++) {
        for (int x = 0; x < TOAST_W; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float outer = in_round_rect(px, py, 2, 2, TOAST_W - 4, TOAST_H - 4, 24);
            if (outer <= 0) { put(x, y, 0, 0, 0, 0); continue; }
            rgb c = mix(edge, card, in_round_rect(px, py, 4, 4, TOAST_W - 8, TOAST_H - 8, 22));

            float badge = in_circle(px, py, bcx, bcy, br);
            if (badge > 0) {
                c = mix(c, mix(accent, white, 0.35f), badge);                      /* aro claro */
                c = mix(c, accent, in_circle(px, py, bcx, bcy, br - 4));
                float mark;
                if (v->kind == TOAST_UNLOCK) mark = in_star(px, py, bcx, bcy + 1, 24);
                else if (v->kind == TOAST_WARN) {
                    mark = in_segment(px, py, bcx, bcy - 19, bcx, bcy + 5, 9);
                    float dot = in_circle(px, py, bcx, bcy + 19, 5.5f);
                    if (dot > mark) mark = dot;
                } else {
                    mark = in_segment(px, py, bcx - 17, bcy + 1, bcx - 5, bcy + 13, 9);
                    float b = in_segment(px, py, bcx - 5, bcy + 13, bcx + 18, bcy - 12, 9);
                    if (b > mark) mark = b;
                }
                c = mix(c, white, mark);
            }
            if (pill_w) c = mix(c, yellow, in_round_rect(px, py, pill_x, pill_y, (float)pill_w, pill_h, pill_h / 2));
            put(x, y, (unsigned)c.r, (unsigned)c.g, (unsigned)c.b, (unsigned)(outer * 250));
        }
    }

    const int tx = 116, tw = TOAST_W - tx - 18;
    if (pill_w) draw_text(put, &tf_small, (int)pill_x + 10, (int)pill_y + 3, pts, npts, on_yellow, yellow);
    /* si la descripcion cabe en una linea (o no hay), el texto baja para quedar centrado en la tarjeta */
    uint8_t probe[160];
    int nprobe = to_latin1(v->desc, probe, sizeof(probe));
    int dy = nprobe == 0 ? 18 : text_width(&tf_small, probe, nprobe) <= tw ? 9 : 0;
    draw_wrapped(put, &tf_small, tx, 17 + dy, tw - (pill_w ? pill_w + 8 : 0), 18, 1, v->title, head, card);
    draw_wrapped(put, &tf_big, tx, 37 + dy, tw, 24, 1, v->name, ink, card);
    draw_wrapped(put, &tf_small, tx, 68 + dy, tw, 19, 2, v->desc, soft, card);
}
