#include "sfx.h"

#include <malloc.h>
#include <string.h>
#include <switch.h>

#include "sfx_abrir_menu_bin.h"
#include "sfx_aceptar_1_bin.h"
#include "sfx_aceptar_2_bin.h"
#include "sfx_navegar_bin.h"
#include "sfx_regresar_bin.h"

#define RATE 48000 /* PCM 16 bit mono, generado por scripts/assets.py */

static const AudioRendererConfig k_config = {
    .output_rate     = AudioRendererOutputRate_48kHz,
    .num_voices      = SFX_COUNT,
    .num_effects     = 0,
    .num_sinks       = 1,
    .num_mix_objs    = 1,
    .num_mix_buffers = 2,
};

static bool g_ok;
static AudioDriver g_drv;
static AudioDriverWaveBuf g_wave[SFX_COUNT];
static void *g_pool;

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

void sfx_play(sfx_id id) {
    if (!g_ok || id < 0 || id >= SFX_COUNT) return;
    /* reiniciar la voz: si el mismo sonido seguia sonando, empieza de nuevo */
    audrvVoiceStop(&g_drv, id);
    audrvVoiceAddWaveBuf(&g_drv, id, &g_wave[id]);
    audrvVoiceStart(&g_drv, id);
}

void sfx_update(void) {
    if (g_ok) audrvUpdate(&g_drv);
}

void sfx_exit(void) {
    if (!g_ok) return;
    g_ok = false;
    audrvClose(&g_drv);
    audrenExit();
    free(g_pool);
    g_pool = NULL;
}
