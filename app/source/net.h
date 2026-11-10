/* Conexion a internet de la app (solo consola). Todo es opcional: sin red, la app funciona igual. */
#pragma once
#include <stdbool.h>
#include <stddef.h>

void net_init(void);
void net_exit(void);
bool net_online(void);          /* la consola tiene internet ahora mismo */
const char *net_last_error(void); /* texto tecnico del ultimo fallo, para el registro */

/* Tiene la forma de gr_http_fn (ver account.h). */
int net_http(const char *url, const char *bearer, const char *json_body, char *out, size_t cap, long *status);
