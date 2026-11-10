#include "net.h"

#include <stdio.h>
#include <string.h>

#include <curl/curl.h>
#include <switch.h>

#include "account.h"
#include "ui.h"

static bool g_sockets, g_nifm;
static char g_error[CURL_ERROR_SIZE + 32];

void net_init(void) {
    g_sockets = R_SUCCEEDED(socketInitializeDefault());
    g_nifm = R_SUCCEEDED(nifmInitialize(NifmServiceType_User));
    if (g_sockets) curl_global_init(CURL_GLOBAL_DEFAULT);
}

void net_exit(void) {
    if (g_sockets) { curl_global_cleanup(); socketExit(); }
    if (g_nifm) nifmExit();
    g_sockets = g_nifm = false;
}

bool net_online(void) {
    if (!g_sockets || !g_nifm) return false;
    NifmInternetConnectionType type;
    u32 strength;
    NifmInternetConnectionStatus status;
    return R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &strength, &status)) &&
           status == NifmInternetConnectionStatus_Connected;
}

const char *net_last_error(void) { return g_error; }

typedef struct { char *buf; size_t cap, len; } sink;

static size_t on_data(char *data, size_t size, size_t n, void *user) {
    sink *s = user;
    size_t total = size * n, room = s->cap - 1 - s->len;
    size_t take = total < room ? total : room; /* lo que no quepa se descarta; las respuestas son chicas */
    memcpy(s->buf + s->len, data, take);
    s->len += take;
    return total;
}

int net_http(const char *url, const char *bearer, const char *json_body, char *out, size_t cap, long *status) {
    out[0] = 0;
    *status = 0;
    g_error[0] = 0;
    if (!net_online()) { snprintf(g_error, sizeof(g_error), "sin internet"); return -1; }

    CURL *c = curl_easy_init();
    if (!c) { snprintf(g_error, sizeof(g_error), "curl_easy_init fallo"); return -1; }

    char auth[4200];
    struct curl_slist *h = NULL;
    h = curl_slist_append(h, "apikey: " GR_BACKEND_KEY);
    h = curl_slist_append(h, "Content-Type: application/json");
    if (bearer) {
        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", bearer);
        h = curl_slist_append(h, auth);
    }

    char errbuf[CURL_ERROR_SIZE] = "";
    sink s = { out, cap, 0 };
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_data);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &s);
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "Greeny/" UI_APP_VERSION);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    /* La verificacion del certificado queda activada (valor por defecto): usa los certificados de la consola. */

    CURLcode rc = curl_easy_perform(c);
    out[s.len] = 0;
    if (rc == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, status);
    else snprintf(g_error, sizeof(g_error), "curl %d: %s", (int)rc, errbuf[0] ? errbuf : curl_easy_strerror(rc));

    memset(auth, 0, sizeof(auth));
    curl_slist_free_all(h);
    curl_easy_cleanup(c);
    return rc == CURLE_OK ? 0 : -1;
}

int net_get(const char *url, void *out, size_t cap, size_t *len, long *status) {
    *len = 0;
    *status = 0;
    if (!net_online()) return -1;

    CURL *c = curl_easy_init();
    if (!c) return -1;
    struct curl_slist *h = curl_slist_append(NULL, "apikey: " GR_BACKEND_KEY);
    /* on_data deja sitio para un 0 final que aqui no hace falta: se le da un byte de mas de margen */
    sink s = { out, cap, 0 };
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_data);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &s);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "Greeny/" UI_APP_VERSION);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 25L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);

    CURLcode rc = curl_easy_perform(c);
    if (rc == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, status);
    *len = s.len;
    curl_slist_free_all(h);
    curl_easy_cleanup(c);
    return rc == CURLE_OK ? 0 : -1;
}
