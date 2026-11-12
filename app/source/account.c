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

bool gr_json_int(const char *json, const char *key, int *out) {
    const char *p = find_value(json, key);
    if (!p || !((*p >= '0' && *p <= '9') || *p == '-')) return false;
    *out = atoi(p);
    return true;
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
    else if (strstr(msg, "no hay nadie con ese codigo")) snprintf(err, errlen, "No hay nadie con ese código de amigo.");
    else if (strstr(msg, "6 numeros")) snprintf(err, errlen, "El código de amigo tiene 6 números.");
    else if (strstr(msg, "tu propio codigo")) snprintf(err, errlen, "Ese es tu propio código.");
    else if (strstr(msg, "Could not find the function")) snprintf(err, errlen, "El servidor todavía no tiene esta función.");
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
        else if (!strcmp(line, "followers")) { a->followers = atoi(v); a->social_known = true; }
        else if (!strcmp(line, "following")) a->following = atoi(v);
        else if (!strcmp(line, "friend_code")) snprintf(a->friend_code, sizeof(a->friend_code), "%s", v);
        else if (!strcmp(line, "joined")) snprintf(a->joined, sizeof(a->joined), "%.10s", v);
        else if (!strcmp(line, "likes")) a->likes = atoi(v);
        else if (!strcmp(line, "avatar")) a->avatar = atoi(v);
        else if (!strcmp(line, "admin")) a->admin = v[0] == '1';
        else if (!strcmp(line, "donor")) a->donor = v[0] == '1';
        else if (!strcmp(line, "country")) snprintf(a->country, sizeof(a->country), "%.2s", v);
        else if (!strcmp(line, "caption_en")) snprintf(a->caption_en, sizeof(a->caption_en), "%s", v);
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
    if (ok && a->social_known) ok = fprintf(f, "followers=%d\nfollowing=%d\n", a->followers, a->following) > 0;
    if (ok && a->friend_code[0]) ok = fprintf(f, "friend_code=%s\n", a->friend_code) > 0;
    if (ok && a->joined[0]) ok = fprintf(f, "joined=%s\n", a->joined) > 0;
    if (ok && a->likes > 0) ok = fprintf(f, "likes=%d\n", a->likes) > 0;
    if (ok && a->avatar > 0) ok = fprintf(f, "avatar=%d\n", a->avatar) > 0;
    if (ok && a->admin) ok = fprintf(f, "admin=1\n") > 0;
    if (ok && a->donor) ok = fprintf(f, "donor=1\n") > 0;
    if (ok && a->country[0]) ok = fprintf(f, "country=%s\n", a->country) > 0;
    if (ok && a->caption_en[0]) ok = fprintf(f, "caption_en=%s\n", a->caption_en) > 0;
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

gr_net_result gr_fetch_social(gr_http_fn http, gr_account *a) {
    char body[128], resp[RESP_MAX];
    if (!a->token[0] || !a->linked) return GR_NET_OK;
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\"}", a->token);
    gr_net_result r = call(http, "/rest/v1/rpc/device_profile", NULL, body, resp, sizeof(resp), NULL, 0);
    if (r != GR_NET_OK) return r;
    int followers, following;
    if (gr_json_int(resp, "followers", &followers) && gr_json_int(resp, "following", &following)) {
        a->followers = followers;
        a->following = following;
        a->social_known = true;
    }
    gr_json_string(resp, "friend_code", a->friend_code, sizeof(a->friend_code));
    char created[40] = "";
    if (gr_json_string(resp, "created_at", created, sizeof(created)) && strlen(created) >= 10) snprintf(a->joined, sizeof(a->joined), "%.10s", created);
    int avatar, likes;
    if (gr_json_int(resp, "avatar", &avatar) && avatar > 0) a->avatar = avatar;
    if (gr_json_int(resp, "likes", &likes) && likes >= 0) a->likes = likes;
    bool admin;
    if (gr_json_bool(resp, "admin", &admin)) a->admin = admin;
    bool donor;
    if (gr_json_bool(resp, "donor", &donor)) a->donor = donor;
    char country[8] = "";
    gr_json_string(resp, "country", country, sizeof(country)); /* null = sin pais */
    snprintf(a->country, sizeof(a->country), "%.2s", country);
    a->caption_en[0] = 0;
    gr_json_string(resp, "caption_en", a->caption_en, sizeof(a->caption_en));
    return GR_NET_OK;
}

