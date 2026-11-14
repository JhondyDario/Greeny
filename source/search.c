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
#define PTR_LEVEL_MAX 480  /* direcciones que se siguen por nivel, repartidas entre las candidatas */
#define PTR_ROOTS 12       /* candidatas hacia las que se buscan punteros a la vez */
#define CHAINS_MAX 28

static u64 g_main, g_main_size, g_heap, g_end;
static int g_width = 4; /* bytes del tipo de la busqueda en curso */
static int g_step;
static unsigned long g_last_value; /* el ultimo numero que se busco o filtro */
static unsigned g_last_count;     /* candidatas, cadenas o cadenas buenas de la ultima orden */

typedef struct { u64 loc; u16 off; s16 parent; u8 root; } node; /* loc guarda un puntero a (nodo padre - off); root = de que candidata viene */
typedef struct { u64 base_off; u8 n; u16 off[PTR_DEPTH]; } chain; /* main+base_off->off[0]->off[1]... */

/* Memoria de trabajo: se pide solo mientras se ejecuta una orden, para que el sysmodule
 * no ocupe mas memoria del sistema cuando nadie usa el buscador. */
typedef struct {
    u8 buf[CHUNK];
    u8 buf2[CHUNK];   /* el mismo trozo, como estaba en la foto de la memoria */
    u64 batch[1024];
    node level[PTR_DEPTH + 1][PTR_LEVEL_MAX];
    int root_n[PTR_ROOTS];   /* cuantas direcciones lleva cada candidata en el nivel que se esta llenando */
    int nroots;
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

/* Solo los datos del juego (heap y variables de su codigo): asi la foto de "no se el numero" no
 * se pasa media hora copiando memoria de graficos y audio, que no guarda nada de la partida. */
static bool g_only_game_data;
static u64 g_walk_total; /* bytes recorridos en esta pasada */

static bool game_data(u32 type) {
    type &= 0xFF;
    return type == MemType_Heap || type == MemType_CodeMutable || type == MemType_ModuleCodeMutable || type == MemType_CodeWritable;
}

static void walk_memory(chunk_fn fn, void *ctx) {
    u64 addr = 0;
    while (addr < g_end) {
        MemoryInfo info;
        if (R_FAILED(dmntchtQueryCheatProcessMemory(&info, addr)) || info.size == 0) break;
        u64 next = info.addr + info.size;
        if (next <= addr) break;
        bool data = (info.perm & Perm_Rw) == Perm_Rw && (info.type & 0xFF) != MemType_Unmapped && (info.type & 0xFF) != MemType_Io &&
                    (info.type & 0xFF) != MemType_Reserved;
        if (data && g_only_game_data && !game_data(info.type)) data = false;
        if (data) {
            for (u64 a = info.addr; a < next; a += CHUNK) {
                g_walk_total += CHUNK;
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
        /* Si no esta en main, se sigue subiendo desde aqui. Cada candidata tiene su propio cupo y se
         * queda con las de offset mas chico: sin cupo, una candidata con muchisimos punteros encima
         * dejaba sin sitio a la buena y no salia ninguna cadena. */
        node *lvl = g_level[p->level];
        int *n = &g_level_n[p->level];
        u8 root = g_level[p->level - 1][parent].root;
        int quota = PTR_LEVEL_MAX / (g_w->nroots > 0 ? g_w->nroots : 1);
        if (g_w->root_n[root] < quota && *n < PTR_LEVEL_MAX) {
            lvl[*n] = (node){ loc, off, parent, root };
            (*n)++;
            g_w->root_n[root]++;
        } else {
            int worst = -1;
            for (int k = 0; k < *n; k++)
                if (lvl[k].root == root && (worst < 0 || lvl[k].off > lvl[worst].off)) worst = k;
            if (worst >= 0 && off < lvl[worst].off) lvl[worst] = (node){ loc, off, parent, root };
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
    g_w->nroots = 0;
    FILE *c = fopen(CAND_PATH, "rb");
    u64 target = 0;
    int idx = 0;
    while (c && g_level_n[0] < PTR_ROOTS && fread(&target, 8, 1, c) == 1) {
        if (which < 0 || idx == which) {
            if (target >= g_main && target < g_main + g_main_size) {
                /* esta en main: es fija y no necesita punteros */
                if (g_nchains < CHAINS_MAX) { g_chains[g_nchains].base_off = target - g_main; g_chains[g_nchains].n = 0; g_nchains++; }
            } else {
                g_level[0][g_level_n[0]] = (node){ target, 0, -1, (u8)g_level_n[0] };
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
        g_w->nroots = g_level_n[0];
        memset(g_w->root_n, 0, sizeof(g_w->root_n));
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

static bool g_check_any; /* modo "no se el numero": no hay valor que comparar, solo se muestra lo que lee cada cadena */

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
    if (g_check_any) fprintf(r, "Lo que lee cada cadena ahora:\n");
    else fprintf(r, "Comprobando las cadenas con el valor %lu:\n", value);
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
        bool ok = broke < 0 && (g_check_any || matches(raw, value));
        if (ok) good++;
        fprintf(r, "%s  main+0x%lX", ok ? (g_check_any ? "lee  " : "SIRVE") : "no   ", g_chains[i].base_off);
        for (int k = 0; k < g_chains[i].n; k++) fprintf(r, "->0x%X", g_chains[i].off[k]);
        if (broke >= 0) fprintf(r, " u%d   (puntero vacio en el paso %d)\n", g_width * 8, broke + 1);
        else fprintf(r, " u%d   = %u\n", g_width * 8, now);
    }
    g_last_count = (unsigned)good;
    if (g_check_any)
        fprintf(r, "%d de %d se pueden leer. Cambia el estado en el juego y comprueba otra vez: sirve la que cambie como esperas.\n", good, g_nchains);
    else
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
    else if (sscanf(line, "ptr %d", &which) == 1) do_ptr(which);
    else if (sscanf(line, "check %lu", &value) == 1) do_check(value);
    else result_status("ERROR: orden desconocida.");
    free(g_w);
    g_w = NULL;
}

/* ---------- cuando no se sabe el numero: cambio / no cambio ----------
 * Sirve para cosas que no son un contador a la vista (el traje, si vas montado, un nivel
 * terminado). Se guarda una foto de toda la memoria en la SD y despues se va filtrando:
 * "cambio" deja lo que es distinto de la ultima vez, "no cambio" deja lo que sigue igual.
 * La foto ocupa lo mismo que la memoria del juego (uno o dos GB) y tarda varios minutos. */

#define SNAP_PATH DIR_S "/snap.bin"
#define VALS_PATH DIR_S "/vals.bin"
#define VALS_TMP DIR_S "/vals.tmp"
#define MAX_VALS 8000000u /* 96 MB de archivo */

typedef struct { u64 addr; u32 val; } __attribute__((packed)) val_entry;
typedef struct { u64 addr; u32 size; } __attribute__((packed)) snap_head;

typedef struct { FILE *out; bool failed, too_big; u64 bytes; } snap_ctx;

#define SNAP_MAX (1536ull << 20) /* si pasa de 1,5 GB, la busqueda no termina nunca: mejor parar */

static bool snap_chunk(u64 addr, const u8 *data, size_t size, void *vctx) {
    snap_ctx *c = vctx;
    if (c->bytes + size > SNAP_MAX) { c->too_big = true; return false; }
    if ((c->bytes >> 27) != ((c->bytes + size) >> 27)) { /* cada 128 MB, para que se vea que avanza */
        char msg[96];
        snprintf(msg, sizeof(msg), "Guardando la foto de la memoria... %llu MB", (unsigned long long)((c->bytes + size) >> 20));
        result_status(msg);
    }
    snap_head h = { addr, (u32)size };
    if (fwrite(&h, sizeof(h), 1, c->out) != 1 || fwrite(data, 1, size, c->out) != size) { c->failed = true; return false; }
    c->bytes += size;
    return true;
}

static bool do_snap(void) {
    result_status("Guardando una foto de la memoria en la SD... tarda varios minutos.");
    remove(VALS_PATH);
    remove(CAND_PATH);
    snap_ctx c = { fopen(SNAP_PATH, "wb"), false, false, 0 };
    if (!c.out) { result_status("ERROR: no se pudo escribir en la SD."); return false; }
    g_only_game_data = true;
    walk_memory(snap_chunk, &c);
    g_only_game_data = false;
    if (c.too_big) {
        fclose(c.out);
        remove(SNAP_PATH);
        result_status("ERROR: este juego tiene demasiada memoria para el modo \"no se el numero\". Usa un numero (mira la guia).");
        return false;
    }
    if (fclose(c.out) != 0) c.failed = true;
    if (c.failed) {
        remove(SNAP_PATH);
        result_status("ERROR: no cabe la foto de la memoria en la SD. Hacen falta unos 3 GB libres.");
        return false;
    }
    g_width = 4;
    g_step = 1;
    g_last_value = 0;
    g_last_count = (unsigned)(c.bytes >> 20);
    save_state();
    char msg[160];
    snprintf(msg, sizeof(msg), "Foto guardada (%u MB). Ahora cambia eso en el juego y pulsa \"cambio\".", g_last_count);
    result_status(msg);
    return true;
}

static void write_vals(const char *headline, u32 count) {
    FILE *r;
    result_begin(&r);
    if (!r) return;
    fprintf(r, "%s\n", headline);
    fprintf(r, "Paso %d  |  %u candidata(s)\n", g_step, count);
    FILE *v = fopen(VALS_PATH, "rb"), *c = count <= MAX_CAND ? fopen(CAND_PATH, "wb") : NULL;
    val_entry e;
    char where[48];
    for (u32 i = 0; v && fread(&e, sizeof(e), 1, v) == 1; i++) {
        if (i < SHOWN) {
            describe(e.addr, where, sizeof(where));
            fprintf(r, "#%u  %s = %u (0x%X)\n", i, where, e.val, e.val);
        }
        if (c) fwrite(&e.addr, 8, 1, c); /* las mismas direcciones, para buscar punteros despues */
        else if (i >= SHOWN) break;
    }
    if (v) fclose(v);
    if (c) fclose(c);
    if (count > SHOWN) fprintf(r, "Sigue filtrando: cambia eso y pulsa \"cambio\", o dejalo igual y pulsa \"no cambio\".\n");
    else if (count > 0) fprintf(r, "Ya son pocas. Apunta que valor tiene cada una en cada estado y busca punteros.\n");
    else fprintf(r, "Ninguna. Guarda el plan otra vez y empieza de nuevo.\n");
    fclose(r);
}

/* primer filtro: la memoria de ahora contra la foto */
static void diff_snapshot(bool want_changed) {
    FILE *in = fopen(SNAP_PATH, "rb"), *out = fopen(VALS_TMP, "wb");
    if (!in || !out) { if (in) fclose(in); if (out) fclose(out); result_status("ERROR: no hay foto de la memoria. Empieza de nuevo."); return; }
    result_status("Comparando la memoria con la foto... tarda varios minutos.");
    u32 kept = 0;
    bool overflow = false;
    snap_head h;
    while (!overflow && fread(&h, sizeof(h), 1, in) == 1 && h.size <= CHUNK) {
        if (fread(g_w->buf2, 1, h.size, in) != h.size) break;
        if (R_FAILED(dmntchtReadCheatProcessMemory(h.addr, g_buf, h.size))) continue; /* ese trozo ya no existe */
        for (u32 i = 0; i + 4 <= h.size; i += 4) {
            u32 a, b;
            memcpy(&a, g_w->buf2 + i, 4);
            memcpy(&b, g_buf + i, 4);
            if ((a != b) != want_changed) continue;
            if (kept >= MAX_VALS) { overflow = true; break; }
            val_entry e = { h.addr + i, b };
            fwrite(&e, sizeof(e), 1, out);
            kept++;
        }
    }
    fclose(in);
    fclose(out);
    if (overflow) {
        /* demasiadas cosas distintas (o iguales): no caben. Se toma una foto nueva y se repite. */
        remove(VALS_TMP);
        if (do_snap())
            result_status(want_changed ? "Cambiaron demasiadas cosas a la vez. Hice una foto nueva: cambia SOLO eso y pulsa \"cambio\"."
                                       : "Casi todo sigue igual: asi no se puede empezar. Hice una foto nueva: cambia eso y pulsa \"cambio\".");
        g_last_count = 0;
        return;
    }
    remove(VALS_PATH);
    rename(VALS_TMP, VALS_PATH);
    remove(SNAP_PATH); /* ya no hace falta, y ocupa mucho */
    g_step++;
    g_last_count = kept;
    save_state();
    write_vals("Primer filtro terminado.", kept);
}

/* filtros siguientes: cada candidata contra el valor que tenia la ultima vez */
static void diff_values(bool want_changed) {
    FILE *in = fopen(VALS_PATH, "rb"), *out = fopen(VALS_TMP, "wb");
    if (!in || !out) { if (in) fclose(in); if (out) fclose(out); result_status("ERROR: no se pudo leer la lista de candidatas."); return; }
    result_status("Filtrando las candidatas...");
    u32 kept = 0;
    u64 win = 0;
    size_t win_n = 0;
    val_entry e;
    while (fread(&e, sizeof(e), 1, in) == 1) {
        if (!(win_n && e.addr >= win && e.addr + 4 <= win + win_n)) {
            win = e.addr;
            win_n = CHUNK;
            if (R_FAILED(dmntchtReadCheatProcessMemory(win, g_buf, win_n))) {
                win_n = 4;
                if (R_FAILED(dmntchtReadCheatProcessMemory(win, g_buf, win_n))) { win_n = 0; continue; }
            }
        }
        u32 now;
        memcpy(&now, g_buf + (e.addr - win), 4);
        if ((now != e.val) != want_changed) continue;
        e.val = now;
        fwrite(&e, sizeof(e), 1, out);
        kept++;
    }
    fclose(in);
    fclose(out);
    remove(VALS_PATH);
    rename(VALS_TMP, VALS_PATH);
    g_step++;
    g_last_count = kept;
    save_state();
    write_vals(want_changed ? "Filtro \"cambio\" terminado." : "Filtro \"no cambio\" terminado.", kept);
}

static void do_diff(bool want_changed) {
    struct stat st;
    if (stat(VALS_PATH, &st) == 0) diff_values(want_changed);
    else diff_snapshot(want_changed);
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
    bool unknown = step == 0; /* "no se el numero": cambio / no cambio */
    g_check_any = unknown;
    if (unknown && (action == SEARCH_NEXT || action == SEARCH_SAME)) {
        if (g_step == 0) {
            bool ok = do_snap();
            snprintf(line1, cap1, ok ? "Foto de la memoria guardada" : "No se pudo guardar la foto");
            snprintf(line2, cap2, ok ? "Cambia eso en el juego y pulsa ARRIBA." : "Hacen falta unos 3 GB libres en la SD.");
        } else {
            do_diff(action == SEARCH_NEXT);
            snprintf(line1, cap1, "Paso %d (%s)", g_step, action == SEARCH_NEXT ? "cambio" : "no cambio");
            if (g_last_count == 0) snprintf(line2, cap2, "Sin candidatas o foto nueva. Mira Greeny.");
            else if (g_last_count <= SHOWN) snprintf(line2, cap2, "%u candidata(s). Ya puedes buscar punteros.", g_last_count);
            else snprintf(line2, cap2, "%u candidatas. Sigue filtrando.", g_last_count);
        }
    } else if (action == SEARCH_SAME) {
        snprintf(line1, cap1, "IZQUIERDA es para \"no cambio\"");
        snprintf(line2, cap2, "Solo sirve con el plan \"no se el numero\".");
    } else if (action == SEARCH_NEXT) {
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
        if (unknown) snprintf(line1, cap1, "Cadenas leidas");
        else snprintf(line1, cap1, "Comprobado con el %lu", g_last_value);
        if (unknown) snprintf(line2, cap2, "%u se pueden leer. Mira result.txt.", g_last_count);
        else snprintf(line2, cap2, g_last_count ? "%u cadena(s) sirven. Mira result.txt." : "%u sirven. Entra a la partida y repite.", g_last_count);
    }
    free(g_w);
    g_w = NULL;
}

/* ---------- buscar texto ---------- */

#define FIND_PATH "sdmc:/config/greeny/find.txt"
#define FIND_MAX_HITS 80
#define FIND_BEFORE 0x400
#define FIND_AFTER 0xC00

typedef struct { const char (*texts)[32]; int ntexts; u64 hits[FIND_MAX_HITS]; u8 which[FIND_MAX_HITS]; int nhits; } find_ctx;

static bool find_chunk(u64 addr, const u8 *data, size_t size, void *vctx) {
    find_ctx *c = vctx;
    for (int t = 0; t < c->ntexts; t++) {
        size_t len = strlen(c->texts[t]);
        if (len == 0 || len > size) continue;
        for (size_t i = 0; i + len <= size; i++) { /* lo que caiga justo entre dos trozos se pierde: no importa */
            if (data[i] != (u8)c->texts[t][0] || memcmp(data + i, c->texts[t], len)) continue;
            if (c->nhits >= FIND_MAX_HITS) return false;
            c->which[c->nhits] = (u8)t;
            c->hits[c->nhits++] = addr + i;
        }
    }
    return true;
}

int search_find_text(const char (*texts)[32], int ntexts, uint64_t main_base, uint64_t main_size, uint64_t heap_base, uint64_t space_end) {
    g_main = main_base; g_main_size = main_size; g_heap = heap_base; g_end = space_end;
    g_w = malloc(sizeof(*g_w));
    find_ctx *c = calloc(1, sizeof(*c));
    FILE *f = fopen(FIND_PATH, "w");
    if (!g_w || !c || !f) { free(g_w); g_w = NULL; free(c); if (f) fclose(f); return -1; }
    c->texts = texts; c->ntexts = ntexts;
    g_only_game_data = true;
    g_walk_total = 0;
    walk_memory(find_chunk, c);
    g_only_game_data = false;
    fprintf(f, "main=0x%lX heap=0x%lX recorrido=%lu MB encontrados=%d\n", (unsigned long)main_base, (unsigned long)heap_base,
            (unsigned long)(g_walk_total >> 20), c->nhits);
    for (int i = 0; i < c->nhits; i++) {
        u64 start = (c->hits[i] - FIND_BEFORE) & ~0xFull;
        fprintf(f, "\"%s\" en 0x%lX; desde 0x%lX:\n", texts[c->which[i]], (unsigned long)c->hits[i], (unsigned long)start);
        for (u64 a = start; a < start + FIND_BEFORE + FIND_AFTER; a += 0x100) { /* por partes: si un trozo no se puede leer, el resto si */
            if (R_FAILED(dmntchtReadCheatProcessMemory(a, g_buf, 0x100))) { fprintf(f, "??\n"); continue; }
            for (int k = 0; k < 0x100; k++) fprintf(f, "%02X", g_buf[k]);
            fputc('\n', f);
        }
    }
    fclose(f);
    int n = c->nhits;
    free(c);
    free(g_w);
    g_w = NULL;
    return n;
}
