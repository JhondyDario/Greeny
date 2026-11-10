#include "account.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESP_MAX 4096
#define PUSH_MAX (96 * 1024)

/* ---------- JSON minimo ----------
 * Las respuestas que nos interesan son objetos planos y chicos, asi que basta
 * con buscar "clave": y leer el valor que sigue. */

size_t gr_json_escape(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p; p++) {
        char esc[8];
        size_t len;
        if (*p == '"' || *p == '\\') { esc[0] = '\\'; esc[1] = (char)*p; len = 2; }
        else if (*p < 0x20) len = (size_t)snprintf(esc, sizeof(esc), "\\u%04x", *p);
        else { esc[0] = (char)*p; len = 1; }
        if (n + len >= cap) break;
        memcpy(dst + n, esc, len);
        n += len;
    }
    if (cap) dst[n] = 0;
    return n;
}

/* apunta al primer caracter del valor de `key`, o NULL */
static const char *find_value(const char *json, const char *key) {
    char pat[48];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    size_t plen = strlen(pat);
    for (const char *p = json; (p = strstr(p, pat)); p += plen) {
        const char *q = p + plen;
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
        if (*q != ':') continue; /* era un valor que casualmente se llama igual */
        q++;
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
        return q;
    }
    return NULL;
}

static void utf8_put(char *out, size_t cap, size_t *n, unsigned cp) {
    char b[4];
    int len;
    if (cp < 0x80) { b[0] = (char)cp; len = 1; }
    else if (cp < 0x800) { b[0] = (char)(0xC0 | (cp >> 6)); b[1] = (char)(0x80 | (cp & 0x3F)); len = 2; }
    else { b[0] = (char)(0xE0 | (cp >> 12)); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); b[2] = (char)(0x80 | (cp & 0x3F)); len = 3; }
    if (*n + (size_t)len < cap) { memcpy(out + *n, b, (size_t)len); *n += (size_t)len; }
}

bool gr_json_string(const char *json, const char *key, char *out, size_t cap) {
    if (cap) out[0] = 0;
    const char *p = find_value(json, key);
    if (!p || *p != '"') return false;
    size_t n = 0;
    for (p++; *p && *p != '"'; p++) {
        if (*p != '\\') { if (n + 1 < cap) out[n++] = *p; continue; }
        p++;
        if (!*p) break;
        switch (*p) {
            case 'n': utf8_put(out, cap, &n, '\n'); break;
            case 't': utf8_put(out, cap, &n, '\t'); break;
            case 'r': case 'b': case 'f': break;
            case 'u': {
                unsigned cp = 0;
                int i = 0;
                for (; i < 4 && p[1 + i]; i++) {
                    char c = p[1 + i];
                    unsigned d = c >= '0' && c <= '9' ? (unsigned)(c - '0') : c >= 'a' && c <= 'f' ? (unsigned)(c - 'a' + 10)
                               : c >= 'A' && c <= 'F' ? (unsigned)(c - 'A' + 10) : 16;
                    if (d > 15) break;
                    cp = cp * 16 + d;
                }
                p += i;
                utf8_put(out, cap, &n, i == 4 ? cp : '?');
                break;
            }
            default: if (n + 1 < cap) out[n++] = *p; break; /* \" \\ \/ */
        }
    }
    if (cap) out[n] = 0;
    return *p == '"';
}

bool gr_json_bool(const char *json, const char *key, bool *out) {
    const char *p = find_value(json, key);
    if (!p) return false;
    if (!strncmp(p, "true", 4)) { *out = true; return true; }
    if (!strncmp(p, "false", 5)) { *out = false; return true; }
    return false;
}

/* Mensaje de error del servidor, traducido cuando es uno de los que el usuario puede ver. */
static void server_error(const char *resp, char *err, size_t errlen) {
    char msg[160];
    if (!gr_json_string(resp, "message", msg, sizeof(msg)) && !gr_json_string(resp, "msg", msg, sizeof(msg)) &&
        !gr_json_string(resp, "error_description", msg, sizeof(msg)))
        snprintf(msg, sizeof(msg), "el servidor rechazo la solicitud");
    if (strstr(msg, "Invalid login credentials")) snprintf(err, errlen, "Correo o contraseña incorrectos.");
    else if (strstr(msg, "Email not confirmed")) snprintf(err, errlen, "Falta confirmar el correo de la cuenta.");
    else if (strstr(msg, "consola no vinculada")) snprintf(err, errlen, "Esta consola ya no está vinculada.");
    else snprintf(err, errlen, "%s", msg);
}

