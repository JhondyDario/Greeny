#include "engine.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- utilidades de texto ---------- */

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = 0;
    return s;
}

static void copy_str(char *dst, size_t cap, const char *src) {
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
        /* no cortar un caracter UTF-8 por la mitad */
        while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;
    }
    memcpy(dst, src, n);
    dst[n] = 0;
}

static bool parse_u64(const char *s, uint64_t *out) {
    if (!*s || *s == '-' || *s == '+') return false;
    char *end;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 0);
    if (errno || *end) return false;
    *out = v;
    return true;
}

static bool parse_s64(const char *s, int64_t *out) {
    if (!*s) return false;
    char *end;
    errno = 0;
    long long v = strtoll(s, &end, 0);
    if (errno || *end) return false;
    *out = v;
    return true;
}

static bool parse_f64(const char *s, double *out) {
    if (!*s) return false;
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (errno || *end) return false;
    *out = v;
    return true;
}

static const struct { const char *name; uint8_t type; } k_types[] = {
    {"u8", LG_U8}, {"u16", LG_U16}, {"u32", LG_U32}, {"u64", LG_U64},
    {"s8", LG_S8}, {"s16", LG_S16}, {"s32", LG_S32}, {"s64", LG_S64},
    {"f32", LG_F32}, {"f64", LG_F64},
};

static const struct { const char *name; uint8_t op; } k_ops[] = {
    {"==", LG_EQ}, {"!=", LG_NE}, {">", LG_GT}, {">=", LG_GE},
    {"<", LG_LT}, {"<=", LG_LE}, {"&", LG_BITS_SET}, {"!&", LG_BITS_CLEAR},
};

static bool type_is_float(uint8_t t)  { return t == LG_F32 || t == LG_F64; }
static bool type_is_signed(uint8_t t) { return t >= LG_S8 && t <= LG_S64; }

static size_t type_size(uint8_t t) {
    switch (t) {
        case LG_U8:  case LG_S8:  return 1;
        case LG_U16: case LG_S16: return 2;
        case LG_U32: case LG_S32: case LG_F32: return 4;
        default: return 8;
    }
}

/* Rango valido del valor de comparacion segun el tipo. */
static bool value_fits(uint8_t t, uint64_t u, int64_t s) {
    switch (t) {
        case LG_U8:  return u <= UINT8_MAX;
        case LG_U16: return u <= UINT16_MAX;
        case LG_U32: return u <= UINT32_MAX;
        case LG_S8:  return s >= INT8_MIN && s <= INT8_MAX;
        case LG_S16: return s >= INT16_MIN && s <= INT16_MAX;
        case LG_S32: return s >= INT32_MIN && s <= INT32_MAX;
        default: return true;
    }
}

/* ---------- parser de condiciones ---------- */

/* "main+0x1A2B->0x10->0x48" */
static const char *parse_addr(char *s, lg_cond *c) {
    char *p = s;
    if (!strncmp(p, "main", 4))      c->region = LG_REGION_MAIN;
    else if (!strncmp(p, "heap", 4)) c->region = LG_REGION_HEAP;
    else return "la direccion debe empezar con main+ o heap+";
    p += 4;
    if (*p != '+') return "falta el + despues de main/heap";
    p++;

    c->nderef = 0;
    char *arrow = strstr(p, "->");
    if (arrow) *arrow = 0;
    if (!parse_u64(p, &c->offset)) return "offset invalido";

    while (arrow) {
        p = arrow + 2;
        arrow = strstr(p, "->");
        if (arrow) *arrow = 0;
        if (c->nderef >= LG_MAX_DEREF) return "demasiados niveles de puntero (max 8)";
        if (!parse_u64(p, &c->deref[c->nderef])) return "offset de puntero invalido";
        c->nderef++;
    }
    return NULL;
}

