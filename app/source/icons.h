/*
 * Iconos de los logros: se bajan del catalogo cuando hay internet y se guardan
 * en la SD ya reducidos, para que sin red se sigan viendo.
 *   <root>/cache/<TITLE_ID>/<ach_id>.bin   imagen RGBA de GR_ICON_SIZE de lado
 *   <root>/cache/<TITLE_ID>/<ach_id>.src   de que archivo del servidor salio
 * Portable: la descarga la pone quien llama (gr_get_fn), asi se prueba en PC.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GR_ICON_SIZE 60
#define GR_ICON_BYTES (GR_ICON_SIZE * GR_ICON_SIZE * 4)

/* GET. Devuelve 0 si el servidor respondio (codigo en *status) y distinto de 0
 * si no hubo conexion. Deja *len bytes en out. */
typedef int (*gr_get_fn)(const char *url, void *out, size_t cap, size_t *len, long *status);

/* Pone al dia los iconos de un juego. Solo baja los que faltan o cambiaron.
 * Devuelve 0 si pudo consultar el catalogo, -1 si no hubo conexion.
 * *downloaded = cuantos bajo esta vez. */
int gr_icons_sync(gr_get_fn get, const char *root, const char *tid, int *downloaded);

/* Lee un icono guardado. `rgba` debe tener GR_ICON_BYTES. false si no esta. */
bool gr_icon_load(const char *root, const char *tid, const char *ach_id, uint8_t *rgba);

/* Pasa un icono a gris y lo apaga un poco: asi se ven los logros bloqueados. */
void gr_icon_gray(const uint8_t *src, uint8_t *dst);
