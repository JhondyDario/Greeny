/* Lo minimo de libnx para compilar source/search.c en PC (ver test/search/test_search.c). */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64; typedef int16_t s16; typedef int32_t s32;
typedef u32 Result;
#define R_FAILED(rc) ((rc) != 0)
#define R_SUCCEEDED(rc) ((rc) == 0)
typedef struct { u64 addr; u64 size; u32 type; u32 attr; u32 perm; u32 ipc_refcount; u32 device_refcount; u32 padding; } MemoryInfo;
enum { Perm_R = 1, Perm_W = 2, Perm_X = 4, Perm_Rw = 3 };
enum { MemType_ModuleCodeMutable = 9, MemType_CodeWritable = 0x15 };
enum { MemType_Unmapped = 0, MemType_Io = 1, MemType_CodeMutable = 4, MemType_Heap = 5, MemType_Reserved = 0x10 };
