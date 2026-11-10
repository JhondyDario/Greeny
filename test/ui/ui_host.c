/* Renderiza las pantallas de la app en PC a archivos PPM para revisarlas.
 *   ./ui_host <fuente.ttf> <raiz de SD falsa> <carpeta de salida> <carpeta app/data>
 * La raiz de SD falsa debe tener config/greeny/ y, si se quiere, atmosphere/contents/... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../app/source/diag.h"
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

static float g_time; /* instante del fondo animado con el que se guarda la captura */

/* `layer` es la capa transparente de la pantalla: se compone sobre el fondo como hace la app */
static void save(const gfx_canvas *layer, const char *dir, const char *name, const char *sfx) {
    static uint32_t *final_px;
    if (!final_px) final_px = malloc(UI_W * UI_H * 4);
    gfx_canvas out = { final_px, UI_W, UI_H, UI_W };
    ui_background(&out, g_time);
    gfx_compose(&out, layer);
    const gfx_canvas *c = &out;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s%s.ppm", dir, name, sfx);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6 %d %d 255\n", c->w, c->h);
    for (int i = 0; i < c->w * c->h; i++) fwrite(&c->px[i], 1, 3, f);
    fclose(f);
}

/* caratula falsa (degradado 256x256) pasada por el mismo camino que la real */
static uint8_t *fake_cover(int size) {
    uint8_t *big = malloc(256 * 256 * 4), *small = malloc((size_t)size * size * 4);
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            uint8_t *p = big + (y * 256 + x) * 4;
            p[0] = 230; p[1] = (uint8_t)(60 + x / 2); p[2] = (uint8_t)(40 + y / 2); p[3] = 255;
        }
    gfx_scale_rgba(big, 256, 256, small, size, size);
    gfx_round_corners(small, size, size, size / 6);
    free(big);
    return small;
}

static gr_model m;

int main(int argc, char **argv) {
    if (argc != 5) { fprintf(stderr, "uso: %s <ttf> <sd> <salida> <app/data>\n", argv[0]); return 2; }
    if (!gfx_font_from_file(argv[1])) { fprintf(stderr, "no se pudo abrir la fuente\n"); return 1; }
    const char *sd = argv[2], *out = argv[3];
    char root[512];
    snprintf(root, sizeof(root), "%s/config/greeny", sd);

    static ui_assets a;
    a.mascot_small = slurp(argv[4], "mascot_small.bin", 50 * 75 * 4); a.small_w = 50; a.small_h = 75;
    gfx_canvas c = { malloc(UI_W * UI_H * 4), UI_W, UI_H, UI_W };

    gr_scan(&m, root);
    /* la ultima entrada queda con caratula; las demas muestran el recuadro con la inicial */
    if (m.ngames > 0) {
        a.covers[m.ngames - 1] = fake_cover(UI_COVER);
        a.covers_big[m.ngames - 1] = fake_cover(UI_COVER_BIG);
    }

    gr_diag_env env = { .sd = sd, .root = root, .atmosphere = true, .sysmodule_running = false, .covers_ok = true,
                        .applet_mode = true, .prev_run_known = true, .prev_run_clean = false };
    snprintf(env.prev_last_step, sizeof(env.prev_last_step), "caratulas");
    static gr_diag_item items[GR_DIAG_MAX];
    int nitems = gr_diag_run(items, GR_DIAG_MAX, &env, &m);

    /* variantes: tema x idioma x vista */
    for (int v = 0; v < 4; v++) {
        bool dark = v & 1, en = v & 2;
        char sfx[32];
        snprintf(sfx, sizeof(sfx), "%s%s", dark ? "_dark" : "", en ? "_en" : "");
        ui_set_dark(dark);
        ui_set_english(en);
        g_time = 0;

        for (int grid = 0; grid < 2; grid++) {
            gr_config cfg;
            gr_config_defaults(&cfg);
            cfg.games_view = cfg.ach_view = grid ? GR_VIEW_GRID : GR_VIEW_LIST;
            char name[64];

            ui_home(&c, &a, &m, &cfg, m.ngames > 1 ? 1 : 0, 0);
            save(&c, out, grid ? "home_grid" : "home", sfx);

            for (int g = 0; g < m.ngames; g++) {
                gr_open(&m, g, root);
                snprintf(name, sizeof(name), grid ? "detail%d_grid" : "detail%d", g);
                ui_detail(&c, &a, &m, &cfg, g, 1, 0);
                save(&c, out, name, sfx);
                ui_layout l = ui_detail_layout(&cfg);
                int rows = (m.cur.nach + l.cols - 1) / l.cols;
                if (rows > l.rows) {
                    snprintf(name, sizeof(name), grid ? "detail%d_grid_end" : "detail%d_end", g);
                    ui_detail(&c, &a, &m, &cfg, g, m.cur.nach - 1, rows - l.rows);
                    save(&c, out, name, sfx);
                }
            }
        }

        gr_config cfg;
        gr_config_defaults(&cfg);
        cfg.popup = false;
        cfg.games_view = GR_VIEW_GRID;
        ui_settings(&c, &a, &cfg, UI_OPT_THEME, true, dark);
        save(&c, out, "settings", sfx);
        ui_diag(&c, &a, items, nitems);
        save(&c, out, "diag", sfx);

        /* el mismo inicio en otros dos instantes, para ver que el fondo se mueve */
        cfg.games_view = GR_VIEW_LIST;
        ui_home(&c, &a, &m, &cfg, 0, 0);
        g_time = 12.0f; save(&c, out, "home_t12", sfx);
        g_time = 300.0f; save(&c, out, "home_t300", sfx);
        g_time = 0;

        static gr_model empty;
        static ui_assets bare;
        bare = a;
        memset(bare.covers, 0, sizeof(bare.covers));
        memset(bare.covers_big, 0, sizeof(bare.covers_big));
        ui_home(&c, &bare, &empty, &cfg, 0, 0);
        save(&c, out, "home_empty", sfx);
    }
    printf("%d juego(s), %d comprobaciones de diagnostico\n", m.ngames, nitems);
    for (int i = 0; i < nitems; i++) printf("  [%d] %s: %s\n", items[i].state, items[i].title_es, items[i].detail_es);
    return 0;
}
