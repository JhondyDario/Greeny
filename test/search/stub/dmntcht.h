#pragma once
#include <switch.h>
Result dmntchtReadCheatProcessMemory(u64 address, void *buffer, size_t size);
Result dmntchtQueryCheatProcessMemory(MemoryInfo *mem_info, u64 address);
