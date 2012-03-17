/* -*- C -*-
// -------------------------------------------------------------------
// MiniLock - a quick mostly user space lock 
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include "atomic.h"
#if defined(WIN32)
# include <windows.h>
#endif

#include "GThreads.h"
// #include <pthread.h>
// #include <QMutex>
// #include <QWaitCondition>

#define OBEY_HAVE_INTEL_ATOMIC_BUILTINS 1


/* ============================================================ 
// PART1 - THE WAITING .
// This part must define the four macros MUTEX_ENTER,
// MUTEX_LEAVE, COND_WAIT and COND_WAKEALL working
// on a single monitor in a way that is consistent with
// the pthread semantics. 
*/


#if defined(WIN32)
# define USE_WINDOWS_WAIT 1
#elif defined(__cplusplus) && defined(_GTHREADS_H_)
# define USE_GTHREAD_WAIT 1
#elif defined(__cplusplus) && defined(QMUTEX_H)
# define USE_QT4_WAIT 1
#elif defined(PTHREAD_MUTEX_INITIALIZER)
# define USE_PTHREAD_WAIT 1
#endif


#if USE_PTHREAD_WAIT
static pthread_mutex_t ptm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  ptc = PTHREAD_COND_INITIALIZER;
# define MUTEX_ENTER  pthread_mutex_lock(&ptm)
# define MUTEX_LEAVE  pthread_mutex_unlock(&ptm)
# define COND_WAIT    pthread_cond_wait(&ptc,&ptm)
# define COND_WAKEALL pthread_cond_broadcast(&ptc)
#endif


#if USE_GTHREAD_WAIT
static GMonitor m;
# define MUTEX_ENTER  m.enter()
# define MUTEX_LEAVE  m.leave()
# define COND_WAIT    m.wait()
# define COND_WAKEALL m.broadcast()
#endif


#if USE_QT4_WAIT
static QMutex qtm;
static QWaitCondition qtc;
# define MUTEX_ENTER  qtm.lock()
# define MUTEX_LEAVE  qtm.unlock()
# define COND_WAIT    qtc.wait(&qtm)
# define COND_WAKEALL qtc.wakeAll()
#endif


#if USE_WINDOWS_WAIT
static LONG ini = 0;
static CRITICAL_SECTION cs;
static HANDLE ev = 0;
static void mutex_enter()
{
  if (!InterlockedExchange(&ini, 1))
    {
      InitializeCriticalSection(&cs);
      ev = CreateEvent(NULL, TRUE, FALSE, NULL);
      assert(ev);
    }
  EnterCriticalSection(&cs);
}
static void cond_wait()
{
  ResetEvent(&ev);
  LeaveCriticalSection(&cs);
  WaitForSingleObject(ev, INFINITE);
  EnterCriticalSection(&cs);
}
# define MUTEX_ENTER mutex_enter()
# define MUTEX_LEAVE LeaveCriticalSection(&cs)
# define COND_WAIT   cond_wait()
# define COND_WAKEALL SetEvent(ev)
#endif

#if ! defined(COND_WAKEALL) || ! defined(COND_WAIT)
# error "Could not select suitable waiting code"
#endif


/* ============================================================ 
// PART2 - ATOMIC PRIMITIVES
// This part should define very fast SYNC_XXX and SYNC_REL
// macros that perform atomic operations.
// Intel builtins functions are very nice. 
// Windows interlocked functions are nice.
// Otherwise we have to use assembly code.
// When these are not defined we simply use
// the monitor macros to implement 
// slow replacement functions.
*/


