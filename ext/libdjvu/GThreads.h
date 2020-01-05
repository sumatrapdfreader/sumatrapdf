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

#ifndef _GTHREADS_H_
#define _GTHREADS_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


/** @name GThreads.h

    Files #"GThreads.h"# and #"GThreads.cpp"# implement common entry points
    for multithreading on multiple platforms.  Each execution thread is
    represented by an instance of class \Ref{GThread}.  Synchronization is
    provided by class \Ref{GMonitor} which implements a monitor (C.A.R Hoare,
    Communications of the ACM, 17(10), 1974).

    @memo
    Portable threads
    @author
    L\'eon Bottou <leonb@research.att.com> -- initial implementation.\\
    Praveen Guduru <praveen@sanskrit.lz.att.com> -- mac implementation.

// From: Leon Bottou, 1/31/2002
// Almost unchanged by Lizardtech.
// GSafeFlags should go because it not as safe as it claims.
// Reduced to only WINTHREADS and POSIXTHREADS around djvulibre-3.5.25

*/
//@{


#include "DjVuGlobal.h"
#include "GException.h"

// Known platforms
# ifdef _WIN32
#  define WINTHREADS 1
# elif HAVE_PTHREAD
#  define POSIXTHREADS 1
# else
#  error "Libdjvu requires thread support"
# endif


// ----------------------------------------
// INCLUDES

#if WINTHREADS
#ifndef _WINDOWS_
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif
#endif

#if POSIXTHREADS
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#undef TRY
#undef CATCH
#define _CMA_NOWRAPPERS_
#include <pthread.h>
#endif


// ----------------------------------------
// PORTABLE CLASSES


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif



/** Thread class.  A multithreaded process is composed of a main execution
    thread and of several secondary threads.  Each secondary thread is
    represented by a #GThread# object.  The amount of memory required for the
    stack of a secondary thread is defined when the #GThread# object is
    constructed.  The execution thread is started when function
    \Ref{GThread::create} is called.  The destructor of class GThread waits
    until the thread terminanes.  Note that the execution can be terminated at
    any time (with possible prejudice) by calling \Ref{GThread::terminate}.

    Several static member functions control the thread scheduler.  Function
    \Ref{GThread::yield} relinquishes the processor to another thread.
    Function \Ref{GThread::select} (#COTHREADS# only) provides a thread-aware
    replacement for the well-known unix system call #select#.  

    {\bf Note} --- Both the copy constructor and the copy operator are declared
    as private members. It is therefore not possible to make multiple copies
    of instances of this class, as implied by the class semantic. */

class GThread {
public:
  /** Constructs a new thread object.  Memory is allocated for the
      thread, but the thread is not started. 
      Argument #stacksize# is used by the #COTHREADS# model only for
      specifying the amount of memory needed for the processor stack. A
      negative value will be replaced by a suitable default value of 128Kb.
      A minimum value of 32Kb is silently enforced. */
  GThread(int stacksize = -1);
  /** Destructor.  Destroying the thread object while the thread is running is
      perfectly ok since it only destroys the thread identifier.  Execution
      will continue without interference. */
  ~GThread();
  /** Starts the thread. The new thread executes function #entry# with
      argument #arg#.  The thread terminates when the function returns.  A
      thread cannot be restarted after its termination. You must create a new
      #GThread# object. */
  int  create(void (*entry)(void*), void *arg);
  /** Terminates a thread with extreme prejudice. The thread is removed from
      the scheduling list.  Execution terminates regardless of the execution
      status of the thread function. Automatic variables may or may not be
      destroyed. This function must be considered as a last resort since
      memory may be lost. */
  void terminate();
  /** Causes the current thread to relinquish the processor.  The scheduler
      selects a thread ready to run and transfers control to that thread.  The
      actual effect of #yield# heavily depends on the selected implementation.
      Function #yield# usually returns zero when the execution of the current
      thread is resumed.  It may return a positive number when it can
      determine that the current thread will remain the only runnable thread
      for some time.  You may then call function \Ref{get_select} to
      obtain more information. */
  static int yield();
  /** Returns a value which uniquely identifies the current thread. */
  static void *current();
#if WINTHREADS
private:
  HANDLE hthr;
  DWORD  thrid;
#elif POSIXTHREADS
private:
  pthread_t hthr;
  static void *start(void *arg);
#endif
public:
  // Should be considered as private
  void (*xentry)(void*);
  void  *xarg;
private:
  // Disable default members
  GThread(const GThread&);
  GThread& operator=(const GThread&);
};


