/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: the wasm half of Arena_posix.cpp. There is no virtual address space to
// reserve: emscripten's mmap allocates for real and mprotect is not
// implemented, so a reserve is an allocation and a commit is a no-op. The
// default reserve is 1 MB instead of 64 (see gArenaDefaultReserveSize) and
// the arena chains another block when one fills up.

#include "base/Base.h"

#include <stdlib.h>
#include <string.h>

// wasm pages are 64k; matching them keeps the arena's rounding aligned with
// what the heap actually grows by
constexpr u64 kWasmPageSize = 64 * 1024;

u64 ArenaPageSize() {
    return kWasmPageSize;
}

u64 ArenaLargePageSize() {
    return kWasmPageSize;
}

bool ArenaCommit(void*, u64, bool) {
    return true;
}

void* ArenaReserve(u64 size) {
    if (size == 0) {
        return nullptr;
    }
    void* base = aligned_alloc((size_t)kWasmPageSize, (size_t)size);
    if (base) {
        memset(base, 0, (size_t)size);
    }
    return base;
}

void* ArenaReserveAndCommit(u64 size, bool) {
    return ArenaReserve(size);
}

void ArenaReleaseMemory(void* base, u64) {
    free(base);
}