/* "<direccion> <tipo> <operador> <valor>" */
static const char *parse_cond(char *s, lg_cond *c) {
    char *tok[4];
    int n = 0;
    char *p = s;
    while (n < 4) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        tok[n++] = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = 0;
    }
    while (*p && isspace((unsigned char)*p)) p++;
    if (n != 4 || *p) return "formato: <direccion> <tipo> <operador> <valor>";

    memset(c, 0, sizeof(*c));
    const char *e = parse_addr(tok[0], c);
    if (e) return e;

    size_t i;
    for (i = 0; i < sizeof(k_types) / sizeof(k_types[0]); i++)
        if (!strcmp(tok[1], k_types[i].name)) { c->type = k_types[i].type; break; }
    if (i == sizeof(k_types) / sizeof(k_types[0])) return "tipo desconocido (u8..u64, s8..s64, f32, f64)";

    for (i = 0; i < sizeof(k_ops) / sizeof(k_ops[0]); i++)
        if (!strcmp(tok[2], k_ops[i].name)) { c->op = k_ops[i].op; break; }
    if (i == sizeof(k_ops) / sizeof(k_ops[0])) return "operador desconocido (== != > >= < <= & !&)";

    bool mask = c->op == LG_BITS_SET || c->op == LG_BITS_CLEAR;
    if (type_is_float(c->type)) {
        if (mask) return "& y !& no aplican a f32/f64";
        if (!parse_f64(tok[3], &c->f)) return "valor decimal invalido";
    } else if (type_is_signed(c->type) && !mask) {
        if (!parse_s64(tok[3], &c->s)) return "valor entero invalido";
        if (!value_fits(c->type, 0, c->s)) return "el valor no cabe en ese tipo";
    } else {
        if (!parse_u64(tok[3], &c->u)) return "valor entero sin signo invalido";
        /* una mascara se compara contra los bits crudos, asi que usa el rango sin signo */
        uint8_t ut = type_is_signed(c->type) ? (uint8_t)(c->type - LG_S8 + LG_U8) : c->type;
        if (!value_fits(ut, c->u, 0)) return "el valor no cabe en ese tipo";
    }
    return NULL;
}

/* "<direccion con cadena> <tipo entero> <largo> <min> <max>" */
static const char *parse_scan(char *s, lg_scan *sc) {
    char *tok[5];
    int n = 0;
    char *p = s;
    while (n < 5) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        tok[n++] = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = 0;
    }
    while (*p && isspace((unsigned char)*p)) p++;
    if (n != 5 || *p) return "formato: <direccion->cadena> <tipo> <largo> <min> <max>";

    memset(sc, 0, sizeof(*sc));
    const char *e = parse_addr(tok[0], &sc->addr);
    if (e) return e;
    if (sc->addr.nderef == 0) return "scan necesita al menos un nivel de puntero (->)";
    if (sc->addr.offset & 7) return "el offset inicial de scan debe ser multiplo de 8";

    size_t i;
    for (i = 0; i < sizeof(k_types) / sizeof(k_types[0]); i++)
        if (!strcmp(tok[1], k_types[i].name)) { sc->addr.type = k_types[i].type; break; }
    if (i == sizeof(k_types) / sizeof(k_types[0]) || type_is_float(sc->addr.type) || sc->addr.type == LG_U64)
        return "tipo de scan invalido (u8 u16 u32, s8 s16 s32 s64)";

    if (!parse_u64(tok[2], &sc->len) || sc->len == 0 || sc->len > LG_SCAN_MAX_LEN || (sc->len & 7))
        return "largo de scan invalido (multiplo de 8, max 0x100000)";
    if (!parse_s64(tok[3], &sc->min) || !parse_s64(tok[4], &sc->max) || sc->min > sc->max)
        return "rango min/max de scan invalido";
    return NULL;
}

/* ---------- parser del archivo ---------- */

static bool valid_id(const char *s) {
    if (!*s || strlen(s) >= LG_ID_LEN) return false;
    for (; *s; s++)
        if (!isalnum((unsigned char)*s) && *s != '_' && *s != '-') return false;
    return true;
}

lg_ach *lg_find(lg_game *g, const char *id) {
    for (int i = 0; i < g->nach; i++)
        if (!strcmp(g->ach[i].id, id)) return &g->ach[i];
    return NULL;
}

lg_tier lg_ach_tier(const lg_ach *a) {
    if (a->tier != LG_TIER_AUTO) return (lg_tier)a->tier;
    return a->points <= 10 ? LG_TIER_BRONZE : a->points <= 25 ? LG_TIER_SILVER : LG_TIER_GOLD;
}

const char *lg_ach_name(const lg_ach *a, bool english) {
    return english && a->name_en[0] ? a->name_en : a->name;
}

const char *lg_ach_desc(const lg_ach *a, bool english) {
    return english && a->desc_en[0] ? a->desc_en : a->desc;
}

#define FAIL(msg) do { snprintf(err, errlen, "linea %d: %s", lineno, (msg)); return -1; } while (0)

