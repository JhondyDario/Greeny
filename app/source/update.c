#include "update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "account.h" /* gr_json_string */

static void parse_ver(const char *s, long v[3]) {
    v[0] = v[1] = v[2] = 0;
    while (*s == 'v' || *s == 'V' || *s == ' ') s++;
    for (int i = 0; i < 3 && *s; i++) {
        v[i] = strtol(s, (char **)&s, 10);
        if (*s != '.') break;
        s++;
    }
}

bool gr_version_newer(const char *latest, const char *current) {
    long a[3], b[3];
    parse_ver(latest, a);
    parse_ver(current, b);
    for (int i = 0; i < 3; i++)
        if (a[i] != b[i]) return a[i] > b[i];
    return false;
}

bool gr_parse_release(const char *json, gr_release *out) {
    memset(out, 0, sizeof(*out));
    char tag[32] = "";
    if (!gr_json_string(json, "tag_name", tag, sizeof(tag)) || !tag[0]) return false;
    const char *t = tag;
    while (*t == 'v' || *t == 'V') t++;
    snprintf(out->version, sizeof(out->version), "%.23s", t);
    gr_json_string(json, "body", out->notes, sizeof(out->notes));
    /* entre los archivos de la release, el que se llama greeny.nro */
    const char *assets = strstr(json, "\"assets\"");
    for (const char *p = assets; p && (p = strstr(p, "\"name\"")); p += 6) {
        const char *q = p + 6;
        while (*q == ' ' || *q == ':') q++;
        if (strncmp(q, "\"greeny.nro\"", 12) != 0) continue;
        /* el enlace y el tamaño estan en el mismo objeto: hasta la siguiente llave de cierre */
        const char *start = p, *end = strchr(p, '}');
        while (start > assets && *start != '{') start--;
        if (!end) break;
        size_t n = (size_t)(end - start + 1);
        char *obj = malloc(n + 1);
        if (!obj) return false;
        memcpy(obj, start, n);
        obj[n] = 0;
        gr_json_string(obj, "browser_download_url", out->nro_url, sizeof(out->nro_url));
        int size = 0;
        if (gr_json_int(obj, "size", &size)) out->size = size;
        free(obj);
        break;
    }
    return out->nro_url[0] && !strncmp(out->nro_url, "https://github.com/", 19);
}

bool gr_nro_valid(const unsigned char *head, size_t head_len, long file_size) {
    return head_len >= 0x14 && !memcmp(head + 0x10, "NRO0", 4) && file_size > 512 * 1024 && file_size < 64L * 1024 * 1024;
}
