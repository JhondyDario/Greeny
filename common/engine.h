/*
 * Greeny - motor de logros (portable, sin dependencias de libnx).
 * Se compila igual en la Switch y en PC para poder probarlo.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LG_MAX_ACH    64
#define LG_MAX_COND   6
#define LG_MAX_DEREF  8
#define LG_ID_LEN     32
#define LG_NAME_LEN   48
#define LG_DESC_LEN   96
#define LG_DEFAULT_HOLD_MS 500
#define LG_DEFAULT_POINTS 10
#define LG_MAX_SCAN   4
#define LG_SCAN_MAX_LEN 0x100000

typedef enum { LG_REGION_MAIN, LG_REGION_HEAP } lg_region;

typedef enum {
    LG_U8, LG_U16, LG_U32, LG_U64,
    LG_S8, LG_S16, LG_S32, LG_S64,
    LG_F32, LG_F64,
} lg_type;

typedef enum {
    LG_EQ, LG_NE, LG_GT, LG_GE, LG_LT, LG_LE,
    LG_BITS_SET,   /* (v & x) == x */
    LG_BITS_CLEAR, /* (v & x) == 0 */
} lg_op;

typedef struct {
    uint8_t  region;
    uint8_t  type;
    uint8_t  op;
    uint8_t  nderef;
    uint64_t offset;
    uint64_t deref[LG_MAX_DEREF];
    uint64_t u;   /* valor para tipos sin signo y mascaras */
    int64_t  s;   /* valor para tipos con signo */
    double   f;   /* valor para f32/f64 */
    /* ultimo valor leido, para el modo debug */
    uint64_t dbg_raw;
    bool     dbg_ok;     /* la ultima lectura funciono */
    bool     dbg_seen;   /* ya se evaluo al menos una vez */
    bool     dbg_dirty;  /* cambio desde la ultima vez que se reporto */
} lg_cond;

typedef struct {
    char     id[LG_ID_LEN];
    char     name[LG_NAME_LEN];
    char     desc[LG_DESC_LEN];
    char     name_en[LG_NAME_LEN]; /* traduccion opcional; "" = usar name */
    char     desc_en[LG_DESC_LEN];
    uint32_t hold_ms;
    uint16_t points;
    uint8_t  ncond;
    bool     unlocked;
    uint64_t unlocked_at; /* timestamp unix del desbloqueo; 0 = desconocido */
    bool     holding;
    uint64_t true_since;
    lg_cond  cond[LG_MAX_COND];
} lg_ach;

/* Busqueda de punteros estaticos: recorre [addr.offset, addr.offset+len) de a 8
 * bytes, trata cada valor como el primer puntero de la cadena y reporta los
 * offsets cuyo dato final cae en [min, max]. Sirve para reubicar una cadena
 * conocida cuando el juego se actualiza. */
typedef struct {
    lg_cond  addr;   /* region, offset inicial, cadena y tipo */
    uint64_t len;
    int64_t  min, max;
} lg_scan;

typedef struct {
    uint64_t offset;  /* offset dentro de la region donde esta el puntero */
    int64_t  value;
} lg_scan_hit;

typedef struct {
    char   game[LG_NAME_LEN];
    char   version[16]; /* version del juego para mostrar, ej. "1.1.0" */
    char   build[33];  /* prefijo hex del build id esperado; "" = no validar */
    bool   debug;      /* evalua todas las condiciones y marca los cambios de valor */
    int    nscan;
    lg_scan scan[LG_MAX_SCAN];
    int    nach;
    lg_ach ach[LG_MAX_ACH];
} lg_game;

typedef struct {
    uint64_t main_base;
    uint64_t heap_base;
} lg_bases;

/* Lee `size` bytes de la memoria del juego. Devuelve false si no se pudo. */
typedef bool (*lg_read_fn)(void *ctx, uint64_t addr, void *out, size_t size);

/* Parsea un archivo de definiciones. `text` se modifica. 0 = ok; si falla,
 * `err` trae el mensaje con el numero de linea. */
int lg_parse(lg_game *g, char *text, char *err, size_t errlen);

/* Evalua todos los logros pendientes. Escribe en `out` los indices de los que
 * se acaban de desbloquear y devuelve cuantos fueron. */
int lg_tick(lg_game *g, const lg_bases *b, lg_read_fn rd, void *ctx,
            uint64_t now_ms, int *out, int max_out);

/* Evalua una sola condicion (expuesto para pruebas). */
bool lg_eval_cond(lg_cond *c, const lg_bases *b, lg_read_fn rd, void *ctx);

lg_ach *lg_find(lg_game *g, const char *id);

/* Nombre/descripcion en ingles si existe la traduccion; si no, el texto base. */
const char *lg_ach_name(const lg_ach *a, bool english);
const char *lg_ach_desc(const lg_ach *a, bool english);

/* Ejecuta una busqueda. Llena `hits` (hasta max_hits) y devuelve el total de
 * candidatos encontrados, que puede ser mayor que max_hits. */
int lg_scan_run(const lg_scan *sc, const lg_bases *b, lg_read_fn rd, void *ctx,
                lg_scan_hit *hits, int max_hits);

/* Persistencia de desbloqueos: una linea "id|timestamp" por logro. */
int lg_store_load(lg_game *g, const char *path);               /* devuelve cuantos marco */
int lg_store_append(const char *path, const char *id, uint64_t timestamp); /* 0 = ok */
