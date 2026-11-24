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

u64 ArenaLargePageSize() {
    static const u64 largePageSize = []() -> u64 {
        SIZE_T size = GetLargePageMinimum();
        return size ? (u64)size : ArenaPageSize();
    }();
    return largePageSize;
}

bool ArenaCommit(void* base, u64 size, bool largePages) {
    if (size == 0) {
        return true;
    }
    DWORD flags = MEM_COMMIT;
    if (largePages) {
        flags |= MEM_LARGE_PAGES;
    }
    return VirtualAlloc(base, (SIZE_T)size, flags, PAGE_READWRITE) != nullptr;
}

void* ArenaReserve(u64 size) {
    return VirtualAlloc(nullptr, (SIZE_T)size, MEM_RESERVE, PAGE_READWRITE);
}

void* ArenaReserveAndCommit(u64 size, bool largePages) {
    DWORD flags = MEM_RESERVE | MEM_COMMIT;
    if (largePages) {
        flags |= MEM_LARGE_PAGES;
    }
    return VirtualAlloc(nullptr, (SIZE_T)size, flags, PAGE_READWRITE);
}

void ArenaReleaseMemory(void* base, u64 size) {
    (void)size;
    VirtualFree(base, 0, MEM_RELEASE);
}

#endif
