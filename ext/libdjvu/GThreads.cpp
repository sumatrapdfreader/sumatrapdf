//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjVuLibre-3.5
//C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
//C- Copyright (c) 2001  AT&T
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either Version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C- 
//C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
//C- Lizardtech Software.  Lizardtech Software has authorized us to
//C- replace the original DjVu(r) Reference Library notice by the following
//C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
//C-
//C-  ------------------------------------------------------------------
//C- | DjVu (r) Reference Library (v. 3.5)
//C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
//C- | The DjVu Reference Library is protected by U.S. Pat. No.
//C- | 6,058,214 and patents pending.
//C- |
//C- | This software is subject to, and may be distributed under, the
//C- | GNU General Public License, either Version 2 of the license,
//C- | or (at your option) any later version. The license should have
//C- | accompanied the software or you may obtain a copy of the license
//C- | from the Free Software Foundation at http://www.fsf.org .
//C- |
//C- | The computer code originally released by LizardTech under this
//C- | license and unmodified by other parties is deemed "the LIZARDTECH
//C- | ORIGINAL CODE."  Subject to any third party intellectual property
//C- | claims, LizardTech grants recipient a worldwide, royalty-free, 
//C- | non-exclusive license to make, use, sell, or otherwise dispose of 
//C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the 
//C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU 
//C- | General Public License.   This grant only confers the right to 
//C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to 
//C- | the extent such infringement is reasonably necessary to enable 
//C- | recipient to make, have made, practice, sell, or otherwise dispose 
//C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to 
//C- | any greater extent that may be necessary to utilize further 
//C- | modifications or combinations.
//C- |
//C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
//C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
//C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//C- +------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation
#endif

// This file defines machine independent classes
// for running and synchronizing threads.
// - Author: Leon Bottou, 01/1998

// From: Leon Bottou, 1/31/2002
// Almost unchanged by Lizardtech.
// GSafeFlags should go because it not as safe as it claims.

#include "GThreads.h"
#include "GException.h"
#include "DjVuMessageLite.h"
#include <stdlib.h>
#include <stdio.h>

// ----------------------------------------
// Consistency check

#if THREADMODEL!=NOTHREADS
#ifdef USE_EXCEPTION_EMULATION
#warning "Compiler must support thread safe exceptions"
#endif //USE_EXCEPTION_EMULATION
#if defined(__GNUC__)
#if (__GNUC__<2) || ((__GNUC__==2) && (__GNUC_MINOR__<=8))
#warning "GCC 2.8 exceptions are not thread safe."
#warning "Use properly configured EGCS-1.1 or greater."
#endif // (__GNUC__<2 ...
#endif // defined(__GNUC__)
#endif // THREADMODEL!=NOTHREADS

#ifndef _DEBUG
#if defined(DEBUG) 
#define _DEBUG /* */
#elif DEBUGLVL >= 1
#define _DEBUG /* */
#endif
#endif

#if THREADMODEL==WINTHREADS
# include <process.h>
#endif
#if THREADMODEL==COTHREADS
# include <setjmp.h>
# include <string.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/time.h>
#endif


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


// ----------------------------------------
// NOTHREADS
// ----------------------------------------

#if THREADMODEL==NOTHREADS
int
GThread::create( void (*entry)(void*), void *arg)
{
  (*entry)(arg);
  return 0;
}
#endif


// ----------------------------------------
// WIN32 IMPLEMENTATION
// ----------------------------------------

#if THREADMODEL==WINTHREADS

static unsigned __stdcall 
start(void *arg)
{
  GThread *gt = (GThread*)arg;
  try 
    {
      G_TRY
        {
          gt->xentry( gt->xarg );
        }
      G_CATCH(ex)
        {
          ex.perror();
          DjVuMessageLite::perror( ERR_MSG("GThreads.uncaught") );
#ifdef _DEBUG
          abort();
#endif
        }
      G_ENDCATCH;
    }
  catch(...)
    {
      DjVuMessageLite::perror( ERR_MSG("GThreads.unrecognized") );
#ifdef _DEBUG
      abort();
#endif
    }
  return 0;
}

GThread::GThread(int stacksize)
  : hthr(0), thrid(0), xentry(0), xarg(0)
{
}

GThread::~GThread()
{
  if (hthr)
    CloseHandle(hthr);
  hthr = 0;
  thrid = 0;
}

int  
GThread::create(void (*entry)(void*), void *arg)
{
  if (hthr)
    return -1;
  xentry = entry;
  xarg = arg;
  unsigned uthread = 0;
  hthr = (HANDLE)_beginthreadex(NULL, 0, start, (void*)this, 0, &uthread);
  thrid = (DWORD) uthread;
  if (hthr)
    return 0;
  return -1;
}

void 
GThread::terminate()
{
  OutputDebugString("Terminating thread.\n");
  if (hthr)
    TerminateThread(hthr,0);
}

int
GThread::yield()
{
  Sleep(0);
  return 0;
}

void *
GThread::current()
{
  return (void*) GetCurrentThreadId();
}

struct thr_waiting {
  struct thr_waiting *next;
  struct thr_waiting *prev;
  BOOL   waiting;
  HANDLE gwait;
};

GMonitor::GMonitor()
  : ok(0), count(1), head(0), tail(0)
{
  InitializeCriticalSection(&cs);
  locker = GetCurrentThreadId();
  ok = 1;
}

GMonitor::~GMonitor()
{
  ok = 0;
  EnterCriticalSection(&cs);
  for (struct thr_waiting *w=head; w; w=w->next)
    SetEvent(w->gwait);
  LeaveCriticalSection(&cs);
  DeleteCriticalSection(&cs); 
}

void 
GMonitor::enter()
{
  DWORD self = GetCurrentThreadId();
  if (count>0 || self!=locker)
    {
      if (ok)
        EnterCriticalSection(&cs);
      locker = self;
      count = 1;
    }
  count -= 1;
}

void 
GMonitor::leave()
{
  DWORD self = GetCurrentThreadId();
  if (ok && (count>0 || self!=locker))
    G_THROW( ERR_MSG("GThreads.not_acq_broad") );
  count += 1;
  if (count > 0)
    {
      count = 1;
      if (ok)
        LeaveCriticalSection(&cs);
    }
}

