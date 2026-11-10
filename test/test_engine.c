/* Pruebas del motor en PC con memoria falsa. make -C test */
#include "../common/config.h"
#include "../common/engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail, g_total;
#define CHECK(x) do { g_total++; if (!(x)) { g_fail++; printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

#define MAIN_BASE 0x8000000ull
#define HEAP_BASE 0x2000000000ull
#define REGION_SZ 0x1000

static uint8_t g_main[REGION_SZ], g_heap[REGION_SZ];
static int g_reads;

static bool fake_read(void *ctx, uint64_t addr, void *out, size_t size) {
    (void)ctx;
    g_reads++;
    if (addr >= MAIN_BASE && addr + size <= MAIN_BASE + REGION_SZ) {
        memcpy(out, g_main + (addr - MAIN_BASE), size);
        return true;
    }
    if (addr >= HEAP_BASE && addr + size <= HEAP_BASE + REGION_SZ) {
        memcpy(out, g_heap + (addr - HEAP_BASE), size);
        return true;
    }
    return false;
}

static const lg_bases g_bases = { MAIN_BASE, HEAP_BASE };

static void put(uint8_t *mem, size_t off, const void *v, size_t n) { memcpy(mem + off, v, n); }
#define PUT(mem, off, type, val) do { type _t = (val); put(mem, off, &_t, sizeof(_t)); } while (0)

static int parse(lg_game *g, const char *src, char *err, size_t errlen) {
    char *copy = strdup(src);
    int rc = lg_parse(g, copy, err, errlen);
    free(copy);
    return rc;
}

/* evalua una sola condicion escrita como texto */
static bool cond(const char *expr) {
    static lg_game g;
    char src[256], err[128];
    snprintf(src, sizeof(src), "[t]\nwhen=%s\n", expr);
    if (parse(&g, src, err, sizeof(err)) != 0) {
        printf("no parseo '%s': %s\n", expr, err);
        g_fail++;
        return false;
    }
    return lg_eval_cond(&g.ach[0].cond[0], &g_bases, fake_read, NULL);
}

static bool bad(const char *src, const char *expect) {
    static lg_game g;
    char err[128] = "";
    if (parse(&g, src, err, sizeof(err)) == 0) { printf("debio fallar: %s\n", src); return false; }
    if (!strstr(err, expect)) { printf("error inesperado '%s' (esperaba '%s')\n", err, expect); return false; }
    return true;
}

