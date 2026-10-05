/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include <unistd.h>

int AtomicRefCountAdd(AtomicRefCount* v) {
    return __atomic_add_fetch(v, 1, __ATOMIC_SEQ_CST);
}

int AtomicRefCountDec(AtomicRefCount* v) {
    return __atomic_sub_fetch(v, 1, __ATOMIC_SEQ_CST);
}

bool AtomicBoolGet(AtomicBool* p) {
    return __atomic_load_n(p, __ATOMIC_SEQ_CST) != 0;
}

void AtomicBoolSet(AtomicBool* p, bool v) {
    __atomic_store_n(p, v ? 1 : 0, __ATOMIC_SEQ_CST);
}

int AtomicIntGet(AtomicInt* p) {
    return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}

void AtomicIntSet(AtomicInt* p, int v) {
    __atomic_store_n(p, v, __ATOMIC_SEQ_CST);
}

int AtomicIntAdd(AtomicInt* p, int v) {
    return __atomic_add_fetch(p, v, __ATOMIC_SEQ_CST);
}

int AtomicIntInc(AtomicInt* p) {
    return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
}

int AtomicIntDec(AtomicInt* p) {
    return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST);
}

void* AtomicPtrGet(AtomicPtr* p) {
    return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}

void AtomicPtrSet(AtomicPtr* p, void* v) {
    __atomic_store_n(p, v, __ATOMIC_SEQ_CST);
}

// stores v and returns what was there before
void* AtomicPtrExchange(AtomicPtr* p, void* v) {
    return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST);
}

// portable stand-in for Win32 GetTickCount64(): monotonic milliseconds.
// milliseconds since some unspecified epoch, monotonic; a portable stand-in for
// the Win32 GetTickCount64() (which <windows.h> already declares on Windows).
u64 GetTickCount64() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((u64)ts.tv_sec * 1000) + ((u64)ts.tv_nsec / 1000000);
}

// milliseconds since the unix epoch (1970-01-01), for timestamps we persist.
// CLOCK_REALTIME, not the monotonic clock GetTickCount64() uses.
i64 UnixTimeMsNow() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ((i64)ts.tv_sec * 1000) + ((i64)ts.tv_nsec / 1000000);
}

// ng: the win32 name and representation, filled from CLOCK_REALTIME.
void GetSystemTimeAsFileTime(FILETIME* ft) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    u64 ticks = kFileTimeUnixEpoch + ((u64)ts.tv_sec * kFileTimeTicksPerSec) + (u64)ts.tv_nsec / 100;
    *ft = FileTimeFromU64(ticks);
}

UINT GetACP() {
    return CP_UTF8;
}

// ng: sysconf instead of GlobalMemoryStatusEx. Emscripten has neither, so
// there the caller sees "unknown" and assumes no memory pressure.
bool GetPhysMemoryInfo(u64* availPhysOut, int* loadPercentOut, u64* totalPhysOut) {
#if defined(_SC_PHYS_PAGES) && defined(_SC_AVPHYS_PAGES)
    long pageSize = sysconf(_SC_PAGESIZE);
    long total = sysconf(_SC_PHYS_PAGES);
    long avail = sysconf(_SC_AVPHYS_PAGES);
    if (pageSize <= 0 || total <= 0 || avail < 0) {
        return false;
    }
    *availPhysOut = (u64)avail * (u64)pageSize;
    *loadPercentOut = (int)(100 - (avail * 100 / total));
    if (totalPhysOut) {
        *totalPhysOut = (u64)total * (u64)pageSize;
    }
    return true;
#else
    (void)availPhysOut;
    (void)loadPercentOut;
    (void)totalPhysOut;
    return false;
#endif
}

// POSIX has no portable LC_MEASUREMENT query. Locale territories using the
// US customary system are enough to choose Letter instead of A4 here.
static int MeasurementSystemForLocale(Str locale) {
    for (int i = 0; i + 2 < len(locale); i++) {
        if (locale.s[i] != '_' && locale.s[i] != '-') {
            continue;
        }
        char a = (char)tolower((unsigned char)locale.s[i + 1]);
        char b = (char)tolower((unsigned char)locale.s[i + 2]);
        bool imperial = (a == 'u' && b == 's') || (a == 'l' && b == 'r') || (a == 'm' && b == 'm');
        return imperial ? 1 : 0;
    }
    return 0;
}

int GetMeasurementSystem() {
    const char* locale = getenv("LC_MEASUREMENT");
    if (!locale || !*locale) {
        locale = getenv("LC_ALL");
    }
    if (!locale || !*locale) {
        locale = getenv("LANG");
    }
    return MeasurementSystemForLocale(Str(locale));
}

bool MeasurementSystem_UnitTests() {
    return MeasurementSystemForLocale(StrL("en_US.UTF-8")) == 1 && MeasurementSystemForLocale(StrL("en-US")) == 1 &&
           MeasurementSystemForLocale(StrL("en_LR")) == 1 && MeasurementSystemForLocale(StrL("my_MM.UTF-8")) == 1 &&
           MeasurementSystemForLocale(StrL("pl_PL.UTF-8")) == 0 && MeasurementSystemForLocale(StrL("C.UTF-8")) == 0;
}
