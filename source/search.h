/*
 * Buscador de valores: sirve para encontrar en que parte de la memoria guarda el juego
 * un numero (estrellas, sellos, vidas...) y la cadena de punteros que lleva hasta el,
 * que es lo que se escribe en un archivo de logros.
 *
 * Se maneja con archivos en sdmc:/config/greeny/search/ (los escribe la app):
 *   cmd.txt      la orden: "new u32 10", "next 11", "ptr 0", "check 12"
 *   result.txt   lo que encontro, en texto para leerlo en la app
 *   cand.bin     direcciones candidatas (u64 seguidos)
 *   chains.bin   cadenas de punteros encontradas
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Llamar de vez en cuando con el juego enganchado. Si hay una orden pendiente la ejecuta
 * (puede tardar minutos: mientras tanto no se revisan logros). */
void search_poll(uint64_t main_base, uint64_t main_size, uint64_t heap_base, uint64_t space_end);

/* Manejo sin salir del juego: la app deja un plan (plan.txt: "u32 10 1" = tipo, numero con el que
 * se empieza y cuanto cambia cada vez; con 0 al final es "no se el numero": cambio / no cambio) y dentro del juego se avanza con una combinacion de botones. */
/* Busca hasta 4 textos en los datos del juego y anota en sdmc:/config/greeny/find.txt lo que hay
 * alrededor de cada sitio donde salen. Sirve para dar con tablas del juego (lista de niveles...).
 * Devuelve cuantas veces los encontro. Puede tardar un par de minutos. */
int search_find_text(const char (*texts)[32], int ntexts, uint64_t main_base, uint64_t main_size, uint64_t heap_base, uint64_t space_end);

typedef enum { SEARCH_NEXT, SEARCH_PTR, SEARCH_CHECK, SEARCH_SAME } search_action;
bool search_has_plan(void);
/* Ejecuta el paso (puede tardar minutos) y deja en line1 y line2 un resumen para mostrar en el aviso. */
void search_button(search_action action, uint64_t main_base, uint64_t main_size, uint64_t heap_base, uint64_t space_end,
                   char *line1, size_t cap1, char *line2, size_t cap2);