static void test_parse(void) {
    static lg_game g;
    char err[128];
    const char *src =
        "\xEF\xBB\xBF# comentario\n"
        "game = Juego de Prueba\r\n"
        "version = 1.1.0\n"
        "build = 3ca12dfaaf9c82da\n"
        "\n"
        "[monedas_100]\n"
        "name = Rico\n"
        "desc = Junta 100 monedas\n"
        "points = 25\n"
        "when = main+0x10 u32 >= 100\n"
        "\n"
        "[jefe-final]\n"
        "hold = 0\n"
        "when = heap+0x20->0x8->0x4 u8 & 0x80\n"
        "when = main+0x14 f32 > 0.5\n";
    CHECK(parse(&g, src, err, sizeof(err)) == 0);
    CHECK(g.nach == 2);
    CHECK(!strcmp(g.game, "Juego de Prueba"));
    CHECK(!strcmp(g.build, "3CA12DFAAF9C82DA"));
    CHECK(!strcmp(g.version, "1.1.0"));
    CHECK(g.ach[0].points == 25 && g.ach[1].points == LG_DEFAULT_POINTS);
    CHECK(bad("[a]\npoints=1001\nwhen=main+0 u8 == 1\n", "points invalido"));
    CHECK(!strcmp(g.ach[0].id, "monedas_100"));
    CHECK(!strcmp(g.ach[0].name, "Rico"));
    CHECK(!strcmp(g.ach[0].desc, "Junta 100 monedas"));
    CHECK(g.ach[0].hold_ms == LG_DEFAULT_HOLD_MS);
    CHECK(g.ach[0].ncond == 1 && g.ach[0].cond[0].offset == 0x10 && g.ach[0].cond[0].u == 100);
    CHECK(!strcmp(g.ach[1].name, "jefe-final")); /* sin name usa el id */
    CHECK(g.ach[1].hold_ms == 0 && g.ach[1].ncond == 2);
    CHECK(g.ach[1].cond[0].region == LG_REGION_HEAP && g.ach[1].cond[0].nderef == 2);
    CHECK(g.ach[1].cond[0].deref[0] == 8 && g.ach[1].cond[0].deref[1] == 4);
    CHECK(g.ach[1].cond[0].op == LG_BITS_SET && g.ach[1].cond[1].type == LG_F32);

    CHECK(bad("", "ningun logro"));
    CHECK(bad("game=x\n", "ningun logro"));
    CHECK(bad("[a]\n", "ultimo logro no tiene"));
    CHECK(bad("[a]\n[b]\nwhen=main+0 u8 == 1\n", "linea 2: el logro anterior no tiene"));
    CHECK(bad("[a]\nwhen=main+0 u8 == 1\n[a]\nwhen=main+0 u8 == 1\n", "linea 3: id repetido"));
    CHECK(bad("[a b]\nwhen=main+0 u8 == 1\n", "id invalido"));
    CHECK(bad("[a\nwhen=main+0 u8 == 1\n", "mal cerrada"));
    CHECK(bad("when=main+0 u8 == 1\n", "antes del primer logro"));
    CHECK(bad("build=XYZ\n", "hexadecimal"));
    CHECK(bad("[a]\nfoo=1\n", "clave desconocida"));
    CHECK(bad("[a]\nsolo texto\n", "clave=valor"));
    CHECK(bad("[a]\nwhen=stack+0 u8 == 1\n", "main+ o heap+"));
    CHECK(bad("[a]\nwhen=main0 u8 == 1\n", "falta el +"));
    CHECK(bad("[a]\nwhen=main+zz u8 == 1\n", "offset invalido"));
    CHECK(bad("[a]\nwhen=main+0->1->2->3->4->5 u8 == 1\n", "demasiados niveles"));
    CHECK(bad("[a]\nwhen=main+0->x u8 == 1\n", "offset de puntero"));
    CHECK(bad("[a]\nwhen=main+0 u8 ==\n", "formato"));
    CHECK(bad("[a]\nwhen=main+0 u8 == 1 2\n", "formato"));
    CHECK(bad("[a]\nwhen=main+0 u7 == 1\n", "tipo desconocido"));
    CHECK(bad("[a]\nwhen=main+0 u8 => 1\n", "operador desconocido"));
    CHECK(bad("[a]\nwhen=main+0 u8 == 256\n", "no cabe"));
    CHECK(bad("[a]\nwhen=main+0 u8 == -1\n", "sin signo invalido"));
    CHECK(bad("[a]\nwhen=main+0 s8 == 128\n", "no cabe"));
    CHECK(bad("[a]\nwhen=main+0 s8 & 0x100\n", "no cabe"));
    CHECK(bad("[a]\nwhen=main+0 u32 == 1.5\n", "invalido"));
    CHECK(bad("[a]\nwhen=main+0 f32 & 1\n", "no aplican"));
    CHECK(bad("[a]\nwhen=main+0 f32 == abc\n", "decimal invalido"));
    CHECK(bad("[a]\nhold=abc\nwhen=main+0 u8 == 1\n", "hold invalido"));
    CHECK(bad("[a]\nwhen=main+0 u8 == 1\nwhen=main+0 u8 == 1\nwhen=main+0 u8 == 1\nwhen=main+0 u8 == 1\n"
              "when=main+0 u8 == 1\nwhen=main+0 u8 == 1\nwhen=main+0 u8 == 1\n", "linea 8: demasiadas condiciones"));

    /* limite de 64 logros */
    static char big[8192];
    size_t off = 0;
    for (int i = 0; i < 65; i++)
        off += (size_t)snprintf(big + off, sizeof(big) - off, "[a%d]\nwhen=main+0 u8 == 1\n", i);
    CHECK(bad(big, "demasiados logros"));

    /* nombres largos se truncan sin romper UTF-8 */
    char longsrc[512] = "[a]\nname=";
    for (int i = 0; i < 40; i++) strcat(longsrc, "\xC3\xB1"); /* ñ x40 = 80 bytes */
    strcat(longsrc, "\nwhen=main+0 u8 == 1\n");
    CHECK(parse(&g, longsrc, err, sizeof(err)) == 0);
    size_t n = strlen(g.ach[0].name);
    CHECK(n < LG_NAME_LEN && n % 2 == 0 && n >= LG_NAME_LEN - 2);
}

