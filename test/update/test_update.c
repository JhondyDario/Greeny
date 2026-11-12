/* Prueba del actualizador: version mas nueva, lectura de la release y NRO valido. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "update.h"
int main(void) {
    assert(gr_version_newer("0.2.1", "0.2.0") && gr_version_newer("v1.0.0", "0.9.9") && gr_version_newer("0.10.0", "0.9.0"));
    assert(!gr_version_newer("0.2.0", "0.2.0") && !gr_version_newer("0.1.9", "0.2.0") && !gr_version_newer("garbage", "0.2.0"));
    const char *json = "{\"tag_name\": \"v1.0.0\", \"name\": \"Greeny 1.0\", \"body\": \"- Logros nuevos\\n- Arreglos\", \"assets\": ["
                       "{\"name\": \"greeny.zip\", \"size\": 6000000, \"browser_download_url\": \"https://github.com/JhondyDario/Greeny/releases/download/v1.0.0/greeny.zip\"},"
                       "{\"name\": \"greeny.nro\", \"size\": 7300000, \"browser_download_url\": \"https://github.com/JhondyDario/Greeny/releases/download/v1.0.0/greeny.nro\"}]}";
    gr_release r;
    assert(gr_parse_release(json, &r));
    assert(!strcmp(r.version, "1.0.0") && r.size == 7300000 && strstr(r.nro_url, "/greeny.nro") && !strcmp(r.notes, "- Logros nuevos\n- Arreglos"));
    assert(!gr_parse_release("{\"tag_name\": \"v1.0.0\", \"assets\": []}", &r)); /* sin greeny.nro no se ofrece */
    assert(!gr_parse_release("{\"message\": \"Not Found\"}", &r));
    const char *evil = "{\"tag_name\": \"v9.0.0\", \"assets\": [{\"name\": \"greeny.nro\", \"browser_download_url\": \"https://otro.sitio/greeny.nro\"}]}";
    assert(!gr_parse_release(evil, &r)); /* solo descargas de GitHub */
    unsigned char head[0x20] = {0};
    memcpy(head + 0x10, "NRO0", 4);
    assert(gr_nro_valid(head, sizeof(head), 7000000) && !gr_nro_valid(head, sizeof(head), 1000));
    head[0x10] = 'X';
    assert(!gr_nro_valid(head, sizeof(head), 7000000));
    printf("actualizador: OK\n");
    return 0;
}