gr_net_result gr_set_country(gr_http_fn http, gr_account *a, const char *code, char *err, size_t errlen) {
    char body[160], resp[RESP_MAX];
    if (!a->token[0] || !a->linked) { snprintf(err, errlen, "Esta consola no está vinculada."); return GR_NET_REJECTED; }
    if (strlen(code) != 2 && code[0]) { snprintf(err, errlen, "País inválido."); return GR_NET_REJECTED; }
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"code\":\"%.2s\"}", a->token, code);
    gr_net_result r = call(http, "/rest/v1/rpc/device_set_country", NULL, body, resp, sizeof(resp), err, errlen);
    if (r == GR_NET_OK) snprintf(a->country, sizeof(a->country), "%.2s", code);
    return r;
}

gr_net_result gr_set_caption(gr_http_fn http, gr_account *a, const char *text, char *err, size_t errlen) {
    char t[160], body[320], resp[RESP_MAX];
    if (!a->token[0] || !a->linked) { snprintf(err, errlen, "Esta consola no está vinculada."); return GR_NET_REJECTED; }
    gr_json_escape(t, sizeof(t), text);
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"caption\":\"%s\"}", a->token, t);
    gr_net_result r = call(http, "/rest/v1/rpc/device_set_caption", NULL, body, resp, sizeof(resp), err, errlen);
    if (r == GR_NET_OK) snprintf(a->caption, sizeof(a->caption), "%s", text);
    return r;
}

int gr_parse_captions(char *json, gr_caption *out, int max) {
    int n = 0;
    for (char *p = strchr(json, '{'); p && n < max; ) {
        char *end = strchr(p, '}');
        if (!end) break;
        char saved = end[1];
        end[1] = 0;
        memset(&out[n], 0, sizeof(out[n]));
        if (gr_json_string(p, "text_es", out[n].es, sizeof(out[n].es)) && out[n].es[0]) {
            if (!gr_json_string(p, "text_en", out[n].en, sizeof(out[n].en)) || !out[n].en[0]) memcpy(out[n].en, out[n].es, sizeof(out[n].en));
            n++;
        }
        end[1] = saved;
        p = strchr(end + 1, '{');
    }
    return n;
}

static void rank_row(const char *obj, gr_rank_row *r) {
    memset(r, 0, sizeof(*r));
    gr_json_int(obj, "position", &r->position);
    gr_json_string(obj, "username", r->username, sizeof(r->username));
    gr_json_string(obj, "display_name", r->display, sizeof(r->display));
    gr_json_int(obj, "avatar", &r->avatar);
    char c[8] = "";
    gr_json_string(obj, "country", c, sizeof(c));
    snprintf(r->country, sizeof(r->country), "%.2s", c);
    gr_json_int(obj, "points", &r->points);
    gr_json_int(obj, "total_points", &r->total_points);
    gr_json_int(obj, "achievements", &r->achievements);
}