void
GMonitor::signal()
{
  if (ok)
    {
      DWORD self = GetCurrentThreadId();
      if (count>0 || self!=locker)
        G_THROW( ERR_MSG("GThreads.not_acq_signal") );
      for (struct thr_waiting *w=head; w; w=w->next)
        if (w->waiting) 
          {
            SetEvent(w->gwait);
            w->waiting = FALSE;
            break; // Only one thread is allowed to run!
          }
    }
}

void
GMonitor::broadcast()
{
  if (ok)
    {
      DWORD self = GetCurrentThreadId();
      if (count>0 || self!=locker)
        G_THROW( ERR_MSG("GThreads.not_acq_broad") );
      for (struct thr_waiting *w=head; w; w=w->next)
        if (w->waiting)
            {
              SetEvent(w->gwait);
              w->waiting = FALSE;
            }
    }
}

void
GMonitor::wait()
{
  // Check state
  DWORD self = GetCurrentThreadId();
  if (count>0 || self!=locker)
    G_THROW( ERR_MSG("GThreads.not_acq_wait") );
  // Wait
  if (ok)
    {
      // Prepare wait record
      struct thr_waiting waitrec;
      waitrec.waiting = TRUE;
      waitrec.gwait = CreateEvent(NULL,FALSE,FALSE,NULL);
      waitrec.next = 0;
      waitrec.prev = tail;
      // Link wait record (protected by critical section)
      *(waitrec.next ? &waitrec.next->prev : &tail) = &waitrec; 
      *(waitrec.prev ? &waitrec.prev->next : &head) = &waitrec;
      // Start wait
      int sav_count = count;
      count = 1;
      LeaveCriticalSection(&cs);
      WaitForSingleObject(waitrec.gwait,INFINITE);
      // Re-acquire
      EnterCriticalSection(&cs);
      count = sav_count;
      locker = self;
      // Unlink wait record
      *(waitrec.next ? &waitrec.next->prev : &tail) = waitrec.prev;
      *(waitrec.prev ? &waitrec.prev->next : &head) = waitrec.next;
      CloseHandle(waitrec.gwait);
    }
}

void
GMonitor::wait(unsigned long timeout) 
{
  // Check state
  DWORD self = GetCurrentThreadId();
  if (count>0 || self!=locker)
    G_THROW( ERR_MSG("GThreads.not_acq_wait") );
  // Wait
  if (ok)
    {
      // Prepare wait record
      struct thr_waiting waitrec;
      waitrec.waiting = TRUE;
      waitrec.gwait = CreateEvent(NULL,FALSE,FALSE,NULL);
      waitrec.next = 0;
      waitrec.prev = tail;
      // Link wait record (protected by critical section)
      *(waitrec.prev ? &waitrec.prev->next : &head) = &waitrec;
      *(waitrec.next ? &waitrec.next->prev : &tail) = &waitrec; 
      // Start wait
      int sav_count = count;
      count = 1;
      LeaveCriticalSection(&cs);
      WaitForSingleObject(waitrec.gwait,timeout);
      // Re-acquire
      EnterCriticalSection(&cs);
      count = sav_count;
      locker = self;
      // Unlink wait record
      *(waitrec.next ? &waitrec.next->prev : &tail) = waitrec.prev;
      *(waitrec.prev ? &waitrec.prev->next : &head) = waitrec.next;
      CloseHandle(waitrec.gwait);
    }
}

#endif



// ----------------------------------------
// MACTHREADS IMPLEMENTATION (obsolete)
// ----------------------------------------

#if THREADMODEL==MACTHREADS

// Doubly linked list of waiting threads
struct thr_waiting {
  struct thr_waiting *next;     // ptr to next waiting thread record
  struct thr_waiting *prev;     // ptr to ptr to this waiting thread
  unsigned long thid;           // id of waiting thread
  int *wchan;                   // cause of the wait
};
static struct thr_waiting *first_waiting_thr = 0;
static struct thr_waiting *last_waiting_thr = 0;


// Stops current thread. 
// Argument ``self'' must be current thread id.
// Assumes ``ThreadBeginCritical'' has been called before.
static void
macthread_wait(ThreadID self, int *wchan)
{
  // Prepare and link wait record
  struct thr_waiting wait; // no need to malloc :-)
  wait.thid = self;
  wait.wchan = wchan;
  wait.next = 0;
  wait.prev = last_waiting_thr;
  *(wait.prev ? &wait.prev->next : &first_waiting_thr ) = &wait;
  *(wait.next ? &wait.next->prev : &last_waiting_thr ) = &wait;
  // Leave critical section and start waiting.
  (*wchan)++;
  SetThreadStateEndCritical(self, kStoppedThreadState, kNoThreadID);
  // The Apple documentation says that the above call reschedules a new
  // thread.  Therefore it will only return when the thread wakes up.
  ThreadBeginCritical();
  (*wchan)--;
  // Unlink wait record
  *(wait.prev ? &wait.prev->next : &first_waiting_thr ) = wait.next;
  *(wait.next ? &wait.next->prev : &last_waiting_thr ) = wait.prev;
  // Returns from the wait.
}

// Wakeup one thread or all threads waiting on cause wchan
static void
macthread_wakeup(int *wchan, int onlyone)
{
  if (*wchan == 0)
    return;
  for (struct thr_waiting *q=first_waiting_thr; q; q=q->next)
    if (q->wchan == wchan) {
      // Found a waiting thread
      q->wchan = 0;
      SetThreadState(q->thid, kReadyThreadState, kNoThreadID);
      if (onlyone)
        return;
    }
}

GThread::GThread(int stacksize) 
  : thid(kNoThreadID), xentry(0), xarg(0)
{
}

GThread::~GThread(void)
{
  thid = kNoThreadID;
}

pascal void *
GThread::start(void *arg)
{
  GThread *gt = (GThread*)arg;
  try 
    {
      G_TRY
        {
          (gt->xentry)(gt->xarg);
        }
      G_CATCH(ex)
        {
          ex.perror();
          DjVuMessageLite::perror( ERR_MSG("GThreads.uncaught") );
#ifdef _DEBUG
          abort();
#endif
        }
      G_ENDCATCH;
    }
  catch(...)
    {
      DjVuMessageLite::perror( ERR_MSG("GThreads.unrecognized") );
#ifdef _DEBUG
      abort();
#endif
    }
  return 0;
}

