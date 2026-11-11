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
#include <stdint.h>

/* Llamar de vez en cuando con el juego enganchado. Si hay una orden pendiente la ejecuta
 * (puede tardar minutos: mientras tanto no se revisan logros). */
void search_poll(uint64_t main_base, uint64_t main_size, uint64_t heap_base, uint64_t space_end);