static void test_eval(void) {
    memset(g_main, 0, sizeof(g_main));
    memset(g_heap, 0, sizeof(g_heap));

    PUT(g_main, 0x10, uint32_t, 100);
    CHECK(cond("main+0x10 u32 == 100"));
    CHECK(cond("main+16 u32 == 0x64"));
    CHECK(!cond("main+0x10 u32 != 100"));
    CHECK(cond("main+0x10 u32 >= 100"));
    CHECK(cond("main+0x10 u32 <= 100"));
    CHECK(!cond("main+0x10 u32 > 100"));
    CHECK(!cond("main+0x10 u32 < 100"));
    CHECK(cond("main+0x10 u8 == 100"));     /* byte bajo, little-endian */
    CHECK(cond("main+0x11 u8 == 0"));

    /* el ancho importa: basura en los bytes de al lado no debe colarse */
    PUT(g_main, 0x20, uint64_t, 0xFFFFFFFF00000005ull);
    CHECK(cond("main+0x20 u32 == 5"));
    CHECK(cond("main+0x20 u16 == 5"));
    CHECK(cond("main+0x20 u64 == 0xFFFFFFFF00000005"));
    CHECK(cond("main+0x20 u64 > 0x7FFFFFFFFFFFFFFF")); /* u64 se compara sin signo */

    /* con signo */
    PUT(g_main, 0x30, int32_t, -5);
    CHECK(cond("main+0x30 s32 == -5"));
    CHECK(cond("main+0x30 s32 < 0"));
    CHECK(cond("main+0x30 u32 > 0x7FFFFFFF")); /* mismo dato leido sin signo */
    PUT(g_main, 0x34, int8_t, -1);
    CHECK(cond("main+0x34 s8 == -1"));
    CHECK(cond("main+0x34 u8 == 255"));
    PUT(g_main, 0x36, int16_t, -300);
    CHECK(cond("main+0x36 s16 <= -300"));
    PUT(g_main, 0x38, int64_t, -1);
    CHECK(cond("main+0x38 s64 < 0"));

    /* flotantes */
    PUT(g_main, 0x40, float, 0.75f);
    CHECK(cond("main+0x40 f32 > 0.5"));
    CHECK(cond("main+0x40 f32 == 0.75"));
    CHECK(!cond("main+0x40 f32 >= 1"));
    PUT(g_main, 0x48, double, -2.5);
    CHECK(cond("main+0x48 f64 < -2"));
    PUT(g_main, 0x50, uint32_t, 0x7FC00000); /* NaN nunca cumple nada */
    CHECK(!cond("main+0x50 f32 != 1"));
    CHECK(!cond("main+0x50 f32 < 1"));

    /* bits */
    PUT(g_main, 0x60, uint8_t, 0x85);
    CHECK(cond("main+0x60 u8 & 0x80"));
    CHECK(cond("main+0x60 u8 & 0x85"));
    CHECK(!cond("main+0x60 u8 & 0x82"));    /* tienen que estar TODOS */
    CHECK(cond("main+0x60 u8 !& 0x42"));
    CHECK(!cond("main+0x60 u8 !& 0x04"));
    CHECK(cond("main+0x60 s8 & 0x80"));     /* mascara sobre tipo con signo */

    /* punteros: main+0x100 -> heap+0x200; [heap+0x200+0x8] -> heap+0x300; valor en +0x4 */
    PUT(g_main, 0x100, uint64_t, HEAP_BASE + 0x200);
    PUT(g_heap, 0x208, uint64_t, HEAP_BASE + 0x300);
    PUT(g_heap, 0x304, uint16_t, 777);
    PUT(g_heap, 0x210, uint32_t, 42);
    CHECK(cond("main+0x100->0x10 u32 == 42"));
    CHECK(cond("main+0x100->0x8->0x4 u16 == 777"));
    CHECK(cond("heap+0x208->4 u16 == 777"));
    CHECK(cond("heap+0x210 u32 == 42"));

    /* puntero nulo, puntero basura y fuera de rango: falso, nunca crashea */
    CHECK(!cond("main+0x108->0x0 u32 == 0"));
    PUT(g_main, 0x110, uint64_t, 0xDEADBEEF00ull);
    CHECK(!cond("main+0x110->0x0 u32 == 0"));
    CHECK(!cond("main+0x110->0x0 u32 != 0"));
    CHECK(!cond("main+0xFFFFFF u8 == 0"));
    CHECK(!cond("main+0xFFE u32 == 0"));    /* se sale por 2 bytes */

    /* base desconocida (sin heap) */
    static lg_game g;
    char err[128];
    CHECK(parse(&g, "[a]\nwhen=heap+0x210 u32 == 42\n", err, sizeof(err)) == 0);
    lg_bases nob = { MAIN_BASE, 0 };
    CHECK(!lg_eval_cond(&g.ach[0].cond[0], &nob, fake_read, NULL));
}