int
GThread::create(void (*entry)(void*), void *arg)
{
  if (xentry || thid!=kNoThreadID)
    return -1;
  xentry = entry;
  xarg = arg;
  int err = NewThread( kCooperativeThread, GThread::start , this, 0,
                       kCreateIfNeeded, (void**)nil, &thid );
  if( err != noErr )
    return err;
  return 0;
}

void
GThread::terminate()
{
  if (thid != kNoThreadID) {
    DisposeThread( thid, NULL, false );
    thid = kNoThreadID;
  }
}

int
GThread::yield()
{
  YieldToAnyThread();
  return 0;
}

void*
GThread::current()
{
  unsigned long thid = kNoThreadID;
  GetCurrentThread(&thid);
  return (void*) thid;
}


// GMonitor implementation
GMonitor::GMonitor() 
  : ok(0), count(1), locker(0), wlock(0), wsig(0)
{
  locker = kNoThreadID;
  ok = 1;
}

GMonitor::~GMonitor() 
{
  ok = 0;
  ThreadBeginCritical();
  macthread_wakeup(&wsig, 0);
  macthread_wakeup(&wlock, 0);
  ThreadEndCritical();
  YieldToAnyThread();
}

void 
GMonitor::enter() 
{
  ThreadID self;
  GetCurrentThread(&self);
  ThreadBeginCritical();
  if (count>0 || self!=locker)
    {
      while (ok && count<=0)
        macthread_wait(self, &wlock);
      count = 1;
      locker = self;
    }
  count -= 1;
  ThreadEndCritical();
}

void 
GMonitor::leave() 
{
  ThreadID self;
  GetCurrentThread(&self);
  if (ok && (count>0 || self!=locker))
    G_THROW( ERR_MSG("GThreads.not_acq_leave") );
  ThreadBeginCritical();
  if (++count > 0)
    macthread_wakeup(&wlock, 1);
  ThreadEndCritical();
}

void 
GMonitor::signal() 
{
  ThreadID self;
  GetCurrentThread(&self);
  if (count>0 || self!=locker)
    G_THROW( ERR_MSG("GThreads.not_acq_signal") );
  ThreadBeginCritical();
  macthread_wakeup(&wsig, 1);
  ThreadEndCritical();
}

void 
GMonitor::broadcast() 
{
  ThreadID self;
  GetCurrentThread(&self);
  if (count>0 || self!=locker)
    G_THROW( ERR_MSG("GThreads.not_acq_broad") );
  ThreadBeginCritical();
  macthread_wakeup(&wsig, 0);
  ThreadEndCritical();
}

void 
GMonitor::wait() 
{
  // Check state
  ThreadID self;
  GetCurrentThread(&self);
  if (count>0 || locker!=self)
    G_THROW( ERR_MSG("GThreads.not_acq_wait") );
  // Wait
  if (ok)
    {
      // Atomically release monitor and wait
      ThreadBeginCritical();
      int sav_count = count;
      count = 1;
      macthread_wakeup(&wlock, 1);
      macthread_wait(self, &wsig);
      // Re-acquire
      while (ok && count<=0)
        macthread_wait(self, &wlock);
      count = sav_count;
      locker = self;
      ThreadEndCritical();
    }
}

void 
GMonitor::wait(unsigned long timeout) 
{
  // Timeouts are not used for anything important.
  // Just ignore the timeout and wait the regular way.
  if (timeout > 0)
    wait();
}

#endif



// ----------------------------------------
// POSIXTHREADS IMPLEMENTATION
// ----------------------------------------

#if THREADMODEL==POSIXTHREADS

#if defined(CMA_INCLUDE)
#define DCETHREADS
#define pthread_key_create pthread_keycreate
#else
#define pthread_mutexattr_default  NULL
#define pthread_condattr_default   NULL
#endif


void *
GThread::start(void *arg)
{
  GThread *gt = (GThread*)arg;
#ifdef DCETHREADS
#ifdef CANCEL_ON
  pthread_setcancel(CANCEL_ON);
  pthread_setasynccancel(CANCEL_ON);
#endif
#else // !DCETHREADS
#ifdef PTHREAD_CANCEL_ENABLE
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
#endif
#ifdef PTHREAD_CANCEL_ASYNCHRONOUS
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
#endif
#endif
  // Catch exceptions
#ifdef __EXCEPTIONS
  try 
    {
#endif 
      G_TRY
        {
          (gt->xentry)(gt->xarg);
        }
      G_CATCH(ex)
        {
          ex.perror();
          DjVuMessageLite::perror( ERR_MSG("GThreads.uncaught") );
#ifdef _DEBUG
          abort();
#endif
        }
      G_ENDCATCH;
#ifdef __EXCEPTIONS
    }
  catch(...)
    {
          DjVuMessageLite::perror( ERR_MSG("GThreads.unrecognized") );
#ifdef _DEBUG
      abort();
#endif
    }
#endif
  return 0;
}


// GThread

GThread::GThread(int stacksize) : 
  hthr(0), xentry(0), xarg(0)
{
}

GThread::~GThread()
{
  hthr = 0;
}

int  
GThread::create(void (*entry)(void*), void *arg)
{
  if (xentry || xarg)
    return -1;
  xentry = entry;
  xarg = arg;
#ifdef DCETHREADS
  int ret = pthread_create(&hthr, pthread_attr_default, GThread::start, (void*)this);
  if (ret >= 0)
    pthread_detach(hthr);
#else
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int ret = pthread_create(&hthr, &attr, start, (void*)this);
  pthread_attr_destroy(&attr);
#endif
  return ret;
}

void 
GThread::terminate()
{
  if (xentry || xarg)
    pthread_cancel(hthr);
}

int
GThread::yield()
{
#ifdef DCETHREADS
  pthread_yield();
#else
  // should use sched_yield() when available.
  static struct timeval timeout = { 0, 0 };
  ::select(0, 0,0,0, &timeout);
#endif
  return 0;
}

