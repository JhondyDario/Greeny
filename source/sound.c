/*
 * El audio se abre solo mientras suena y se cierra despues, para no quedarse
 * con una sesion de audren y memoria que el juego podria necesitar.
 * El sonido se sintetiza (dos notas con caida), no hay ningun archivo.
 */
#include "sound.h"

#include <malloc.h>
#include <math.h>
#include <string.h>
#include <switch.h>

#define RATE        48000
#define NOTE1_MS    110
#define NOTE2_MS    420
#define TOTAL_SAMPLES ((NOTE1_MS + NOTE2_MS) * RATE / 1000)
#define POOL_SIZE   ((TOTAL_SAMPLES * 2 + 0xFFF) & ~0xFFF)
#define CLOSE_AFTER_MS 900
#define PI 3.14159265358979f

static const AudioRendererConfig k_config = {
    .output_rate     = AudioRendererOutputRate_48kHz,
    .num_voices      = 2,
    .num_effects     = 0,
    .num_sinks       = 1,
    .num_mix_objs    = 1,
    .num_mix_buffers = 2,
};

static bool g_pending, g_active, g_failed;
static Result g_fail_rc;
static u64 g_close_at;
static AudioDriver g_drv;
static AudioDriverWaveBuf g_wave;
static void *g_pool;

bool sound_failed(void) { return g_failed; }
uint32_t sound_fail_rc(void) { return g_fail_rc; }

void sound_play(void) {
    if (!g_failed) g_pending = true;
}

/* una nota: fundamental + un poco de octava, con ataque corto y caida exponencial */
static void synth_note(s16 *out, int n, float freq, float decay) {
    for (int i = 0; i < n; i++) {
        float t = (float)i / RATE;
        float env = expf(-t * decay);
        float attack = i < 96 ? (float)i / 96.0f : 1.0f;
        float release = n - i < 480 ? (float)(n - i) / 480.0f : 1.0f; /* sin chasquido al final */
        float v = sinf(2 * PI * freq * t) + 0.3f * sinf(4 * PI * freq * t);
        out[i] = (s16)(v * env * attack * release * 0.28f * 32767.0f);
    }
}

static Result start(void) {
    g_pool = memalign(0x1000, POOL_SIZE);
    if (!g_pool) return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    memset(g_pool, 0, POOL_SIZE);

    s16 *pcm = (s16 *)g_pool;
    int n1 = NOTE1_MS * RATE / 1000;
    synth_note(pcm, n1, 1318.5f, 9.0f);                       /* Mi6 */
    synth_note(pcm + n1, TOTAL_SAMPLES - n1, 2093.0f, 7.0f);  /* Do7 */
    armDCacheFlush(g_pool, POOL_SIZE);

    Result rc = smInitialize();
    if (R_FAILED(rc)) goto fail_pool;
    rc = audrenInitialize(&k_config);
    smExit();
    if (R_FAILED(rc)) goto fail_pool;

    rc = audrvCreate(&g_drv, &k_config, 2);
    if (R_FAILED(rc)) goto fail_audren;

    int mpid = audrvMemPoolAdd(&g_drv, g_pool, POOL_SIZE);
    audrvMemPoolAttach(&g_drv, mpid);
    static const u8 sink_channels[] = { 0, 1 };
    audrvDeviceSinkAdd(&g_drv, AUDREN_DEFAULT_DEVICE_NAME, 2, sink_channels);

    rc = audrvUpdate(&g_drv);
    if (R_FAILED(rc)) goto fail_drv;
    rc = audrenStartAudioRenderer();
    if (R_FAILED(rc)) goto fail_drv;

    memset(&g_wave, 0, sizeof(g_wave));
    g_wave.data_raw = g_pool;
    g_wave.size = TOTAL_SAMPLES * 2;
    g_wave.start_sample_offset = 0;
    g_wave.end_sample_offset = TOTAL_SAMPLES;

    audrvVoiceInit(&g_drv, 0, 1, PcmFormat_Int16, RATE);
    audrvVoiceSetDestinationMix(&g_drv, 0, AUDREN_FINAL_MIX_ID);
    audrvVoiceSetMixFactor(&g_drv, 0, 1.0f, 0, 0);
    audrvVoiceSetMixFactor(&g_drv, 0, 1.0f, 0, 1);
    audrvVoiceAddWaveBuf(&g_drv, 0, &g_wave);
    audrvVoiceStart(&g_drv, 0);
    rc = audrvUpdate(&g_drv);
    if (R_FAILED(rc)) goto fail_drv;
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
        if (now_ms >= g_close_at) {
            stop();
            g_active = false;
        } else {
            audrvUpdate(&g_drv);
        }
        return;
    }
    if (!g_pending) return;
    g_pending = false;

    Result rc = start();
    if (R_FAILED(rc)) {
        g_failed = true;
        g_fail_rc = rc;
        return;
    }
    g_active = true;
    g_close_at = now_ms + CLOSE_AFTER_MS;
}