gr_net_result gr_ranking(gr_http_fn http, const gr_account *a, const char *scope, const char *country, const char *title,
                         gr_rank_row *top, int *ntop, gr_rank_row *me, int *total) {
    *ntop = 0;
    *total = 0;
    memset(me, 0, sizeof(*me));
    char body[320], q1[8], q2[40];
    char *resp = malloc(16 * 1024);
    if (!resp) return GR_NET_BAD;
    snprintf(q1, sizeof(q1), "%.2s", country ? country : "");
    snprintf(q2, sizeof(q2), "%.16s", title ? title : "");
    int n = snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"scope\":\"%s\"", a->token,
                     !strcmp(scope, "country") ? "country" : !strcmp(scope, "friends") ? "friends" : "global");
    if (q1[0]) n += snprintf(body + n, sizeof(body) - (size_t)n, ",\"country\":\"%s\"", q1);
    if (q2[0]) n += snprintf(body + n, sizeof(body) - (size_t)n, ",\"title\":\"%s\"", q2);
    snprintf(body + n, sizeof(body) - (size_t)n, "}");
    gr_net_result r = call(http, "/rest/v1/rpc/device_ranking", NULL, body, resp, 16 * 1024, NULL, 0);
    if (r == GR_NET_OK) {
        gr_json_int(resp, "total", total);
        /* "me" es un objeto o null */
        char *m = strstr(resp, "\"me\":");
        if (m && (m = strchr(m, ':'))) {
            m++;
            while (*m == ' ') m++; /* Postgres deja un espacio despues de los dos puntos */
            char *end = *m == '{' ? strchr(m, '}') : NULL;
            if (end) { char saved = end[1]; end[1] = 0; rank_row(m, me); end[1] = saved; }
        }
        char *t = strstr(resp, "\"top\":");
        char *stop = t ? strchr(t, ']') : NULL;
        for (char *p = t ? strchr(t, '{') : NULL; p && stop && p < stop && *ntop < GR_RANK_TOP; ) {
            char *end = strchr(p, '}');
            if (!end) break;
            char saved = end[1];
            end[1] = 0;
            rank_row(p, &top[*ntop]);
            if (top[*ntop].username[0]) (*ntop)++;
            end[1] = saved;
            p = strchr(end + 1, '{');
        }
    }
    free(resp);
    return r;
}

gr_net_result gr_follow_user(gr_http_fn http, const gr_account *a, const char *username, char *err, size_t errlen) {
    char u[64], body[256], resp[RESP_MAX];
    if (!a->token[0] || !a->linked) { snprintf(err, errlen, "Esta consola no está vinculada."); return GR_NET_REJECTED; }
    gr_json_escape(u, sizeof(u), username);
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"who\":\"%s\"}", a->token, u);
    return call(http, "/rest/v1/rpc/device_follow_user", NULL, body, resp, sizeof(resp), err, errlen);
}

/* Fin del objeto JSON que empieza en `p` ('{'), sin hacer caso a llaves dentro de textos. */
static char *object_end(char *p) {
    int depth = 0;
    bool str = false;
    for (; *p; p++) {
        if (str) {
            if (*p == '\\' && p[1]) p++;
            else if (*p == '"') str = false;
        } else if (*p == '"') str = true;
        else if (*p == '{') depth++;
        else if (*p == '}' && --depth == 0) return p;
    }
    return NULL;
}

int gr_parse_reviews(char *json, gr_review *out, int max) {
    int n = 0;
    for (char *p = strchr(json, '{'); p && n < max; ) {
        char *end = object_end(p);
        if (!end) break;
        char saved = end[1];
        end[1] = 0;
        gr_review *r = &out[n];
        memset(r, 0, sizeof(*r));
        /* el texto primero; luego se tapa con espacios */
        char *b = strstr(p, "\"body\":");
        if (b && gr_json_string(p, "body", r->body, sizeof(r->body))) {
            char *v = strchr(b + 6, '"');
            if (v) for (v++; *v && *v != '"'; v++) { if (*v == '\\' && v[1]) *v++ = ' '; *v = ' '; }
        }
        const char *idv = find_value(p, "id");
        if (idv) r->id = atoll(idv);
        gr_json_string(p, "username", r->username, sizeof(r->username));
        gr_json_string(p, "display_name", r->display, sizeof(r->display));
        gr_json_int(p, "avatar", &r->avatar);
        char c[8] = "";
        gr_json_string(p, "country", c, sizeof(c));
        snprintf(r->country, sizeof(r->country), "%.2s", c);
        gr_json_bool(p, "donor", &r->donor);
        gr_json_int(p, "total_points", &r->total_points);
        gr_json_int(p, "achievements", &r->achievements);
        gr_json_int(p, "likes", &r->likes);
        gr_json_bool(p, "liked", &r->liked);
        gr_json_bool(p, "mine", &r->mine);
        gr_json_bool(p, "friend", &r->friend_);
        char d[40] = "";
        gr_json_string(p, "created_at", d, sizeof(d));
        snprintf(r->date, sizeof(r->date), "%.10s", d);
        if (r->id > 0 && r->username[0] && r->body[0]) n++;
        end[1] = saved;
        p = strchr(end + 1, '{');
    }
    return n;
}

