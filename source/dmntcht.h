/*
 * Cliente minimo del servicio dmnt:cht de Atmosphère (solo lo que usa Greeny).
 * Basado en dmntcht.c/h de Atmosphère-NX, Copyright (c) 2018-2019 Atmosphère-NX, GPLv2.
 */
#pragma once
#include <switch.h>

typedef struct {
    u64 base;
    u64 size;
} DmntMemoryRegionExtents;

typedef struct {
    u64 process_id;
    u64 title_id;
    DmntMemoryRegionExtents main_nso_extents;
    DmntMemoryRegionExtents heap_extents;
    DmntMemoryRegionExtents alias_extents;
    DmntMemoryRegionExtents address_space_extents;
    u8 main_nso_build_id[0x20];
} DmntCheatProcessMetadata;

Result dmntchtInitialize(void);
void   dmntchtExit(void);
Result dmntchtHasCheatProcess(bool *out);
Result dmntchtGetCheatProcessMetadata(DmntCheatProcessMetadata *out_metadata);
Result dmntchtForceOpenCheatProcess(void);
Result dmntchtReadCheatProcessMemory(u64 address, void *buffer, size_t size);