static void test_tick(void) {
    static lg_game g;
    char err[128];
    int out[8];
    memset(g_main, 0, sizeof(g_main));

    const char *src =
        "[a]\nwhen=main+0x10 u32 >= 100\n"                              /* hold 500 por defecto */
        "[b]\nhold=0\nwhen=main+0x10 u32 >= 50\nwhen=main+0x14 u8 == 1\n" /* AND */
        "[c]\nhold=2000\nwhen=main+0x18 u8 == 0\n";                      /* cierto desde el arranque */
    CHECK(parse(&g, src, err, sizeof(err)) == 0);

    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 1000, out, 8) == 0);

    /* b: las dos condiciones a la vez */
    PUT(g_main, 0x10, uint32_t, 60);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 1250, out, 8) == 0);
    PUT(g_main, 0x14, uint8_t, 1);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 1500, out, 8) == 1 && out[0] == 1);
    CHECK(g.ach[1].unlocked);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 1750, out, 8) == 0); /* no se repite */

    /* a: un parpadeo de menos de 500 ms no cuenta */
    PUT(g_main, 0x10, uint32_t, 100);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 2000, out, 8) == 0);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 2250, out, 8) == 0);
    PUT(g_main, 0x10, uint32_t, 0);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 2500, out, 8) == 0);
    PUT(g_main, 0x10, uint32_t, 100);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 2750, out, 8) == 0); /* el reloj reinicia */
    CHECK(!g.ach[0].unlocked);

    /* c lleva cierto desde t=1000 con hold 2000: cae en t=3000; a todavia no (250 < 500) */
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 3000, out, 8) == 1 && out[0] == 2);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 3250, out, 8) == 1 && out[0] == 0);
    CHECK(g.ach[0].unlocked && g.ach[1].unlocked && g.ach[2].unlocked);

    /* ya desbloqueados no vuelven a leer memoria */
    g_reads = 0;
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 4000, out, 8) == 0);
    CHECK(g_reads == 0);

    /* si `out` se llena, el resto queda para el proximo tick, no se pierde */
    CHECK(parse(&g, "[x]\nhold=0\nwhen=main+0x18 u8 == 0\n[y]\nhold=0\nwhen=main+0x18 u8 == 0\n", err, sizeof(err)) == 0);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 5000, out, 1) == 1 && out[0] == 0);
    CHECK(!g.ach[1].unlocked);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 5250, out, 1) == 1 && out[0] == 1);
}

