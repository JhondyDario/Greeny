/* Decodificador de OGG Vorbis (stb_vorbis, dominio publico) para la musica de fondo. */
#define STB_VORBIS_NO_PUSHDATA_API
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "third_party/stb_vorbis.c"

/* Lee hasta `frames` muestras en mono. stb_vorbis no sabe entregar un archivo estereo
 * directamente en mono con esta funcion (tiene un assert), asi que se lee en estereo
 * por partes y se promedian los dos canales aqui. Devuelve cuantas escribio; 0 = fin. */
int gr_ogg_read_mono(stb_vorbis *f, short *out, int frames) {
    static short tmp[2 * 2048];
    int channels = stb_vorbis_get_info(f).channels >= 2 ? 2 : 1, done = 0;
    while (done < frames) {
        int want = frames - done > 2048 ? 2048 : frames - done;
        int got = stb_vorbis_get_samples_short_interleaved(f, channels, tmp, want * channels);
        if (got <= 0) break;
        if (channels == 2) for (int i = 0; i < got; i++) out[done + i] = (short)((tmp[2 * i] + tmp[2 * i + 1]) / 2);
        else for (int i = 0; i < got; i++) out[done + i] = tmp[i];
        done += got;
    }
    return done;
}
