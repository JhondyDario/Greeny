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

/* Tiene la forma de gr_get_fn (ver icons.h). Sirve para bajar imagenes. */
int net_get(const char *url, void *out, size_t cap, size_t *len, long *status);
/* Baja `url` a un archivo (siguiendo redirecciones), sin mandar llaves de Greeny.
 * *done y *total se van llenando para mostrar el avance (total 0 = desconocido). 0 = ok. */
int net_download(const char *url, const char *path, volatile long *done, volatile long *total);