int lg_parse(lg_game *g, char *text, char *err, size_t errlen) {
    memset(g, 0, sizeof(*g));
    lg_ach *cur = NULL;
    int lineno = 0;
    char *line = text;

    /* BOM de UTF-8 */
    if ((unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
        line += 3;

    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) *next++ = 0;
        lineno++;

        char *s = trim(line);
        line = next;
        if (!*s || *s == '#') continue;

        if (*s == '[') {
            char *close = strchr(s, ']');
            if (!close || close[1]) FAIL("seccion mal cerrada, se esperaba [id]");
            *close = 0;
            char *id = trim(s + 1);
            if (!valid_id(id)) FAIL("id invalido (letras, numeros, _ y -, max 31)");
            if (lg_find(g, id)) FAIL("id repetido");
            if (cur && cur->ncond == 0) FAIL("el logro anterior no tiene ninguna condicion when=");
            if (g->nach >= LG_MAX_ACH) FAIL("demasiados logros (max 64)");
            cur = &g->ach[g->nach++];
            copy_str(cur->id, sizeof(cur->id), id);
            copy_str(cur->name, sizeof(cur->name), id);
            cur->hold_ms = LG_DEFAULT_HOLD_MS;
            cur->points = LG_DEFAULT_POINTS;
            cur->prog_cond = -2; /* automatico, se decide al final */
            continue;
        }

        char *eq = strchr(s, '=');
        if (!eq) FAIL("se esperaba clave=valor");
        *eq = 0;
        char *key = trim(s);
        char *val = trim(eq + 1);

        if (!cur) {
            if (!strcmp(key, "game")) copy_str(g->game, sizeof(g->game), val);
            else if (!strcmp(key, "version")) copy_str(g->version, sizeof(g->version), val);
            else if (!strcmp(key, "build")) {
                if (strlen(val) > 32) FAIL("build muy largo (max 32 digitos hex)");
                for (char *p = val; *p; p++) {
                    if (!isxdigit((unsigned char)*p)) FAIL("build debe ser hexadecimal");
                    *p = (char)toupper((unsigned char)*p);
                }
                copy_str(g->build, sizeof(g->build), val);
            } else if (!strcmp(key, "scan")) {
                if (g->nscan >= LG_MAX_SCAN) FAIL("demasiados scan (max 4)");
                const char *e = parse_scan(val, &g->scan[g->nscan]);
                if (e) FAIL(e);
                g->nscan++;
            } else if (!strcmp(key, "watch")) { /* "<direccion> <bytes>" */
                if (g->nwatch >= LG_MAX_WATCH) FAIL("demasiados watch (max 16)");
                char *sp = strrchr(val, ' ');
                uint64_t size = 0;
                if (!sp || !parse_u64(trim(sp + 1), &size) || size == 0 || size > LG_WATCH_MAX) FAIL("watch: falta el tamaño (de 1 a 0x800 bytes)");
                *sp = 0;
                lg_watch *w = &g->watch[g->nwatch];
                memset(w, 0, sizeof(*w));
                const char *e = parse_addr(trim(val), &w->addr);
                if (e) FAIL(e);
                w->size = (uint32_t)size;
                g->nwatch++;
            } else if (!strcmp(key, "debug")) {
                if (strcmp(val, "0") && strcmp(val, "1")) FAIL("debug debe ser 0 o 1");
                g->debug = val[0] == '1';
            } else FAIL("clave desconocida antes del primer logro (game, version, build, debug, scan, watch)");
            continue;
        }

        if (!strcmp(key, "name")) copy_str(cur->name, sizeof(cur->name), val);
        else if (!strcmp(key, "desc")) copy_str(cur->desc, sizeof(cur->desc), val);
        else if (!strcmp(key, "name.en")) copy_str(cur->name_en, sizeof(cur->name_en), val);
        else if (!strcmp(key, "desc.en")) copy_str(cur->desc_en, sizeof(cur->desc_en), val);
        else if (!strcmp(key, "hold")) {
            uint64_t h;
            if (!parse_u64(val, &h) || h > 600000) FAIL("hold invalido (milisegundos, 0..600000)");
            cur->hold_ms = (uint32_t)h;
        } else if (!strcmp(key, "points")) {
            uint64_t pts;
            if (!parse_u64(val, &pts) || pts > 1000) FAIL("points invalido (0..1000)");
            cur->points = (uint16_t)pts;
        } else if (!strcmp(key, "progress")) {
            uint64_t p;
            if (!parse_u64(val, &p) || p > LG_MAX_COND) FAIL("progress invalido (0 = sin progreso, o el numero de la condicion when)");
            cur->prog_cond = (int8_t)((int)p - 1);
        } else if (!strcmp(key, "tier")) {
            if (!strcmp(val, "bronce") || !strcmp(val, "bronze")) cur->tier = LG_TIER_BRONZE;
            else if (!strcmp(val, "plata") || !strcmp(val, "silver")) cur->tier = LG_TIER_SILVER;
            else if (!strcmp(val, "oro") || !strcmp(val, "gold")) cur->tier = LG_TIER_GOLD;
            else FAIL("tier invalido (bronce, plata u oro)");
        } else if (!strcmp(key, "when")) {
            if (cur->ncond >= LG_MAX_COND) FAIL("demasiadas condiciones en un logro (max 6)");
            const char *e = parse_cond(val, &cur->cond[cur->ncond]);
            if (e) FAIL(e);
            cur->ncond++;
        } else FAIL("clave desconocida (name, desc, name.en, desc.en, points, tier, progress, hold, when)");
    }

    lineno++;
    if (cur && cur->ncond == 0) FAIL("el ultimo logro no tiene ninguna condicion when=");
    if (g->nach == 0 && g->nscan == 0 && g->nwatch == 0) FAIL("el archivo no define ningun logro");

    /* la meta de cada logro, para mostrar su progreso */
    for (int i = 0; i < g->nach; i++) {
        lg_ach *a = &g->ach[i];
        if (a->prog_cond == -2) {
            a->prog_cond = -1;
            for (int k = 0; k < a->ncond && a->prog_cond < 0; k++) {
                const lg_cond *q = &a->cond[k];
                if (type_is_float(q->type)) continue;
                uint64_t n = type_is_signed(q->type) ? (q->s < 0 ? 0 : (uint64_t)q->s) : q->u;
                if ((q->op == LG_GE && n >= 2) || (q->op == LG_GT && n >= 1)) a->prog_cond = (int8_t)k;
            }
        }
        if (a->prog_cond >= a->ncond) FAIL("progress apunta a una condicion que no existe");
        if (a->prog_cond >= 0) {
            const lg_cond *q = &a->cond[a->prog_cond];
            uint64_t n = type_is_signed(q->type) ? (q->s < 0 ? 0 : (uint64_t)q->s) : q->u;
            if (type_is_float(q->type) || (q->op != LG_GE && q->op != LG_GT)) FAIL("progress tiene que apuntar a una condicion >= o >");
            a->prog_goal = q->op == LG_GT ? n + 1 : n;
        }
    }
    return 0;
}

