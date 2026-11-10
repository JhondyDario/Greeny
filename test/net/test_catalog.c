/* Baja del Supabase real los archivos de logros de una lista de juegos, con el programa curl.
 *   ./test_catalog <carpeta vacia> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../../app/source/account.h"
#include "../../app/source/catalog.h"
#include "../../common/engine.h"

static int g_requests;

static int get_curl(const char *url, void *out, size_t cap, size_t *len, long *status) {
    char outfile[] = "/tmp/greeny_cat_XXXXXX";
    if (mkstemp(outfile) < 0) return -1;
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "curl -sS -g -m 30 -o '%s' -w '%%{http_code}' '%s' -H 'apikey: %s' 2>/dev/null", outfile, url, GR_BACKEND_KEY);
    FILE *p = popen(cmd, "r");
    char code[16] = "";
    if (p) { if (!fgets(code, sizeof(code), p)) code[0] = 0; pclose(p); }
    *status = atol(code);
    FILE *f = fopen(outfile, "rb");
    *len = f ? fread(out, 1, cap, f) : 0;
    if (f) fclose(f);
    remove(outfile);
    g_requests++;
    return *status > 0 ? 0 : -1;
}
static int get_down(const char *url, void *out, size_t cap, size_t *len, long *status) {
    (void)url; (void)out; (void)cap; (void)len; (void)status;
    return -1;
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "uso: %s <carpeta>\n", argv[0]); return 2; }
    const char *root = argv[1];
    /* Mario (esta en el catalogo), un juego que no esta, y un id mal escrito */
    const char tids[3][17] = { "010028600EBDA000", "0100FFFFFFFFF000", "nojuego" };
    int n = 0, fail = 0;
    char path[512];

    if (gr_catalog_sync(get_down, root, tids, 3, &n) != -1) { printf("FALLO: sin red debe devolver -1\n"); fail++; }

    int rc = gr_catalog_sync(get_curl, root, tids, 3, &n);
    printf("primera vez: rc=%d, bajados=%d, pedidos=%d\n", rc, n, g_requests);
    if (rc != 0 || n != 1) { printf("FALLO: debia bajar solo Mario\n"); fail++; }

    /* lo que bajo tiene que ser un archivo que el motor entiende */
    snprintf(path, sizeof(path), "%s/games/010028600EBDA000.txt", root);
    FILE *f = fopen(path, "rb");
    static char buf[70000];
    size_t len = f ? fread(buf, 1, sizeof(buf) - 1, f) : 0;
    if (f) fclose(f);
    buf[len] = 0;
    static lg_game g;
    char err[128] = "";
    if (!len || lg_parse(&g, buf, err, sizeof(err)) != 0) { printf("FALLO: archivo invalido: %s\n", err); fail++; }
    else printf("archivo: %zu bytes, juego \"%s\", version %s, %d logros, %d busquedas\n", len, g.game, g.version, g.nach, g.nscan);

    g_requests = 0;
    rc = gr_catalog_sync(get_curl, root, tids, 3, &n);
    printf("segunda vez: rc=%d, bajados=%d, pedidos=%d (solo la lista)\n", rc, n, g_requests);
    if (rc != 0 || n != 0 || g_requests != 1) { printf("FALLO: no debe volver a bajar\n"); fail++; }

    /* un archivo puesto a mano (sin .src) no se pisa */
    snprintf(path, sizeof(path), "%s/games/010028600EBDA000.src", root);
    remove(path);
    snprintf(path, sizeof(path), "%s/games/010028600EBDA000.txt", root);
    f = fopen(path, "w"); fputs("game = Mi version\n[a]\nwhen = main+0 u8 == 1\n", f); fclose(f);
    rc = gr_catalog_sync(get_curl, root, tids, 3, &n);
    f = fopen(path, "r"); if (!fgets(buf, 64, f)) buf[0] = 0; fclose(f);
    printf("archivo manual: bajados=%d, primera linea: %s", n, buf);
    if (n != 0 || strncmp(buf, "game = Mi version", 17)) { printf("FALLO: piso un archivo manual\n"); fail++; }

    /* si la marca es de otra version, se actualiza */
    snprintf(path, sizeof(path), "%s/games/010028600EBDA000.src", root);
    f = fopen(path, "w"); fputs("2000-01-01T00:00:00+00:00\n", f); fclose(f);
    rc = gr_catalog_sync(get_curl, root, tids, 3, &n);
    printf("marca vieja: bajados=%d\n", n);
    if (n != 1) { printf("FALLO: debia actualizar\n"); fail++; }

    printf("%s\n", fail ? "HAY FALLOS" : "catalogo: todo bien");
    return fail ? 1 : 0;
}
