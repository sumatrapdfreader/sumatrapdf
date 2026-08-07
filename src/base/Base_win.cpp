/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

int AtomicRefCountAdd(AtomicRefCount* v) {
    return (int)InterlockedIncrement(v);
}

int AtomicRefCountDec(AtomicRefCount* v) {
    return (int)InterlockedDecrement(v);
}

bool AtomicBoolGet(AtomicBool* p) {
    return InterlockedOr(p, 0) != 0;
}

void AtomicBoolSet(AtomicBool* p, bool v) {
    InterlockedExchange(p, v ? 1 : 0);
}

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

void* AtomicPtrGet(AtomicPtr* p) {
    // comparing nullptr against nullptr never stores, so this is just an
    // atomic read - there is no InterlockedGetPointer
    return InterlockedCompareExchangePointer(p, nullptr, nullptr);
}

void AtomicPtrSet(AtomicPtr* p, void* v) {
    InterlockedExchangePointer(p, v);
}

// stores v and returns what was there before
void* AtomicPtrExchange(AtomicPtr* p, void* v) {
    return InterlockedExchangePointer(p, v);
}