#ifndef OBEY_HAVE_INTEL_ATOMIC_BUILTINS
# if defined(__INTEL_COMPILER)
#  define USE_INTEL_ATOMIC_BUILTINS 1
# elif defined(__GNUC__) && (__GNUC__ == 4) && (__GNUC_MINOR__>= 1)
#  define USE_INTEL_ATOMIC_BUILTINS 1
# endif
#endif
#if HAVE_INTEL_ATOMIC_BUILTINS
# define USE_INTEL_ATOMIC_BUILTINS 1
#elif defined(WIN32) && !defined(USE_WIN32_INTERLOCKED)
# define USE_WIN32_INTERLOCKED 1
#elif defined(__GNUC__) && defined(__i386__)
# define USE_GCC_I386_ASM 1
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__amd64__))
# define USE_GCC_I386_ASM 1
#elif defined(__GNUC__) && (defined(__ppc__) || defined(__powerpc__))
# define USE_GCC_PPC_ASM 1
#endif


#if USE_INTEL_ATOMIC_BUILTINS && !HAVE_SYNC
# define SYNC_ACQ(l)     (! __sync_lock_test_and_set(l, 1))
# define SYNC_REL(l)     (__sync_lock_release(l))
# define SYNC_INC(l)     (__sync_add_and_fetch(l, 1))
# define SYNC_DEC(l)     (__sync_add_and_fetch(l, -1))
# define SYNC_CAS(l,o,n) (__sync_bool_compare_and_swap(l,o,n))
# define HAVE_SYNC 1
#endif


#if USE_WIN32_INTERLOCKED && !HAVE_SYNC
# define SYNC_ACQ(l) \
  (!InterlockedExchange((LONG volatile *)(l),1))
# if defined(_M_ALPHA) || defined(_M_PPC) || defined(_M_IA64)
#  define SYNC_REL(l) \
  (InterlockedExchange((LONG volatile *)(l),0))
# else
#  define SYNC_REL(l) \
  (*(int volatile *)(l)=0)
# endif
# define SYNC_INC(l) \
  (InterlockedIncrement((LONG volatile *)(l)))
# define SYNC_DEC(l) \
  (InterlockedDecrement((LONG volatile *)(l)))
# define SYNC_CAS(l,o,n) \
  (InterlockedCompareExchange((LONG volatile *)(l),n,o)==(o))
# define HAVE_SYNC 1
#endif


#if USE_GCC_I386_ASM && !HAVE_SYNC
static int xchgl(int volatile *atomic, int newval) 
{
  int oldval;
  __asm__ __volatile__ ("xchgl %0, %1"
			: "=r" (oldval), "+m" (*atomic)
			: "0" (newval), "m" (*atomic)); 
  return oldval; 
}
static int xaddl(int volatile *atomic, int add) 
{
  int val; /* This works for the 486 and later */
  __asm__ __volatile__("lock; xaddl %0, %1" 
                       : "=r" (val), "+m" (*atomic) 
                       : "m" (*atomic), "0" (add) );
  return val;
}
static int cmpxchglf(int volatile *atomic, int oldval, int newval)
{
  int ret;
  __asm __volatile ("lock; cmpxchgl %2, %1\n"
                    "sete %%al; movzbl %%al,%0"
                    : "=a" (ret), "=m" (*atomic)
                    : "r" (newval), "m" (*atomic), "0" (oldval));
  return ret;
}
# define SYNC_ACQ(l)     (! xchgl(l,1))
# define SYNC_REL(l)     (*(int volatile *)l = 0)
# define SYNC_INC(l)     (xaddl(l, 1) + 1)
# define SYNC_DEC(l)     (xaddl(l, -1) - 1)
# define SYNC_CAS(l,o,n) (cmpxchglf(l,o,n))
# define HAVE_SYNC 1
#endif


