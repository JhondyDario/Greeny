/* Valida un archivo de logros en PC antes de copiarlo a la SD.
 *   ./lgcheck 0100XXXXXXXXXXXX.txt */
#include "../common/engine.h"

#include <stdio.h>
#include <stdlib.h>

static const char *k_types[] = {"u8","u16","u32","u64","s8","s16","s32","s64","f32","f64"};
static const char *k_ops[]   = {"==","!=",">",">=","<","<=","&","!&"};

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "uso: %s <archivo.txt>\n", argv[0]); return 2; }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    static char buf[64 * 1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    int too_big = !feof(f);
    fclose(f);
    if (too_big) { fprintf(stderr, "archivo muy grande (max 64 KB)\n"); return 1; }
    buf[n] = 0;

    static lg_game g;
    char err[160];
    if (lg_parse(&g, buf, err, sizeof(err)) != 0) {
        fprintf(stderr, "ERROR %s\n", err);
        return 1;
    }

    printf("juego: %s\nversion: %s\nbuild: %s\nlogros: %d\n\n", g.game[0] ? g.game : "(sin nombre)",
           g.version[0] ? g.version : "(sin indicar)", g.build[0] ? g.build : "(no se valida)", g.nach);
    if (g.debug) printf("debug: ACTIVADO (anota los valores leidos en log.txt)\n\n");
    for (int i = 0; i < g.nscan; i++) {
        lg_scan *sc = &g.scan[i];
        printf("scan %d: %s+0x%llX..+0x%llX", i + 1, sc->addr.region == LG_REGION_MAIN ? "main" : "heap",
               (unsigned long long)sc->addr.offset, (unsigned long long)(sc->addr.offset + sc->len));
        for (int d = 0; d < sc->addr.nderef; d++) printf("->0x%llX", (unsigned long long)sc->addr.deref[d]);
        printf(" %s en [%lld, %lld]\n", k_types[sc->addr.type], (long long)sc->min, (long long)sc->max);
    }
    if (g.nscan) printf("\n");
    for (int i = 0; i < g.nach; i++) {
        lg_ach *a = &g.ach[i];
        printf("[%s] %s  (%u pts, hold %u ms)\n", a->id, a->name, a->points, a->hold_ms);
        if (a->desc[0]) printf("    %s\n", a->desc);
        for (int k = 0; k < a->ncond; k++) {
            lg_cond *c = &a->cond[k];
            printf("    %s+0x%llX", c->region == LG_REGION_MAIN ? "main" : "heap", (unsigned long long)c->offset);
            for (int d = 0; d < c->nderef; d++) printf("->0x%llX", (unsigned long long)c->deref[d]);
            printf(" %s %s ", k_types[c->type], k_ops[c->op]);
            if (c->type == LG_F32 || c->type == LG_F64) printf("%g\n", c->f);
            else if (c->type >= LG_S8 && c->type <= LG_S64 && c->op < LG_BITS_SET) printf("%lld\n", (long long)c->s);
            else printf("%llu (0x%llX)\n", (unsigned long long)c->u, (unsigned long long)c->u);
        }
    }
    return 0;
}