/* ---------- evaluacion ---------- */

static void dbg_record(lg_cond *c, bool ok, uint64_t raw) {
    if (!c->dbg_seen || c->dbg_ok != ok || c->dbg_raw != raw) c->dbg_dirty = true;
    c->dbg_seen = true;
    c->dbg_ok = ok;
    c->dbg_raw = raw;
}

bool lg_resolve(const lg_cond *c, const lg_bases *b, lg_read_fn rd, void *ctx, uint64_t *out) {
    uint64_t base = c->region == LG_REGION_MAIN ? b->main_base : b->heap_base;
    if (!base) return false;
    uint64_t addr = base + c->offset;

    for (int i = 0; i < c->nderef; i++) {
        uint64_t ptr = 0;
        if (!rd(ctx, addr, &ptr, sizeof(ptr)) || ptr == 0) return false;
        addr = ptr + c->deref[i];
    }
    *out = addr;
    return true;
}

/* Resuelve la direccion final y lee el dato crudo. */
static bool read_raw(const lg_cond *c, const lg_bases *b, lg_read_fn rd, void *ctx, uint8_t raw[8]) {
    uint64_t addr;
    return lg_resolve(c, b, rd, ctx, &addr) && rd(ctx, addr, raw, type_size(c->type));
}

bool lg_eval_cond(lg_cond *c, const lg_bases *b, lg_read_fn rd, void *ctx) {
    uint8_t raw[8] = {0};
    size_t sz = type_size(c->type);
    bool ok = read_raw(c, b, rd, ctx, raw);
    uint64_t rawval = 0;
    if (ok) memcpy(&rawval, raw, sz);
    dbg_record(c, ok, rawval);
    if (!ok) return false;

    if (type_is_float(c->type)) {
        double v;
        if (c->type == LG_F32) { float t; memcpy(&t, raw, 4); v = t; }
        else memcpy(&v, raw, 8);
        if (v != v) return false; /* NaN */
        switch (c->op) {
            case LG_EQ: return v == c->f;
            case LG_NE: return v != c->f;
            case LG_GT: return v > c->f;
            case LG_GE: return v >= c->f;
            case LG_LT: return v < c->f;
            case LG_LE: return v <= c->f;
            default: return false;
        }
    }

    uint64_t u = 0;
    memcpy(&u, raw, sz); /* little-endian, igual que la Switch */

    if (c->op == LG_BITS_SET)   return (u & c->u) == c->u;
    if (c->op == LG_BITS_CLEAR) return (u & c->u) == 0;

    if (type_is_signed(c->type)) {
        int64_t v;
        switch (c->type) {
            case LG_S8:  v = (int8_t)u;  break;
            case LG_S16: v = (int16_t)u; break;
            case LG_S32: v = (int32_t)u; break;
            default:     v = (int64_t)u; break;
        }
        switch (c->op) {
            case LG_EQ: return v == c->s;
            case LG_NE: return v != c->s;
            case LG_GT: return v > c->s;
            case LG_GE: return v >= c->s;
            case LG_LT: return v < c->s;
            case LG_LE: return v <= c->s;
            default: return false;
        }
    }

    switch (c->op) {
        case LG_EQ: return u == c->u;
        case LG_NE: return u != c->u;
        case LG_GT: return u > c->u;
        case LG_GE: return u >= c->u;
        case LG_LT: return u < c->u;
        case LG_LE: return u <= c->u;
        default: return false;
    }
}