static void test_debug(void) {
    static lg_game g;
    char err[128];
    int out[4];
    memset(g_main, 0, sizeof(g_main));
    memset(g_heap, 0, sizeof(g_heap));

    CHECK(bad("debug=2\n", "debug debe ser"));
    const char *src = "debug = 1\n[a]\nhold=0\nwhen=main+0x10 u8 == 9\nwhen=main+0x100->0x4 u16 == 7\n";
    CHECK(parse(&g, src, err, sizeof(err)) == 0 && g.debug);
    lg_cond *c0 = &g.ach[0].cond[0], *c1 = &g.ach[0].cond[1];
    CHECK(!c0->dbg_seen && !c0->dbg_dirty);

    /* en debug se evaluan todas aunque la primera sea falsa */
    PUT(g_main, 0x10, uint32_t, 0xAABBCC03);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 0, out, 4) == 0);
    CHECK(c0->dbg_seen && c0->dbg_dirty && c0->dbg_ok && c0->dbg_raw == 3); /* solo el ancho del tipo */
    CHECK(c1->dbg_seen && c1->dbg_dirty && !c1->dbg_ok);                    /* puntero nulo */

    /* sin cambios no se ensucia */
    c0->dbg_dirty = c1->dbg_dirty = false;
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 250, out, 4) == 0);
    CHECK(!c0->dbg_dirty && !c1->dbg_dirty);

    /* cambia el valor / el puntero empieza a resolver */
    PUT(g_main, 0x10, uint8_t, 4);
    PUT(g_main, 0x100, uint64_t, HEAP_BASE + 0x40);
    PUT(g_heap, 0x44, uint16_t, 7);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 500, out, 4) == 0);
    CHECK(c0->dbg_dirty && c0->dbg_raw == 4);
    CHECK(c1->dbg_dirty && c1->dbg_ok && c1->dbg_raw == 7);

    PUT(g_main, 0x10, uint8_t, 9);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 750, out, 4) == 1);

    /* sin debug hay corto circuito: la segunda ni se lee */
    CHECK(parse(&g, "[a]\nwhen=main+0x10 u8 == 1\nwhen=main+0x11 u8 == 1\n", err, sizeof(err)) == 0 && !g.debug);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 0, out, 4) == 0);
    CHECK(g.ach[0].cond[0].dbg_seen && !g.ach[0].cond[1].dbg_seen);
}