/** Monitor class.  Monitors have been first described in (C.A.R Hoare,
    Communications of the ACM, 17(10), 1974).  This mechanism provides the
    basic mutual exclusion (mutex) and thread notification facilities
    (condition variables).
    
    Only one thread can own the monitor at a given time.  Functions
    \Ref{enter} and \Ref{leave} can be used to acquire and release the
    monitor. This mutual exclusion provides an efficient way to protect
    segment of codes ({\em critical sections}) which should not be
    simultaneously executed by two threads. Class \Ref{GMonitorLock} provides
    a convenient way to do this effectively.
    
    When the thread owning the monitor calls function \Ref{wait}, the monitor
    is released and the thread starts waiting until another thread calls
    function \Ref{signal} or \Ref{broadcast}.  When the thread wakes-up, it
    re-acquires the monitor and function #wait# returns.  Since the signaling
    thread must acquire the monitor before calling functions #signal# and
    #broadcast#, the signaled thread will not be able to re-acquire the
    monitor until the signaling thread(s) releases the monitor.
    
    {\bf Note} --- Both the copy constructor and the copy operator are declared
    as private members. It is therefore not possible to make multiple copies
    of instances of this class, as implied by the class semantic. */

class GMonitor
{
public:
  GMonitor();
  ~GMonitor();
  /** Enters the monitor.  If the monitor is acquired by another thread this
      function waits until the monitor is released.  The current thread then
      acquires the monitor.  Calls to #enter# and #leave# may be nested. */
  void enter();
  /** Leaves the monitor.  The monitor counts how many times the current
      thread has entered the monitor.  Function #leave# decrement this count.
      The monitor is released when this count reaches zero.  An exception is
      thrown if this function is called by a thread which does not own the
      monitor. */
  void leave();
  /** Waits until the monitor is signaled.  The current thread atomically
      releases the monitor and waits until another thread calls function
      #signal# or #broadcast#.  Function #wait# then re-acquires the monitor
      and returns.  An exception is thrown if this function is called by a
      thread which does not own the monitor. */
  void wait();
  /** Waits until the monitor is signaled or a timeout is reached.  The
      current thread atomically releases the monitor and waits until another
      thread calls function #signal# or #broadcast# or a maximum of #timeout#
      milliseconds.  Function #wait# then re-acquires the monitor and returns.
      An exception is thrown if this function is called by a thread which does
      not own the monitor. */
  void wait(unsigned long timeout);
  /** Signals one waiting thread.  Function #signal# wakes up at most one of
      the waiting threads for this monitor.  An exception is thrown if this
      function is called by a thread which does not own the monitor. */
  void signal();
  /** Signals all waiting threads. Function #broadcast# wakes up all the
      waiting threads for this monitor.  An exception is thrown if this
      function is called by a thread which does not own the monitor. */
  void broadcast();
private:
#if WINTHREADS
  int ok;
  int count;
  DWORD locker;
  CRITICAL_SECTION cs;
  struct thr_waiting *head;
  struct thr_waiting *tail;
#elif POSIXTHREADS
  int ok;
  int count;
  pthread_t locker;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
#endif  
private:
  // Disable default members
  GMonitor(const GMonitor&);
  GMonitor& operator=(const GMonitor&);
};





// ----------------------------------------
// SCOPE LOCK


/** Wrapper for mutually exclusive code.
    This class locks a specified critical section (see \Ref{GCriticalSection})
    at construction time and unlocks it at destruction time. It provides a
    convenient way to take advantage of the C++ implicit destruction of
    automatic variables in order to make sure that the monitor is
    released when exiting the protected code.  The following code will release
    the monitor when the execution thread leaves the protected scope, either
    because the protected code has executed successfully, or because an
    exception was thrown.
    \begin{verbatim}
      {      -- protected scope
         static GMonitor theMonitor;
         GMonitorLock lock(&theMonitor)
         ... -- protected code
      }
    \end{verbatim} 
    This construct will do nothing when passed a null pointer.
*/
class GMonitorLock 
{
private:
  GMonitor *gsec;
public:
  /** Constructor. Enters the monitor #gsec#. */
  GMonitorLock(GMonitor *gsec) : gsec(gsec) 
    { if (gsec) gsec->enter(); };
  /** Destructor. Leaves the associated monitor. */
  ~GMonitorLock() 
    { if (gsec) gsec->leave(); };
};