static bool same_address(const lg_cond *x, const lg_cond *y) {
    if (x->region != y->region || x->offset != y->offset || x->nderef != y->nderef || x->type != y->type) return false;
    for (int i = 0; i < x->nderef; i++)
        if (x->deref[i] != y->deref[i]) return false;
    return true;
}

/* Anota el progreso del logro: el valor de su condicion meta, si se mantiene 10 segundos
 * y respeta los limites que el mismo logro pone a esa direccion. */
static void track_progress(lg_game *g, lg_ach *a, uint64_t now_ms) {
    const lg_cond *pc = &a->cond[a->prog_cond];
    bool ok = pc->dbg_ok;
    uint64_t v = pc->dbg_raw;
    if (ok && type_is_signed(pc->type)) {
        int64_t s;
        switch (pc->type) {
            case LG_S8:  s = (int8_t)v;  break;
            case LG_S16: s = (int16_t)v; break;
            case LG_S32: s = (int32_t)v; break;
            default:     s = (int64_t)v; break;
        }
        v = s < 0 ? 0 : (uint64_t)s;
    }
    for (int k = 0; ok && k < a->ncond; k++) {
        const lg_cond *q = &a->cond[k];
        if (k == a->prog_cond || !same_address(q, pc) || type_is_float(q->type)) continue;
        uint64_t lim = type_is_signed(q->type) ? (q->s < 0 ? 0 : (uint64_t)q->s) : q->u;
        if (q->op == LG_LE && v > lim) ok = false;
        if (q->op == LG_LT && v >= lim) ok = false;
    }
    if (!ok) { a->prog_cand_ok = false; return; }
    if (!a->prog_cand_ok || a->prog_cand != v) {
        a->prog_cand_ok = true;
        a->prog_cand = v;
        a->prog_cand_since = now_ms;
        return;
    }
    if (now_ms - a->prog_cand_since < 10000) return;
    if (!a->prog_known || a->prog_value != v) {
        a->prog_known = true;
        a->prog_value = v;
        g->prog_dirty = true;
    }
}

