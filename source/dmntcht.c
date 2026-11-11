/*
 * Basado en dmntcht.c de Atmosphère-NX, Copyright (c) 2018-2019 Atmosphère-NX, GPLv2.
 */
#define NX_SERVICE_ASSUME_NON_DOMAIN
#include "dmntcht.h"

static Service g_dmntchtSrv;
static bool g_dmntchtOpen;

/* Necesita una sesion de sm abierta. */
Result dmntchtInitialize(void) {
    if (g_dmntchtOpen) return 0;
    Result rc = smGetService(&g_dmntchtSrv, "dmnt:cht");
    if (R_SUCCEEDED(rc)) g_dmntchtOpen = true;
    return rc;
}

void dmntchtExit(void) {
    if (!g_dmntchtOpen) return;
    serviceClose(&g_dmntchtSrv);
    g_dmntchtOpen = false;
}

Result dmntchtHasCheatProcess(bool *out) {
    u8 tmp = 0;
    Result rc = serviceDispatchOut(&g_dmntchtSrv, 65000, tmp);
    if (R_SUCCEEDED(rc) && out) *out = tmp & 1;
    return rc;
}

Result dmntchtGetCheatProcessMetadata(DmntCheatProcessMetadata *out_metadata) {
    return serviceDispatchOut(&g_dmntchtSrv, 65002, *out_metadata);
}

Result dmntchtForceOpenCheatProcess(void) {
    return serviceDispatch(&g_dmntchtSrv, 65003);
}

Result dmntchtReadCheatProcessMemory(u64 address, void *buffer, size_t size) {
    const struct {
        u64 address;
        u64 size;
    } in = { address, size };
    return serviceDispatchIn(&g_dmntchtSrv, 65102, in,
        .buffer_attrs = { SfBufferAttr_Out | SfBufferAttr_HipcMapAlias },
        .buffers = { { buffer, size } },
    );
}

Result dmntchtQueryCheatProcessMemory(MemoryInfo *mem_info, u64 address) {
    return serviceDispatchInOut(&g_dmntchtSrv, 65104, address, *mem_info);
}
