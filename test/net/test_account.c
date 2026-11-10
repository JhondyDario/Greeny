/* Pruebas de la cuenta. Sin argumentos: solo lo que no necesita red.
 *   ./test_account --red     ademas habla con el Supabase real usando el programa curl.
 * Con --red se crea una vinculacion de prueba sin reclamar; el servidor la borra sola a los 10 minutos. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../app/source/account.h"

static int g_fail, g_total;
#define CHECK(x) do { g_total++; if (!(x)) { g_fail++; printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

/* transporte de prueba: el programa curl */
static int http_curl(const char *url, const char *bearer, const char *body, char *out, size_t cap, long *status) {
    char bodyfile[] = "/tmp/greeny_body_XXXXXX", outfile[] = "/tmp/greeny_out_XXXXXX";
    int fd = mkstemp(bodyfile);
    if (fd < 0) return -1;
    if (write(fd, body, strlen(body)) < 0) { close(fd); return -1; }
    close(fd);
    close(mkstemp(outfile));

    char cmd[8192];
    int n = snprintf(cmd, sizeof(cmd), "curl -sS -m 25 -o '%s' -w '%%{http_code}' -X POST '%s' -H 'apikey: %s' -H 'Content-Type: application/json'",
                     outfile, url, GR_BACKEND_KEY);
    if (bearer) n += snprintf(cmd + n, sizeof(cmd) - (size_t)n, " -H 'Authorization: Bearer %s'", bearer);
    snprintf(cmd + n, sizeof(cmd) - (size_t)n, " --data-binary @'%s' 2>/dev/null", bodyfile);

    FILE *p = popen(cmd, "r");
    char code[16] = "";
    if (p) { if (!fgets(code, sizeof(code), p)) code[0] = 0; pclose(p); }
    *status = atol(code);

    FILE *f = fopen(outfile, "rb");
    size_t got = f ? fread(out, 1, cap - 1, f) : 0;
    if (f) fclose(f);
    out[got] = 0;
    remove(bodyfile);
    remove(outfile);
    return *status > 0 ? 0 : -1;
}

/* transportes falsos */
static char g_last_url[256], g_last_body[4096], g_last_bearer[64];
static const char *g_reply = "";
static long g_reply_status = 200;
static int http_fake(const char *url, const char *bearer, const char *body, char *out, size_t cap, long *status) {
    snprintf(g_last_url, sizeof(g_last_url), "%s", url);
    snprintf(g_last_body, sizeof(g_last_body), "%s", body);
    snprintf(g_last_bearer, sizeof(g_last_bearer), "%s", bearer ? bearer : "");
    snprintf(out, cap, "%s", g_reply);
    *status = g_reply_status;
    return 0;
}
static int http_down(const char *url, const char *bearer, const char *body, char *out, size_t cap, long *status) {
    (void)url; (void)bearer; (void)body; (void)cap; (void)status;
    out[0] = 0;
    return -1;
}

static void test_json(void) {
    char out[64];
    bool b = false;
    const char *j = "[{\"linked\":true,\"username\":\"megaJHONDY\",\"display_name\":null,\"caption\":\"Dice \\\"hola\\\" \\u00f1 \\\\ fin\"}]";
    CHECK(gr_json_bool(j, "linked", &b) && b);
    CHECK(gr_json_string(j, "username", out, sizeof(out)) && !strcmp(out, "megaJHONDY"));
    CHECK(!gr_json_string(j, "display_name", out, sizeof(out)) && out[0] == 0); /* null */
    CHECK(gr_json_string(j, "caption", out, sizeof(out)) && !strcmp(out, "Dice \"hola\" \xC3\xB1 \\ fin"));
    CHECK(!gr_json_string(j, "no_existe", out, sizeof(out)));
    CHECK(!gr_json_bool("[]", "linked", &b));
    /* un valor que se llama como la clave no confunde */
    CHECK(gr_json_string("{\"a\":\"username\",\"username\":\"x\"}", "username", out, sizeof(out)) && !strcmp(out, "x"));
    /* no desborda */
    char tiny[4];
    CHECK(gr_json_string(j, "username", tiny, sizeof(tiny)) && !strcmp(tiny, "meg"));

    char esc[64];
    gr_json_escape(esc, sizeof(esc), "a\"b\\c\nd");
    CHECK(!strcmp(esc, "a\\\"b\\\\c\\u000ad"));
    char small[6];
    gr_json_escape(small, sizeof(small), "\"\"\"\"\"\"");
    CHECK(strlen(small) <= 5 && strlen(small) % 2 == 0); /* nunca deja un escape a medias */
}

