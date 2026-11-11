#include "search.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "dmntcht.h"

#define DIR_S "sdmc:/config/greeny/search"
#define CMD_PATH DIR_S "/cmd.txt"
#define RESULT_PATH DIR_S "/result.txt"
#define CAND_PATH DIR_S "/cand.bin"
#define CAND_TMP DIR_S "/cand.tmp"
#define CHAINS_PATH DIR_S "/chains.bin"
#define STATE_PATH DIR_S "/state.txt"

#define CHUNK 0x10000
#define MAX_CAND 2000000u  /* 16 MB de archivo; con mas que eso hay que afinar primero */
#define SHOWN 12           /* candidatas que se listan en result.txt */
#define PTR_MAX_OFF 0x800  /* cuanto puede haber entre un puntero y el dato */
#define PTR_DEPTH 7
#define PTR_LEVEL_MAX 192  /* direcciones que se siguen por nivel */
#define CHAINS_MAX 28

static u64 g_main, g_main_size, g_heap, g_end;
static int g_width = 4; /* bytes del tipo de la busqueda en curso */
static int g_step;
static unsigned long g_last_value; /* el ultimo numero que se busco o filtro */
static unsigned g_last_count;     /* candidatas, cadenas o cadenas buenas de la ultima orden */

typedef struct { u64 loc; u16 off; s16 parent; } node;   /* loc guarda un puntero a (nodo padre - off) */
typedef struct { u64 base_off; u8 n; u16 off[PTR_DEPTH]; } chain; /* main+base_off->off[0]->off[1]... */

/* Memoria de trabajo: se pide solo mientras se ejecuta una orden, para que el sysmodule
 * no ocupe mas memoria del sistema cuando nadie usa el buscador. */
typedef struct {
    u8 buf[CHUNK];
    u64 batch[1024];
    node level[PTR_DEPTH + 1][PTR_LEVEL_MAX];
    int level_n[PTR_DEPTH + 1];
    chain chains[CHAINS_MAX];
    int nchains;
} work;
static work *g_w;
#define g_buf (g_w->buf)
#define g_level (g_w->level)
#define g_level_n (g_w->level_n)
#define g_chains (g_w->chains)
#define g_nchains (g_w->nchains)

/* ---------- utilidades ---------- */

static void result_begin(FILE **f) { *f = fopen(RESULT_PATH, "w"); }

static void result_status(const char *text) {
    FILE *f = fopen(RESULT_PATH, "w");
    if (!f) return;
    fprintf(f, "%s\n", text);
    fclose(f);
}

static void describe(u64 addr, char *out, size_t cap) {
    if (addr >= g_main && addr < g_main + g_main_size) snprintf(out, cap, "main+0x%lX", addr - g_main);
    else if (addr >= g_heap) snprintf(out, cap, "heap+0x%lX", addr - g_heap);
    else snprintf(out, cap, "0x%lX", addr);
}

static bool matches(const u8 *p, u64 value) {
    if (g_width == 1) return p[0] == (u8)value;
    if (g_width == 2) { u16 v; memcpy(&v, p, 2); return v == (u16)value; }
    u32 v;
    memcpy(&v, p, 4);
    return v == (u32)value;
}

static void save_state(void) {
    FILE *f = fopen(STATE_PATH, "w");
    if (f) { fprintf(f, "%d %d %lu\n", g_width, g_step, g_last_value); fclose(f); }
}

static void load_state(void) {
    FILE *f = fopen(STATE_PATH, "r");
    if (f) {
        g_last_value = 0;
        if (fscanf(f, "%d %d %lu", &g_width, &g_step, &g_last_value) < 2 || (g_width != 1 && g_width != 2 && g_width != 4)) { g_width = 4; g_step = 0; }
        fclose(f);
    }
}

/* Recorre la memoria del juego que se puede leer y escribir (sus datos), trozo a trozo. */
typedef bool (*chunk_fn)(u64 addr, const u8 *data, size_t size, void *ctx);

