/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/FileWatcher.h"

static Str gFileWatcherSkipPath;

void FileWatcherSetSkipPath(Str path) {
    gFileWatcherSkipPath = path;
}

Str FileWatcherGetSkipPath() {
    return gFileWatcherSkipPath;
}

Kind kindNone = "none";

// if > 1 we won't crash when memory allocation fails
LONG gAllowAllocFailure = 0;

// returns count after adding
int AtomicRefCountAdd(AtomicRefCount* v) {
    return (int)InterlockedIncrement(v);
}

int AtomicRefCountDec(AtomicRefCount* v) {
    return (int)InterlockedDecrement(v);
}

// This exits so that I can add temporary instrumentation
// to catch allocations of a given size and it won't cause
// re-compilation of everything caused by changing Base.h
void* AllocZero(int count, int size) {
    return calloc(count, size);
}

// extraBytes will be filled with 0. Useful for copying zero-terminated strings
void* memdup(const void* data, int n, int extraBytes) {
    // to simplify callers, if data is nullptr, ignore the sizes
    if (!data) {
        return nullptr;
    }
    void* dup = AllocZero(n + extraBytes, 1);
    if (dup) {
        memcpy(dup, data, n);
    }
    return dup;
}

bool memeq(const void* s1, const void* s2, int n) {
    return 0 == memcmp(s1, s2, n);
}

int RoundUp(int n, int rounding) {
    if (rounding <= 1) {
        return n;
    }
    return ((n + rounding - 1) / rounding) * rounding;
}

void* RoundUp(void* d, int rounding) {
    if (rounding <= 1) {
        return d;
    }
    uintptr_t n = (uintptr_t)d;
    n = ((n + rounding - 1) / rounding) * rounding;
    return (void*)n;
}

int RoundToPowerOf2(int size) {
    int n = 1;
    while (n < size) {
        n *= 2;
        if (n <= 0) {
            // overflow: no power of 2 fits in an int
            return -1;
        }
    }
    return n;
}

/* MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
static u32 hash_function_seed = 5381;

u32 MurmurHash2(const void* key, int n) {
    if (n <= 0) {
        return 0;
    }
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    const u32 m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    u32 h = hash_function_seed ^ (u32)n;

    /* Mix 4 bytes at a time into the hash */
    const u8* data = (const u8*)key;

    while (n >= 4) {
        u32 k = *(u32*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        n -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch (n) {
        case 3:
            h ^= data[2] << 16;
        case 2:
            h ^= data[1] << 8;
        case 1:
            h ^= data[0];
            h *= m;
    }

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

u32 MurmurHash2(Str s) {
    return MurmurHash2(s.s, s.len);
}

u32 MurmurHash2(WStr s) {
    return MurmurHash2(s.s, s.len * (int)sizeof(wchar_t));
}

// variation of MurmurHash2 which deals with strings that are
// mostly ASCII and should be treated case independently
u32 MurmurHashWStrI(WStr str) {
    auto a = GetTempArena();
    u8* data = (u8*)a->Alloc(str.len);
    u8* dst = data;
    for (int i = 0; i < str.len; i++) {
        wchar_t c = str.s[i];
        if (c & 0xFF80) {
            *dst++ = 0x80;
            continue;
        }
        if ('A' <= c && c <= 'Z') {
            *dst++ = (u8)(c + 'a' - 'A');
            continue;
        }
        *dst++ = (u8)c;
    }
    return MurmurHash2(data, (int)(dst - data));
}

// variation of MurmurHash2 which deals with strings that are
// mostly ASCII and should be treated case independently
u32 MurmurHashStrI(Str s) {
    TempStr dst = str::DupTemp(s);
    for (int i = 0; i < dst.len; i++) {
        char c = dst.s[i];
        if ('A' <= c && c <= 'Z') {
            dst.s[i] = (char)(c + 'a' - 'A');
        }
    }
    return MurmurHash2(dst);
}

int limitValue(int val, int min, int max) {
    if (min > max) {
        std::swap(min, max);
    }
    ReportIf(min > max);
    if (val < min) {
        return min;
    }
    if (val > max) {
        return max;
    }
    return val;
}

DWORD limitValue(DWORD val, DWORD min, DWORD max) {
    if (min > max) {
        std::swap(min, max);
    }
    ReportIf(min > max);
    if (val < min) {
        return min;
    }
    if (val > max) {
        return max;
    }
    return val;
}

float limitValue(float val, float min, float max) {
    if (min > max) {
        std::swap(min, max);
    }
    ReportIf(min > max);
    if (val < min) {
        return min;
    }
    if (val > max) {
        return max;
    }
    return val;
}

Func0 MkFunc0Void(funcVoidPtr fn) {
    auto res = Func0{};
    res.fn = (void*)fn;
    res.userData = kFuncNoArg;
    return res;
}

#if 0
template <typename T>
Func0 MkMethod0Void(funcVoidPtr fn, T* self) {
    UINT_PTR fnTagged = (UINT_PTR)fn;
    res.fn = (void*)fn;
    res.userData = kFuncNoArg;
    res.self = self;
}
#endif

int setMinMax(int& v, int minVal, int maxVal) {
    if (v < minVal) {
        v = minVal;
    }
    if (v > maxVal) {
        v = maxVal;
    }
    return v;
}

// --- begin: merged from former src/common/base.cpp ---

// Atomic bool operations
bool AtomicBoolGet(AtomicBool* p) {
    return InterlockedOr(p, 0) != 0;
}

void AtomicBoolSet(AtomicBool* p, bool v) {
    InterlockedExchange(p, v ? 1 : 0);
}

// Atomic int operations
int AtomicIntGet(AtomicInt* p) {
    return (int)InterlockedOr(p, 0);
}

void AtomicIntSet(AtomicInt* p, int v) {
    InterlockedExchange(p, (LONG)v);
}

int AtomicIntAdd(AtomicInt* p, int v) {
    return (int)InterlockedAdd(p, (LONG)v);
}

int AtomicIntInc(AtomicInt* p) {
    return (int)InterlockedIncrement(p);
}

int AtomicIntDec(AtomicInt* p) {
    return (int)InterlockedDecrement(p);
}
// --- end: merged from former src/common/base.cpp ---