int lg_tick(lg_game *g, const lg_bases *b, lg_read_fn rd, void *ctx,
            uint64_t now_ms, int *out, int max_out) {
    int n = 0;
    for (int i = 0; i < g->nach; i++) {
        lg_ach *a = &g->ach[i];
        if (a->unlocked) continue;

        bool all = a->ncond > 0;
        int evaluated = 0;
        for (int k = 0; k < a->ncond && (all || g->debug); k++, evaluated++)
            if (!lg_eval_cond(&a->cond[k], b, rd, ctx)) all = false;
        if (a->prog_cond >= 0) {
            if (a->prog_cond >= evaluated) lg_eval_cond(&a->cond[a->prog_cond], b, rd, ctx);
            track_progress(g, a, now_ms);
        }

        if (!all) { a->holding = false; continue; }
        if (!a->holding) { a->holding = true; a->true_since = now_ms; }
        if (now_ms - a->true_since < a->hold_ms) continue;
        /* si no cabe en `out`, se queda pendiente para el proximo tick */
        if (n >= max_out) continue;

        a->unlocked = true;
        a->holding = false;
        out[n++] = i;
    }
    return n;
}

/* ---------- busqueda de punteros ---------- */

/* Un puntero de usuario creible en la Switch: alineado y dentro de 39 bits. */
static bool plausible_ptr(uint64_t p) {
    return p >= 0x8000000ull && p < (1ull << 39) && (p & 3) == 0;
}

int lg_scan_run(const lg_scan *sc, const lg_bases *b, lg_read_fn rd, void *ctx,
                lg_scan_hit *hits, int max_hits) {
    const lg_cond *c = &sc->addr;
    uint64_t base = c->region == LG_REGION_MAIN ? b->main_base : b->heap_base;
    if (!base) return 0;

    uint64_t chunk[0x1000 / 8]; /* en la pila: memoria que ya sabemos que acepta IPC */
    size_t sz = type_size(c->type);
    int total = 0;

    for (uint64_t done = 0; done < sc->len; done += sizeof(chunk)) {
        uint64_t n = sc->len - done;
        if (n > sizeof(chunk)) n = sizeof(chunk);
        if (!rd(ctx, base + c->offset + done, chunk, (size_t)n)) continue;

        for (uint64_t i = 0; i < n / 8; i++) {
            uint64_t ptr = chunk[i];
            bool ok = plausible_ptr(ptr);
            uint64_t addr = ptr + c->deref[0];
            for (int d = 1; ok && d < c->nderef; d++) {
                ok = rd(ctx, addr, &ptr, sizeof(ptr)) && plausible_ptr(ptr);
                addr = ptr + c->deref[d];
            }
            if (!ok) continue;

            uint64_t u = 0;
            if (!rd(ctx, addr, &u, sz)) continue;
            int64_t v;
            switch (c->type) {
                case LG_S8:  v = (int8_t)u;  break;
                case LG_S16: v = (int16_t)u; break;
                case LG_S32: v = (int32_t)u; break;
                default:     v = (int64_t)u; break; /* u8/u16/u32 caben; s64 tal cual */
            }
            if (v < sc->min || v > sc->max) continue;

            if (total < max_hits) {
                hits[total].offset = c->offset + done + i * 8;
                hits[total].value = v;
            }
            total++;
        }
    }
    return total;
}

/* ---------- persistencia ---------- */

int lg_store_load(lg_game *g, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        char *bar = strchr(line, '|');
        uint64_t ts = 0;
        if (bar) {
            *bar = 0;
            ts = strtoull(bar + 1, NULL, 10);
        }
        lg_ach *a = lg_find(g, trim(line));
        if (a && !a->unlocked) { a->unlocked = true; a->unlocked_at = ts; n++; }
    }
    fclose(f);
    return n;
}

int lg_progress_save(lg_game *g, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    int ok = 1;
    for (int i = 0; i < g->nach && ok; i++) {
        const lg_ach *a = &g->ach[i];
        if (!a->unlocked && a->prog_known) ok = fprintf(f, "%s %llu\n", a->id, (unsigned long long)a->prog_value) > 0;
    }
    if (fclose(f) != 0) ok = 0;
    if (ok) g->prog_dirty = false;
    return ok ? 0 : -1;
}

int lg_progress_load(lg_game *g, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char id[LG_ID_LEN + 8];
    unsigned long long v;
    int n = 0;
    while (fscanf(f, "%39s %llu", id, &v) == 2) {
        lg_ach *a = lg_find(g, id);
        if (a && a->prog_cond >= 0) { a->prog_known = true; a->prog_value = v; n++; }
    }
    fclose(f);
    return n;
}

int lg_store_append(const char *path, const char *id, uint64_t timestamp) {
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    int ok = fprintf(f, "%s|%llu\n", id, (unsigned long long)timestamp) > 0;
    if (fclose(f) != 0) ok = 0;
    return ok ? 0 : -1;
}