void*
GThread::current()
{
  pthread_t self = pthread_self();
#if defined(pthread_getunique_np)
  return (void*) pthread_getunique_np( & self );
#elif defined(cma_thread_get_unique)
  return (void*) cma_thread_get_unique( & self );  
#else
  return (void*) self;
#endif
}

// -- GMonitor

GMonitor::GMonitor()
  : ok(0), count(1), locker(0)
{
  // none of this should be necessary ... in theory.
#ifdef PTHREAD_MUTEX_INITIALIZER
  static pthread_mutex_t tmutex=PTHREAD_MUTEX_INITIALIZER;
  memcpy(&mutex,&tmutex,sizeof(mutex));
#endif
#ifdef PTHREAD_COND_INITIALIZER
  static pthread_cond_t tcond=PTHREAD_COND_INITIALIZER;
  memcpy(&cond,&tcond,sizeof(cond));
#endif
  // standard
  pthread_mutex_init(&mutex, pthread_mutexattr_default);
  pthread_cond_init(&cond, pthread_condattr_default); 
  locker = pthread_self();
  ok = 1;
}

GMonitor::~GMonitor()
{
  ok = 0;
  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&mutex); 
}


void 
GMonitor::enter()
{
  pthread_t self = pthread_self();
  if (count>0 || !pthread_equal(locker, self))
    {
      if (ok)
        pthread_mutex_lock(&mutex);
      locker = self;
      count = 1;
    }
  count -= 1;
}

void 
GMonitor::leave()
{
  static pthread_t pthread_null;
  pthread_t self = pthread_self();
  if (ok && (count>0 || !pthread_equal(locker, self)))
    G_THROW( ERR_MSG("GThreads.not_acq_broad") );
  count += 1;
  if (count > 0)
    {
      count = 1;
      locker = pthread_null;
      if (ok)
        pthread_mutex_unlock(&mutex);
    }
}

void
GMonitor::signal()
{
  if (ok)
    {
      pthread_t self = pthread_self();
      if (count>0 || !pthread_equal(locker, self))
        G_THROW( ERR_MSG("GThreads.not_acq_signal") );
      pthread_cond_signal(&cond);
    }
}

void
GMonitor::broadcast()
{
  if (ok)
    {
      pthread_t self = pthread_self();
      if (count>0 || !pthread_equal(locker, self))
        G_THROW( ERR_MSG("GThreads.not_acq_broad") );
      pthread_cond_broadcast(&cond);
    }
}

void
GMonitor::wait()
{
  // Check
  pthread_t self = pthread_self();
  if (count>0 || !pthread_equal(locker, self))
    G_THROW( ERR_MSG("GThreads.not_acq_wait") );
  // Wait
  if (ok)
    {
      // Release
      int sav_count = count;
      count = 1;
      // Wait
      pthread_cond_wait(&cond, &mutex);
      // Re-acquire
      count = sav_count;
      locker = self;
    }      
}

void
GMonitor::wait(unsigned long timeout) 
{
  // Check
  pthread_t self = pthread_self();
  if (count>0 || !pthread_equal(locker, self))
    G_THROW( ERR_MSG("GThreads.not_acq_wait") );
  // Wait
  if (ok)
    {
      // Release
      int sav_count = count;
      count = 1;
      // Wait
      struct timeval  abstv;
      struct timespec absts;
      gettimeofday(&abstv, NULL); // grrr
      absts.tv_sec = abstv.tv_sec + timeout/1000;
      absts.tv_nsec = abstv.tv_usec*1000  + (timeout%1000)*1000000;
      if (absts.tv_nsec > 1000000000) {
        absts.tv_nsec -= 1000000000;
        absts.tv_sec += 1;
      }
      pthread_cond_timedwait(&cond, &mutex, &absts);
      // Re-acquire
      count = sav_count;
      locker = self;
    }      
}

#endif



// ----------------------------------------
// CUSTOM COOPERATIVE THREADS
// ----------------------------------------

#if THREADMODEL==COTHREADS

#ifndef __GNUG__
#error "COTHREADS require G++"
#endif
#if (__GNUC__<2) || ((__GNUC__==2) && (__GNUC_MINOR__<=90))
#warning "COTHREADS require EGCS-1.1.1 with Leon's libgcc patch."
#warning "You may have trouble with thread-unsafe exceptions..."
#define NO_LIBGCC_HOOKS
#endif

// -------------------------------------- constants

// Minimal stack size
#define MINSTACK   (32*1024)
// Default stack size
#define DEFSTACK   (127*1024)
// Maxtime between checking fdesc (ms)
#define MAXFDWAIT    (200)
// Maximum time to wait in any case
#define MAXWAIT (60*60*1000)
// Maximum penalty for hog task (ms)
#define MAXPENALTY (1000)
// Trace task switches
#undef COTHREAD_TRACE
#undef COTHREAD_TRACE_VERBOSE

// -------------------------------------- context switch code

struct mach_state { 
  jmp_buf buf; 
};

static void
mach_switch(mach_state *st1, mach_state *st2)
{ 
#if #cpu(sparc)
  asm("ta 3"); // save register windows
#endif
  if (! setjmp(st1->buf))
    longjmp(st2->buf, 1);
}