static void walk_memory(chunk_fn fn, void *ctx) {
    u64 addr = 0;
    while (addr < g_end) {
        MemoryInfo info;
        if (R_FAILED(dmntchtQueryCheatProcessMemory(&info, addr)) || info.size == 0) break;
        u64 next = info.addr + info.size;
        if (next <= addr) break;
        bool data = (info.perm & Perm_Rw) == Perm_Rw && (info.type & 0xFF) != MemType_Unmapped && (info.type & 0xFF) != MemType_Io &&
                    (info.type & 0xFF) != MemType_Reserved;
        if (data) {
            for (u64 a = info.addr; a < next; a += CHUNK) {
                size_t n = next - a < CHUNK ? (size_t)(next - a) : CHUNK;
                if (R_FAILED(dmntchtReadCheatProcessMemory(a, g_buf, n))) continue;
                if (!fn(a, g_buf, n, ctx)) return;
            }
        }
        addr = next;
    }
}

/* ---------- busqueda nueva y filtro ---------- */

typedef struct { FILE *out; u64 value; u32 count; bool overflow; } new_ctx;

static bool new_chunk(u64 addr, const u8 *data, size_t size, void *vctx) {
    new_ctx *c = vctx;
    for (size_t i = 0; i + (size_t)g_width <= size; i += (size_t)g_width) {
        if (!matches(data + i, c->value)) continue;
        if (c->count >= MAX_CAND) { c->overflow = true; return false; }
        u64 a = addr + i;
        fwrite(&a, sizeof(a), 1, c->out);
        c->count++;
    }
    return true;
}

static void write_candidates(const char *headline, u64 value, u32 count) {
    FILE *r;
    result_begin(&r);
    if (!r) return;
    fprintf(r, "%s\n", headline);
    fprintf(r, "Paso %d  |  valor %lu  |  tipo u%d  |  %u candidata(s)\n", g_step, value, g_width * 8, count);
    FILE *c = fopen(CAND_PATH, "rb");
    if (c) {
        u64 a;
        char where[48];
        for (int i = 0; i < SHOWN && fread(&a, sizeof(a), 1, c) == 1; i++) {
            describe(a, where, sizeof(where));
            fprintf(r, "#%d  %s\n", i, where);
        }
        fclose(c);
    }
    if (count > SHOWN) fprintf(r, "Hay mas de %d: cambia el numero en el juego y filtra con el valor nuevo.\n", SHOWN);
    else if (count > 0) fprintf(r, "Ya son pocas: elige una y busca sus punteros.\n");
    else fprintf(r, "Ninguna. Empieza una busqueda nueva (quiza con otro tipo).\n");
    fclose(r);
}

static void do_new(int width, u64 value) {
    g_width = width;
    g_step = 1;
    result_status("Buscando en toda la memoria del juego... puede tardar un minuto o mas.");
    new_ctx c = { fopen(CAND_PATH, "wb"), value, 0, false };
    if (!c.out) { result_status("ERROR: no se pudo escribir en la SD."); return; }
    walk_memory(new_chunk, &c);
    fclose(c.out);
    g_last_value = value;
    g_last_count = c.count;
    save_state();
    write_candidates(c.overflow ? "Demasiadas coincidencias: solo se guardaron las primeras. Filtra para reducirlas."
                                : "Busqueda nueva terminada.", value, c.count);
}

static void do_next(u64 value) {
    load_state();
    FILE *in = fopen(CAND_PATH, "rb");
    if (!in) { result_status("ERROR: no hay una busqueda empezada. Haz primero una busqueda nueva."); return; }
    FILE *out = fopen(CAND_TMP, "wb");
    if (!out) { fclose(in); result_status("ERROR: no se pudo escribir en la SD."); return; }
    result_status("Filtrando las candidatas...");

    u64 *batch = g_w->batch;
    u32 kept = 0;
    size_t n;
    u64 win = 0;      /* inicio del trozo que hay en g_buf */
    size_t win_n = 0; /* 0 = ninguno */
    while ((n = fread(batch, sizeof(u64), 1024, in)) > 0) {
        for (size_t i = 0; i < n; i++) {
            u64 a = batch[i];
            /* las direcciones vienen en orden: se lee un trozo y se aprovecha para las vecinas */
            if (!(win_n && a >= win && a + (u64)g_width <= win + win_n)) {
                win = a;
                win_n = CHUNK;
                if (R_FAILED(dmntchtReadCheatProcessMemory(win, g_buf, win_n))) {
                    win_n = (size_t)g_width; /* quiza el trozo se sale de lo mapeado: leer solo el dato */
                    if (R_FAILED(dmntchtReadCheatProcessMemory(win, g_buf, win_n))) { win_n = 0; continue; }
                }
            }
            if (matches(g_buf + (a - win), value)) { fwrite(&a, sizeof(a), 1, out); kept++; }
        }
    }
    fclose(in);
    fclose(out);
    remove(CAND_PATH);
    rename(CAND_TMP, CAND_PATH);
    g_step++;
    g_last_value = value;
    g_last_count = kept;
    save_state();
    write_candidates("Filtro terminado.", value, kept);
}