static void test_scan(void) {
    static lg_game g;
    char err[128];
    lg_scan_hit hits[4];
    memset(g_main, 0, sizeof(g_main));
    memset(g_heap, 0, sizeof(g_heap));

    CHECK(bad("scan=main+0x100 u32 0x100 1 99\n", "al menos un nivel"));
    CHECK(bad("scan=main+0x104->0x8 u32 0x100 1 99\n", "multiplo de 8"));
    CHECK(bad("scan=main+0x100->0x8 f32 0x100 1 99\n", "tipo de scan"));
    CHECK(bad("scan=main+0x100->0x8 u32 0 1 99\n", "largo de scan"));
    CHECK(bad("scan=main+0x100->0x8 u32 0x200000 1 99\n", "largo de scan"));
    CHECK(bad("scan=main+0x100->0x8 u32 0x100 99 1\n", "rango"));
    CHECK(bad("scan=main+0x100->0x8 u32 0x100 1\n", "formato"));

    /* un archivo solo con scan (sin logros) es valido */
    CHECK(parse(&g, "scan = main+0x0->0x18->0x0->0x48 u32 0x1000 1 999\n", err, sizeof(err)) == 0);
    CHECK(g.nscan == 1 && g.nach == 0 && g.scan[0].len == 0x1000 && g.scan[0].addr.nderef == 3);

    /* la cadena buena cuelga de main+0x7A8: -> heap+0x100 (+0x18) -> heap+0x200 (+0) -> heap+0x300 (+0x48) = 7 */
    PUT(g_main, 0x7A8, uint64_t, HEAP_BASE + 0x100);
    PUT(g_heap, 0x118, uint64_t, HEAP_BASE + 0x200);
    PUT(g_heap, 0x200, uint64_t, HEAP_BASE + 0x300);
    PUT(g_heap, 0x348, uint32_t, 7);
    /* ruido: punteros que no resuelven, basura, y una cadena que resuelve fuera de rango */
    PUT(g_main, 0x10, uint64_t, 0xDEADBEEF00ull);
    PUT(g_main, 0x18, uint64_t, 5);
    PUT(g_main, 0x20, uint64_t, HEAP_BASE + 0x101);            /* desalineado */
    PUT(g_main, 0x28, uint64_t, HEAP_BASE + 0x400);            /* segundo puntero nulo */
    PUT(g_main, 0x30, uint64_t, HEAP_BASE + 0x500);
    PUT(g_heap, 0x518, uint64_t, HEAP_BASE + 0x600);
    PUT(g_heap, 0x600, uint64_t, HEAP_BASE + 0x700);
    PUT(g_heap, 0x748, uint32_t, 5000);                        /* fuera de 1..999 */

    CHECK(lg_scan_run(&g.scan[0], &g_bases, fake_read, NULL, hits, 4) == 1);
    CHECK(hits[0].offset == 0x7A8 && hits[0].value == 7);

    /* el valor cambia y el scan lo sigue */
    PUT(g_heap, 0x348, uint32_t, 12);
    CHECK(lg_scan_run(&g.scan[0], &g_bases, fake_read, NULL, hits, 4) == 1 && hits[0].value == 12);

    /* devuelve el total aunque no quepan */
    PUT(g_heap, 0x748, uint32_t, 3);
    CHECK(lg_scan_run(&g.scan[0], &g_bases, fake_read, NULL, hits, 1) == 2);
    CHECK(hits[0].offset == 0x30 && hits[0].value == 3);

    /* rango que se sale de la memoria legible: no crashea, encuentra lo que pueda */
    CHECK(parse(&g, "scan = main+0x800->0x18->0x0->0x48 u32 0x4000 1 999\n", err, sizeof(err)) == 0);
    CHECK(lg_scan_run(&g.scan[0], &g_bases, fake_read, NULL, hits, 4) == 0);

    /* un solo nivel y tipo con signo */
    PUT(g_main, 0x40, uint64_t, HEAP_BASE + 0x800);
    PUT(g_heap, 0x804, int16_t, -3);
    CHECK(parse(&g, "scan = main+0x0->0x4 s16 0x100 -5 -1\n", err, sizeof(err)) == 0);
    CHECK(lg_scan_run(&g.scan[0], &g_bases, fake_read, NULL, hits, 4) == 1);
    CHECK(hits[0].offset == 0x40 && hits[0].value == -3);

    /* sin base conocida */
    lg_bases nob = { 0, 0 };
    CHECK(lg_scan_run(&g.scan[0], &nob, fake_read, NULL, hits, 4) == 0);
}