static void 
mach_start(mach_state *st1, void *pc, char *stacklo, char *stackhi)
{ 
#if #cpu(sparc)
  asm("ta 3"); // save register windows
#endif
  if (! setjmp(st1->buf))
    {
      // The following code must perform two tasks:
      // -- set stack pointer to a proper value between #stacklo# and #stackhi#.
      // -- branch to or call the function at address #pc#.
      // This function never returns ... so there is no need to save anything
#if #cpu(mips)
      char *sp = (char*)(((unsigned long)stackhi-16) & ~0xff);
      asm volatile ("move $sp,%0\n\t"  // set new stack pointer
                    "move $25,%1\n\t"  // call subroutine via $25
                    "jal  $25\n\t"     // call subroutine via $25
                    "nop"              // delay slot
                    : : "r" (sp), "r" (pc) );
#elif #cpu(i386)
      char *sp = (char*)(((unsigned long)stackhi-16) & ~0xff);
      asm volatile ("movl %0,%%esp\n\t" // set new stack pointer
                    "call *%1"          // call function
                    : : "r" (sp), "r" (pc) );
#elif #cpu(sparc)
      char *sp = (char*)(((unsigned long)stackhi-16) & ~0xff);
      asm volatile ("ta 3\n\t"          // saving the register windows will not hurt.
                    "mov %0,%%sp\n\t"   // set new stack pointer
                    "call %1,0\n\t"     // call function
                    "nop"               // delay slot
                    : : "r" (sp), "r" (pc) );
#elif #cpu(hppa)
      char *sp = (char*)(((unsigned long)stacklo+128+255) & ~0xff);
      asm volatile("copy %0,%%sp\n\t"       // set stack pointer
                   "copy %1,%%r22\n\t"      // set call address
                   ".CALL\n\t"              // call pseudo instr (why?)
                   "bl $$dyncall,%%r31\n\t" // call 
                   "copy %%r31,%%r2"        // delay slot ???
                   : : "r" (sp), "r" (pc) );
#elif #cpu(alpha)
      char *sp = (char*)(((unsigned long)stackhi-16) & ~0xff);
      asm volatile ("bis $31,%0,$30\n\t"  // set new stack pointer
                    "bis $31,%1,$27\n\t"  // load function pointer
                    "jsr $26,($27),0"     // call function
                    : : "r" (sp), "r" (pc) );
#elif #cpu(powerpc)
      char *sp = (char*)(((unsigned long)stackhi-16) & ~0xff);
      asm volatile ("mr 1,%0\n\t"         // set new stack pointer
                    "mr 0,%1\n\t"         // load func pointer into r0
                    "mtlr 0\n\t"          // load link register with r0
                    "blrl"                // branch
                    : : "r" (sp), "r" (pc) );
#elif #cpu(m68k) && defined(COTHREAD_UNTESTED)
      char *sp = (char*)(((unsigned long)stackhi-16) & ~0xff);
      asm volatile ("move%.l %0,%Rsp\n\t" // set new stack pointer
                    "jmp %a1"             // branch to address %1
                    : : "r" (sp), "a" (pc) );
#elif #cpu(arm) && defined(COTHREAD_UNTESTED)
      char *sp = (char*)(((unsigned long)stackhi-16) & ~0xff);
      asm volatile ("mov %|sp, %0\n\t" // set new stack pointer
# if defined(__ARM_ARCH_4__) || defined(__ARM_ARCH_4T__)
                    "mov %|pc, %1"     // branch to address %1
# else
                    "bx %1"     // branch to address %1
# endif
                    : : "r" (sp), "r" (pc) );
#else
#error "COTHREADS not supported on this machine."
#error "Try -DTHREADMODEL=NOTHREADS."
#endif
      // We should never reach this point
      abort();
      // Note that this call to abort() makes sure
      // that function mach_start() is compiled as a non-leaf
      // function. It is indeed a non-leaf function since the
      // piece of assembly code calls a function, but the compiler
      // would not know without the call to abort() ...
    }
}

#ifdef CHECK
// This code can be used to verify that task switching works.
char stack[16384];
mach_state st1, st2;
void th2() {
  puts("2b"); mach_switch(&st2, &st1);
  puts("4b"); mach_switch(&st2, &st1);
  puts("6b"); mach_switch(&st2, &st1);
}
void th2relay() {
  th2(); puts("ooops\n");
}
void th1() {
  mach_start(&st1, (void*)th2relay, stack, stack+sizeof(stack));
  puts("3a"); mach_switch(&st1, &st2);
  puts("5a"); mach_switch(&st1, &st2);
}
int main() { 
  puts("1a"); th1(); puts("6a"); 
}
#endif



// -------------------------------------- select

struct coselect {
  int nfds;
  fd_set rset;
  fd_set wset;
  fd_set eset;
};

static void 
coselect_merge(coselect *dest, coselect *from)
{
  int i;
  int nfds = from->nfds;
  if (nfds > dest->nfds)
    dest->nfds = nfds;
  for (i=0; i<nfds; i++) if (FD_ISSET(i, &from->rset)) FD_SET(i, &dest->rset);
  for (i=0; i<nfds; i++) if (FD_ISSET(i, &from->wset)) FD_SET(i, &dest->wset);
  for (i=0; i<nfds; i++) if (FD_ISSET(i, &from->eset)) FD_SET(i, &dest->eset);
}

static int
coselect_test(coselect *c)
{
  static timeval tmzero = {0,0};
  fd_set copyr = c->rset;
  fd_set copyw = c->wset;
  fd_set copye = c->eset;
  return select(c->nfds, &copyr, &copyw, &copye, &tmzero);
}


// -------------------------------------- cotask

class GThread::cotask {
public:
#ifndef NO_LIBGCC_HOOKS
  cotask(const int xstacksize,void *);
#else
  cotask(const int xstacksize);
#endif
  ~cotask();
  class GThread::cotask *next;
  class GThread::cotask *prev;
  // context
  mach_state regs;
  // stack information
  char *stack;
  GPBuffer<char> gstack;
  int stacksize;
  // timing information
  unsigned long over;
  // waiting information
  void *wchan;
  coselect *wselect;
  unsigned long *maxwait;
  // delete after termination
  bool autodelete;
  // egcs exception support
#ifndef NO_LIBGCC_HOOKS
  void *ehctx;
#endif
};

#ifndef NO_LIBGCC_HOOKS
GThread::cotask::cotask(const int xstacksize, void *xehctx)
#else
GThread::cotask::cotask(const int xstacksize)
#endif
: next(0), prev(0), gstack(stack,xstacksize), stacksize(xstacksize),
  over(0), wchan(0), wselect(0), maxwait(0), autodelete(false)
#ifndef NO_LIBGCC_HOOKS
  ,ehctx(xehctx)
#endif
{
  memset(&regs,0,sizeof(regs));
}

static GThread::cotask *maintask = 0;
static GThread::cotask *curtask  = 0;
static GThread::cotask *autodeletetask = 0;
static unsigned long globalmaxwait = 0;
static void (*scheduling_callback)(int) = 0;
static timeval time_base;


