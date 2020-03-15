#ifndef _WDL_ATOMIC_H_
#define _WDL_ATOMIC_H_

#include "wdltypes.h"

#ifdef _WIN32

static int wdl_atomic_incr(int *v) { return (int) InterlockedIncrement((LONG *)v); }
static int wdl_atomic_decr(int *v) { return (int) InterlockedDecrement((LONG *)v); }
static int wdl_atomic_incr(volatile int *v) { return (int) InterlockedIncrement((LONG *)v); }
static int wdl_atomic_decr(volatile int *v) { return (int) InterlockedDecrement((LONG *)v); }

#elif (!defined(__APPLE__) || !defined(__ppc__)) && (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))))

static WDL_STATICFUNC_UNUSED int wdl_atomic_incr(int *v) { return __sync_add_and_fetch(v,1); }
static WDL_STATICFUNC_UNUSED int wdl_atomic_decr(int *v) { return __sync_add_and_fetch(v,~0); }
static WDL_STATICFUNC_UNUSED int wdl_atomic_incr(volatile int *v) { return __sync_add_and_fetch(v,1); }
static WDL_STATICFUNC_UNUSED int wdl_atomic_decr(volatile int *v) { return __sync_add_and_fetch(v,~0); }

#elif defined(__APPLE__)
// used by GCC < 4.2 on OSX
#include <libkern/OSAtomic.h>

static WDL_STATICFUNC_UNUSED int wdl_atomic_incr(int *v) { return (int) OSAtomicIncrement32Barrier((int32_t*)v); }
static WDL_STATICFUNC_UNUSED int wdl_atomic_decr(int *v) { return (int) OSAtomicDecrement32Barrier((int32_t*)v); }
static WDL_STATICFUNC_UNUSED int wdl_atomic_incr(volatile int *v) { return (int) OSAtomicIncrement32Barrier((int32_t*)v); }
static WDL_STATICFUNC_UNUSED int wdl_atomic_decr(volatile int *v) { return (int) OSAtomicDecrement32Barrier((int32_t*)v); }
#else

// unsupported! 
#pragma message("Need win32 or apple or gcc 4.2+ for wdlatomic.h, doh")

#endif

#endif
