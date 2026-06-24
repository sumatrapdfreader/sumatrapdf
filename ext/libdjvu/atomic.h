/* -*- C -*-
// -------------------------------------------------------------------
// Atomic primitives
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* This file defines macros or functions performing
// the following atomic operations with a full memory barrier. 
//
//   int atomicIncrement(int volatile *var) 
//   { *var += 1; return *var; } 
//   
//   int atomicDecrement(int volatile *var);
//   { *var -= 1; return *var; } 
//   
//   int atomicCompareAndSwap(int volatile *var, int oldval, int newval);
//   { int val = *var; if (val == oldval) { *var = newval };  returl val; }
//   
//   int atomicExchange(int volatile *var, int val);
//   { int tmp = *var; *var = val; return tmp; }
//   
//   void* atomicExchangePointer(void* volatile *var, int val);
//   { void* tmp = *var; *var = val; return tmp; }
*/

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(ATOMIC_MACROS) && defined(_WIN64)
# define ATOMIC_MACROS "WIN64"
# include <windows.h>
# define atomicIncrement(var) \
  (int)(InterlockedIncrement((LONG volatile*)(var)))
# define atomicDecrement(var) \
  (int)(InterlockedDecrement((LONG volatile*)(var)))
# define atomicCompareAndSwap(var,ov,nv) \
  (InterlockedCompareExchange((LONG volatile*)(var),(LONG)(nv),(LONG)(ov)))
# define atomicExchange(var,nv) \
  (int)(InterlockedExchange((LONG volatile*)(var),(LONG)(nv)))
# define atomicExchangePointer(var,nv) \
  (void*)(InterlockedExchangePointer((PVOID volatile*)(var),(PVOID)(nv)))
#endif

#if !defined(ATOMIC_MACROS) && defined(_WIN32)
# define ATOMIC_MACROS "WIN32"
# include <windows.h>
# define atomicIncrement(var) \
  (int)(InterlockedIncrement((LONG volatile*)(var)))
# define atomicDecrement(var) \
  (int)(InterlockedDecrement((LONG volatile*)(var)))
# define atomicCompareAndSwap(var,ov,nv) \
  (InterlockedCompareExchange((LONG volatile*)(var),(LONG)(nv),(LONG)(ov)))
# define atomicExchange(var,nv) \
  (int)(InterlockedExchange((LONG volatile*)(var),(LONG)(nv)))
# define atomicExchangePointer(var,nv) \
  (void*)(InterlockedExchange((LONG volatile*)(var),(LONG)(nv)))
#endif

#if !defined(ATOMIC_MACROS) && defined(HAVE_INTEL_ATOMIC_BUILTINS)
# define ATOMIC_MACROS "INTEL"
# define atomicIncrement(var) \
  (__sync_add_and_fetch((int volatile *)(var), 1))
# define atomicDecrement(var) \
  (__sync_add_and_fetch((int volatile *)(var), -1))
# define atomicCompareAndSwap(var,ov,nv) \
  (__sync_val_compare_and_swap((int volatile*)(var),(int)(ov),(int)(nv)))
# if defined(__i386__) || defined(__x86_64__) || defined(__amd64__)
#  define atomicExchange(var,nv) \
   (__sync_lock_test_and_set((int volatile*)(var),(int)(nv)))
#  define atomicExchangePointer(var,nv) \
   (__sync_lock_test_and_set((void* volatile*)(var),(void*)(nv)))
# else
  static inline int atomicExchange(int volatile *var, int nv) {
    int ov; do { ov = *var;  /* overkill */
    } while (! __sync_bool_compare_and_swap(var, ov, nv));
    return ov;
  }
  static inline void* atomicExchangePointer(void* volatile *var, void* nv) {
    void *ov; do { ov = *var;  /* overkill */
    } while (! __sync_bool_compare_and_swap(var, ov, nv));
    return ov;
  }
# endif
#endif

#if !defined(ATOMIC_MACROS) && defined(__GNUC__)
# if defined(__i386__) || defined(__amd64__) || defined(__x86_64__)
#  define ATOMIC_MACROS "GNU86"
  static inline int atomicIncrement(int volatile *var) {
    int ov; __asm__ __volatile__ ("lock; xaddl %0, %1" 
          : "=r" (ov), "=m" (*var) : "0" (1), "m" (*var) : "cc" );
    return ov + 1;
  }
  static inline int atomicDecrement(int volatile *var) {
    int ov; __asm__ __volatile__ ("lock; xaddl %0, %1" 
         : "=r" (ov), "=m" (*var) : "0" (-1), "m" (*var) : "cc" );
    return ov - 1;
  }
  static inline int atomicExchange(int volatile *var, int nv) {
    int ov; __asm__ __volatile__ ("xchgl %0, %1"
        : "=r" (ov), "=m" (*var) : "0" (nv), "m" (*var)); 
    return ov; 
  }
  static inline int atomicCompareAndSwap(int volatile *var, int ov, int nv) {
    int rv; __asm __volatile ("lock; cmpxchgl %2, %1"
        : "=a" (rv), "=m" (*var) : "r" (nv), "0" (ov), "m" (*var) : "cc");
    return rv;
  }
  static inline void *atomicExchangePointer(void * volatile *var, void *nv) {
    void *ov;  __asm__ __volatile__ (
#  if defined(__x86_64__) || defined(__amd64__)
         "xchgq %0, %1"
#  else
         "xchgl %0, %1"
#  endif
         : "=r" (ov), "=m" (*var) : "0" (nv), "m" (*var)); 
    return ov; 
  }
# endif
#endif


#ifndef ATOMIC_MACROS
  /* emulation */
  extern int atomicIncrement(int volatile *var);
  extern int atomicDecrement(int volatile *var);
  extern int atomicCompareAndSwap(int volatile *var, int ov, int nv);
  extern int atomicExchange(int volatile *var, int nv);
  extern void* atomicExchangePointer(void* volatile *var, void* nv);
#endif

  
# ifdef __cplusplus
}
# endif

#endif