static gr_net_result call(gr_http_fn http, const char *path, const char *bearer, const char *body, char *resp, size_t cap,
                          char *err, size_t errlen) {
    char url[256];
    long status = 0;
    snprintf(url, sizeof(url), GR_BACKEND_URL "%s", path);
    resp[0] = 0;
    if (http(url, bearer, body, resp, cap, &status) != 0) {
        if (err) snprintf(err, errlen, "No hay conexión.");
        return GR_NET_OFFLINE;
    }
    if (status >= 200 && status < 300) return GR_NET_OK;
    if (err) server_error(resp, err, errlen);
    return GR_NET_REJECTED;
}

/* ---------- archivo de la cuenta ---------- */

void gr_account_load(gr_account *a, const char *path) {
    memset(a, 0, sizeof(*a));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[160];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *v = eq + 1;
        if (!strcmp(line, "token")) snprintf(a->token, sizeof(a->token), "%s", v);
        else if (!strcmp(line, "linked")) a->linked = v[0] == '1';
        else if (!strcmp(line, "username")) snprintf(a->username, sizeof(a->username), "%s", v);
        else if (!strcmp(line, "display")) snprintf(a->display, sizeof(a->display), "%s", v);
        else if (!strcmp(line, "caption")) snprintf(a->caption, sizeof(a->caption), "%s", v);
    }
    fclose(f);
    if (!a->token[0]) a->linked = false;
}

int gr_account_save(const gr_account *a, const char *path) {
    if (!a->token[0]) return remove(path) == 0 ? 0 : 0; /* sin llave no hay nada que guardar */
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    int ok = fprintf(f, "token=%s\nlinked=%d\nusername=%s\ndisplay=%s\ncaption=%s\n", a->token, a->linked ? 1 : 0, a->username,
                     a->display, a->caption) > 0;
    if (fclose(f) != 0) ok = 0;
    return ok ? 0 : -1;
}

/* ---------- llamadas ---------- */

gr_net_result gr_link_start(gr_http_fn http, const char *device_name, gr_account *a, char code[8], char *err, size_t errlen) {
    char name[80], body[128], resp[RESP_MAX];
    gr_json_escape(name, sizeof(name), device_name);
    snprintf(body, sizeof(body), "{\"device_name\":\"%s\"}", name);
    gr_net_result r = call(http, "/rest/v1/rpc/device_link_start", NULL, body, resp, sizeof(resp), err, errlen);
    if (r != GR_NET_OK) return r;

    char token[80], c[16];
    if (!gr_json_string(resp, "device_token", token, sizeof(token)) || strlen(token) < 32 ||
        !gr_json_string(resp, "link_code", c, sizeof(c)) || strlen(c) != 6) {
        snprintf(err, errlen, "Respuesta inesperada del servidor.");
        return GR_NET_BAD;
    }
    memset(a, 0, sizeof(*a));
    snprintf(a->token, sizeof(a->token), "%s", token);
    memcpy(code, c, 7);
    return GR_NET_OK;
}

gr_net_result gr_whoami(gr_http_fn http, gr_account *a, bool *exists) {
    char body[128], resp[RESP_MAX];
    *exists = false;
    if (!a->token[0]) return GR_NET_OK;
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\"}", a->token);
    gr_net_result r = call(http, "/rest/v1/rpc/device_whoami", NULL, body, resp, sizeof(resp), NULL, 0);
    if (r != GR_NET_OK) return r;

    bool linked;
    if (!gr_json_bool(resp, "linked", &linked)) return GR_NET_OK; /* lista vacia: la llave no existe */
    *exists = true;
    a->linked = linked;
    if (linked) {
        gr_json_string(resp, "username", a->username, sizeof(a->username));
        gr_json_string(resp, "display_name", a->display, sizeof(a->display));
        gr_json_string(resp, "caption", a->caption, sizeof(a->caption));
    }
    return GR_NET_OK;
}

