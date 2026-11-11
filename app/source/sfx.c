#include "sfx.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "sfx_abrir_menu_bin.h"
#include "sfx_aceptar_1_bin.h"
#include "sfx_aceptar_2_bin.h"
#include "sfx_navegar_bin.h"
#include "sfx_regresar_bin.h"

#define STB_VORBIS_HEADER_ONLY
#include "third_party/stb_vorbis.c"

int gr_ogg_read_mono(stb_vorbis *f, short *out, int frames); /* vorbis_impl.c */

#define RATE 48000 /* PCM 16 bit mono, generado por scripts/assets.py */

/* musica: una voz mas, alimentada con trozos de un cuarto de segundo */
#define MUSIC_VOICE SFX_COUNT
#define MUSIC_BUFS 4
#define MUSIC_BUF_BYTES 0xC000 /* multiplo de pagina; 12288 cuadros en estereo */
#define MUSIC_VOLUME 0.45f

static const AudioRendererConfig k_config = {
    .output_rate     = AudioRendererOutputRate_48kHz,
    .num_voices      = SFX_COUNT + 2, /* los sonidos, la musica y una de sobra */
    .num_effects     = 0,
    .num_sinks       = 1,
    .num_mix_objs    = 1,
    .num_mix_buffers = 2,
};

static bool g_ok;
static AudioDriver g_drv;
static AudioDriverWaveBuf g_wave[SFX_COUNT];
static void *g_pool;

static stb_vorbis *g_ogg;
static int g_ogg_channels;
static bool g_music_eof, g_music_finished;
static void *g_music_pool;
static AudioDriverWaveBuf g_music_wave[MUSIC_BUFS];

bool sfx_init(void) {
    /* mismo orden que sfx_id */
    const u8 *src[SFX_COUNT] = { sfx_navegar_bin, sfx_aceptar_1_bin, sfx_aceptar_2_bin, sfx_abrir_menu_bin, sfx_regresar_bin };
    size_t size[SFX_COUNT] = { sfx_navegar_bin_size, sfx_aceptar_1_bin_size, sfx_aceptar_2_bin_size, sfx_abrir_menu_bin_size,
                               sfx_regresar_bin_size };
    size_t offset[SFX_COUNT], total = 0;
    for (int i = 0; i < SFX_COUNT; i++) {
        size[i] &= ~(size_t)1;
        offset[i] = total;
        total += (size[i] + 0xFFF) & ~(size_t)0xFFF;
    }

    g_pool = memalign(0x1000, total);
    if (!g_pool) return false;
    memset(g_pool, 0, total);
    for (int i = 0; i < SFX_COUNT; i++) memcpy((u8 *)g_pool + offset[i], src[i], size[i]);
    armDCacheFlush(g_pool, total);

    if (R_FAILED(audrenInitialize(&k_config))) goto fail_pool;
    if (R_FAILED(audrvCreate(&g_drv, &k_config, 2))) goto fail_audren;

    int mpid = audrvMemPoolAdd(&g_drv, g_pool, total);
    audrvMemPoolAttach(&g_drv, mpid);
    static const u8 sink_channels[] = { 0, 1 };
    audrvDeviceSinkAdd(&g_drv, AUDREN_DEFAULT_DEVICE_NAME, 2, sink_channels);
    if (R_FAILED(audrvUpdate(&g_drv)) || R_FAILED(audrenStartAudioRenderer())) goto fail_drv;

    for (int i = 0; i < SFX_COUNT; i++) {
        memset(&g_wave[i], 0, sizeof(g_wave[i]));
        g_wave[i].data_raw = (u8 *)g_pool + offset[i];
        g_wave[i].size = size[i];
        g_wave[i].end_sample_offset = (s32)(size[i] / 2);
        audrvVoiceInit(&g_drv, i, 1, PcmFormat_Int16, RATE);
        audrvVoiceSetDestinationMix(&g_drv, i, AUDREN_FINAL_MIX_ID);
        audrvVoiceSetMixFactor(&g_drv, i, 0.7f, 0, 0);
        audrvVoiceSetMixFactor(&g_drv, i, 0.7f, 0, 1);
    }
    /* memoria para la musica; si no se puede reservar, simplemente no habra musica */
    g_music_pool = memalign(0x1000, MUSIC_BUFS * MUSIC_BUF_BYTES);
    if (g_music_pool) {
        memset(g_music_pool, 0, MUSIC_BUFS * MUSIC_BUF_BYTES);
        int mid = audrvMemPoolAdd(&g_drv, g_music_pool, MUSIC_BUFS * MUSIC_BUF_BYTES);
        audrvMemPoolAttach(&g_drv, mid);
    }
    audrvUpdate(&g_drv);
    g_ok = true;
    return true;

fail_drv:
    audrvClose(&g_drv);
fail_audren:
    audrenExit();
fail_pool:
    free(g_pool);
    g_pool = NULL;
    return false;
}

static bool g_sfx_on = true;
void sfx_set_enabled(bool on) { g_sfx_on = on; }

