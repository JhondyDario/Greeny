/* Sonidos de la interfaz de la app (solo consola). Si el audio falla, la app sigue muda. */
#pragma once
#include <stdbool.h>

/* Mismo orden que APP_SFX en scripts/assets.py (assets/sounds/<nombre>.wav). */
typedef enum {
    SFX_NAV,        /* moverse por las listas */
    SFX_ACCEPT,     /* A sobre un elemento */
    SFX_OPTION,     /* cambiar una opcion de las que se eligen */
    SFX_TOGGLE,     /* encender o apagar un interruptor */
    SFX_XY,         /* acciones con X o Y */
    SFX_LR,         /* L o R: pestañas y atajos */
    SFX_DETAIL,     /* abrir un logro o pasar al siguiente con las flechas */
    SFX_OPEN1,      /* abrir una pantalla */
    SFX_OPEN2,      /* abrir una pantalla grande que casi nunca se abre (acerca de, diagnostico, actualizar) */
    SFX_OPEN3,      /* abrir un panel pequeño (fotos, frases, paises) */
    SFX_BACK1,      /* volver */
    SFX_BACK2,      /* volver al inicio */
    SFX_ERROR,      /* algo salio mal */
    SFX_READ,       /* marcar avisos como leidos */
    SFX_NOTICE,     /* llego un aviso nuevo */
    SFX_SOCIAL,     /* me gusta, seguir a alguien */
    SFX_MENU,       /* abrir el perfil, el ranking o el buscador */
    SFX_SUCCESS,    /* algo salio bien (mensaje de "listo") */
    SFX_WELCOME,    /* la presentacion al abrir la app */
    SFX_COUNT
} sfx_id;
/* nombres de antes */
#define SFX_ACCEPT_ALT SFX_XY
#define SFX_OPEN SFX_OPEN2
#define SFX_BACK SFX_BACK1

bool sfx_init(void);
void sfx_play(sfx_id id);
void sfx_set_enabled(bool on); /* apagar o encender los sonidos de los botones (la musica va aparte) */
void sfx_update(void); /* una vez por cuadro */
void sfx_exit(void);

/* Musica de fondo: un archivo OGG leido de la SD poco a poco. Suena una vez; cuando termina,
 * sfx_music_finished() pasa a true y quien llama decide que sigue (la misma u otra).
 * false si el archivo no esta o no se puede leer. Llamar otra vez cambia de cancion. */
bool sfx_music_play(const char *path);
void sfx_music_stop(void);
bool sfx_music_playing(void);
void sfx_music_pause(bool paused); /* al salir al menu HOME y al volver */
bool sfx_music_finished(void); /* la cancion ya sono entera */
const char *sfx_music_note(void); /* que paso la ultima vez que se intento poner musica, para el registro */
unsigned sfx_music_played(void);  /* samples que ya sonaron de la cancion actual */
