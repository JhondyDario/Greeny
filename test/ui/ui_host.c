/* Renderiza las pantallas de la app en PC a archivos PPM para revisarlas.
 *   ./ui_host <fuente.ttf> <carpeta de datos> <carpeta de salida> <carpeta app/data> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../app/source/gfx.h"
#include "../../app/source/model.h"
#include "../../app/source/ui.h"

static uint8_t *slurp(const char *dir, const char *name, size_t expect) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    uint8_t *buf = malloc(expect);
    if (fread(buf, 1, expect, f) != expect) { fprintf(stderr, "%s: tamaño inesperado\n", path); exit(1); }
    fclose(f);
    return buf;
}

static void save(const gfx_canvas *c, const char *dir, const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6 %d %d 255\n", c->w, c->h);
    for (int i = 0; i < c->w * c->h; i++) fwrite(&c->px[i], 1, 3, f);
    fclose(f);
}

/* caratula falsa (degradado 256x256) pasada por el mismo camino que la real */
static uint8_t *fake_cover(void) {
    uint8_t *big = malloc(256 * 256 * 4), *small = malloc(UI_COVER * UI_COVER * 4);
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            uint8_t *p = big + (y * 256 + x) * 4;
            p[0] = 230; p[1] = (uint8_t)(60 + x / 2); p[2] = (uint8_t)(40 + y / 2); p[3] = 255;
        }
    gfx_scale_rgba(big, 256, 256, small, UI_COVER, UI_COVER);
    gfx_round_corners(small, UI_COVER, UI_COVER, 12);
    free(big);
    return small;
}

static gr_model m;

int main(int argc, char **argv) {
    if (argc != 5) { fprintf(stderr, "uso: %s <ttf> <datos> <salida> <app/data>\n", argv[0]); return 2; }
    if (!gfx_font_from_file(argv[1])) { fprintf(stderr, "no se pudo abrir la fuente\n"); return 1; }

    static ui_assets a;
    a.mascot_big = slurp(argv[4], "mascot_big.bin", 220 * 330 * 4);   a.big_w = 220;  a.big_h = 330;
    a.mascot_small = slurp(argv[4], "mascot_small.bin", 50 * 75 * 4); a.small_w = 50; a.small_h = 75;
    gfx_canvas c = { malloc(UI_W * UI_H * 4), UI_W, UI_H, UI_W };

    gr_scan(&m, argv[2]);
    /* la ultima entrada queda con caratula; las demas muestran el recuadro con la inicial */
    if (m.ngames > 0) a.covers[m.ngames - 1] = fake_cover();

    for (int en = 0; en < 2; en++) {
        const char *sfx = en ? "_en" : "";
        char name[48];
        ui_set_english(en);

        ui_home(&c, &a, &m, 0, 0);
        snprintf(name, sizeof(name), "home%s", sfx);
        save(&c, argv[3], name);

        for (int g = 0; g < m.ngames; g++) {
            gr_open(&m, g, argv[2]);
            snprintf(name, sizeof(name), "detail%d%s", g, sfx);
            ui_detail(&c, &a, &m, g, 1, 0);
            save(&c, argv[3], name);
        }

        gr_config cfg;
        gr_config_defaults(&cfg);
        cfg.popup = false;
        ui_settings(&c, &a, &cfg, 1, true);
        snprintf(name, sizeof(name), "settings%s", sfx);
        save(&c, argv[3], name);

        static gr_model empty;
        static ui_assets bare;
        bare = a;
        memset(bare.covers, 0, sizeof(bare.covers));
        ui_home(&c, &bare, &empty, 0, 0);
        snprintf(name, sizeof(name), "home_empty%s", sfx);
        save(&c, argv[3], name);
    }
    printf("%d juego(s) renderizados\n", m.ngames);
    return 0;
}
