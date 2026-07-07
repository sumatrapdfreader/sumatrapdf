/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include <sys/mman.h>
#include <unistd.h>

#include "base/Arena.h"

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

u64 ArenaPageSize() {
    static u64 pageSize = 0;
    if (pageSize == 0) {
        long size = sysconf(_SC_PAGESIZE);
        pageSize = size > 0 ? (u64)size : 4096;
    }
    return pageSize;
}

u64 ArenaLargePageSize() {
    return ArenaPageSize();
}

bool ArenaCommit(void* base, u64 size, bool largePages) {
    if (size == 0) {
        return true;
    }
    (void)largePages;
    return mprotect(base, (size_t)size, PROT_READ | PROT_WRITE) == 0;
}

void* ArenaReserve(u64 size) {
    void* base = mmap(nullptr, (size_t)size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return base == MAP_FAILED ? nullptr : base;
}

void* ArenaReserveAndCommit(u64 size, bool largePages) {
    (void)largePages;
    void* base = mmap(nullptr, (size_t)size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return base == MAP_FAILED ? nullptr : base;
}

void ArenaReleaseMemory(void* base, u64 size) {
    munmap(base, (size_t)size);
}