/* ---------- cadena de punteros ---------- */

typedef struct { int level; u64 tmin, tmax; u64 sorted[PTR_LEVEL_MAX]; s16 index[PTR_LEVEL_MAX]; int n; } ptr_ctx;

static void add_chain(int level, int parent_idx, u64 loc, u16 off) {
    if (g_nchains >= CHAINS_MAX) return;
    chain *c = &g_chains[g_nchains];
    c->base_off = loc - g_main;
    c->n = 0;
    c->off[c->n++] = off;
    /* de ahi hacia el dato: cada nodo padre aporta su offset */
    int idx = parent_idx;
    for (int l = level - 1; l >= 1 && c->n < PTR_DEPTH; l--) {
        c->off[c->n++] = g_level[l][idx].off;
        idx = g_level[l][idx].parent;
    }
    g_nchains++;
}

static bool ptr_chunk(u64 addr, const u8 *data, size_t size, void *vctx) {
    ptr_ctx *p = vctx;
    for (size_t i = 0; i + 8 <= size; i += 8) {
        u64 v;
        memcpy(&v, data + i, 8);
        if (v + PTR_MAX_OFF < p->tmin || v > p->tmax) continue;
        /* el destino mas cercano por encima de v */
        int lo = 0, hi = p->n;
        while (lo < hi) { int mid = (lo + hi) / 2; if (p->sorted[mid] < v) lo = mid + 1; else hi = mid; }
        if (lo >= p->n || p->sorted[lo] - v > PTR_MAX_OFF) continue;
        u64 loc = addr + i;
        u16 off = (u16)(p->sorted[lo] - v);
        s16 parent = p->index[lo];
        if (loc >= g_main && loc < g_main + g_main_size) { add_chain(p->level, parent, loc, off); continue; }
        /* si no esta en main, se sigue subiendo desde aqui; se guardan las de offset mas chico */
        node *lvl = g_level[p->level];
        int *n = &g_level_n[p->level];
        if (*n < PTR_LEVEL_MAX) { lvl[*n] = (node){ loc, off, parent }; (*n)++; }
        else {
            int worst = 0;
            for (int k = 1; k < *n; k++) if (lvl[k].off > lvl[worst].off) worst = k;
            if (off < lvl[worst].off) lvl[worst] = (node){ loc, off, parent };
        }
    }
    return g_nchains < CHAINS_MAX;
}

static void write_chains(const char *headline) {
    FILE *r;
    result_begin(&r);
    if (!r) return;
    fprintf(r, "%s\n", headline);
    for (int i = 0; i < g_nchains; i++) {
        fprintf(r, "#%d  main+0x%lX", i, g_chains[i].base_off);
        for (int k = 0; k < g_chains[i].n; k++) fprintf(r, "->0x%X", g_chains[i].off[k]);
        fprintf(r, " u%d\n", g_width * 8);
    }
    if (g_nchains) fprintf(r, "Cierra el juego, abrelo otra vez y usa Comprobar con el valor actual: las que sigan dando el numero sirven.\n");
    fclose(r);
    FILE *b = fopen(CHAINS_PATH, "wb");
    if (b) { fwrite(&g_nchains, sizeof(g_nchains), 1, b); fwrite(g_w->chains, sizeof(chain), (size_t)g_nchains, b); fclose(b); }
}

