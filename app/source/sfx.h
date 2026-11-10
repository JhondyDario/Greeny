/* Sonidos de la interfaz de la app (solo consola). Si el audio falla, la app sigue muda. */
#pragma once
#include <stdbool.h>

typedef enum { SFX_NAV, SFX_ACCEPT, SFX_BACK, SFX_COUNT } sfx_id;

bool sfx_init(void);
void sfx_play(sfx_id id);
void sfx_update(void); /* una vez por cuadro */
void sfx_exit(void);
