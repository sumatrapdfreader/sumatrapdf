/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"

#include "utils/Log.h"

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
// re-compilation of everything caused by changing BaseUtil.h
void* AllocZero(size_t count, size_t size) {
    return calloc(count, size);
}

// extraBytes will be filled with 0. Useful for copying zero-terminated strings
void* memdup(const void* data, size_t len, size_t extraBytes) {
    // to simplify callers, if data is nullptr, ignore the sizes
    if (!data) {
        return nullptr;
    }
    void* dup = AllocZero(len + extraBytes, 1);
    if (dup) {
        memcpy(dup, data, len);
    }
    return dup;
}

bool memeq(const void* s1, const void* s2, size_t len) {
    return 0 == memcmp(s1, s2, len);
}

size_t RoundUp(size_t n, size_t rounding) {
    return ((n + rounding - 1) / rounding) * rounding;
}

int RoundUp(int n, int rounding) {
    if (rounding <= 1) {
        return n;
    }
    return ((n + rounding - 1) / rounding) * rounding;
}

char* RoundUp(char* d, int rounding) {
    if (rounding <= 1) {
        return d;
    }
    uintptr_t n = (uintptr_t)d;
    n = ((n + rounding - 1) / rounding) * rounding;
    return (char*)n;
}

size_t RoundToPowerOf2(size_t size) {
    size_t n = 1;
    while (n < size) {
        n *= 2;
        if (0 == n) {
            // TODO: no power of 2
            return (size_t)-1;
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

u32 MurmurHash2(const void* key, size_t len) {
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    const u32 m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    u32 h = hash_function_seed ^ (u32)len;

    /* Mix 4 bytes at a time into the hash */
    const u8* data = (const u8*)key;

    while (len >= 4) {
        u32 k = *(u32*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch (len) {
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

// variation of MurmurHash2 which deals with strings that are
// mostly ASCII and should be treated case independently
u32 MurmurHashWStrI(const WCHAR* str) {
    size_t len = str::Len(str);
    auto a = GetTempAllocator();
    u8* data = (u8*)a->Alloc((int)len);
    WCHAR c;
    u8* dst = data;
    while (true) {
        c = *str++;
        if (!c) {
            break;
        }
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
    return MurmurHash2(data, len);
}

// variation of MurmurHash2 which deals with strings that are
// mostly ASCII and should be treated case independently
u32 MurmurHashStrI(const char* s) {
    char* dst = str::DupTemp(s);
    char c;
    size_t len = 0;
    while (*s) {
        c = *s++;
        len++;
        if ('A' <= c && c <= 'Z') {
            c = (c + 'a' - 'A');
        }
        *dst++ = c;
    }
    return MurmurHash2(dst - len, len);
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