gr_net_result gr_reviews(gr_http_fn http, const gr_account *a, const char *tid, gr_review *out, int max, int *count) {
    char body[200];
    *count = 0;
    char *resp = malloc(64 * 1024);
    if (!resp) return GR_NET_BAD;
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"title\":\"%.16s\"}", a->token, tid);
    gr_net_result r = call(http, "/rest/v1/rpc/device_reviews", NULL, body, resp, 64 * 1024, NULL, 0);
    if (r == GR_NET_OK) *count = gr_parse_reviews(resp, out, max);
    free(resp);
    return r;
}

gr_net_result gr_review_save(gr_http_fn http, const gr_account *a, const char *tid, const char *text, char *err, size_t errlen) {
    char t[1600], body[1800], resp[RESP_MAX];
    if (!a->token[0] || !a->linked) { snprintf(err, errlen, "Esta consola no está vinculada."); return GR_NET_REJECTED; }
    gr_json_escape(t, sizeof(t), text);
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"title\":\"%.16s\",\"body\":\"%s\"}", a->token, tid, t);
    return call(http, "/rest/v1/rpc/device_review_save", NULL, body, resp, sizeof(resp), err, errlen);
}

gr_net_result gr_review_like(gr_http_fn http, const gr_account *a, long long id, bool liked, char *err, size_t errlen) {
    char body[200], resp[RESP_MAX];
    if (!a->token[0] || !a->linked) { snprintf(err, errlen, "Esta consola no está vinculada."); return GR_NET_REJECTED; }
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"review\":%lld,\"liked\":%s}", a->token, id, liked ? "true" : "false");
    return call(http, "/rest/v1/rpc/device_review_like", NULL, body, resp, sizeof(resp), err, errlen);
}

int gr_parse_notifs(char *json, gr_notif *out, int max, int *unread) {
    int n = 0;
    *unread = 0;
    gr_json_int(json, "unread", unread);
    char *items = strstr(json, "\"items\"");
    for (char *p = items ? strchr(items, '{') : NULL; p && n < max; ) {
        char *end = object_end(p);
        if (!end) break;
        char saved = end[1];
        end[1] = 0;
        gr_notif *r = &out[n];
        memset(r, 0, sizeof(*r));
        const char *idv = find_value(p, "id");
        if (idv) r->id = atoll(idv);
        char kind[16] = "";
        gr_json_string(p, "kind", kind, sizeof(kind));
        r->kind = !strcmp(kind, "like") ? GR_NOTIF_LIKE : !strcmp(kind, "follow") ? GR_NOTIF_FOLLOW
                : !strcmp(kind, "achievement") ? GR_NOTIF_ACH : !strcmp(kind, "badge") ? GR_NOTIF_BADGE : GR_NOTIF_OTHER;
        gr_json_bool(p, "read", &r->read);
        gr_json_string(p, "actor", r->actor, sizeof(r->actor));
        gr_json_string(p, "actor_name", r->actor_name, sizeof(r->actor_name));
        gr_json_int(p, "actor_avatar", &r->actor_avatar);
        gr_json_string(p, "title_id", r->title_id, sizeof(r->title_id));
        gr_json_string(p, "game", r->game, sizeof(r->game));
        gr_json_string(p, "ach_name", r->ach_name, sizeof(r->ach_name));
        gr_json_string(p, "ach_name_en", r->ach_name_en, sizeof(r->ach_name_en));
        gr_json_string(p, "badge", r->badge, sizeof(r->badge));
        char d[40] = "";
        gr_json_string(p, "created_at", d, sizeof(d));
        snprintf(r->date, sizeof(r->date), "%.10s", d);
        if (r->id > 0 && r->kind != GR_NOTIF_OTHER) n++;
        end[1] = saved;
        p = strchr(end + 1, '{');
    }
    return n;
}

