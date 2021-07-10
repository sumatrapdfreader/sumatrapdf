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

TempStr TempStrDup(const char* s, size_t cb) {
    char* res = str::Dup(gTempAllocator, s, cb);
    if (cb == (size_t)-1) {
        // TODO: optimize to remove str::Len(). Add version of str::Dup()
        // that returns std::string_view
        cb = str::Len(res);
    }
    return TempStr(res, cb);
}

TempWstr TempWstrDup(const WCHAR* s, size_t cch) {
    WCHAR* res = str::Dup(gTempAllocator, s, cch);
    if (cch == (size_t)-1) {
        cch = str::Len(res);
    }
    return TempWstr(res, cch);
}

/*
Todo: could further optimize by removing memory copy in common
case by estimating converted size, allocating from temp allocator
and only re-allocating when estimated buf wasn't big enough

Alternatively, just simplify to always as for size first and
allocate desired size from allocator.
By passing temp allocator we get TempTo* for free.
Going over the source string twice is probably better than 2
allocations in common case.

Also, the Utf8ToWcharBuf() and others that use buf can probably be
all replaced by TempTo* functions.
*/

TempStr TempToUtf8(const WCHAR* s, size_t cch) {
    if (!s) {
        CrashIf((int)cch > 0);
        return TempStr();
    }
    strconv::StackWstrToUtf8 buf(s, cch);
    return TempStrDup(buf.Get(), buf.size());
}

TempStr TempToUtf8(std::wstring_view sv) {
    strconv::StackWstrToUtf8 buf(sv.data(), sv.size());
    return TempStrDup(buf.Get(), buf.size());
}

TempWstr TempToWstr(const char* s, size_t cb) {
    if (!s) {
        CrashIf((int)cb > 0);
        return TempWstr();
    }
    strconv::StackUtf8ToWstr buf(s, cb);
    return TempWstrDup(buf.Get(), buf.size());
}

TempWstr TempToWstr(std::string_view sv) {
    strconv::StackUtf8ToWstr buf(sv.data(), sv.size());
    return TempWstrDup(buf.Get(), buf.size());
}
