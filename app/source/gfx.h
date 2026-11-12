/* Dibujo por software sobre un lienzo RGBA8888. Portable: corre igual en PC para pruebas. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t *px;   /* R | G<<8 | B<<16 | A<<24 */
    int w, h;
    int stride;     /* en pixeles */
} gfx_canvas;

typedef uint32_t gfx_color;
#define GFX_RGB(r, g, b) ((gfx_color)((r) | ((g) << 8) | ((b) << 16) | 0xFF000000u))

bool gfx_font_from_memory(const void *data, size_t size);
bool gfx_font_from_file(const char *path);

void gfx_clear(gfx_canvas *c, gfx_color col);
/* Deja el lienzo totalmente transparente, para usarlo como capa sobre un fondo. */
void gfx_clear_transparent(gfx_canvas *c);
/* Pinta `layer` encima de `dst` respetando su transparencia. Mismo tamaño los dos. */
void gfx_compose(gfx_canvas *dst, const gfx_canvas *layer);
/* Igual que gfx_compose, pero con la capa desplazada (dx, dy) pixeles y a media transparencia
 * (alpha de 0 a 256). Para las transiciones entre pantallas. */
void gfx_compose_fx(gfx_canvas *dst, const gfx_canvas *layer, int dx, int dy, unsigned alpha);
/* Aro redondeado (solo el borde, de `thick` pixeles hacia dentro) mezclado con lo que haya debajo.
 * Para el cursor que se desliza entre elementos. alpha de 0 a 256. */
void gfx_ring(gfx_canvas *c, int x, int y, int w, int h, int r, int thick, gfx_color col, unsigned alpha);
/* Circulo translucido (alpha 0..255), para fondos. */
void gfx_circle_alpha(gfx_canvas *c, int cx, int cy, int r, gfx_color col, unsigned alpha);
void gfx_rect(gfx_canvas *c, int x, int y, int w, int h, gfx_color col);
/* Rectangulo con degradado horizontal de `left` a `right`. */
void gfx_gradient(gfx_canvas *c, int x, int y, int w, int h, gfx_color left, gfx_color right);
/* Rectangulo redondeado semitransparente con degradado vertical (efecto vidrio).
 * Sobre la capa transparente deja el alfa puesto, asi se ve el fondo de atras. */
void gfx_glass(gfx_canvas *c, int x, int y, int w, int h, int r, gfx_color top, gfx_color bottom, unsigned a_top, unsigned a_bottom);
void gfx_round_rect(gfx_canvas *c, int x, int y, int w, int h, int r, gfx_color col);
void gfx_circle(gfx_canvas *c, int cx, int cy, int r, gfx_color col);
void gfx_line(gfx_canvas *c, int x0, int y0, int x1, int y1, int thick, gfx_color col);

/* Texto UTF-8. (x, y) es la esquina superior izquierda de la linea. */
int  gfx_text_width(int size, const char *s);
void gfx_text(gfx_canvas *c, int x, int y, int size, gfx_color col, const char *s);
void gfx_text_right(gfx_canvas *c, int right, int y, int size, gfx_color col, const char *s);
/* Igual que gfx_text pero corta con "..." si no cabe en max_w. */
void gfx_text_fit(gfx_canvas *c, int x, int y, int size, gfx_color col, const char *s, int max_w);

/* Imagen RGBA8888 sin premultiplicar, tamaño real. */
void gfx_image(gfx_canvas *c, int x, int y, const uint8_t *rgba, int w, int h);

/* Reescala una imagen RGBA (promedio por area) a dw x dh. */
void gfx_scale_rgba(const uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh);
/* Redondea las esquinas de una imagen RGBA bajando su alfa. */
void gfx_round_corners(uint8_t *rgba, int w, int h, int r);
