/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#if OS_WIN

u64 ArenaPageSize() {
    static const u64 pageSize = []() -> u64 {
        SYSTEM_INFO info = {};
        GetSystemInfo(&info);
        return info.dwPageSize;
    }();
    return pageSize;
}

bool ArenaCommit(void* base, u64 size) {
    if (size == 0) {
        return true;
    }
    return VirtualAlloc(base, (SIZE_T)size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

void* ArenaReserve(u64 size) {
    return VirtualAlloc(nullptr, (SIZE_T)size, MEM_RESERVE, PAGE_READWRITE);
}

void ArenaReleaseMemory(void* base, u64 size) {
    (void)size;
    VirtualFree(base, 0, MEM_RELEASE);
}

#endif