GThread::cotask::~cotask()
{
  gstack.resize(0);
#ifndef NO_LIBGCC_HOOKS
  if (ehctx)
    free(ehctx);
  ehctx = 0;
#endif
}

static void 
cotask_free(GThread::cotask *task)
{
#ifdef COTHREAD_TRACE
  DjVuPrintErrorUTF8("cothreads: freeing task %p with autodelete=%d\n", 
          task,task->autodelete);
#endif
  if (task!=maintask)
  {
    delete task;
  }
}


// -------------------------------------- time

static unsigned long
time_elapsed(int reset=1)
{
  timeval tm;
  gettimeofday(&tm, NULL);
  long msec = (tm.tv_usec-time_base.tv_usec)/1000;
  unsigned long elapsed = (long)(tm.tv_sec-time_base.tv_sec)*1000 + msec;
  if (reset && elapsed>0)
    {
#ifdef COTHREAD_TRACE
#ifdef COTHREAD_TRACE_VERBOSE
      DjVuPrintErrorUTF8("cothreads: %4ld ms in task %p\n", elapsed, curtask);
#endif
#endif
      time_base.tv_sec = tm.tv_sec;
      time_base.tv_usec += msec*1000;
    }
  return elapsed;
}


// -------------------------------------- scheduler

static int
cotask_yield()
{
  // ok
  if (! maintask)
    return 0;
  // get elapsed time and return immediately when it is too small
  unsigned long elapsed = time_elapsed();
  if (elapsed==0 && curtask->wchan==0 && curtask->prev && curtask->next)
    return 0;
  // adjust task running time
  curtask->over += elapsed;
  if (curtask->over > MAXPENALTY)
    curtask->over = MAXPENALTY;
  // start scheduling
 reschedule:
  // try unblocking tasks
  GThread::cotask *n = curtask->next;
  GThread::cotask *q = n;
  do 
    { 
      if (q->wchan)
        {
	  if (q->maxwait && *q->maxwait<=elapsed) 
            {
              *q->maxwait = 0;
              q->wchan=0; 
              q->maxwait=0; 
              q->wselect=0; 
            }
          else if (q->wselect && globalmaxwait<=elapsed && coselect_test(q->wselect))
            {
              q->wchan=0;
              if (q->maxwait)
                *q->maxwait -= elapsed;
              q->maxwait = 0; 
              q->wselect=0; 
            }
          if (q->maxwait)
            *q->maxwait -= elapsed;
        }
      q = q->next;
    } 
  while (q!=n);
  // adjust globalmaxwait
  if (globalmaxwait < elapsed)
    globalmaxwait = MAXFDWAIT;
  else
    globalmaxwait -= elapsed;
  // find best candidate
  static int count;
  unsigned long best = MAXPENALTY + 1;
  GThread::cotask *r = 0;
  count = 0;
  q = n;
  do 
    { 
      if (! q->wchan)
        {
          count += 1;
          if (best > q->over)
            {
              r = q;
              best = r->over;
            } 
        }
      q = q->next;
    } 
  while (q != n);
  // found
  if (count > 0)
    {
      // adjust over 
      q = n;
      do 
        { 
          q->over = (q->over>best ? q->over-best : 0);
          q = q->next;
        } 
      while (q != n);
      // Switch
      if (r != curtask)
        {
#ifdef COTHREAD_TRACE
          DjVuPrintErrorUTF8("cothreads: ----- switch to %p [%ld]\n", r, best);
#endif
          GThread::cotask *old = curtask;
          curtask = r;
          mach_switch(&old->regs, &curtask->regs);
        }
      // handle autodelete
      if (autodeletetask && autodeletetask->autodelete) 
        cotask_free(autodeletetask);
      autodeletetask = 0;
      // return 
      if (count == 1)
        return 1;
      return 0;
    }
  // No task ready
  count = 0;
  unsigned long minwait = MAXWAIT;
  coselect allfds;
  allfds.nfds = 1;
  FD_ZERO(&allfds.rset);
  FD_ZERO(&allfds.wset);
  FD_ZERO(&allfds.eset);
  q = n;
  do 
    {
      if (q->maxwait || q->wselect)
        count += 1;
      if (q->maxwait && *q->maxwait<minwait)
        minwait = *q->maxwait;
      if (q->wselect)
        coselect_merge(&allfds, q->wselect);
      q = q->next;
    } 
  while (q != n);
  // abort on deadlock
  if (count == 0) {
    DjVuMessageLite::perror( ERR_MSG("GThreads.panic") );
    abort();
  }
  // select
  timeval tm;
  tm.tv_sec = minwait/1000;
  tm.tv_usec = 1000*(minwait-1000*tm.tv_sec);
  select(allfds.nfds,&allfds.rset, &allfds.wset, &allfds.eset, &tm);
  // reschedule
  globalmaxwait = 0;
  elapsed = time_elapsed();
  goto reschedule;
}


static void
cotask_terminate(GThread::cotask *task)
{
#ifdef COTHREAD_TRACE
  DjVuPrintErrorUTF8("cothreads: terminating task %p\n", task);
#endif
  if (task && task!=maintask)
    {
      if (task->prev && task->next)
        {
          if (scheduling_callback)
            (*scheduling_callback)(GThread::CallbackTerminate);
          task->prev->next = task->next;
          task->next->prev = task->prev;
          // mark task as terminated
          task->prev = 0; 
          // self termination
          if (task == curtask)
            {
              if (task->autodelete)
                autodeletetask = task;
              cotask_yield();
            }
        }
    }
}


static void
cotask_wakeup(void *wchan, int onlyone)
{
  if (maintask && curtask)
    {
      GThread::cotask *n = curtask->next;
      GThread::cotask *q = n;
      do 
        { 
          if (q->wchan == wchan)
            {
              q->wchan=0; 
              q->maxwait=0; 
              q->wselect=0; 
              q->over = 0;
	      if (onlyone)
		return;
            }
          q = q->next;
        } 
      while (q!=n);
    }
}


// -------------------------------------- select / get_select