// ----------------------------------------
// GSAFEFLAGS (not so safe)


/** A thread safe class representing a set of flags. The flags are protected
    by \Ref{GMonitor}, which is attempted to be locked whenever somebody
    accesses the flags. One can modify the class contents using one of
    two functions: \Ref{test_and_modify}() and \Ref{wait_and_modify}().
    Both of them provide atomic operation of testing (first) and modification
    (second). The flags remain locked between the moment of testing and
    modification, which guarantees, that their state cannot be changed in
    between of these operations. */
class GSafeFlags : public GMonitor
{
private:
   volatile long flags;
public:
      /// Constructs #GSafeFlags# object.
   GSafeFlags(long flags=0);

      /** Assignment operator. Will also wake up threads waiting for the
	  flags to change. */
   GSafeFlags & operator=(long flags);

      /** Returns the value of the flags */
   operator long(void) const;
      /** Modifies the flags by ORing them with the provided mask. A broadcast
	  will be sent after the modification is done. */
   GSafeFlags &	operator|=(long mask);
      /** Modifies the flags by ANDing them with the provided mask. A broadcast
	  will be sent after the modification is done. */
   GSafeFlags &	operator&=(long mask);

      /** If all bits mentioned in #set_mask# are set in the flags and all
	  bits mentioned in #clr_mask# are cleared in the flags, it sets all
	  bits from #set_mask1# in the flags, clears all flags from
	  #clr_mask1# in the flags and returns #TRUE#. Otherwise returns
	  #FALSE#. */
   bool	test_and_modify(long set_mask, long clr_mask,
			long set_mask1, long clr_mask1);

      /** Waits until all bits mentioned in #set_mask# are set in the flags
	  and all bits mentioned in #clr_flags# are cleared in the flags.
	  After that it sets bits from #set_mask1# and clears bits from
	  #clr_mask1# in the flags. */
   void	wait_and_modify(long set_mask, long clr_mask,
			long set_mask1, long clr_mask1);

      /** Waits until all bits set in #set_mask# are set in the flags and
	  all bits mentioned in #clr_mask# are cleared in the flags. */
   void	wait_for_flags(long set_mask, long clr_mask=0) const;

      /** Modifies the flags by setting all bits mentioned in #set_mask#
	  and clearing all bits mentioned in #clr_mask#. If the flags have
	  actually been modified, a broadcast will be sent. */
   void	modify(long set_mask, long clr_mask);
};

inline
GSafeFlags::GSafeFlags(long xflags) 
  : flags(xflags) 
{
}

inline void
GSafeFlags::wait_for_flags(long set_mask, long clr_mask) const
{
   ((GSafeFlags *) this)->wait_and_modify(set_mask, clr_mask, 0, 0);
}

inline void
GSafeFlags::modify(long set_mask, long clr_mask)
{
   test_and_modify(0, 0, set_mask, clr_mask);
}

inline GSafeFlags &
GSafeFlags::operator|=(long mask)
{
   test_and_modify(0, 0, mask, 0);
   return *this;
}

inline GSafeFlags &
GSafeFlags::operator&=(long mask)
{
   test_and_modify(0, 0, 0, ~mask);
   return *this;
}

//@}




// ----------------------------------------
// COMPATIBILITY CLASSES


// -- these classes are no longer documented.

class GCriticalSection : protected GMonitor 
{
public:
  void lock() 
    { GMonitor::enter(); };
  void unlock() 
    { GMonitor::leave(); };
};

class GEvent : protected GMonitor 
{
private:
  int status;
public:
  GEvent() 
    : status(0) { };
  void set() 
    { if (!status) { enter(); status=1; signal(); leave(); } };
  void wait() 
    { enter(); if (!status) GMonitor::wait(); status=0; leave(); };
  void wait(int timeout) 
    { enter(); if (!status) GMonitor::wait(timeout); status=0; leave(); };
};

class GCriticalSectionLock
{
private:
  GCriticalSection *gsec;
public:
  GCriticalSectionLock(GCriticalSection *gsec) : gsec(gsec) 
    { if (gsec) gsec->lock(); };
  ~GCriticalSectionLock() 
    { if (gsec) gsec->unlock(); };
};


// ----------------------------------------

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif //_GTHREADS_H_

