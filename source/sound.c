/*
 * El audio se abre solo mientras suena y se cierra despues, para no quedarse
 * con una sesion de audren y memoria que el juego podria necesitar.
 * Los sonidos van dentro del ejecutable: PCM 16 bit mono a 32 kHz
 * (se generan con scripts/assets.py a partir de assets/sounds/).
 */
#include "sound.h"

#include <malloc.h>
#include <string.h>
#include <switch.h>

#include "snd_cargado_bin.h"
#include "snd_logro_bin.h"

#define RATE 32000
#define TAIL_MS 250 /* margen despues del ultimo sample antes de cerrar el audio */

static const AudioRendererConfig k_config = {
    .output_rate     = AudioRendererOutputRate_48kHz,
    .num_voices      = 2,
    .num_effects     = 0,
    .num_sinks       = 1,
    .num_mix_objs    = 1,
    .num_mix_buffers = 2,
};

static bool g_queued[SOUND_COUNT];
static bool g_active, g_failed;
static Result g_fail_rc;
static u64 g_close_at;
static AudioDriver g_drv;
static AudioDriverWaveBuf g_wave;
static void *g_pool;

bool sound_failed(void) { return g_failed; }
uint32_t sound_fail_rc(void) { return g_fail_rc; }

void sound_play(sound_id id) {
    if (!g_failed && id >= 0 && id < SOUND_COUNT) g_queued[id] = true;
}

static void get_pcm(sound_id id, const u8 **data, size_t *size) {
    if (id == SOUND_LOADED) { *data = snd_cargado_bin; *size = snd_cargado_bin_size; }
    else { *data = snd_logro_bin; *size = snd_logro_bin_size; }
    *size &= ~(size_t)1;
}

static Result start(sound_id id, u32 *out_ms) {
    const u8 *pcm;
    size_t size;
    get_pcm(id, &pcm, &size);
    size_t pool_size = (size + 0xFFF) & ~(size_t)0xFFF;

    /* audren necesita memoria alineada a pagina que pueda mapear: se copia al heap */
    g_pool = memalign(0x1000, pool_size);
    if (!g_pool) return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    memcpy(g_pool, pcm, size);
    memset((u8 *)g_pool + size, 0, pool_size - size);
    armDCacheFlush(g_pool, pool_size);

    Result rc = smInitialize();
    if (R_FAILED(rc)) goto fail_pool;
    rc = audrenInitialize(&k_config);
    smExit();
    if (R_FAILED(rc)) goto fail_pool;

    rc = audrvCreate(&g_drv, &k_config, 2);
    if (R_FAILED(rc)) goto fail_audren;

    int mpid = audrvMemPoolAdd(&g_drv, g_pool, pool_size);
    audrvMemPoolAttach(&g_drv, mpid);
    static const u8 sink_channels[] = { 0, 1 };
    audrvDeviceSinkAdd(&g_drv, AUDREN_DEFAULT_DEVICE_NAME, 2, sink_channels);

    rc = audrvUpdate(&g_drv);
    if (R_FAILED(rc)) goto fail_drv;
    rc = audrenStartAudioRenderer();
    if (R_FAILED(rc)) goto fail_drv;

    memset(&g_wave, 0, sizeof(g_wave));
    g_wave.data_raw = g_pool;
    g_wave.size = size;
    g_wave.start_sample_offset = 0;
    g_wave.end_sample_offset = (s32)(size / 2);

    audrvVoiceInit(&g_drv, 0, 1, PcmFormat_Int16, RATE);
    audrvVoiceSetDestinationMix(&g_drv, 0, AUDREN_FINAL_MIX_ID);
    audrvVoiceSetMixFactor(&g_drv, 0, 0.8f, 0, 0);
    audrvVoiceSetMixFactor(&g_drv, 0, 0.8f, 0, 1);
    audrvVoiceAddWaveBuf(&g_drv, 0, &g_wave);
    audrvVoiceStart(&g_drv, 0);
    rc = audrvUpdate(&g_drv);
    if (R_FAILED(rc)) goto fail_drv;

    *out_ms = (u32)(size / 2 * 1000 / RATE) + TAIL_MS;
    return 0;

fail_drv:
    audrvClose(&g_drv);
fail_audren:
    audrenExit();
fail_pool:
    free(g_pool);
    g_pool = NULL;
    return rc;
}

static void stop(void) {
    audrvVoiceStop(&g_drv, 0);
    audrvUpdate(&g_drv);
    audrvClose(&g_drv);
    audrenExit();
    free(g_pool);
    g_pool = NULL;
}

void sound_update(uint64_t now_ms) {
    if (g_failed) return;

    if (g_active) {
        if (now_ms < g_close_at) { audrvUpdate(&g_drv); return; }
        stop();
        g_active = false;
    }

    for (int id = 0; id < SOUND_COUNT; id++) {
        if (!g_queued[id]) continue;
        g_queued[id] = false;

        u32 ms = 0;
        Result rc = start((sound_id)id, &ms);
        if (R_FAILED(rc)) {
            g_failed = true;
            g_fail_rc = rc;
            return;
        }
        g_active = true;
        g_close_at = now_ms + ms;
        return;
    }
}
