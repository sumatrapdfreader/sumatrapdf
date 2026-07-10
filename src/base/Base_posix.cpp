/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

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

// portable stand-in for Win32 GetTickCount64(): monotonic milliseconds.
u64 GetTickCount64() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((u64)ts.tv_sec * 1000) + ((u64)ts.tv_nsec / 1000000);
}
