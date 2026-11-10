/*
 * Catalogo en linea: baja a la SD los archivos de logros de los juegos que el
 * usuario tiene instalados, para que no tenga que copiarlos a mano.
 *   <root>/games/<TITLE_ID>.txt   el archivo de logros
 *   <root>/games/<TITLE_ID>.src   de que version del catalogo salio
 * Un archivo puesto a mano (sin su .src) no se toca nunca.
 * Portable: la descarga la pone quien llama.
 */
#pragma once
#include "icons.h" /* gr_get_fn */

/* `tids` son los juegos instalados (16 hex en mayusculas). Devuelve 0 si pudo
 * consultar el catalogo, -1 si no hubo conexion. *downloaded = archivos nuevos o actualizados. */
int gr_catalog_sync(gr_get_fn get, const char *root, const char (*tids)[17], int count, int *downloaded);
