/* -*- C -*-
// -------------------------------------------------------------------
// MiniLock - a quick user space lock 
// Copyright (c) 2008  Leon Bottou. All rights reserved
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// ------------------------------------------------------------------- */

#ifndef ATOMIC_H
#define ATOMIC_H

/* ------------------------------------------------------------
//  These are primitives to implement very quick locks.
//  
//  Non-blocking usage:
//    if (atomicAcquire(&lock)) {
//      ... // do something protected by the lock
//      atomicRelease(&lock);
//    }
//
//  Blocking usage:
//    atomicAcquireOrSpin(&lock);
//    // do something protected by the lock
//    atomicRelease(&lock);
//
//  Rules of thumb:
//  - Acquire and release from the same function with 
//    no intervening function calls.
//  - Do not use AcquireOrSpin for waiting a long time.
//    No more than a few microseconds please.
//
//  Memory ordering:
//  Viewed from another processor
//  - load/stores performed by this cpu after the acquire 
//    cannot appear to have happened before the acquire.
//  - load/stores performed by this cpu before the release 
//    cannot appear to have happened after the release.
//
//  Implementation:
//  All depends on the definitions from the initial include file.
//  To perform the non blocking operations:
//  - use intel builtins if available (icc, gcc>=4.1).
//  - use win32 interlocked operations (win32).
//  - use inline assembly code for some platforms.
//  - use pthreads
//  To perform the waiting when spinning takes to long:
//  - use win32 critical sections and events.
//  - use pthreads mutex and conditions.
//  This is controlled by the preprocessor symbols:
//    WIN32 
//    __GNUC__ __GNUC_MAJOR__ __GNUC_MINOR__  
//    __INTEL_COMPILER
//  and can be overriden by defining
//    HAVE_INTEL_ATOMIC_BUILTINS
//    OBEY_HAVE_INTEL_ATOMIC_BUILTINS
//  and by tweaking the files include in atomic.h.
// ------------------------------------------------------------ */


# ifdef __cplusplus
extern "C" {
#endif
  
/* { int tmp = *lock; *lock = 1; return !tmp; }. */
int atomicAcquire(int volatile *lock);
  
/* { while (!atomicAcquire(lock)) { spin/yield/wait } } */
void atomicAcquireOrSpin(int volatile *lock);

/* { *lock = 0; } */
void atomicRelease(int volatile *lock);

/* { *var += 1; return *var; } */
int atomicIncrement(int volatile *var);

/* { *var -= 1; return *var; } */
int atomicDecrement(int volatile *var);

/* { if (*var == oldval) { *var = newval; return TRUE; } return FALSE; } */
int atomicCompareAndSwap(int volatile *var, int oldval, int newval);


# ifdef __cplusplus
}
#endif

#endif
