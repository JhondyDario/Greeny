/* Sonidos de la interfaz de la app (solo consola). Si el audio falla, la app sigue muda. */
#pragma once
#include <stdbool.h>

/* ACCEPT = boton A; ACCEPT_ALT = otras acciones (recargar, cambiar vista, pestañas); OPEN = entrar a otra pantalla */
typedef enum { SFX_NAV, SFX_ACCEPT, SFX_ACCEPT_ALT, SFX_OPEN, SFX_BACK, SFX_COUNT } sfx_id;

bool sfx_init(void);
void sfx_play(sfx_id id);
void sfx_update(void); /* una vez por cuadro */
void sfx_exit(void);

/* Musica de fondo: un archivo OGG leido de la SD poco a poco. Suena una vez; cuando termina,
 * sfx_music_finished() pasa a true y quien llama decide que sigue (la misma u otra).
 * false si el archivo no esta o no se puede leer. Llamar otra vez cambia de cancion. */
bool sfx_music_play(const char *path);
void sfx_music_stop(void);
bool sfx_music_playing(void);
bool sfx_music_finished(void); /* la cancion ya sono entera */
const char *sfx_music_note(void); /* que paso la ultima vez que se intento poner musica, para el registro */
unsigned sfx_music_played(void);  /* samples que ya sonaron de la cancion actual */
