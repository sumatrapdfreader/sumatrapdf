/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef BaseUtil_h
#define BaseUtil_h

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

// #include <winsock2.h>
#include <windows.h>
#include <unknwn.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <windowsx.h>
#include <winsafer.h>
#include <gdiplus.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef DEBUG
#define _CRTDBG_MAP_ALLOC
#endif

#include <algorithm>
#include <stdlib.h>

// TODO: this breaks placement new
#ifdef DEBUG
#include <crtdbg.h>
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#include <wchar.h>
#include <string.h>

/* Few most common includes for C stdlib */
#include <assert.h>
#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <locale.h>
#include <malloc.h>
#include <io.h>
#include <fcntl.h>

#define _USE_MATH_DEFINES
#include <math.h>

#if defined(__MINGW32__)
#include "mingw_compat.h"
#endif

#include <functional>


template <typename T>
inline T *AllocArray(size_t n)
{
    return (T*)calloc(n, sizeof(T));
}

template <typename T>
inline T *AllocStruct()
{
    return (T*)calloc(1, sizeof(T));
}

#define dimof(X)    (sizeof(X)/sizeof((X)[0]))
#define NoOp()      ((void)0)

/* compile-time assert */
#define STATIC_ASSERT(exp, name) typedef int assert_##name [(exp) != FALSE]

typedef unsigned char uint8;
typedef int16_t   int16;
typedef uint16_t uint16;
typedef int32_t   int32;
typedef uint32_t uint32;
typedef int64_t   int64;
typedef uint64_t uint64;

STATIC_ASSERT(2 == sizeof(int16),   int16_is_2_bytes);
STATIC_ASSERT(2 == sizeof(uint16), uint16_is_2_bytes);
STATIC_ASSERT(4 == sizeof(int32),   int32_is_4_bytes);
STATIC_ASSERT(4 == sizeof(uint32), uint32_is_4_bytes);
STATIC_ASSERT(8 == sizeof(int64),   int64_is_8_bytes);
STATIC_ASSERT(8 == sizeof(uint64), uint64_is_8_bytes);

#pragma warning(push)
#pragma warning(disable: 6011) // silence /analyze: de-referencing a NULL pointer
// Note: it's inlined to make it easier on crash reports analyzer (if wasn't inlined
// CrashMe() would show up as the cause of several different crash sites)
//
// Note: I tried doing this via RaiseException(0x40000015, EXCEPTION_NONCONTINUABLE, 0, 0);
// but it seemed to confuse callstack walking
inline void CrashMe()
{
    char *p = NULL;
    *p = 0;
}
#pragma warning(pop)

// CrashIf() is like assert() except it crashes in debug and pre-release builds.
// The idea is that assert() indicates "can't possibly happen" situation and if
// it does happen, we would like to fix the underlying cause.
// In practice in our testing we rarely get notified when an assert() is triggered
// and they are disabled in builds running on user's computers.
// Now that we have crash reporting, we can get notified about such cases if we
// use CrashIf() instead of assert(), which we should be doing from now on.
//
// Enabling it in pre-release builds but not in release builds is trade-off between
// shipping small executables (each CrashIf() adds few bytes of code) and having
// more testing on user's machines and not only in our personal testing.
// To crash uncoditionally use CrashAlwaysIf(). It should only be used in
// rare cases where we really want to know a given condition happens. Before
// each release we should audit the uses of CrashAlawysIf()
//
// Just as with assert(), the condition is not guaranteed to be executed
// in some builds, so it shouldn't contain the actual logic of the code

#define CrashAlwaysIf(cond) \
    do { if (cond) \
        CrashMe(); \
    __analysis_assume(!(cond)); } while (0)

#if defined(SVN_PRE_RELEASE_VER) || defined(DEBUG)
#define CrashIf(cond) CrashAlwaysIf(cond)
#else
#define CrashIf(cond) __analysis_assume(!(cond))
#endif

// Sometimes we want to assert only in debug build (not in pre-release)
#if defined(DEBUG)
#define CrashIfDebugOnly(cond) CrashAlwaysIf(cond)
#else
#define CrashIfDebugOnly(cond) __analysis_assume(!(cond))
#endif

// AssertCrash is like assert() but crashes like CrashIf()
// It's meant to make converting assert() easier (converting to
// CrashIf() requires inverting the condition, which can introduce bugs)
#define AssertCrash(exp) CrashIf(!(exp))

template <typename T>
inline T limitValue(T val, T min, T max)
{
    assert(max >= min);
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

inline void *memdup(const void *data, size_t len)
{
    void *dup = malloc(len);
    if (dup)
        memcpy(dup, data, len);
    return dup;
}

inline bool memeq(const void *s1, const void *s2, size_t len)
{
    return 0 == memcmp(s1, s2, len);
}

size_t      RoundToPowerOf2(size_t size);
uint32_t    MurmurHash2(const void *key, size_t len);

static inline size_t RoundUp(size_t n, size_t rounding)
{
    return ((n+rounding-1)/rounding)*rounding;
}

static inline int RoundUp(int n, int rounding)
{
    return ((n+rounding-1)/rounding)*rounding;
}

template <typename T>
void ListInsert(T** root, T* el)
{
    el->next = *root;
    *root = el;
}

template <typename T>
bool ListRemove(T** root, T* el)
{
    T **currPtr = root;
    T *curr;
    for (;;) {
        curr = *currPtr;
        if (!curr)
            return false;
        if (curr == el)
            break;
        currPtr = &(curr->next);
    }
    *currPtr = el->next;
    return true;
}

#include "Allocator.h"
#include "GeomUtil.h"
#include "Scoped.h"
#include "StrUtil.h"
#include "Vec.h"

/* In debug mode, VS 2010 instrumentations complains about GetRValue() etc.
This adds equivalent functions that don't have this problem and ugly
substitutions to make sure we don't use Get*Value() in the future */

static inline BYTE GetRValueSafe(COLORREF rgb)
{
    rgb = rgb & 0xff;
    return (BYTE)rgb;
}

static inline BYTE GetGValueSafe(COLORREF rgb)
{
    rgb = (rgb >> 8) & 0xff;
    return (BYTE)rgb;
}

static inline BYTE GetBValueSafe(COLORREF rgb)
{
    rgb = (rgb >> 16) & 0xff;
    return (BYTE)rgb;
}

#undef GetRValue
#define GetRValue UseGetRValueSafeInstead
#undef GetGValue
#define GetGValue UseGetGValueSafeInstead
#undef GetBValue
#define GetBValue UseGetBValueSafeInstead

#endif