static void do_ptr(int which) {
    load_state();
    g_nchains = 0;
    memset(g_level_n, 0, sizeof(g_w->level_n));

    /* which >= 0: solo esa candidata. which < 0: todas las que queden (hasta 8); una cadena sirve
     * llegue a la que llegue, y asi no hay que adivinar cual de ellas es la buena. */
    FILE *c = fopen(CAND_PATH, "rb");
    u64 target = 0;
    int idx = 0;
    while (c && g_level_n[0] < 8 && fread(&target, 8, 1, c) == 1) {
        if (which < 0 || idx == which) {
            if (target >= g_main && target < g_main + g_main_size) {
                /* esta en main: es fija y no necesita punteros */
                if (g_nchains < CHAINS_MAX) { g_chains[g_nchains].base_off = target - g_main; g_chains[g_nchains].n = 0; g_nchains++; }
            } else {
                g_level[0][g_level_n[0]] = (node){ target, 0, -1 };
                g_level_n[0]++;
            }
        }
        idx++;
    }
    if (c) fclose(c);
    if (g_level_n[0] == 0 && g_nchains == 0) { result_status("ERROR: no hay candidatas. Busca y filtra primero."); return; }

    for (int level = 1; level <= PTR_DEPTH && g_nchains < CHAINS_MAX; level++) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Buscando punteros, nivel %d de %d... (%d cadena(s) hasta ahora)", level, PTR_DEPTH, g_nchains);
        result_status(msg);
        static ptr_ctx p; /* 2 KB: se queda fijo */
        p.level = level;
        p.n = g_level_n[level - 1];
        if (p.n == 0) break;
        for (int i = 0; i < p.n; i++) { p.sorted[i] = g_level[level - 1][i].loc; p.index[i] = (s16)i; }
        for (int i = 1; i < p.n; i++) /* ordenar por direccion (son pocas) */
            for (int k = i; k > 0 && p.sorted[k] < p.sorted[k - 1]; k--) {
                u64 t = p.sorted[k]; p.sorted[k] = p.sorted[k - 1]; p.sorted[k - 1] = t;
                s16 ti = p.index[k]; p.index[k] = p.index[k - 1]; p.index[k - 1] = ti;
            }
        p.tmin = p.sorted[0];
        p.tmax = p.sorted[p.n - 1];
        walk_memory(ptr_chunk, &p);
        if (g_nchains >= 12) break; /* con unas cuantas basta para comprobar */
    }
    g_last_count = (unsigned)g_nchains;
    write_chains(g_nchains ? "Cadenas de punteros encontradas:" : "No se encontro ninguna cadena desde main. Prueba con otra candidata.");
}

static void do_check(u64 value) {
    load_state();
    FILE *b = fopen(CHAINS_PATH, "rb");
    if (!b || fread(&g_nchains, sizeof(g_nchains), 1, b) != 1 || g_nchains < 0 || g_nchains > CHAINS_MAX ||
        fread(g_chains, sizeof(chain), (size_t)g_nchains, b) != (size_t)g_nchains) {
        if (b) fclose(b);
        result_status("ERROR: no hay cadenas guardadas. Busca punteros primero.");
        return;
    }
    fclose(b);
    FILE *r;
    result_begin(&r);
    if (!r) return;
    fprintf(r, "Comprobando las cadenas con el valor %lu:\n", value);
    int good = 0;
    for (int i = 0; i < g_nchains; i++) {
        u64 addr = g_main + g_chains[i].base_off;
        int broke = -1;
        for (int k = 0; broke < 0 && k < g_chains[i].n; k++) {
            u64 ptr = 0;
            if (R_FAILED(dmntchtReadCheatProcessMemory(addr, &ptr, 8)) || ptr == 0) broke = k;
            addr = ptr + g_chains[i].off[k];
        }
        u8 raw[4] = { 0 };
        if (broke < 0 && R_FAILED(dmntchtReadCheatProcessMemory(addr, raw, (size_t)g_width))) broke = g_chains[i].n;
        u32 now = 0;
        memcpy(&now, raw, (size_t)g_width);
        bool ok = broke < 0 && matches(raw, value);
        if (ok) good++;
        fprintf(r, "%s  main+0x%lX", ok ? "SIRVE" : "no   ", g_chains[i].base_off);
        for (int k = 0; k < g_chains[i].n; k++) fprintf(r, "->0x%X", g_chains[i].off[k]);
        if (broke >= 0) fprintf(r, " u%d   (puntero vacio en el paso %d)\n", g_width * 8, broke + 1);
        else fprintf(r, " u%d   = %u\n", g_width * 8, now);
    }
    g_last_count = (unsigned)good;
    fprintf(r, "%d de %d dan el valor.%s\n", good, g_nchains,
            good ? " Las que dicen SIRVE van en el archivo de logros."
                 : " Si acabas de abrir el juego, entra primero a la partida (donde se ve el numero) y comprueba otra vez.");
    fclose(r);
}

