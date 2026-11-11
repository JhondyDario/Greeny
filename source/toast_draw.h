/* Dibujo del aviso que sale dentro del juego. Portable: se prueba en PC (test/toast_host.c). */
#pragma once
#include <stdbool.h>

#define TOAST_W 448
#define TOAST_H 128
#define TOAST_BADGE 76

typedef enum {
    TOAST_INFO,   /* logros cargados: circulo verde con check */
    TOAST_UNLOCK, /* logro desbloqueado: estrella sobre bronce, plata u oro */
    TOAST_WARN,   /* logros desactivados: circulo rojo con ! */
} toast_kind;

typedef struct {
    toast_kind kind;
    int  tier;   /* 1 bronce, 2 plata, 3 oro; solo en TOAST_UNLOCK */
    int  points; /* puntos del logro; negativo = no mostrar */
    bool dark;   /* tarjeta oscura */
    const char *title, *name, *desc; /* UTF-8 */
    const unsigned char *badge;      /* dibujo RGBA de TOAST_BADGE de lado, o NULL: se pinta el circulo sencillo */
} toast_view;

/* Recibe cada pixel una o mas veces; el ultimo valor es el bueno. Componentes de 0 a 255. */
typedef void (*toast_put_fn)(int x, int y, unsigned r, unsigned g, unsigned b, unsigned a);

void toast_draw(const toast_view *v, toast_put_fn put);
