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

// forbid inlinining to not blow out the size of callers
NO_INLINE Allocator* GetTempAllocator() {
    if (gTempAllocator) {
        return gTempAllocator;
    }
    gTempAllocator = new PoolAllocator();
    // this can be large because 64k is nothing and it's used frequently
    gTempAllocator->minBlockSize = 64 * 1024;
    return gTempAllocator;
}

void DestroyTempAllocator() {
    delete gTempAllocator;
    gTempAllocator = nullptr;
}

void ResetTempAllocator() {
    if (gTempAllocator) {
        gTempAllocator->Reset(true);
    }
}

TempStr TempStrDup(const char* s, size_t cb) {
    char* res = str::Dup(GetTempAllocator(), s, cb);
    if (cb == (size_t)-1) {
        // TODO: optimize to remove str::Len(). Add version of str::Dup()
        // that returns std::string_view
        cb = str::Len(res);
    }
    return TempStr(res, cb);
}
TempStr TempStrDup(std::string_view sv) {
    return TempStrDup(sv.data(), sv.size());
}

TempWstr TempWstrDup(const WCHAR* s, size_t cch) {
    WCHAR* res = str::Dup(GetTempAllocator(), s, cch);
    if (cch == (size_t)-1) {
        cch = str::Len(res);
    }
    return TempWstr(res, cch);
}

TempWstr TempWstrDup(std::wstring_view sv) {
    return TempWstrDup(sv.data(), sv.size());
}

TempStr TempToUtf8(const WCHAR* s, size_t cch) {
    if (!s) {
        CrashIf((int)cch > 0);
        return TempStr();
    }
    auto v = strconv::WstrToUtf8V(s, cch, GetTempAllocator());
    return TempStr{v.data(), v.size()};
}

TempStr TempToUtf8(std::wstring_view sv) {
    auto v = strconv::WstrToUtf8V(sv, GetTempAllocator());
    return TempStr{v.data(), v.size()};
}

TempWstr TempToWstr(const char* s, size_t cb) {
    if (!s) {
        CrashIf((int)cb > 0);
        return TempWstr();
    }
    auto v = strconv::Utf8ToWstrV(s, cb, GetTempAllocator());
    return TempWstr{v.data(), v.size()};
}

TempWstr TempToWstr(std::string_view sv) {
    auto v = strconv::Utf8ToWstrV(sv, GetTempAllocator());
    return TempWstr{v.data(), v.size()};
}