static void test_flows_fake(const char *tmp) {
    gr_account a;
    char code[8], err[160], path[256];
    bool exists;

    /* sin red */
    CHECK(gr_link_start(http_down, "Switch", &a, code, err, sizeof(err)) == GR_NET_OFFLINE);
    CHECK(!strcmp(err, "No hay conexión."));

    /* vincular */
    g_reply = "[{\"link_code\":\"JSH6PN\",\"device_token\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"expires_at\":\"x\"}]";
    g_reply_status = 200;
    CHECK(gr_link_start(http_fake, "Switch de \"J\"", &a, code, err, sizeof(err)) == GR_NET_OK);
    CHECK(!strcmp(code, "JSH6PN") && strlen(a.token) == 64 && !a.linked);
    CHECK(strstr(g_last_url, "/rest/v1/rpc/device_link_start") && !strcmp(g_last_body, "{\"device_name\":\"Switch de \\\"J\\\"\"}"));

    g_reply = "{\"nada\":1}";
    gr_account b;
    CHECK(gr_link_start(http_fake, "S", &b, code, err, sizeof(err)) == GR_NET_BAD);

    /* whoami: sin reclamar, reclamada, y llave borrada */
    g_reply = "[{\"linked\":false,\"username\":null,\"display_name\":null,\"caption\":null}]";
    CHECK(gr_whoami(http_fake, &a, &exists) == GR_NET_OK && exists && !a.linked);
    g_reply = "[{\"linked\":true,\"username\":\"megaJHONDY\",\"display_name\":\"Jhondy\",\"caption\":\"Cazatrofeos\"}]";
    CHECK(gr_whoami(http_fake, &a, &exists) == GR_NET_OK && exists && a.linked);
    CHECK(!strcmp(a.username, "megaJHONDY") && !strcmp(a.display, "Jhondy") && !strcmp(a.caption, "Cazatrofeos"));
    g_reply = "[]";
    CHECK(gr_whoami(http_fake, &a, &exists) == GR_NET_OK && !exists);
    CHECK(gr_whoami(http_down, &a, &exists) == GR_NET_OFFLINE);

    /* guardar y leer */
    snprintf(path, sizeof(path), "%s/account.ini", tmp);
    CHECK(gr_account_save(&a, path) == 0);
    gr_account c;
    gr_account_load(&c, path);
    CHECK(!strcmp(c.token, a.token) && c.linked && !strcmp(c.username, "megaJHONDY") && !strcmp(c.caption, "Cazatrofeos"));
    gr_account empty = {0};
    gr_account_save(&empty, path); /* sin llave: se borra el archivo */
    gr_account_load(&c, path);
    CHECK(c.token[0] == 0 && !c.linked);

    /* subir logros: arma bien la lista y salta lineas basura */
    char dir[256], file[300];
    snprintf(dir, sizeof(dir), "%s/unlocked", tmp);
    mkdir(dir, 0777);
    snprintf(file, sizeof(file), "%s/010028600EBDA000.txt", dir);
    FILE *f = fopen(file, "w");
    fputs("monedas_5|1794264260\nvidas_10|0\nmalo \"id|5\n\nreserva_gato\n", f);
    fclose(f);
    snprintf(file, sizeof(file), "%s/notas.txt", dir);
    f = fopen(file, "w"); fputs("x|1\n", f); fclose(f);

    int sent, added;
    g_reply = "2";
    CHECK(gr_push_unlocks(http_fake, &a, tmp, &sent, &added, err, sizeof(err)) == GR_NET_OK);
    CHECK(sent == 3 && added == 2);
    CHECK(strstr(g_last_body, "{\"title_id\":\"010028600EBDA000\",\"ach_id\":\"monedas_5\",\"unlocked_at\":1794264260}"));
    CHECK(strstr(g_last_body, "\"ach_id\":\"reserva_gato\",\"unlocked_at\":0}") && !strstr(g_last_body, "malo") && !strstr(g_last_body, "notas"));
    CHECK(g_last_body[strlen(g_last_body) - 2] == ']' && g_last_body[strlen(g_last_body) - 1] == '}');

    g_reply = "{\"code\":\"P0001\",\"message\":\"consola no vinculada\"}";
    g_reply_status = 400;
    CHECK(gr_push_unlocks(http_fake, &a, tmp, &sent, &added, err, sizeof(err)) == GR_NET_REJECTED);
    CHECK(!strcmp(err, "Esta consola ya no está vinculada."));
    CHECK(gr_push_unlocks(http_fake, &empty, tmp, &sent, &added, err, sizeof(err)) == GR_NET_REJECTED && sent == 0);

    /* entrar con correo: token -> registrar consola -> whoami */
    g_reply = "{\"error_code\":\"invalid_credentials\",\"msg\":\"Invalid login credentials\"}";
    CHECK(gr_login_password(http_fake, "a@b.c", "mala", "S", &b, err, sizeof(err)) == GR_NET_REJECTED);
    CHECK(!strcmp(err, "Correo o contraseña incorrectos."));
    CHECK(!strcmp(g_last_body, "{\"email\":\"a@b.c\",\"password\":\"mala\"}") && strstr(g_last_url, "/auth/v1/token?grant_type=password"));
}

