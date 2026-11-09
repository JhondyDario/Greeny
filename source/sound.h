/* Sonido de "logro desbloqueado" por el renderizador de audio (audren). */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Pide que suene. Si ya esta sonando, no hace nada. */
void sound_play(void);

/* Llamar en cada vuelta del loop principal. */
void sound_update(uint64_t now_ms);

/* true si el audio no se pudo abrir (se sigue funcionando sin sonido). */
bool sound_failed(void);
uint32_t sound_fail_rc(void);