#if USE_GCC_PPC_ASM && !HAVE_SYNC
static int xchg_acq(int volatile *atomic, int newval) 
{
  int oldval;
  __asm __volatile ("1: lwarx   %0,0,%2\n"
                    "   stwcx.  %3,0,%2\n"
                    "   bne-    1b\n"
                    "   isync"
                    : "=&r" (oldval), "+m" (*atomic)
                    : "b" (atomic), "r" (newval), "m" (*atomic)
                    : "cr0", "memory");
  return oldval;
}
static void st_rel(int volatile *atomic, int newval)
{
  __asm __volatile ("sync" ::: "memory");
  *atomic = newval;
}
static int addlx(int volatile *atomic, int add) 
{
  int val;
  __asm __volatile ("1: lwarx  %0,0,%2\n"
                    "   add    %0,%0,%3\n"
                    "   stwcx. %0,0,%2\n"
                    "   bne-   1b"
                    : "=&b" (val), "+m" (*atomic)  
                    : "b" (atomic), "r" (add), "m" (*atomic)           
                    : "cr0", "memory");
  return val;
}
static int cmpxchgl(int volatile *atomic, int oldval, int newval)
{
  int ret;
  __asm __volatile ("   sync\n"
                    "1: lwarx  %0,0,%1\n"
                    "   cmpw   %0,%2\n"
                    "   bne    2f\n"
                    "   stwcx. %3,0,%1\n"
                    "   bne-   1b\n"
                    "2: isync"
                    : "=&r" (ret)
                    : "b" (atomic), "r" (oldval), "r" (newval)
                    : "cr0", "memory");
  return ret;
}
# define SYNC_ACQ(l)     (!xchg_acq(l,1))
# define SYNC_REL(l)     (st_rel(l,0))
# define SYNC_INC(l)     (addlx(l, 1))
# define SYNC_DEC(l)     (addlx(l, -1))
# define SYNC_CAS(l,o,n) (cmpxchgl(l,o,n)==o)
# define HAVE_SYNC 1
#endif


/* ============================================================ 
// PART3 - THE IMPLEMENTATION
*/

#if HAVE_SYNC

/* We have fast synchronization */

int volatile nwaiters = 0;
int volatile dummy;

int 
atomicAcquire(int volatile *lock)
{
  return SYNC_ACQ(lock);
}
    
void 
atomicAcquireOrSpin(int volatile *lock)
{
  int spin = 16;
  while (spin >= 0 && ! SYNC_ACQ(lock))
    spin -= 1;
  if (spin < 0)
    {
      MUTEX_ENTER;
      nwaiters += 1;
      while (! SYNC_ACQ(lock))
        COND_WAIT;
      nwaiters -= 1;
      MUTEX_LEAVE;
    }
}

void 
atomicRelease(int volatile *lock)
{
  SYNC_REL(lock);
  if (nwaiters > 0)
    {
      MUTEX_ENTER;
      if (nwaiters > 0)
        COND_WAKEALL;
      MUTEX_LEAVE;
    }
}

int
atomicIncrement(int volatile *var)
{
  return SYNC_INC(var);
}

int 
atomicDecrement(int volatile *var)
{
  return SYNC_DEC(var);
}

int 
atomicCompareAndSwap(int volatile *var, int oldval, int newval)
{
  return SYNC_CAS(var,oldval,newval);
}



#else

int 
atomicAcquire(int volatile *lock)
{
  int tmp;
  MUTEX_ENTER;
  if ((tmp = !*lock))
    *lock = 1;
  MUTEX_LEAVE;
  return tmp;
}

void 
atomicAcquireOrSpin(int volatile *lock)
{
  MUTEX_ENTER;
  while (*lock)
    COND_WAIT;
  *lock = 1;
  MUTEX_LEAVE;
}

void 
atomicRelease(int volatile *lock)
{
  MUTEX_ENTER;
  *lock = 0;
  COND_WAKEALL;
  MUTEX_LEAVE;
}

int
atomicIncrement(int volatile *var)
{
  int res;
  MUTEX_ENTER;
  res = ++(*var);
  MUTEX_LEAVE;
  return res;
}

int 
atomicDecrement(int volatile *var)
{
  int res;
  MUTEX_ENTER;
  res = --(*var);
  MUTEX_LEAVE;
  return res;
}

int 
atomicCompareAndSwap(int volatile *var, int oldval, int newval)
{
  int ret;
  MUTEX_ENTER;
  ret = *var;
  if (ret == oldval)
    *var = newval;
  MUTEX_LEAVE;
  return (ret == oldval);
}

#endif  /* HAVE_SYNC */


