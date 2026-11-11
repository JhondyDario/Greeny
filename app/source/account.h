/*
 * Cuenta de Greeny en la consola: vincular, saber quien soy y subir logros.
 * Portable: la conexion real la pone quien llama (gr_http_fn), asi se prueba en PC.
 *
 * La consola no guarda la contraseña. Guarda una llave propia (token) que solo
 * sirve para subir logros a la cuenta, en sdmc:/config/greeny/account.ini.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>

#define GR_BACKEND_URL "https://mlwzlirkjgpyyldbtoci.supabase.co"
#define GR_BACKEND_KEY "sb_publishable_r4Y8LjTEzu7A2Z9ecr2hEw_Jr-OT8RC" /* publica por diseño */
#define GR_WEB_URL     "jhondydario.github.io/Greeny"

/* POST de un JSON. Devuelve 0 si hubo respuesta del servidor (sea cual sea su
 * codigo, que deja en *status) y distinto de 0 si no se pudo conectar.
 * `bearer` puede ser NULL. La respuesta queda en `out` terminada en 0. */
typedef int (*gr_http_fn)(const char *url, const char *bearer, const char *json_body, char *out, size_t cap, long *status);

typedef enum {
    GR_NET_OK,
    GR_NET_OFFLINE,   /* no hubo conexion */
    GR_NET_REJECTED,  /* el servidor dijo que no; el motivo queda en err */
    GR_NET_BAD,       /* respuesta que no se entiende */
} gr_net_result;

typedef struct {
    char token[80];      /* llave de esta consola; "" = nunca se vinculo */
    bool linked;         /* la ultima vez que se pregunto, estaba vinculada a un usuario */
    char username[24];
    char display[36];
    char caption[64];
    bool social_known;   /* ya se consultaron seguidores y siguiendo alguna vez */
    int  followers, following;
    char friend_code[24];  /* GR-123456; "" si aun no se sabe */
    int  avatar;           /* foto de perfil elegida (1..); 0 = aun no se sabe */
} gr_account;

void gr_account_load(gr_account *a, const char *path);
int  gr_account_save(const gr_account *a, const char *path); /* 0 = ok */

/* Pide un codigo de vinculacion. Deja la llave nueva en a->token (sin vincular aun). */
gr_net_result gr_link_start(gr_http_fn http, const char *device_name, gr_account *a, char code[8], char *err, size_t errlen);

/* Pregunta al servidor de quien es esta consola y actualiza `a`.
 * *exists = false si la llave ya no existe (codigo vencido o consola desvinculada desde la web). */
gr_net_result gr_whoami(gr_http_fn http, gr_account *a, bool *exists);

/* Trae seguidores y siguiendo (lo unico del perfil que la consola no puede calcular sola). */
gr_net_result gr_fetch_social(gr_http_fn http, gr_account *a);

/* Sigue a alguien por su codigo de amigo. Deja en `who` el usuario seguido. */
gr_net_result gr_follow_code(gr_http_fn http, const gr_account *a, const char *code, char *who, size_t wholen,
                             char *err, size_t errlen);

/* Cambia la foto de perfil por la numero `picture` de las que trae Greeny. */
gr_net_result gr_set_avatar(gr_http_fn http, gr_account *a, int picture, char *err, size_t errlen);

/* Entra con correo y contraseña y registra la consola de una vez. */
gr_net_result gr_login_password(gr_http_fn http, const char *email, const char *password, const char *device_name,
                                gr_account *a, char *err, size_t errlen);

/* Sube todos los logros guardados en `root`/unlocked. *sent = cuantos se enviaron,
 * *added = cuantos eran nuevos para el servidor. */
gr_net_result gr_push_unlocks(gr_http_fn http, const gr_account *a, const char *root, int *sent, int *added,
                              char *err, size_t errlen);

/* Utilidades de JSON expuestas para las pruebas. */
size_t gr_json_escape(char *dst, size_t cap, const char *src);                 /* devuelve lo escrito, sin el 0 */
bool   gr_json_string(const char *json, const char *key, char *out, size_t cap); /* false si no esta o es null */
bool   gr_json_bool(const char *json, const char *key, bool *out);
bool   gr_json_int(const char *json, const char *key, int *out);