static int
cotask_select(int nfds, 
              fd_set *rfds, fd_set *wfds, fd_set *efds,
              struct timeval *tm)
{
  // bypass
  if (maintask==0 || (tm && tm->tv_sec==0 && tm->tv_usec<1000))
    return select(nfds, rfds, wfds, efds, tm);
  // copy parameters
  unsigned long maxwait = 0;
  coselect parm;
  // set waiting info
  curtask->wchan = (void*)&parm;
  if (rfds || wfds || efds)
    {
      parm.nfds = nfds;
      if (rfds) { parm.rset=*rfds; } else { FD_ZERO(&parm.rset); }
      if (wfds) { parm.wset=*wfds; } else { FD_ZERO(&parm.wset); }
      if (efds) { parm.eset=*efds; } else { FD_ZERO(&parm.eset); }
      curtask->wselect = &parm;
    }
  if (tm) 
    {
      maxwait = time_elapsed(0) + tm->tv_sec*1000 + tm->tv_usec/1000;
      curtask->maxwait = &maxwait;
    }
  // reschedule
  cotask_yield();
  // call select to update masks
  if (tm)
    {
      tm->tv_sec = maxwait/1000;
      tm->tv_usec = 1000*(maxwait-1000*tm->tv_sec);
    }
  static timeval tmzero = {0,0};
  return select(nfds, rfds, wfds, efds, &tmzero);
}


static void 
cotask_get_select(int &nfds, fd_set *rfds, fd_set *wfds, fd_set *efds, 
                  unsigned long &timeout)
{
  int ready = 1;
  unsigned long minwait = MAXWAIT;
  unsigned long elapsed = time_elapsed(0);
  coselect allfds;
  allfds.nfds=0;
  FD_ZERO(&allfds.rset);
  FD_ZERO(&allfds.wset);
  FD_ZERO(&allfds.eset);
  if (curtask)
    {
      GThread::cotask *q=curtask->next;
      while (q != curtask)
        {
          ready++;
          if (q->wchan)
            {
              if (q->wselect) 
                coselect_merge(&allfds, q->wselect);
              if (q->maxwait && *q->maxwait<minwait)
                minwait = *q->maxwait;
              ready--;
            }
          q = q->next;
        }
    }
  timeout = 0;
  nfds=allfds.nfds;
  *rfds=allfds.rset;
  *wfds=allfds.wset;
  *efds=allfds.eset;
  if (ready==1 && minwait>elapsed)
    timeout = minwait-elapsed;
}



// -------------------------------------- libgcc hook

#ifndef NO_LIBGCC_HOOKS
// These are exported by Leon's patched version of libgcc.a
// Let's hope that the egcs people will include the patch in
// the distributions.
extern "C" 
{
  extern void* (*__get_eh_context_ptr)(void);
  extern void* __new_eh_context(void);
}

// This function is called via the pointer __get_eh_context_ptr
// by the internal mechanisms of egcs.  It must return the 
// per-thread event handler context.  This is necessary to
// implement thread safe exceptions on some machine and/or 
// when flag -fsjlj-exception is set.
static void *
cotask_get_eh_context()
{
  if (curtask)
    return curtask->ehctx;
  else if (maintask)
    return maintask->ehctx;
  DjVuMessageLite::perror( ERR_MSG("GThreads.co_panic") );
  abort();
}
#endif



// -------------------------------------- GThread

void 
GThread::set_scheduling_callback(void (*call)(int))
{
  if (scheduling_callback)
    G_THROW( ERR_MSG("GThreads.dupl_callback") );
  scheduling_callback = call;
}


GThread::GThread(int stacksize)
  : task(0), xentry(0), xarg(0)
{
  // check argument
  if (stacksize < 0)
    stacksize = DEFSTACK;
  if (stacksize < MINSTACK)
    stacksize = MINSTACK;
  // initialization
  if (! maintask)
    {
#ifndef NO_LIBGCC_HOOKS
      static GThread::cotask comaintask(0,(*__get_eh_context_ptr)());
      __get_eh_context_ptr = cotask_get_eh_context;
#else
      static GThread::cotask comaintask(0);
#endif
      maintask = &comaintask;
//      memset(maintask, 0, sizeof(GThread::cotask));
      maintask->next = maintask;
      maintask->prev = maintask;
      gettimeofday(&time_base,NULL);
      curtask = maintask;
    }
  // allocation
#ifndef NO_LIBGCC_HOOKS
  task = new GThread::cotask(stacksize,__new_eh_context());
#else
  task = new GThread::cotask(stacksize);
#endif
}


GThread::~GThread()
{
  if (task && task!=maintask)
  {
    if (task->prev) // running
      task->autodelete = true;
    else
      cotask_free(task);
    task = 0;
  }
}

#if __GNUC__ >= 3
# if __GNUC_MINOR__ >= 4
#  define noinline __attribute__((noinline,used))
# elif __GNUC_MINOR >= 2
#  define noinline __attribute__((noinline))
# endif
#endif
#ifndef noinline
# define noinline /**/
#endif

static noinline void startone(void);
static noinline void starttwo(GThread *thr);
static GThread * volatile starter;

static void
startone(void)
{
  GThread *thr = starter;
  mach_switch(&thr->task->regs, &curtask->regs);
  // Registers may still contain an improper pointer
  // to the exception context.  We should neither 
  // register cleanups nor register handlers.
  starttwo(thr);
  abort();
}

static void 
starttwo(GThread *thr)
{
  // Hopefully this function reacquires 
  // an exception context pointer. Therefore
  // we can register the exception handlers.
  // It is placed after ``startone'' to avoid inlining.
#ifdef __EXCEPTIONS
  try 
    {
#endif 
      G_TRY
        {
          thr->xentry( thr->xarg );
        }
      G_CATCH(ex)
        {
          ex.perror();
          DjVuMessageLite::perror( ERR_MSG("GThreads.uncaught") );
#ifdef _DEBUG
          abort();
#endif
        }
      G_ENDCATCH;
#ifdef __EXCEPTIONS
    }
  catch(...)
    {
          DjVuMessageLite::perror( ERR_MSG("GThreads.unrecognized") );
#ifdef _DEBUG
      abort();
#endif
    }
#endif 
  cotask_terminate(curtask);
  GThread::yield();
  // Do not add anything below this line!
  // Nothing should reach it anyway.
  abort();
}