void sfx_play(sfx_id id) {
    if (!g_ok || !g_sfx_on || id < 0 || id >= SFX_COUNT) return;
    /* reiniciar la voz: si el mismo sonido seguia sonando, empieza de nuevo */
    audrvVoiceStop(&g_drv, id);
    audrvVoiceAddWaveBuf(&g_drv, id, &g_wave[id]);
    audrvVoiceStart(&g_drv, id);
}

/* Llena un trozo con lo que sigue de la cancion. Al llegar al final no pone nada mas. */
static void music_fill(int i) {
    s16 *buf = (s16 *)((u8 *)g_music_pool + (size_t)i * MUSIC_BUF_BYTES);
    int frames = gr_ogg_read_mono(g_ogg, buf, MUSIC_BUF_BYTES / 2);
    if (frames == 0) { g_music_eof = true; return; }
    armDCacheFlush(buf, MUSIC_BUF_BYTES);
    memset(&g_music_wave[i], 0, sizeof(g_music_wave[i]));
    g_music_wave[i].data_raw = buf;
    g_music_wave[i].size = (size_t)frames * (size_t)g_ogg_channels * 2;
    g_music_wave[i].end_sample_offset = frames;
    audrvVoiceAddWaveBuf(&g_drv, MUSIC_VOICE, &g_music_wave[i]);
}

void sfx_music_stop(void) {
    if (!g_ogg) return;
    audrvVoiceStop(&g_drv, MUSIC_VOICE);
    audrvUpdate(&g_drv);
    stb_vorbis_close(g_ogg);
    g_ogg = NULL;
    memset(g_music_wave, 0, sizeof(g_music_wave));
}

bool sfx_music_playing(void) { return g_ogg != NULL; }
bool sfx_music_finished(void) { return g_ogg != NULL && g_music_finished; }

static char g_music_note[96] = "sin intentar";
const char *sfx_music_note(void) { return g_music_note; }

bool sfx_music_play(const char *path) {
    if (!g_ok) { snprintf(g_music_note, sizeof(g_music_note), "el audio de la app no arranco"); return false; }
    if (!g_music_pool) { snprintf(g_music_note, sizeof(g_music_note), "sin memoria para la musica"); return false; }
    sfx_music_stop();
    int err = 0;
    g_ogg = stb_vorbis_open_filename(path, &err, NULL);
    if (!g_ogg) { snprintf(g_music_note, sizeof(g_music_note), "no se pudo abrir el OGG (error %d)", err); return false; }
    stb_vorbis_info info = stb_vorbis_get_info(g_ogg);
    /* Siempre en mono: es exactamente el mismo camino que usan los sonidos de la app, que ya
     * funcionan en la consola (una voz estereo necesita mas recursos del sistema de audio). */
    g_ogg_channels = 1;
    if (info.sample_rate < 8000 || info.sample_rate > 48000) {
        snprintf(g_music_note, sizeof(g_music_note), "frecuencia no admitida (%u Hz)", info.sample_rate);
        stb_vorbis_close(g_ogg);
        g_ogg = NULL;
        return false;
    }

    bool voice = audrvVoiceInit(&g_drv, MUSIC_VOICE, 1, PcmFormat_Int16, (int)info.sample_rate);
    audrvVoiceSetDestinationMix(&g_drv, MUSIC_VOICE, AUDREN_FINAL_MIX_ID);
    audrvVoiceSetMixFactor(&g_drv, MUSIC_VOICE, MUSIC_VOLUME, 0, 0);
    audrvVoiceSetMixFactor(&g_drv, MUSIC_VOICE, MUSIC_VOLUME, 0, 1);
    memset(g_music_wave, 0, sizeof(g_music_wave));
    g_music_eof = g_music_finished = false;
    for (int i = 0; i < MUSIC_BUFS; i++) music_fill(i);
    audrvVoiceStart(&g_drv, MUSIC_VOICE);
    Result rc = audrvUpdate(&g_drv);
    snprintf(g_music_note, sizeof(g_music_note), "voz %s, %u Hz, primer trozo %d cuadros, update 0x%X", voice ? "ok" : "FALLO",
             info.sample_rate, (int)g_music_wave[0].end_sample_offset, (unsigned)rc);
    return voice;
}

/* Cuantos samples ha sonado ya la cancion: si se queda en 0, el sistema no la esta reproduciendo. */
unsigned sfx_music_played(void) {
    return g_ogg ? audrvVoiceGetPlayedSampleCount(&g_drv, MUSIC_VOICE) : 0;
}

void sfx_update(void) {
    if (!g_ok) return;
    if (g_ogg) {
        int idle = 0;
        for (int i = 0; i < MUSIC_BUFS; i++) {
            if (g_music_wave[i].state != AudioDriverWaveBufState_Free && g_music_wave[i].state != AudioDriverWaveBufState_Done) continue;
            if (g_music_eof) idle++;
            else music_fill(i);
        }
        /* se acabo el archivo y ya sono hasta el ultimo trozo */
        if (g_music_eof && idle == MUSIC_BUFS) g_music_finished = true;
    }
    audrvUpdate(&g_drv);
}

void sfx_exit(void) {
    if (!g_ok) return;
    sfx_music_stop();
    g_ok = false;
    audrvClose(&g_drv);
    audrenExit();
    free(g_pool);
    g_pool = NULL;
    free(g_music_pool);
    g_music_pool = NULL;
}
