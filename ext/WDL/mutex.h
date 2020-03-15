/*
    WDL - mutex.h
    Copyright (C) 2005 and later, Cockos Incorporated
   
    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
      
*/

/*


  This file provides a simple class that abstracts a mutex or critical section object.
  On Windows it uses CRITICAL_SECTION, on everything else it uses pthread's mutex library.
  It simulates the Critical Section behavior on non-Windows, as well (meaning a thread can 
  safely Enter the mutex multiple times, provided it Leaves the same number of times)
  
*/

#ifndef _WDL_MUTEX_H_
#define _WDL_MUTEX_H_

#ifdef _WIN32
#include <windows.h>
#else

#include <unistd.h>
// define this if you wish to use carbon critical sections on OS X
// #define WDL_MAC_USE_CARBON_CRITSEC

#ifdef WDL_MAC_USE_CARBON_CRITSEC
#include <Carbon/Carbon.h>
#else
#include <pthread.h>
#endif

#endif

#include "wdltypes.h"
#include "wdlatomic.h"

#ifdef _DEBUG
#include <assert.h>
#endif

class WDL_Mutex {
  public:
    WDL_Mutex() 
    {
#ifdef _WIN32
      InitializeCriticalSection(&m_cs);
#elif defined( WDL_MAC_USE_CARBON_CRITSEC)
      MPCreateCriticalRegion(&m_cr);
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER) && !defined(__linux__)
      const pthread_mutex_t tmp = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
      m_mutex = tmp;
#else
      pthread_mutexattr_t attr;
      pthread_mutexattr_init(&attr);
      pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
#ifdef __linux__
      // todo: macos too?
      pthread_mutexattr_setprotocol(&attr,PTHREAD_PRIO_INHERIT);
#endif
      pthread_mutex_init(&m_mutex,&attr);
      pthread_mutexattr_destroy(&attr);
#endif
    }
    ~WDL_Mutex()
    {
#ifdef _WIN32
      DeleteCriticalSection(&m_cs);
#elif defined(WDL_MAC_USE_CARBON_CRITSEC)
      MPDeleteCriticalRegion(m_cr);
#else
      pthread_mutex_destroy(&m_mutex);
#endif
    }

    void Enter()
    {
#ifdef _WIN32
      EnterCriticalSection(&m_cs);
#elif defined(WDL_MAC_USE_CARBON_CRITSEC)
      MPEnterCriticalRegion(m_cr,kDurationForever);
#else
      pthread_mutex_lock(&m_mutex);
#endif
    }

    void Leave()
    {
#ifdef _WIN32
      LeaveCriticalSection(&m_cs);
#elif defined(WDL_MAC_USE_CARBON_CRITSEC)
      MPExitCriticalRegion(m_cr);
#else
      pthread_mutex_unlock(&m_mutex);
#endif
    }

  private:
#ifdef _WIN32
  CRITICAL_SECTION m_cs;
#elif defined(WDL_MAC_USE_CARBON_CRITSEC)
  MPCriticalRegionID m_cr;
#else
  pthread_mutex_t m_mutex;
#endif

  // prevent callers from copying mutexes accidentally
  WDL_Mutex(const WDL_Mutex &cp)
  {
#ifdef _DEBUG
    assert(sizeof(WDL_Mutex) == 0);
#endif
  }
  WDL_Mutex &operator=(const WDL_Mutex &cp)
  {
#ifdef _DEBUG
    assert(sizeof(WDL_Mutex) == 0);
#endif
    return *this;
  }

} WDL_FIXALIGN;

class WDL_MutexLock {
public:
  WDL_MutexLock(WDL_Mutex *m) : m_m(m) { if (m) m->Enter(); }
  ~WDL_MutexLock() { if (m_m) m_m->Leave(); }

  // the caller modifies this, make sure it unlocks the mutex first and locks the new mutex!
  WDL_Mutex *m_m;
} WDL_FIXALIGN;

class WDL_SharedMutex 
{
  public:
    WDL_SharedMutex() { m_sharedcnt=0; }
    ~WDL_SharedMutex() { }

    void LockExclusive()  // note: the calling thread must NOT have any shared locks, or deadlock WILL occur
    { 
      m_mutex.Enter(); 
#ifdef _WIN32
      while (m_sharedcnt>0) Sleep(1);
#else
      while (m_sharedcnt>0) usleep(100);		
#endif
    }
    void UnlockExclusive() { m_mutex.Leave(); }

    void LockShared() 
    { 
      m_mutex.Enter();
      wdl_atomic_incr(&m_sharedcnt);
      m_mutex.Leave();
    }
    void UnlockShared()
    {
      wdl_atomic_decr(&m_sharedcnt);
    }

    void SharedToExclusive() // assumes a SINGLE shared lock by this thread!
    { 
      m_mutex.Enter(); 
#ifdef _WIN32
      while (m_sharedcnt>1) Sleep(1);
#else
      while (m_sharedcnt>1) usleep(100);		
#endif
      UnlockShared();
    }
  
    void ExclusiveToShared() // assumes exclusive locked returns with shared locked
    {
      // already have exclusive lock
      wdl_atomic_incr(&m_sharedcnt);
      m_mutex.Leave();
    }

  private:
    WDL_Mutex m_mutex;
    volatile int m_sharedcnt;

    // prevent callers from copying accidentally
    WDL_SharedMutex(const WDL_SharedMutex &cp)
    {
    #ifdef _DEBUG
      assert(sizeof(WDL_SharedMutex) == 0);
    #endif
    }
    WDL_SharedMutex &operator=(const WDL_SharedMutex &cp)
    {
    #ifdef _DEBUG
      assert(sizeof(WDL_SharedMutex) == 0);
    #endif
      return *this;
    }


} WDL_FIXALIGN;



class WDL_MutexLockShared {
  public:
    WDL_MutexLockShared(WDL_SharedMutex *m) : m_m(m) { if (m) m->LockShared(); }
    ~WDL_MutexLockShared() { if (m_m) m_m->UnlockShared(); }
  private:
    WDL_SharedMutex *m_m;
} WDL_FIXALIGN;

class WDL_MutexLockExclusive {
  public:
    WDL_MutexLockExclusive(WDL_SharedMutex *m) : m_m(m) { if (m) m->LockExclusive(); }
    ~WDL_MutexLockExclusive() { if (m_m) m_m->UnlockExclusive(); }
  private:
    WDL_SharedMutex *m_m;
} WDL_FIXALIGN;


#endif