int 
GThread::create(void (*entry)(void*), void *arg)
{
  if (task->next || task->prev)
    return -1;
  xentry = entry;
  xarg = arg;
  task->wchan = 0;
  task->next = curtask;
  task->prev = curtask->prev;
  task->next->prev = task;
  task->prev->next = task;
  GThread::cotask *old = curtask;
  starter = this;
  mach_start(&old->regs, (void*)startone, 
             task->stack, task->stack+task->stacksize);
  if (scheduling_callback)
    (*scheduling_callback)(CallbackCreate);
  return 0;
}


void 
GThread::terminate()
{
  if (task && task!=maintask)
    cotask_terminate(task);
}

int
GThread::yield()
{
  return cotask_yield();
}

int 
GThread::select(int nfds, 
                fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                struct timeval *timeout)
{
  return cotask_select(nfds, readfds, writefds, exceptfds, timeout);
}

void
GThread::get_select(int &nfds, fd_set *rfds, fd_set *wfds, fd_set *efds, 
                    unsigned long &timeout)
{
  cotask_get_select(nfds, rfds, wfds, efds, timeout);
}

inline void *
GThread::current()
{
  if (curtask && curtask!=maintask)
    return (void*)curtask;
  return (void*)0;
}


// -------------------------------------- GMonitor

GMonitor::GMonitor()
  : count(1), locker(0), wlock(0), wsig(0)
{
  locker = 0;
  ok = 1;
}

GMonitor::~GMonitor()
{
  ok = 0;
  cotask_wakeup((void*)&wsig, 0);
  cotask_wakeup((void*)&wlock, 0);    
  cotask_yield();
  // Because we know how the scheduler works, we know that this single call to
  // yield will run all unblocked tasks and given them the chance to leave the
  // scope of the monitor object.
}

void 
GMonitor::enter()
{
  void *self = GThread::current();
  if (count>0 || self!=locker)
    {
      while (ok && count<=0)
        {
          curtask->wchan = (void*)&wlock;
          wlock++;
          cotask_yield();
          wlock--;
        }
      count = 1;
      locker = self;
    }
  count -= 1;
}

void 
GMonitor::leave()
{
  void *self = GThread::current();
  if (ok && (count>0 || self!=locker))
    G_THROW( ERR_MSG("GThreads.not_acq_leave") );
  if (++count > 0 && wlock > 0)
    cotask_wakeup((void*)&wlock, 1);
}

void
GMonitor::signal()
{
  void *self = GThread::current();
  if (count>0 || self!=locker)
    G_THROW( ERR_MSG("GThreads.not_acq_signal") );
  if (wsig > 0)
    {
      cotask_wakeup((void*)&wsig, 1);
      if (scheduling_callback)
        (*scheduling_callback)(GThread::CallbackUnblock);
    }
}

void
GMonitor::broadcast()
{
  void *self = GThread::current();
  if (count>0 || self!=locker)
    G_THROW( ERR_MSG("GThreads.not_acq_broad") );
  if (wsig > 0)
    {
      cotask_wakeup((void*)&wsig, 0);
      if (scheduling_callback)
        (*scheduling_callback)(GThread::CallbackUnblock);
    }
}

void
GMonitor::wait()
{
  // Check state
  void *self = GThread::current();
  if (count>0 || locker!=self)
    G_THROW( ERR_MSG("GThreads.not_acq_wait") );
  // Wait
  if (ok)
    {
      // Atomically release monitor and wait
      int sav_count = count;
      count = 1;
      curtask->wchan = (void*)&wsig;
      cotask_wakeup((void*)&wlock, 1);
      wsig++;
      cotask_yield();
      wsig--;
      // Re-acquire
      while (ok && count <= 0)
        {
          curtask->wchan = (void*)&wlock;
          wlock++;
          cotask_yield();
          wlock--;
        }
      count = sav_count;
      locker = self;
    }
}

void
GMonitor::wait(unsigned long timeout) 
{
  // Check state
  void *self = GThread::current();
  if (count>0 || locker!=self)
    G_THROW( ERR_MSG("GThreads.not_acq_wait") );
  // Wait
  if (ok)
    {
      // Atomically release monitor and wait
      int sav_count = count;
      count = 1;
      unsigned long maxwait = time_elapsed(0) + timeout;
      curtask->maxwait = &maxwait;
      curtask->wchan = (void*)&wsig;
      cotask_wakeup((void*)&wlock, 1);
      wsig++;
      cotask_yield();
      wsig--;
      // Re-acquire
      while (ok && count<=0)
        {
          curtask->wchan = (void*)&wlock;
          wlock++;
          cotask_yield();
          wlock--;
        }
      count = sav_count;
      locker = self;
    }
}

#endif




// ----------------------------------------
// GSAFEFLAGS 
// ----------------------------------------



GSafeFlags &
GSafeFlags::operator=(long xflags)
{
   enter();
   if (flags!=xflags)
   {
      flags=xflags;
      broadcast();
   }
   leave();
   return *this;
}

GSafeFlags::operator long(void) const
{
   long f;
   ((GSafeFlags *) this)->enter();
   f=flags;
   ((GSafeFlags *) this)->leave();
   return f;
}

bool
GSafeFlags::test_and_modify(long set_mask, long clr_mask,
			    long set_mask1, long clr_mask1)
{
   enter();
   if ((flags & set_mask)==set_mask &&
       (~flags & clr_mask)==clr_mask)
   {
      long new_flags=flags;
      new_flags|=set_mask1;
      new_flags&=~clr_mask1;
      if (new_flags!=flags)
      {
	 flags=new_flags;
	 broadcast();
      }
      leave();
      return true;
   }
   leave();
   return false;
}

void
GSafeFlags::wait_and_modify(long set_mask, long clr_mask,
			    long set_mask1, long clr_mask1)
{
   enter();
   while((flags & set_mask)!=set_mask ||
	 (~flags & clr_mask)!=clr_mask) wait();
   long new_flags=flags;
   new_flags|=set_mask1;
   new_flags&=~clr_mask1;
   if (flags!=new_flags)
   {
      flags=new_flags;
      broadcast();
   }
   leave();
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