static void test_real(const char *tmp) {
    gr_account a;
    char code[8], err[160];
    bool exists;
    int sent, added;

    gr_net_result r = gr_link_start(http_curl, "Prueba automatica", &a, code, err, sizeof(err));
    printf("  link_start -> %d, codigo %s\n", r, r == GR_NET_OK ? code : err);
    CHECK(r == GR_NET_OK && strlen(code) == 6 && strlen(a.token) == 64);

    CHECK(gr_whoami(http_curl, &a, &exists) == GR_NET_OK && exists && !a.linked);

    gr_account ghost = {0};
    snprintf(ghost.token, sizeof(ghost.token), "llave-que-no-existe-0000000000000000");
    CHECK(gr_whoami(http_curl, &ghost, &exists) == GR_NET_OK && !exists);

    r = gr_push_unlocks(http_curl, &a, tmp, &sent, &added, err, sizeof(err));
    printf("  push sin reclamar -> %d (%s), enviados %d\n", r, err, sent);
    CHECK(r == GR_NET_REJECTED && sent == 3 && !strcmp(err, "Esta consola ya no está vinculada."));

    gr_account b;
    r = gr_login_password(http_curl, "nadie-existe@example.com", "clave-incorrecta-123", "Prueba", &b, err, sizeof(err));
    printf("  login malo -> %d (%s)\n", r, err);
    CHECK(r == GR_NET_REJECTED && !strcmp(err, "Correo o contraseña incorrectos."));
}

int main(int argc, char **argv) {
    char tmp[] = "/tmp/greeny_acct_XXXXXX";
    if (!mkdtemp(tmp)) return 2;
    test_json();
    test_flows_fake(tmp);
    if (argc > 1 && !strcmp(argv[1], "--red")) test_real(tmp);
    printf("%d/%d pruebas de cuenta pasaron\n", g_total - g_fail, g_total);
    return g_fail ? 1 : 0;
}
