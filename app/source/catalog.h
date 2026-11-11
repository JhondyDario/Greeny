/*
 * Catalogo en linea: baja a la SD los archivos de logros de los juegos que el
 * usuario tiene instalados, para que no tenga que copiarlos a mano.
 *   <root>/games/<TITLE_ID>.txt   el archivo de logros
 *   <root>/games/<TITLE_ID>.src   de que version del catalogo salio
 * Un archivo puesto a mano (sin su .src) no se toca nunca.
 * Tambien baja la caratula de cada juego a <root>/cache/<TITLE_ID>/cover.img.
 * Portable: la descarga la pone quien llama.
 */
#pragma once
#include "icons.h" /* gr_get_fn */

/* `tids` son los juegos instalados (16 hex en mayusculas). Devuelve 0 si pudo
 * consultar el catalogo, -1 si no hubo conexion. *downloaded = archivos y caratulas nuevos o actualizados. */
int gr_catalog_sync(gr_get_fn get, const char *root, const char (*tids)[17], int count, int *downloaded);

/* ---- buscador: todos los juegos publicados, no solo los instalados ---- */
typedef struct {
    char tid[17];
    char name[64];
    char version[16];
    char status[16]; /* sin_empezar, prueba, jugable o perfecto */
} gr_cat_game;

/* Lista los juegos publicados, por nombre. 0 si respondio el catalogo, -1 sin conexion. */
int gr_catalog_list(gr_get_fn get, gr_cat_game *out, int max, int *count);
/* Baja el archivo de logros de un juego a `text` (sin guardarlo). false si no se pudo. */
bool gr_catalog_definition(gr_get_fn get, const char *tid, char *text, size_t cap);
