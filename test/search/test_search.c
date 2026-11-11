/* Prueba del buscador de valores con un "juego" de mentira: dos zonas de memoria y un numero
 * al final de una cadena de tres punteros que sale de main.
 * Se corre desde una carpeta vacia: escribe en ./sdmc:/config/greeny/search/. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "dmntcht.h"
#include "search.h"

#define MAIN 0x100000ull
#define MAIN_SIZE 0x20000ull
#define HEAP 0x8000000ull
#define HEAP_SIZE 0x300000ull
#define END 0x10000000ull
static u8 g_main[MAIN_SIZE], g_heap[HEAP_SIZE];

static u8 *at(u64 a, size_t n) {
    if (a >= MAIN && a + n <= MAIN + MAIN_SIZE) return g_main + (a - MAIN);
    if (a >= HEAP && a + n <= HEAP + HEAP_SIZE) return g_heap + (a - HEAP);
    return NULL;
}
Result dmntchtReadCheatProcessMemory(u64 a, void *buf, size_t n) { u8 *p = at(a, n); if (!p) return 1; memcpy(buf, p, n); return 0; }
Result dmntchtQueryCheatProcessMemory(MemoryInfo *m, u64 a) {
    memset(m, 0, sizeof(*m));
    if (a < MAIN) { m->addr = 0; m->size = MAIN; }
    else if (a < MAIN + MAIN_SIZE) { m->addr = MAIN; m->size = MAIN_SIZE; m->type = MemType_CodeMutable; m->perm = Perm_Rw; }
    else if (a < HEAP) { m->addr = MAIN + MAIN_SIZE; m->size = HEAP - m->addr; }
    else if (a < HEAP + HEAP_SIZE) { m->addr = HEAP; m->size = HEAP_SIZE; m->type = MemType_Heap; m->perm = Perm_Rw; }
    else { m->addr = HEAP + HEAP_SIZE; m->size = END - m->addr; }
    return 0;
}
static void put64(u64 a, u64 v) { memcpy(at(a, 8), &v, 8); }
static void put32(u64 a, u32 v) { memcpy(at(a, 4), &v, 4); }

/* monta la cadena main+0x1C00->0x18->0x138->0x48 con los objetos en `shift` */
static u64 build(u64 shift, u32 value) {
    memset(g_heap, 0, sizeof(g_heap));
    srand(7);
    for (size_t i = 0; i < HEAP_SIZE; i += 4) { u32 r = (u32)(rand() % 40); memcpy(g_heap + i, &r, 4); } /* ruido: muchos numeros pequeños */
    u64 a = HEAP + 0x10000 + shift, b = HEAP + 0x90000 + shift * 2, c = HEAP + 0x150000 + shift * 3;
    put64(MAIN + 0x1C00, a);
    put64(a + 0x18, b);
    put64(b + 0x138, c);
    put32(c + 0x48, value);
    return c + 0x48;
}
static void cmd(const char *text) {
    FILE *f = fopen("sdmc:/config/greeny/search/cmd.txt", "w");
    fprintf(f, "%s\n", text); fclose(f);
    search_poll(MAIN, MAIN_SIZE, HEAP, END);
}
static int show(const char *title, const char *must) {
    char line[200]; int found = 0;
    printf("--- %s\n", title);
    FILE *f = fopen("sdmc:/config/greeny/search/result.txt", "r");
    while (f && fgets(line, sizeof(line), f)) { fputs(line, stdout); if (strstr(line, must)) found = 1; }
    if (f) fclose(f);
    if (!found) printf("FALLO: no aparece \"%s\"\n", must);
    return !found;
}
int main(void) {
    int fail = 0;
    mkdir("sdmc:", 0777); mkdir("sdmc:/config", 0777); mkdir("sdmc:/config/greeny", 0777); mkdir("sdmc:/config/greeny/search", 0777);
    u64 where = build(0, 10);
    cmd("new u32 10");
    fail += show("busqueda nueva (10)", "candidata");
    put32(where, 11);
    cmd("next 11");
    fail += show("filtro (11)", "1 candidata");
    cmd("ptr 0");
    fail += show("punteros", "main+0x1C00->0x18->0x138->0x48 u32");
    where = build(0x3210, 12); /* "se reinicio el juego": todo cambia de sitio y el numero ahora es 12 */
    cmd("check 12");
    fail += show("comprobar tras reiniciar", "SIRVE  main+0x1C00->0x18->0x138->0x48 u32");
    cmd("check 99");
    fail += show("comprobar con un valor que no es", "0 de");
    /* lo mismo, pero como se usa dentro del juego: un plan y la combinacion de botones */
    remove("sdmc:/config/greeny/search/state.txt");
    FILE *pf = fopen("sdmc:/config/greeny/search/plan.txt", "w");
    fprintf(pf, "u32 10 1\n"); fclose(pf);
    char l1[64], l2[96];
    where = build(0, 10);
    search_button(SEARCH_NEXT, MAIN, MAIN_SIZE, HEAP, END, l1, sizeof(l1), l2, sizeof(l2));
    printf("--- botones\n%s | %s\n", l1, l2);
    put32(where, 11);
    search_button(SEARCH_NEXT, MAIN, MAIN_SIZE, HEAP, END, l1, sizeof(l1), l2, sizeof(l2));
    printf("%s | %s\n", l1, l2);
    if (!strstr(l2, "1 candidata")) { printf("FALLO: el filtro por botones\n"); fail++; }
    search_button(SEARCH_PTR, MAIN, MAIN_SIZE, HEAP, END, l1, sizeof(l1), l2, sizeof(l2));
    printf("%s | %s\n", l1, l2);
    where = build(0x5550, 11); /* se reinicia el juego con el mismo numero guardado */
    search_button(SEARCH_CHECK, MAIN, MAIN_SIZE, HEAP, END, l1, sizeof(l1), l2, sizeof(l2));
    printf("%s | %s\n", l1, l2);
    if (!strstr(l2, "1 cadena")) { printf("FALLO: comprobar por botones\n"); fail++; }
    fail += show("resultado final", "SIRVE  main+0x1C00->0x18->0x138->0x48 u32");

    printf("%s\n", fail ? "HAY FALLOS" : "buscador: todo bien");
    return fail != 0;
}
