#include "common.h"

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