gr_net_result gr_notifications(gr_http_fn http, const gr_account *a, gr_notif *out, int max, int *count, int *unread) {
    char body[160];
    *count = 0;
    *unread = 0;
    if (!a->token[0] || !a->linked) return GR_NET_OK;
    char *resp = malloc(48 * 1024);
    if (!resp) return GR_NET_BAD;
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\"}", a->token);
    gr_net_result r = call(http, "/rest/v1/rpc/device_notifications", NULL, body, resp, 48 * 1024, NULL, 0);
    if (r == GR_NET_OK) *count = gr_parse_notifs(resp, out, max, unread);
    free(resp);
    return r;
}

gr_net_result gr_notifications_read(gr_http_fn http, const gr_account *a, long long id) {
    char body[200], resp[RESP_MAX];
    if (!a->token[0] || !a->linked) return GR_NET_REJECTED;
    if (id < 0) snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"id\":null}", a->token);
    else snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"id\":%lld}", a->token, id);
    return call(http, "/rest/v1/rpc/device_notifications_read", NULL, body, resp, sizeof(resp), NULL, 0);
}

gr_net_result gr_set_avatar(gr_http_fn http, gr_account *a, int picture, char *err, size_t errlen) {
    char body[160], resp[RESP_MAX];
    if (!a->token[0] || !a->linked) { snprintf(err, errlen, "Esta consola no está vinculada."); return GR_NET_REJECTED; }
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"picture\":%d}", a->token, picture);
    gr_net_result r = call(http, "/rest/v1/rpc/device_set_avatar", NULL, body, resp, sizeof(resp), err, errlen);
    if (r == GR_NET_OK) a->avatar = picture;
    return r;
}

gr_net_result gr_follow_code(gr_http_fn http, const gr_account *a, const char *code, char *who, size_t wholen,
                             char *err, size_t errlen) {
    char c[64], body[256], resp[RESP_MAX];
    who[0] = 0;
    if (!a->token[0] || !a->linked) { snprintf(err, errlen, "Esta consola no está vinculada."); return GR_NET_REJECTED; }
    gr_json_escape(c, sizeof(c), code);
    snprintf(body, sizeof(body), "{\"device_token\":\"%s\",\"code\":\"%s\"}", a->token, c);
    gr_net_result r = call(http, "/rest/v1/rpc/device_follow_by_code", NULL, body, resp, sizeof(resp), err, errlen);
    if (r != GR_NET_OK) return r;
    /* la funcion devuelve el usuario como un texto JSON suelto: "nombre" */
    size_t len = strlen(resp);
    if (len >= 3 && resp[0] == '"' && resp[len - 1] == '"') snprintf(who, wholen, "%.*s", (int)(len - 2), resp + 1);
    return GR_NET_OK;
}

gr_net_result gr_login_password(gr_http_fn http, const char *email, const char *password, const char *device_name,
                                gr_account *a, char *err, size_t errlen) {
    /* No pasa por Supabase Auth (alli la web pide captcha): device_login_password comprueba
     * la contraseña en la base de datos, frena los intentos y devuelve la llave de la consola. */
    char e[160], p[200], name[80], body[600], resp[RESP_MAX], token[sizeof(a->token)];
    gr_json_escape(e, sizeof(e), email);
    gr_json_escape(p, sizeof(p), password);
    gr_json_escape(name, sizeof(name), device_name);
    snprintf(body, sizeof(body), "{\"email\":\"%s\",\"password\":\"%s\",\"device_name\":\"%s\"}", e, p, name);
    gr_net_result r = call(http, "/rest/v1/rpc/device_login_password", NULL, body, resp, sizeof(resp), err, errlen);
    memset(body, 0, sizeof(body)); /* la contraseña no se queda en memoria mas de lo necesario */
    memset(p, 0, sizeof(p));
    if (r != GR_NET_OK) return r;
    if (gr_json_string(resp, "error", err, errlen) && err[0]) return GR_NET_REJECTED;
    if (!gr_json_string(resp, "token", token, sizeof(token)) || strlen(token) < 32) {
        snprintf(err, errlen, "Respuesta inesperada del servidor.");
        return GR_NET_BAD;
    }
    memset(a, 0, sizeof(*a));
    memcpy(a->token, token, sizeof(a->token));
    bool exists;
    gr_whoami(http, a, &exists); /* para traer el nombre; si falla, ya se sabra en la proxima */
    if (!a->linked) a->linked = true;
    return GR_NET_OK;
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