/* ---------- ordenes ---------- */

void search_poll(uint64_t main_base, uint64_t main_size, uint64_t heap_base, uint64_t space_end) {
    FILE *f = fopen(CMD_PATH, "r");
    if (!f) return;
    char line[64] = "";
    if (!fgets(line, sizeof(line), f)) line[0] = 0;
    fclose(f);
    remove(CMD_PATH); /* la orden se ejecuta una sola vez */

    g_main = main_base;
    g_main_size = main_size;
    g_heap = heap_base;
    g_end = space_end;
    g_w = malloc(sizeof(*g_w));
    if (!g_w) { result_status("ERROR: sin memoria para buscar."); return; }

    char type[8];
    unsigned long value = 0;
    int which = 0;
    if (sscanf(line, "new %7s %lu", type, &value) == 2) {
        int w = !strcmp(type, "u8") ? 1 : !strcmp(type, "u16") ? 2 : 4;
        do_new(w, value);
    } else if (sscanf(line, "next %lu", &value) == 1) do_next(value);
    else if (sscanf(line, "ptr %d", &which) == 1 && which >= 0) do_ptr(which);
    else if (sscanf(line, "check %lu", &value) == 1) do_check(value);
    else result_status("ERROR: orden desconocida.");
    free(g_w);
    g_w = NULL;
}

/* ---------- manejo desde el juego, con botones ---------- */

#define PLAN_PATH DIR_S "/plan.txt"

bool search_has_plan(void) {
    struct stat st;
    return stat(PLAN_PATH, &st) == 0;
}

void search_button(search_action action, uint64_t main_base, uint64_t main_size, uint64_t heap_base, uint64_t space_end,
                   char *line1, size_t cap1, char *line2, size_t cap2) {
    line1[0] = line2[0] = 0;
    char type[8] = "u32";
    unsigned long start = 0;
    long step = 1;
    FILE *f = fopen(PLAN_PATH, "r");
    if (!f || fscanf(f, "%7s %lu %ld", type, &start, &step) != 3) {
        if (f) fclose(f);
        snprintf(line1, cap1, "No hay plan de busqueda");
        snprintf(line2, cap2, "Preparalo en Greeny antes de abrir el juego.");
        return;
    }
    fclose(f);

    g_main = main_base;
    g_main_size = main_size;
    g_heap = heap_base;
    g_end = space_end;
    g_w = malloc(sizeof(*g_w));
    if (!g_w) { snprintf(line1, cap1, "Sin memoria para buscar"); return; }

    g_step = 0;
    load_state();
    g_last_count = 0;
    if (action == SEARCH_NEXT) {
        if (g_step == 0) do_new(!strcmp(type, "u8") ? 1 : !strcmp(type, "u16") ? 2 : 4, start);
        else do_next((unsigned long)((long)g_last_value + step));
        snprintf(line1, cap1, "Paso %d: busque el %lu", g_step, g_last_value);
        if (g_last_count == 0) snprintf(line2, cap2, "Ninguna candidata. Prepara otro plan.");
        else if (g_last_count <= SHOWN) snprintf(line2, cap2, "%u candidata(s). Ya puedes buscar punteros.", g_last_count);
        else snprintf(line2, cap2, "%u candidatas. Cambia el numero y repite.", g_last_count);
    } else if (action == SEARCH_PTR) {
        do_ptr(-1);
        snprintf(line1, cap1, "Punteros: %u cadena(s)", g_last_count);
        snprintf(line2, cap2, g_last_count ? "Cierra el juego, abrelo y comprueba." : "No salio ninguna desde main.");
    } else {
        do_check(g_last_value);
        snprintf(line1, cap1, "Comprobado con el %lu", g_last_value);
        snprintf(line2, cap2, g_last_count ? "%u cadena(s) sirven. Mira result.txt." : "%u sirven. Entra a la partida y repite.", g_last_count);
    }
    free(g_w);
    g_w = NULL;
}
