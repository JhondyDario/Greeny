/* Baja los iconos de un juego del Supabase real con el programa curl y revisa la cache.
 *   ./test_icons <carpeta vacia> <TITLE_ID> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../app/source/account.h"
#include "../../app/source/icons.h"

static int g_requests;

static int get_curl(const char *url, void *out, size_t cap, size_t *len, long *status) {
    char outfile[] = "/tmp/greeny_get_XXXXXX";
    int fd = mkstemp(outfile);
    if (fd < 0) return -1;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -sS -m 30 -o '%s' -w '%%{http_code}' '%s' -H 'apikey: %s' 2>/dev/null", outfile, url, GR_BACKEND_KEY);
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
    if (argc != 3) { fprintf(stderr, "uso: %s <carpeta> <TITLE_ID>\n", argv[0]); return 2; }
    int n = 0, fail = 0;

    if (gr_icons_sync(get_down, argv[1], argv[2], &n) != -1 || n != 0) { printf("FALLO: sin red debe devolver -1\n"); fail++; }

    int rc = gr_icons_sync(get_curl, argv[1], argv[2], &n);
    printf("primera vez: rc=%d, bajados=%d, pedidos=%d\n", rc, n, g_requests);
    int first = n;
    if (rc != 0 || n == 0) { printf("FALLO: no bajo nada\n"); fail++; }

    g_requests = 0;
    rc = gr_icons_sync(get_curl, argv[1], argv[2], &n);
    printf("segunda vez: rc=%d, bajados=%d, pedidos=%d (solo la lista)\n", rc, n, g_requests);
    if (rc != 0 || n != 0 || g_requests != 1) { printf("FALLO: no debe volver a bajar lo que ya tiene (ni lo que no pudo abrir)\n"); fail++; }

    static uint8_t icon[GR_ICON_BIG_BYTES], gray[GR_ICON_BIG_BYTES];
    if (!gr_icon_load(argv[1], argv[2], "vidas_10", icon)) { printf("FALLO: no se puede leer vidas_10\n"); fail++; }
    else {
        gr_icon_gray(icon, gray, GR_ICON_BIG);
        int corner = icon[3], center = icon[(60 * GR_ICON_BIG + 60) * 4 + 3];
        printf("vidas_10: alfa esquina=%d (redondeada), alfa centro=%d\n", corner, center);
        if (corner != 0 || center != 255) { printf("FALLO: esquinas\n"); fail++; }
        const uint8_t *g = gray + (60 * GR_ICON_BIG + 60) * 4;
        if (g[0] != g[1] || g[1] != g[2]) { printf("FALLO: el gris no es gris\n"); fail++; }
    }
    if (gr_icon_load(argv[1], argv[2], "no_existe", icon) || gr_icon_load(argv[1], argv[2], "../x", icon)) { printf("FALLO: lee iconos que no hay\n"); fail++; }

    printf("%s (%d iconos)\n", fail ? "HAY FALLOS" : "iconos: todo bien", first);
    return fail ? 1 : 0;
}
