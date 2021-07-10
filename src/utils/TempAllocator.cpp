/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

/*
Temp allocator is meant for allocating temporary values that don't need
to outlive this stack frame.
It's an alternative to using various AutoFree* classes.

It's a very fast bump allocator.

You must periodically call ResetTempAllocator()
to free memory used by allocator.
A safe place to call it is inside message windows loop.
*/

static PoolAllocator* gTempAllocator{nullptr};

// must call before any calls using temp allocator
void InitTempAllocator() {
    CrashIf(gTempAllocator);
    gTempAllocator = new PoolAllocator();
    // this can be large because 64k is nothing and it's used frequently
    gTempAllocator->minBlockSize = 64 * 1024;
}

void DestroyTempAllocator() {
    delete gTempAllocator;
    gTempAllocator = nullptr;
}

void ResetTempAllocator() {
    gTempAllocator->Reset(true);
}

TempStr TempStrDup(const char* s, size_t cch) {
    char* res = str::Dup(gTempAllocator, s, cch);
    if (cch == (size_t)-1) {
        // TODO: optimize to remove str::Len(). Add version of str::Dup()
        // that returns std::string_view
        cch = str::Len(res);
    }
    return TempStr(res, cch);
}

TempStr TempToUtf8(const WCHAR* s, size_t cch) {
    if (!s) {
        CrashIf((int)cch > 0);
        return TempStr();
    }
    if (cch == (size_t)-1) {
        cch = str::Len(s);
    }
    strconv::StackWstrToUtf8 buf(s, cch);
    return TempStrDup(buf.Get(), buf.size());
}

TempWstr TempToWstr(const char* s) {
    CrashIf(true); // NYI
    return TempWstr();
}

TempWstr TempToWstr(std::string_view sv) {
    CrashIf(true); // NYI
    return TempWstr();
}
