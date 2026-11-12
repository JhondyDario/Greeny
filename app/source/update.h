/* Actualizar la app desde las versiones (releases) publicadas en GitHub. */
#pragma once
#include <stdbool.h>
#include <stddef.h>

#define GR_UPDATE_API "https://api.github.com/repos/JhondyDario/Greeny/releases/latest"

typedef struct {
    char version[24];   /* "1.2.0", sin la v */
    char nro_url[512];  /* enlace de descarga de greeny.nro */
    char notes[1200];   /* lo que trae la version (el texto de la release) */
    long size;          /* bytes de greeny.nro; 0 si no se sabe */
} gr_release;

/* Lee la respuesta de la API. false si no trae version o no trae greeny.nro. */
bool gr_parse_release(const char *json, gr_release *out);
/* true si `latest` es mas nueva que `current` ("0.2.0" < "0.10.0" < "1.0.0"). Acepta la v delante. */
bool gr_version_newer(const char *latest, const char *current);
/* Un NRO de verdad: "NRO0" en 0x10 y un tamaño razonable. */
bool gr_nro_valid(const unsigned char *head, size_t head_len, long file_size);