static void test_i18n_config(void) {
    static lg_game g;
    char err[128];
    const char *src = "[a]\nname = Hola\ndesc = Mundo\nname.en = Hello\ndesc.en = World\nwhen=main+0 u8 == 1\n"
                      "[b]\nname = Solo\nwhen=main+0 u8 == 1\n";
    CHECK(parse(&g, src, err, sizeof(err)) == 0);
    CHECK(!strcmp(lg_ach_name(&g.ach[0], false), "Hola") && !strcmp(lg_ach_name(&g.ach[0], true), "Hello"));
    CHECK(!strcmp(lg_ach_desc(&g.ach[0], false), "Mundo") && !strcmp(lg_ach_desc(&g.ach[0], true), "World"));
    CHECK(!strcmp(lg_ach_name(&g.ach[1], true), "Solo")); /* sin traduccion cae al texto base */
    CHECK(!strcmp(lg_ach_desc(&g.ach[1], true), ""));

    const char *path = "test_config.tmp";
    gr_config c;
    remove(path);
    gr_config_load(&c, path); /* no existe: valores por defecto */
    CHECK(c.lang == GR_LANG_AUTO && c.sound && c.popup);
    CHECK(c.theme == GR_THEME_AUTO && c.games_view == GR_VIEW_LIST && c.ach_view == GR_VIEW_LIST);

    c.lang = GR_LANG_EN; c.sound = false; c.popup = true;
    c.theme = GR_THEME_DARK; c.games_view = GR_VIEW_GRID; c.ach_view = GR_VIEW_LIST; c.animated_bg = false;
    CHECK(gr_config_save(&c, path) == 0);
    gr_config c2;
    gr_config_load(&c2, path);
    CHECK(c2.lang == GR_LANG_EN && !c2.sound && c2.popup);
    CHECK(c2.theme == GR_THEME_DARK && c2.games_view == GR_VIEW_GRID && c2.ach_view == GR_VIEW_LIST);
    CHECK(!c2.animated_bg);

    /* un config.ini viejo (sin las claves nuevas) se lee con los valores por defecto en lo que falta */
    FILE *old = fopen(path, "w");
    fputs("lang = es\nsound = 0\npopup = 1\n", old);
    fclose(old);
    gr_config_load(&c2, path);
    CHECK(c2.lang == GR_LANG_ES && !c2.sound && c2.theme == GR_THEME_AUTO && c2.ach_view == GR_VIEW_LIST);
    CHECK(c2.animated_bg);

    /* valores fuera de rango no escriben basura */
    gr_config bad_cfg = { 99, -4, 7, 2, true, true, true };
    CHECK(gr_config_save(&bad_cfg, path) == 0);
    gr_config_load(&c2, path);
    CHECK(c2.lang == GR_LANG_AUTO && c2.theme == GR_THEME_AUTO && c2.games_view == GR_VIEW_LIST);

    c.lang = GR_LANG_ES; c.sound = true; c.popup = false;
    CHECK(gr_config_save(&c, path) == 0);
    gr_config_load(&c2, path);
    CHECK(c2.lang == GR_LANG_ES && c2.sound && !c2.popup);

    /* basura y valores invalidos no rompen nada: queda el valor por defecto */
    FILE *f = fopen(path, "w");
    fputs("lang = klingon\ntheme = neon\nach_view = mosaic\nsound = 7\n# popup = 0\nsin igual\npopup=0\n", f);
    fclose(f);
    gr_config_load(&c2, path);
    CHECK(c2.lang == GR_LANG_AUTO && c2.sound && !c2.popup);

    CHECK(gr_config_save(&c, "no/existe/x.ini") != 0);
    remove(path);
}

static void test_store(void) {
    static lg_game g;
    char err[128];
    const char *path = "test_store.tmp";
    const char *src = "[a]\nwhen=main+0 u8 == 1\n[b]\nwhen=main+0 u8 == 1\n[c]\nwhen=main+0 u8 == 1\n";
    remove(path);

    CHECK(parse(&g, src, err, sizeof(err)) == 0);
    CHECK(lg_store_load(&g, path) == 0); /* no existe: no es error */

    CHECK(lg_store_append(path, "a", 1762700000) == 0);
    CHECK(lg_store_append(path, "c", 0) == 0);
    CHECK(lg_store_append(path, "viejo_que_ya_no_existe", 5) == 0);
    CHECK(lg_store_append(path, "a", 1762700001) == 0); /* duplicado */

    CHECK(parse(&g, src, err, sizeof(err)) == 0);
    CHECK(lg_store_load(&g, path) == 2);
    CHECK(g.ach[0].unlocked && !g.ach[1].unlocked && g.ach[2].unlocked);
    CHECK(g.ach[0].unlocked_at == 1762700000 && g.ach[2].unlocked_at == 0); /* gana el primero */

    /* un logro cargado como desbloqueado no vuelve a dispararse */
    int out[4];
    memset(g_main, 0, sizeof(g_main));
    PUT(g_main, 0, uint8_t, 1);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 0, out, 4) == 0);
    CHECK(lg_tick(&g, &g_bases, fake_read, NULL, 600, out, 4) == 1 && out[0] == 1);

    CHECK(lg_store_append("no/existe/esta/carpeta/x.txt", "a", 0) != 0);
    remove(path);
}

int main(void) {
    test_parse();
    test_eval();
    test_tick();
    test_debug();
    test_scan();
    test_i18n_config();
    test_store();
    printf("%d/%d pruebas pasaron\n", g_total - g_fail, g_total);
    return g_fail ? 1 : 0;
}
