/* Prueba del lector de comentarios: llaves y comillas dentro del texto no lo confunden. */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "account.h"
int main(void) {
    char json[] = "[{\"id\": 7, \"body\": \"Buen juego } {\\\"username\\\": \\\"falso\\\", \\\"mine\\\": true} fin\", \"mine\": false, "
                  "\"donor\": true, \"liked\": true, \"likes\": 3, \"avatar\": 9, \"country\": \"MX\", \"username\": \"Ana_99\", "
                  "\"created_at\": \"2026-11-10T10:00:00+00:00\", \"updated_at\": \"2026-11-10T10:00:00+00:00\", \"achievements\": 9, "
                  "\"display_name\": \"Ana\", \"total_points\": 310}, "
                  "{\"id\": 8, \"body\": \"Hola\", \"mine\": true, \"donor\": false, \"liked\": false, \"likes\": 0, \"avatar\": 4, "
                  "\"country\": null, \"username\": \"Jhondy\", \"created_at\": \"2026-11-11T10:00:00+00:00\", \"display_name\": null, \"total_points\": 5}]";
    gr_review r[4];
    int n = gr_parse_reviews(json, r, 4);
    assert(n == 2);
    assert(r[0].id == 7 && !strcmp(r[0].username, "Ana_99") && !r[0].mine && r[0].donor && r[0].liked && r[0].likes == 3);
    assert(!strcmp(r[0].body, "Buen juego } {\"username\": \"falso\", \"mine\": true} fin"));
    assert(!strcmp(r[0].country, "MX") && !strcmp(r[0].date, "2026-11-10") && r[0].total_points == 310 && !strcmp(r[0].display, "Ana"));
    assert(r[1].id == 8 && r[1].mine && !r[1].country[0] && !r[1].display[0] && !strcmp(r[1].body, "Hola"));
    char nj[] = "{\"items\": [{\"id\": 5, \"kind\": \"like\", \"read\": false, \"actor\": \"Ana_99\", \"actor_name\": \"Ana\", \"actor_avatar\": 9, "
                "\"title_id\": \"010028600EBDA000\", \"game\": \"Mario {3D}\", \"created_at\": \"2026-11-12T10:00:00+00:00\"}, "
                "{\"id\": 6, \"kind\": \"badge\", \"read\": true, \"badge\": \"logros_10\", \"created_at\": \"2026-11-11T10:00:00+00:00\"}], \"unread\": 1}";
    gr_notif nt[4];
    int unread = -1, nn = gr_parse_notifs(nj, nt, 4, &unread);
    assert(nn == 2 && unread == 1);
    assert(nt[0].id == 5 && nt[0].kind == GR_NOTIF_LIKE && !nt[0].read && nt[0].actor_avatar == 9 && !strcmp(nt[0].game, "Mario {3D}") && !strcmp(nt[0].date, "2026-11-12"));
    assert(nt[1].kind == GR_NOTIF_BADGE && nt[1].read && !strcmp(nt[1].badge, "logros_10"));
    printf("comentarios y avisos: OK\n");
    return 0;
}
