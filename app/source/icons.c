#include "icons.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "account.h"
#include "gfx.h"
#include "third_party/stb_image.h"

#define LIST_MAX (32 * 1024)
#define IMAGE_MAX (1024 * 1024)

static bool safe_id(const char *s) {
    size_t n = strlen(s);
    if (n == 0 || n > 31) return false;
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_' || *s == '-')) return false;
    return true;
}

/* ruta dentro del bucket: letras, numeros y / . _ - ; sin ".." */
static bool safe_path(const char *s) {
    if (!*s || strlen(s) > 120 || strstr(s, "..") || s[0] == '/') return false;
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_' || *s == '-' ||
              *s == '.' || *s == '/')) return false;
    return true;
}

static void cache_file(char *out, size_t cap, const char *root, const char *tid, const char *id, const char *ext) {
    snprintf(out, cap, "%s/cache/%.16s/%.31s.%s", root, tid, id, ext);
}

bool gr_icon_load(const char *root, const char *tid, const char *ach_id, uint8_t *rgba) {
    char path[320];
    if (!safe_id(ach_id)) return false;
    cache_file(path, sizeof(path), root, tid, ach_id, "bin");
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    bool ok = fread(rgba, 1, GR_ICON_BIG_BYTES, f) == GR_ICON_BIG_BYTES;
    fclose(f);
    return ok;
}

void gr_icon_gray(const uint8_t *src, uint8_t *dst, int side) {
    for (int i = 0; i < side * side; i++) {
        const uint8_t *s = src + i * 4;
        unsigned y = (s[0] * 77u + s[1] * 150u + s[2] * 29u) >> 8;
        y = (y * 3 + 128) / 4; /* un poco lavado, para que no compita con los de color */
        dst[i * 4] = dst[i * 4 + 1] = dst[i * 4 + 2] = (uint8_t)y;
        dst[i * 4 + 3] = s[3];
    }
}

/* ¿el icono guardado ya es el de esa ruta? */
static bool up_to_date(const char *root, const char *tid, const char *id, const char *icon_path) {
    char path[320], have[128] = "";
    cache_file(path, sizeof(path), root, tid, id, "src");
    FILE *f = fopen(path, "r");
    if (!f) return false;
    if (!fgets(have, sizeof(have), f)) have[0] = 0;
    fclose(f);
    have[strcspn(have, "\r\n")] = 0;
    /* "!ruta" = ese archivo ya se bajo y no se pudo abrir: no insistir hasta que lo cambien */
    if (have[0] == '!') return !strcmp(have + 1, icon_path);
    if (strcmp(have, icon_path)) return false;
    cache_file(path, sizeof(path), root, tid, id, "bin");
    struct stat st;
    return stat(path, &st) == 0 && st.st_size == GR_ICON_BIG_BYTES; /* los de 60 de versiones viejas se vuelven a bajar */
}

static bool fetch_one(gr_get_fn get, const char *root, const char *tid, const char *id, const char *icon_path, uint8_t *work) {
    char url[320], path[320];
    size_t len = 0;
    long status = 0;
    snprintf(url, sizeof(url), GR_BACKEND_URL "/storage/v1/object/public/icons/%s", icon_path);
    if (get(url, work, IMAGE_MAX, &len, &status) != 0 || status != 200 || len < 16) return false;

    int w, h, n;
    uint8_t *img = stbi_load_from_memory(work, (int)len, &w, &h, &n, 4);
    if (!img) {
        /* Bajo bien pero no es un PNG ni un JPEG de verdad (por ejemplo un WebP con
         * nombre .png). Se anota para no volver a bajarlo cada vez. */
        cache_file(path, sizeof(path), root, tid, id, "bin");
        remove(path);
        cache_file(path, sizeof(path), root, tid, id, "src");
        FILE *f = fopen(path, "w");
        if (f) { fprintf(f, "!%s\n", icon_path); fclose(f); }
        return false;
    }
    bool ok = false;
    if (w > 0 && h > 0 && w <= 2048 && h <= 2048) {
        static uint8_t small[GR_ICON_BIG_BYTES];
        /* si no es cuadrada se recorta al centro, para no deformarla */
        int side = w < h ? w : h, ox = (w - side) / 2, oy = (h - side) / 2;
        if (side != w || side != h) {
            for (int y = 0; y < side; y++) memmove(img + (size_t)y * side * 4, img + ((size_t)(y + oy) * w + ox) * 4, (size_t)side * 4);
            w = h = side;
        }
        gfx_scale_rgba(img, w, h, small, GR_ICON_BIG, GR_ICON_BIG);
        gfx_round_corners(small, GR_ICON_BIG, GR_ICON_BIG, 22);

        cache_file(path, sizeof(path), root, tid, id, "bin");
        FILE *f = fopen(path, "wb");
        if (f) {
            ok = fwrite(small, 1, GR_ICON_BIG_BYTES, f) == GR_ICON_BIG_BYTES;
            if (fclose(f) != 0) ok = false;
        }
        if (ok) {
            /* la marca se escribe al final: si algo se corta a medias, se vuelve a bajar */
            cache_file(path, sizeof(path), root, tid, id, "src");
            f = fopen(path, "w");
            ok = f && fprintf(f, "%s\n", icon_path) > 0;
            if (f && fclose(f) != 0) ok = false;
        }
    }
    stbi_image_free(img);
    return ok;
}

int gr_icons_sync(gr_get_fn get, const char *root, const char *tid, int *downloaded) {
    *downloaded = 0;
    char url[256], dir[256];
    char *list = malloc(LIST_MAX);
    uint8_t *work = malloc(IMAGE_MAX);
    if (!list || !work) { free(list); free(work); return 0; }

    size_t len = 0;
    long status = 0;
    snprintf(url, sizeof(url), GR_BACKEND_URL "/rest/v1/achievements?select=ach_id,icon_path&title_id=eq.%.16s", tid);
    if (get(url, list, LIST_MAX - 1, &len, &status) != 0) { free(list); free(work); return -1; }
    list[len] = 0;

    if (status == 200) {
        snprintf(dir, sizeof(dir), "%s/cache", root);
        mkdir(dir, 0777);
        snprintf(dir, sizeof(dir), "%s/cache/%.16s", root, tid);
        mkdir(dir, 0777);

        /* la respuesta es una lista de objetos planos: se recorre de { a } */
        for (char *p = strchr(list, '{'); p; ) {
            char *end = strchr(p, '}');
            if (!end) break;
            char saved = end[1];
            end[1] = 0;
            char id[40], icon[128];
            if (gr_json_string(p, "ach_id", id, sizeof(id)) && gr_json_string(p, "icon_path", icon, sizeof(icon)) &&
                safe_id(id) && safe_path(icon) && !up_to_date(root, tid, id, icon) &&
                fetch_one(get, root, tid, id, icon, work))
                (*downloaded)++;
            end[1] = saved;
            p = strchr(end + 1, '{');
        }
    }
    free(list);
    free(work);
    return 0;
}
