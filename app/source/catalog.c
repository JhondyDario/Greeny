#include "catalog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "account.h"
#include "engine.h"

#define CHUNK 40                  /* juegos por consulta, para que la direccion no sea enorme */
#define LIST_MAX (16 * 1024)
#define DEF_JSON_MAX (160 * 1024) /* el archivo viaja con sus saltos de linea escapados */
#define DEF_MAX (64 * 1024)

static bool valid_tid(const char *t) {
    if (strlen(t) != 16) return false;
    for (int i = 0; i < 16; i++)
        if (!((t[i] >= '0' && t[i] <= '9') || (t[i] >= 'A' && t[i] <= 'F'))) return false;
    return true;
}

static bool exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void read_line(const char *path, char *out, size_t cap) {
    out[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (!fgets(out, (int)cap, f)) out[0] = 0;
    fclose(f);
    out[strcspn(out, "\r\n")] = 0;
}

/* Baja el archivo de un juego, comprueba que el motor lo entiende y lo guarda. */
static bool fetch_definition(gr_get_fn get, const char *root, const char *tid, const char *stamp, char *json, char *text) {
    char url[256], path[320];
    size_t len = 0;
    long status = 0;
    snprintf(url, sizeof(url), GR_BACKEND_URL "/rest/v1/games?select=definition&published=eq.true&title_id=eq.%s", tid);
    if (get(url, json, DEF_JSON_MAX - 1, &len, &status) != 0 || status != 200) return false;
    json[len] = 0;
    if (!gr_json_string(json, "definition", text, DEF_MAX) || !text[0]) return false;

    /* nunca se guarda un archivo que el sysmodule no vaya a poder leer */
    static lg_game probe;
    char err[128];
    char *copy = malloc(strlen(text) + 1);
    if (!copy) return false;
    strcpy(copy, text);
    bool ok = lg_parse(&probe, copy, err, sizeof(err)) == 0;
    free(copy);
    if (!ok) return false;

    snprintf(path, sizeof(path), "%s/games/%s.txt", root, tid);
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    ok = fwrite(text, 1, strlen(text), f) == strlen(text);
    if (fclose(f) != 0) ok = false;
    if (!ok) { remove(path); return false; }

    snprintf(path, sizeof(path), "%s/games/%s.src", root, tid);
    f = fopen(path, "w");
    ok = f && fprintf(f, "%s\n", stamp) > 0;
    if (f && fclose(f) != 0) ok = false;
    return ok;
}

/* Baja la caratula que el juego tiene en el catalogo y la deja en la SD tal cual (JPEG o PNG):
 *   <root>/cache/<TITLE_ID>/cover.img   la imagen
 *   <root>/cache/<TITLE_ID>/cover.src   de que archivo del servidor salio */
#define COVER_MAX (1024 * 1024)
static bool fetch_cover(gr_get_fn get, const char *root, const char *tid, const char *cover_path) {
    char url[384], path[320], have[160];
    snprintf(path, sizeof(path), "%s/cache/%s/cover.src", root, tid);
    read_line(path, have, sizeof(have));
    snprintf(path, sizeof(path), "%s/cache/%s/cover.img", root, tid);
    if (!strcmp(have, cover_path) && exists(path)) return false; /* ya esta al dia */
    if (strstr(cover_path, "..") || strlen(cover_path) > 120) return false;

    uint8_t *buf = malloc(COVER_MAX);
    if (!buf) return false;
    size_t len = 0;
    long status = 0;
    snprintf(url, sizeof(url), GR_BACKEND_URL "/storage/v1/object/public/covers/%s", cover_path);
    bool ok = get(url, buf, COVER_MAX, &len, &status) == 0 && status == 200 && len > 16;
    if (ok) {
        char dir[320];
        snprintf(dir, sizeof(dir), "%s/cache", root);
        mkdir(dir, 0777);
        snprintf(dir, sizeof(dir), "%s/cache/%s", root, tid);
        mkdir(dir, 0777);
        FILE *f = fopen(path, "wb");
        ok = f && fwrite(buf, 1, len, f) == len;
        if (f && fclose(f) != 0) ok = false;
        if (ok) {
            snprintf(path, sizeof(path), "%s/cache/%s/cover.src", root, tid);
            f = fopen(path, "w");
            ok = f && fprintf(f, "%s\n", cover_path) > 0;
            if (f && fclose(f) != 0) ok = false;
        }
    }
    free(buf);
    return ok;
}

int gr_catalog_sync(gr_get_fn get, const char *root, const char (*tids)[17], int count, int *downloaded) {
    *downloaded = 0;
    char *list = malloc(LIST_MAX), *json = malloc(DEF_JSON_MAX), *text = malloc(DEF_MAX);
    char *url = malloc(256 + CHUNK * 20);
    int rc = 0;
    if (!list || !json || !text || !url) goto done;

    char dir[256];
    snprintf(dir, sizeof(dir), "%s/games", root);
    mkdir(root, 0777);
    mkdir(dir, 0777);

    for (int start = 0; start < count && rc == 0; start += CHUNK) {
        /* ¿cuales de estos juegos estan en el catalogo, y de cuando es su version? */
        int n = sprintf(url, GR_BACKEND_URL "/rest/v1/games?select=title_id,updated_at,cover_path&published=eq.true&title_id=in.(");
        int added = 0;
        for (int i = start; i < count && i < start + CHUNK; i++) {
            if (!valid_tid(tids[i])) continue;
            n += sprintf(url + n, "%s%s", added ? "," : "", tids[i]);
            added++;
        }
        if (!added) continue;
        sprintf(url + n, ")");

        size_t len = 0;
        long status = 0;
        if (get(url, list, LIST_MAX - 1, &len, &status) != 0) { rc = -1; break; }
        list[len] = 0;
        if (status != 200) continue;

        for (char *p = strchr(list, '{'); p; ) {
            char *end = strchr(p, '}');
            if (!end) break;
            char saved = end[1];
            end[1] = 0;
            char tid[24], stamp[48], have[48], txt[320], src[320], cover[160];
            if (gr_json_string(p, "title_id", tid, sizeof(tid)) && valid_tid(tid) &&
                gr_json_string(p, "cover_path", cover, sizeof(cover)) && cover[0] && fetch_cover(get, root, tid, cover))
                (*downloaded)++; /* la caratula se baja aunque el archivo de logros lo haya puesto el usuario */
            if (gr_json_string(p, "title_id", tid, sizeof(tid)) && valid_tid(tid) &&
                gr_json_string(p, "updated_at", stamp, sizeof(stamp)) && stamp[0]) {
                snprintf(txt, sizeof(txt), "%s/games/%s.txt", root, tid);
                snprintf(src, sizeof(src), "%s/games/%s.src", root, tid);
                read_line(src, have, sizeof(have));
                bool manual = exists(txt) && !have[0];           /* lo puso el usuario: no se toca */
                bool current = exists(txt) && !strcmp(have, stamp);
                if (!manual && !current && fetch_definition(get, root, tid, stamp, json, text)) (*downloaded)++;
            }
            end[1] = saved;
            p = strchr(end + 1, '{');
        }
    }

done:
    free(list);
    free(json);
    free(text);
    free(url);
    return rc;
}

int gr_catalog_list(gr_get_fn get, gr_cat_game *out, int max, int *count) {
    *count = 0;
    const size_t cap = 96 * 1024;
    char *list = malloc(cap);
    if (!list) return -1;
    size_t len = 0;
    long status = 0;
    int rc = get(GR_BACKEND_URL "/rest/v1/games?select=title_id,name,version,status,updated_at&published=eq.true&order=name&limit=300",
                 list, cap - 1, &len, &status);
    if (rc != 0) { free(list); return -1; }
    list[len] = 0;
    if (status == 200) {
        for (char *p = strchr(list, '{'); p && *count < max; ) {
            char *end = strchr(p, '}');
            if (!end) break;
            char saved = end[1];
            end[1] = 0;
            gr_cat_game *g = &out[*count];
            memset(g, 0, sizeof(*g));
            char tid[24];
            if (gr_json_string(p, "title_id", tid, sizeof(tid)) && valid_tid(tid)) {
                memcpy(g->tid, tid, 17);
                if (!gr_json_string(p, "name", g->name, sizeof(g->name)) || !g->name[0]) memcpy(g->name, tid, 17);
                gr_json_string(p, "version", g->version, sizeof(g->version));
                gr_json_string(p, "status", g->status, sizeof(g->status));
                char up[40] = "";
                if (gr_json_string(p, "updated_at", up, sizeof(up))) snprintf(g->updated, sizeof(g->updated), "%.10s", up);
                (*count)++;
            }
            end[1] = saved;
            p = strchr(end + 1, '{');
        }
    }
    free(list);
    return 0;
}

bool gr_catalog_definition(gr_get_fn get, const char *tid, char *text, size_t cap) {
    char url[256];
    if (!valid_tid(tid) || cap < DEF_MAX) return false;
    char *json = malloc(DEF_JSON_MAX);
    if (!json) return false;
    size_t len = 0;
    long status = 0;
    snprintf(url, sizeof(url), GR_BACKEND_URL "/rest/v1/games?select=definition&published=eq.true&title_id=eq.%s", tid);
    bool ok = get(url, json, DEF_JSON_MAX - 1, &len, &status) == 0 && status == 200;
    if (ok) {
        json[len] = 0;
        ok = gr_json_string(json, "definition", text, DEF_MAX) && text[0];
    }
    free(json);
    return ok;
}
