/* Popup de "logro desbloqueado" dibujado en una capa propia de vi. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Encola un popup. Si la cola esta llena se descarta (el logro igual se guarda). */
void toast_push(const char *title, const char *name, const char *desc);

/* Llamar en cada vuelta del loop principal. */
void toast_update(uint64_t now_ms);

/* true si la capa grafica no se pudo crear (se sigue funcionando sin popups). */
bool toast_failed(void);
uint32_t toast_fail_rc(void);
