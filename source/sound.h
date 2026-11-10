/* Sonidos del sysmodule por el renderizador de audio (audren). */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { SOUND_LOADED, SOUND_UNLOCK, SOUND_COUNT } sound_id;

/* Pide que suene. Si ya hay uno sonando, este espera a que termine. */
void sound_play(sound_id id);

/* Llamar en cada vuelta del loop principal. */
void sound_update(uint64_t now_ms);

/* true si el audio no se pudo abrir (se sigue funcionando sin sonido). */
bool sound_failed(void);
uint32_t sound_fail_rc(void);