gr_net_result gr_login_password(gr_http_fn http, const char *email, const char *password, const char *device_name,
                                gr_account *a, char *err, size_t errlen) {
    char e[160], p[200], name[80], body[512];
    char *resp = malloc(16 * 1024); /* la respuesta de inicio de sesion trae el usuario entero */
    if (!resp) { snprintf(err, errlen, "Sin memoria."); return GR_NET_BAD; }

    gr_json_escape(e, sizeof(e), email);
    gr_json_escape(p, sizeof(p), password);
    snprintf(body, sizeof(body), "{\"email\":\"%s\",\"password\":\"%s\"}", e, p);
    gr_net_result r = call(http, "/auth/v1/token?grant_type=password", NULL, body, resp, 16 * 1024, err, errlen);
    memset(body, 0, sizeof(body)); /* la contraseña no se queda en memoria mas de lo necesario */
    memset(p, 0, sizeof(p));

    char *access = NULL;
    if (r == GR_NET_OK) {
        access = malloc(4096);
        if (!access || !gr_json_string(resp, "access_token", access, 4096) || strlen(access) < 20) {
            snprintf(err, errlen, "Respuesta inesperada del servidor.");
            r = GR_NET_BAD;
        }
    }
    if (r == GR_NET_OK) {
        gr_json_escape(name, sizeof(name), device_name);
        snprintf(body, sizeof(body), "{\"device_name\":\"%s\"}", name);
        r = call(http, "/rest/v1/rpc/device_register", access, body, resp, 16 * 1024, err, errlen);
    }
    if (r == GR_NET_OK) {
        /* la funcion devuelve la llave como un texto JSON suelto: "abc..." */
        size_t len = strlen(resp);
        if (len >= 34 && len < sizeof(a->token) + 2 && resp[0] == '"' && resp[len - 1] == '"') {
            memset(a, 0, sizeof(*a));
            memcpy(a->token, resp + 1, len - 2);
            bool exists;
            gr_whoami(http, a, &exists); /* para traer el nombre; si falla, ya se sabra en la proxima */
            if (!a->linked) a->linked = true;
        } else {
            snprintf(err, errlen, "Respuesta inesperada del servidor.");
            r = GR_NET_BAD;
        }
    }
    if (access) memset(access, 0, 4096);
    free(access);
    free(resp);
    return r;
}

static bool valid_title_file(const char *name) {
    if (strlen(name) != 20 || strcmp(name + 16, ".txt")) return false;
    for (int i = 0; i < 16; i++) {
        char c = name[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

static bool valid_ach_id(const char *s) {
    size_t n = strlen(s);
    if (n == 0 || n > 31) return false;
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_' || *s == '-')) return false;
    return true;
}

gr_net_result gr_push_unlocks(gr_http_fn http, const gr_account *a, const char *root, int *sent, int *added,
                              char *err, size_t errlen) {
    *sent = *added = 0;
    if (!a->token[0]) { snprintf(err, errlen, "Esta consola no está vinculada."); return GR_NET_REJECTED; }

    char *body = malloc(PUSH_MAX);
    if (!body) { snprintf(err, errlen, "Sin memoria."); return GR_NET_BAD; }
    size_t n = (size_t)snprintf(body, PUSH_MAX, "{\"device_token\":\"%s\",\"items\":[", a->token);

    char dir[192];
    snprintf(dir, sizeof(dir), "%s/unlocked", root);
    DIR *d = opendir(dir);
    struct dirent *de;
    while (d && (de = readdir(d))) {
        if (!valid_title_file(de->d_name)) continue;
        char path[512], line[96];
        snprintf(path, sizeof(path), "%s/%.20s", dir, de->d_name); /* el nombre ya se valido: 20 caracteres */
        FILE *f = fopen(path, "r");
        if (!f) continue;
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            char *bar = strchr(line, '|');
            unsigned long long ts = 0;
            if (bar) { *bar = 0; ts = strtoull(bar + 1, NULL, 10); }
            if (!valid_ach_id(line)) continue;
            if (n + 128 >= PUSH_MAX) break; /* lo que no quepa se sube en la proxima */
            n += (size_t)snprintf(body + n, PUSH_MAX - n, "%s{\"title_id\":\"%.16s\",\"ach_id\":\"%s\",\"unlocked_at\":%llu}",
                                  *sent ? "," : "", de->d_name, line, ts);
            (*sent)++;
        }
        fclose(f);
    }
    if (d) closedir(d);
    snprintf(body + n, PUSH_MAX - n, "]}");

    gr_net_result r = GR_NET_OK;
    if (*sent > 0) {
        char resp[RESP_MAX];
        r = call(http, "/rest/v1/rpc/device_push_unlocks", NULL, body, resp, sizeof(resp), err, errlen);
        if (r == GR_NET_OK) *added = atoi(resp); /* la funcion devuelve un numero suelto */
    }
    free(body);
    return r;
}
