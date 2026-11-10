/* Dibuja el aviso del juego en PC, sobre un fondo de colores, para verlo sin consola.
 * Uso: toast_host <salida.ppm> */
#include <stdio.h>
#include <string.h>

#include "toast_draw.h"

#define N 6
static unsigned char canvas[N * (TOAST_H + 24)][TOAST_W + 48][3];
static int g_oy;

static void backdrop(int gx, int gy, unsigned char *p) {
    p[0] = (unsigned char)(40 + gx / 3); p[1] = (unsigned char)(90 + (gy * 5) % 120); p[2] = (unsigned char)(200 - gx / 4);
}

/* igual que la consola: 4 bits por componente, mezclado sobre el "juego" */
static void put(int x, int y, unsigned r, unsigned g, unsigned b, unsigned a) {
    if (x < 0 || y < 0 || x >= TOAST_W || y >= TOAST_H) return;
    unsigned char *p = canvas[g_oy + y][24 + x], base[3];
    unsigned q[3] = { (r >> 4) * 17, (g >> 4) * 17, (b >> 4) * 17 }, qa = a >> 4;
    backdrop(24 + x, g_oy + y, base); /* se recalcula: repintar un pixel no acumula mezcla */
    for (int k = 0; k < 3; k++) p[k] = (unsigned char)((q[k] * qa + base[k] * (15 - qa)) / 15);
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    for (int y = 0; y < N * (TOAST_H + 24); y++)
        for (int x = 0; x < TOAST_W + 48; x++) backdrop(x, y, canvas[y][x]);
    toast_view v[N] = {
        { TOAST_UNLOCK, 1, 5, false, "LOGRO DESBLOQUEADO", "Cinco cheles", "Ten 5 monedas" },
        { TOAST_UNLOCK, 2, 20, false, "LOGRO DESBLOQUEADO", "Casi una vida", "Ten 90 monedas sin gastarlas en una vida" },
        { TOAST_UNLOCK, 3, 50, true, "LOGRO DESBLOQUEADO", "Inmortal: el nombre más largo que existe", "Acumula 99 vidas y además escribe una descripción larguísima que no cabe en dos líneas ni queriendo" },
        { TOAST_INFO, 0, -1, false, "LOGROS ACTIVOS", "Super Mario 3D World", "7 de 12 desbloqueados" },
        { TOAST_WARN, 0, -1, false, "LOGROS DESACTIVADOS", "Super Mario 3D World", "Tu juego es otra versión. Hace falta la 1.1.0." },
        { TOAST_WARN, 0, -1, true, "ACHIEVEMENTS OFF", "Pokémon Añil ¿Ñandú?", "The achievement file has an error." },
    };
    for (int i = 0; i < N; i++) { g_oy = 12 + i * (TOAST_H + 24); toast_draw(&v[i], put); }
    FILE *f = fopen(argv[1], "wb");
    if (!f) return 1;
    fprintf(f, "P6\n%d %d\n255\n", TOAST_W + 48, N * (TOAST_H + 24));
    fwrite(canvas, 1, sizeof(canvas), f);
    fclose(f);
    return 0;
}
