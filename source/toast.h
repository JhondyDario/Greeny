/* Popup de "logro desbloqueado" dibujado en una capa propia de vi. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "toast_draw.h"

/* Encola un popup. Si la cola esta llena se descarta (el logro igual se guarda). */
/* tier (1 bronce, 2 plata, 3 oro) y points solo cuentan en TOAST_UNLOCK; points < 0 no se muestra. */
void toast_push(toast_kind kind, int tier, int points, bool dark, const char *title, const char *name, const char *desc);

/* Donde sale (0 derecha, 1 izquierda, 2 centro; siempre arriba) y cuanto dura. Se aplica al siguiente aviso. */
void toast_configure(int pos, int show_ms);

/* Llamar en cada vuelta del loop principal. */
void toast_update(uint64_t now_ms);

/* true si la capa grafica no se pudo crear (se sigue funcionando sin popups). */
bool toast_failed(void);
uint32_t toast_fail_rc(void);
