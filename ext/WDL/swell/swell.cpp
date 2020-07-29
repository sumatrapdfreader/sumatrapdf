/* Cockos SWELL (Simple/Small Win32 Emulation Layer for Linux/OSX)
   Copyright (C) 2006 and later, Cockos, Inc.

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
  

    This file implements a few Windows calls using their posix equivilents

  */

#ifndef SWELL_PROVIDED_BY_APP


#include "swell.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/fcntl.h>
#include <sys/resource.h>


#include "swell-internal.h"

#ifdef SWELL_TARGET_OSX
#include <Carbon/Carbon.h>
#endif

#ifdef __APPLE__
#include <sched.h>
#include <sys/errno.h>
#else
#include <sys/wait.h>
#endif

#ifdef __linux__
#include <linux/sched.h>
#endif

#include <pthread.h>


#include "../wdlatomic.h"
#include "../mutex.h"
#include "../assocarray.h"
#include "../wdlcstring.h"

void Sleep(int ms)
{
  usleep(ms?ms*1000:100);
}

DWORD GetTickCount()
{
#ifdef __APPLE__
  // could switch to mach_getabsolutetime() maybe
  struct timeval tm={0,};
  gettimeofday(&tm,NULL);
  return (DWORD) (tm.tv_sec*1000 + tm.tv_usec/1000);
#else
  struct timespec ts={0,};
  clock_gettime(CLOCK_MONOTONIC,&ts);
  return (DWORD) (ts.tv_sec*1000 + ts.tv_nsec/1000000);
#endif
}


static void intToFileTime(time_t t, FILETIME *out)
{
  // see WDL_DirScan::GetCurrentLastWriteTime and similar
  unsigned long long a=(unsigned long long)t; // seconds since january 1st, 1970
  a += 11644473600ull; // 1601-1970
  a *= 10000000; // seconds to 1/10th microseconds (100 nanoseconds)
  out->dwLowDateTime=a & 0xffffffff;
  out->dwHighDateTime=a>>32;
}

BOOL GetFileTime(int filedes, FILETIME *lpCreationTime, FILETIME *lpLastAccessTime, FILETIME *lpLastWriteTime)
{
  if (WDL_NOT_NORMALLY(filedes<0)) return 0;
  struct stat st;
  if (fstat(filedes,&st)) return 0;
  
  if (lpCreationTime) intToFileTime(st.st_ctime,lpCreationTime);
  if (lpLastAccessTime) intToFileTime(st.st_atime,lpLastAccessTime);
  if (lpLastWriteTime) intToFileTime(st.st_mtime,lpLastWriteTime);

  return 1;
}

BOOL SWELL_PtInRect(const RECT *r, POINT p)
{
  if (!r) return FALSE;
  int tp=r->top;
  int bt=r->bottom;
  if (tp>bt)
  {
    bt=tp;
    tp=r->bottom;
  }
  return p.x>=r->left && p.x<r->right && p.y >= tp && p.y < bt;
}


int MulDiv(int a, int b, int c)
{
  if(c == 0) return 0;
  return (int)((double)a*(double)b/c);
}

unsigned int  _controlfp(unsigned int flag, unsigned int mask)
{
#if !defined(__ppc__) && !defined(__LP64__) && !defined(__arm__)
  unsigned short ret;
  mask &= _MCW_RC; // don't let the caller set anything other than round control for now
  __asm__ __volatile__("fnstcw %0\n\t":"=m"(ret));
  ret=(ret&~(mask<<2))|(flag<<2);
  
  if (mask) __asm__ __volatile__(
	  "fldcw %0\n\t"::"m"(ret));
  return (unsigned int) (ret>>2);
#else
  return 0;
#endif
}

#ifndef SWELL_TARGET_OSX
static WDL_PtrList<void> s_zombie_handles;
void swell_cleanupZombies()
{
  int x = s_zombie_handles.GetSize();
  while (--x>=0)
  {
    HANDLE h = s_zombie_handles.Get(x);
    if (WaitForSingleObject(h,0) != WAIT_TIMEOUT)
      s_zombie_handles.Delete(x,free);
  }
}

#endif

BOOL CloseHandle(HANDLE hand)
{
  SWELL_InternalObjectHeader *hdr=(SWELL_InternalObjectHeader*)hand;
  if (WDL_NOT_NORMALLY(!hdr)) return FALSE;
  if (hdr->type <= INTERNAL_OBJECT_START || hdr->type >= INTERNAL_OBJECT_END) return FALSE;
  
  if (!wdl_atomic_decr(&hdr->count))
  {
    switch (hdr->type)
    {
      case INTERNAL_OBJECT_FILE:
        {
          SWELL_InternalObjectHeader_File *file = (SWELL_InternalObjectHeader_File*)hdr;
          if (file->fp) fclose(file->fp);
        }
      break;
      case INTERNAL_OBJECT_EXTERNALSOCKET: return FALSE; // pure sockets are not to be closed this way;
      case INTERNAL_OBJECT_SOCKETEVENT:
        {
          SWELL_InternalObjectHeader_SocketEvent *se= (SWELL_InternalObjectHeader_SocketEvent *)hdr;
          if (se->socket[0]>=0) close(se->socket[0]);
          if (se->socket[1]>=0) close(se->socket[1]);
        }
      break;
      case INTERNAL_OBJECT_EVENT:
        {
          SWELL_InternalObjectHeader_Event *evt=(SWELL_InternalObjectHeader_Event*)hdr;
          pthread_cond_destroy(&evt->cond);
          pthread_mutex_destroy(&evt->mutex);
        }
      break;
      case INTERNAL_OBJECT_THREAD:
        {
          SWELL_InternalObjectHeader_Thread *thr = (SWELL_InternalObjectHeader_Thread*)hdr;
          void *tmp;
          pthread_join(thr->pt,&tmp);
          pthread_detach(thr->pt);
        }
      break;
#ifdef SWELL_TARGET_OSX
      case INTERNAL_OBJECT_NSTASK:
        {
          SWELL_InternalObjectHeader_NSTask *nst = (SWELL_InternalObjectHeader_NSTask*)hdr;
          extern void SWELL_ReleaseNSTask(void *);
          if (nst->task) SWELL_ReleaseNSTask(nst->task);
        }
      break;
#else
      case INTERNAL_OBJECT_PID:
        swell_cleanupZombies();
        if (WaitForSingleObject(hand,0)==WAIT_TIMEOUT)
        {
          s_zombie_handles.Add(hand);
          return TRUE;
        }
      break;
#endif
    }
    free(hdr);
  }
  return TRUE;
}

HANDLE CreateEventAsSocket(void *SA, BOOL manualReset, BOOL initialSig, const char *ignored)
{
  SWELL_InternalObjectHeader_SocketEvent *buf = (SWELL_InternalObjectHeader_SocketEvent*)malloc(sizeof(SWELL_InternalObjectHeader_SocketEvent));
  buf->hdr.type=INTERNAL_OBJECT_SOCKETEVENT;
  buf->hdr.count=1;
  buf->autoReset = !manualReset;
  buf->socket[0]=buf->socket[1]=-1;
  if (socketpair(AF_UNIX,SOCK_STREAM,0,buf->socket)<0) 
  { 
    free(buf);
    return 0;
  }
  fcntl(buf->socket[0], F_SETFL, fcntl(buf->socket[0],F_GETFL) | O_NONBLOCK); // nonblocking

  char c=0;
  if (initialSig&&buf->socket[1]>=0)
  {
    if (write(buf->socket[1],&c,1) != 1)
    {
      WDL_ASSERT( false /* write to socket failed in CreateEventAsSocket() */ );
    }
  }

  return buf;
}

DWORD WaitForAnySocketObject(int numObjs, HANDLE *objs, DWORD msTO) // only supports special (socket) handles at the moment 
{
  struct pollfd list1[128];
  WDL_TypedBuf<struct pollfd> list2;
  struct pollfd *fds = numObjs > 128 ? list2.ResizeOK(numObjs) : list1;
  if (WDL_NOT_NORMALLY(!fds)) { numObjs = 128; fds = list1; }
  int x, nfds = 0;
  for (x = 0; x < numObjs; x ++)
  {
    SWELL_InternalObjectHeader_SocketEvent *se = (SWELL_InternalObjectHeader_SocketEvent *)objs[x];
    if (WDL_NORMALLY(se) &&
        WDL_NORMALLY(se->hdr.type == INTERNAL_OBJECT_EXTERNALSOCKET || se->hdr.type == INTERNAL_OBJECT_SOCKETEVENT) && 
        WDL_NORMALLY(se->socket[0]>=0))
    {
      fds[nfds].fd = se->socket[0];
      fds[nfds].events = POLLIN;
      fds[nfds].revents = 0;
      nfds++;
    }
  }

  if (nfds>0)
  {
again:
    const int res = poll(fds,nfds,msTO == INFINITE ? -1 : msTO);
    int pos = 0;
    if (res>0) for (x = 0; x < numObjs; x ++)
    {
      SWELL_InternalObjectHeader_SocketEvent *se = (SWELL_InternalObjectHeader_SocketEvent *)objs[x];
      if (WDL_NORMALLY(se) &&
          WDL_NORMALLY(se->hdr.type == INTERNAL_OBJECT_EXTERNALSOCKET || se->hdr.type == INTERNAL_OBJECT_SOCKETEVENT) && 
          WDL_NORMALLY(se->socket[0]>=0))
      {
        if (fds[pos].revents & POLLIN)
        {
          if (se->hdr.type == INTERNAL_OBJECT_SOCKETEVENT && se->autoReset)
          {
            char buf[128];
            if (read(se->socket[0],buf,sizeof(buf))<1) goto again;
          }
          return WAIT_OBJECT_0 + x;
        }
        pos++;
      }
    }
    if (res < 0) return WAIT_FAILED;
  }
  
  return WAIT_TIMEOUT;
}

DWORD WaitForSingleObject(HANDLE hand, DWORD msTO)
{
  SWELL_InternalObjectHeader *hdr=(SWELL_InternalObjectHeader*)hand;
  if (WDL_NOT_NORMALLY(!hdr)) return WAIT_FAILED;
  
  switch (hdr->type)
  {
#ifdef SWELL_TARGET_OSX
    case INTERNAL_OBJECT_NSTASK:
      {
        SWELL_InternalObjectHeader_NSTask *nst = (SWELL_InternalObjectHeader_NSTask*)hdr;
        extern DWORD SWELL_WaitForNSTask(void *,DWORD);
        if (nst->task) return SWELL_WaitForNSTask(nst->task,msTO);
      }
    break;
#else
    case INTERNAL_OBJECT_PID:
      {
        SWELL_InternalObjectHeader_PID *pb = (SWELL_InternalObjectHeader_PID*)hdr;
        if (pb->pid) 
        {
          if (pb->done) return WAIT_OBJECT_0;

          int wstatus=0;
          if (msTO == INFINITE || msTO == 0)
          {
            pid_t v = waitpid(pb->pid,&wstatus,msTO == INFINITE ? 0 : WNOHANG);
            if (v == 0) return WAIT_TIMEOUT;
            if (v < 0) return WAIT_FAILED;
          }
          else
          {
            const DWORD start_t = GetTickCount();
            for (;;)
            {
              pid_t v = waitpid(pb->pid,&wstatus,WNOHANG);
              if (v > 0) break;

              if (v < 0) return WAIT_FAILED;
              if ((GetTickCount()-start_t) > msTO) return WAIT_TIMEOUT;
              Sleep(1);
            }
          }
          if (!pb->done)
          {
            pb->done=1;
            pb->result = WEXITSTATUS(wstatus);
          }
          return WAIT_OBJECT_0;
        }
      }
    break;
#endif
    case INTERNAL_OBJECT_THREAD:
      {
        SWELL_InternalObjectHeader_Thread *thr = (SWELL_InternalObjectHeader_Thread*)hdr;
        void *tmp;
        if (!thr->done) 
        {
          if (!msTO) return WAIT_TIMEOUT;
          if (msTO != INFINITE)
          {
            const DWORD d=GetTickCount();
            while ((GetTickCount()-d)<msTO && !thr->done) Sleep(1);
            if (!thr->done) return WAIT_TIMEOUT;
          }
        }
    
        if (!pthread_join(thr->pt,&tmp)) return WAIT_OBJECT_0;      
      }
    break;
    case INTERNAL_OBJECT_EXTERNALSOCKET:
    case INTERNAL_OBJECT_SOCKETEVENT:
      {
        SWELL_InternalObjectHeader_SocketEvent *se = (SWELL_InternalObjectHeader_SocketEvent *)hdr;
        if (WDL_NOT_NORMALLY(se->socket[0]<0)) Sleep(msTO!=INFINITE?msTO:1);
        else
        {
again:
          struct pollfd fd = { se->socket[0], POLLIN, 0 };
          const int res = poll(&fd,1,msTO==INFINITE?-1 : msTO);
          if (res < 0) return WAIT_FAILED;
          if (res>0 && (fd.revents&POLLIN))
          {
            if (se->hdr.type == INTERNAL_OBJECT_SOCKETEVENT && se->autoReset)
            {
              char buf[128];
              if (read(se->socket[0],buf,sizeof(buf))<1) goto again;
            }
            return WAIT_OBJECT_0;
          } 
          return WAIT_TIMEOUT;
        }
      }
    break;
    case INTERNAL_OBJECT_EVENT:
      {
        SWELL_InternalObjectHeader_Event *evt = (SWELL_InternalObjectHeader_Event*)hdr;    
        int rv=WAIT_OBJECT_0;
        pthread_mutex_lock(&evt->mutex);
        if (msTO == 0)  
        {
          if (!evt->isSignal) rv=WAIT_TIMEOUT;
        }
        else if (msTO == INFINITE)
        {
          while (!evt->isSignal) pthread_cond_wait(&evt->cond,&evt->mutex);
        }
        else
        {
          // timed wait
#ifdef __APPLE__
          struct timespec ts;
          ts.tv_sec = msTO/1000;
          ts.tv_nsec = (msTO%1000)*1000000;
#endif
          while (!evt->isSignal) 
          {
#ifdef __APPLE__
            if (pthread_cond_timedwait_relative_np(&evt->cond,&evt->mutex,&ts)==ETIMEDOUT)
            {
              rv = WAIT_TIMEOUT;
              break;
            }
#else
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC,&ts);
            ts.tv_sec += msTO/1000;
            ts.tv_nsec += (msTO%1000)*1000000;
            if (ts.tv_nsec>=1000000000) 
            {
              int n = ts.tv_nsec/1000000000;
              ts.tv_sec+=n;
              ts.tv_nsec -= ((long long)n * (long long)1000000000);
            }
            if (pthread_cond_timedwait(&evt->cond,&evt->mutex,&ts))
            {
              rv = WAIT_TIMEOUT;
              break;
            }
#endif
            // we should track/correct the timeout amount here since in theory we could end up waiting a bit longer!
          }
        }    
        if (!evt->isManualReset && rv==WAIT_OBJECT_0) evt->isSignal=false;
        pthread_mutex_unlock(&evt->mutex);
  
        return rv;
      }
    break;
  }
  
  return WAIT_FAILED;
}

static void *__threadproc(void *parm)
{
#ifdef SWELL_TARGET_OSX
  void *arp=SWELL_InitAutoRelease();
#endif
  
  SWELL_InternalObjectHeader_Thread *t=(SWELL_InternalObjectHeader_Thread*)parm;
  t->retv=t->threadProc(t->threadParm);  
  t->done=1;
  CloseHandle(parm);  

#ifdef SWELL_TARGET_OSX
  SWELL_QuitAutoRelease(arp);
#endif
  
  pthread_exit(0);
  return 0;
}

DWORD GetCurrentThreadId()
{
  return (DWORD)(INT_PTR)pthread_self(); // this is incorrect on x64
}

HANDLE CreateEvent(void *SA, BOOL manualReset, BOOL initialSig, const char *ignored) 
{
  SWELL_InternalObjectHeader_Event *buf = (SWELL_InternalObjectHeader_Event*)malloc(sizeof(SWELL_InternalObjectHeader_Event));
  buf->hdr.type=INTERNAL_OBJECT_EVENT;
  buf->hdr.count=1;
  buf->isSignal = !!initialSig;
  buf->isManualReset = !!manualReset;
  
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
#ifdef __linux__
  pthread_mutexattr_setprotocol(&attr,PTHREAD_PRIO_INHERIT);
#endif
  pthread_mutex_init(&buf->mutex,&attr);
  pthread_mutexattr_destroy(&attr);

#ifndef __APPLE__
  pthread_condattr_t cattr;
  pthread_condattr_init(&cattr);
  pthread_condattr_setclock(&cattr,CLOCK_MONOTONIC);
  pthread_cond_init(&buf->cond,&cattr);
  pthread_condattr_destroy(&cattr);
#else
  pthread_cond_init(&buf->cond,NULL);
#endif
  
  return (HANDLE)buf;
}

HANDLE CreateThread(void *TA, DWORD stackSize, DWORD (*ThreadProc)(LPVOID), LPVOID parm, DWORD cf, DWORD *tidOut)
{
#ifdef SWELL_TARGET_OSX
  SWELL_EnsureMultithreadedCocoa();
#endif
  SWELL_InternalObjectHeader_Thread *buf = (SWELL_InternalObjectHeader_Thread *)malloc(sizeof(SWELL_InternalObjectHeader_Thread));
  buf->hdr.type=INTERNAL_OBJECT_THREAD;
  buf->hdr.count=2;
  buf->threadProc=ThreadProc;
  buf->threadParm = parm;
  buf->retv=0;
  buf->pt=0;
  buf->done=0;
  pthread_create(&buf->pt,NULL,__threadproc,buf);
  
  if (tidOut) *tidOut=(DWORD)(INT_PTR)buf->pt; // incorrect on x64

  return (HANDLE)buf;
}


BOOL SetThreadPriority(HANDLE hand, int prio)
{
  SWELL_InternalObjectHeader_Thread *evt=(SWELL_InternalObjectHeader_Thread*)hand;

#ifdef __linux__
  static int s_rt_max;
  if (!evt && prio >= 0x10000 && prio < 0x10000 + 100)
  {
    s_rt_max = prio - 0x10000;
    return TRUE;
  }
#endif

  if (WDL_NOT_NORMALLY(!evt || evt->hdr.type != INTERNAL_OBJECT_THREAD)) return FALSE;
  
  if (evt->done) return FALSE;
    
  int pol;
  struct sched_param param;
  memset(&param,0,sizeof(param));

#ifdef __linux__
  // linux only has meaningful priorities if using realtime threads,
  // for this to be enabled the caller should use:
  // #ifdef __linux__
  // SetThreadPriority(NULL,0x10000 + max_thread_priority (0..99));
  // #endif
  if (s_rt_max < 1 || prio <= THREAD_PRIORITY_NORMAL)
  {
    pol = SCHED_NORMAL;
    param.sched_priority=0;
  }
  else 
  {
    int lb = s_rt_max;
    if (prio < THREAD_PRIORITY_TIME_CRITICAL) 
    {
      lb--;
      if (prio < THREAD_PRIORITY_HIGHEST)  
      {
        lb--;
        if (prio < THREAD_PRIORITY_ABOVE_NORMAL) lb--;

        if (lb > 40) lb = 40; // if not HIGHEST or higher, do not permit RT priority of more than 40
      }
    }
    param.sched_priority = lb < 1 ? 1 : lb;
    pol = SCHED_RR;
  }
  return !pthread_setschedparam(evt->pt,pol,&param);
#else
  if (!pthread_getschedparam(evt->pt,&pol,&param))
  {
    // this is for darwin, but might work elsewhere
    param.sched_priority = 31 + prio;

    int mt=sched_get_priority_min(pol);
    if (param.sched_priority<mt||param.sched_priority > (mt=sched_get_priority_max(pol)))param.sched_priority=mt;
    
    if (!pthread_setschedparam(evt->pt,pol,&param))
    {
      return TRUE;
    }
  }
  return FALSE;
#endif
}

BOOL SetEvent(HANDLE hand)
{
  SWELL_InternalObjectHeader_Event *evt=(SWELL_InternalObjectHeader_Event*)hand;
  if (WDL_NOT_NORMALLY(!evt)) return FALSE;
  if (evt->hdr.type == INTERNAL_OBJECT_EVENT) 
  {
    pthread_mutex_lock(&evt->mutex);
    if (!evt->isSignal)
    {
      evt->isSignal = true;
      if (evt->isManualReset) pthread_cond_broadcast(&evt->cond);
      else pthread_cond_signal(&evt->cond);
    }
    pthread_mutex_unlock(&evt->mutex);
    return TRUE;
  }
  if (evt->hdr.type == INTERNAL_OBJECT_SOCKETEVENT)
  {
    SWELL_InternalObjectHeader_SocketEvent *se=(SWELL_InternalObjectHeader_SocketEvent*)hand;
    if (se->socket[1]>=0)
    {
      if (se->socket[0]>=0) 
      {
        struct pollfd fd = { se->socket[0], POLLIN, 0 };
        int res = poll(&fd,1,0);
        if (res > 0 && (fd.revents&POLLIN)) return TRUE; // already set
      }
      char c=0; 
      if (write(se->socket[1],&c,1) != 1)
      {
        WDL_ASSERT( false /* write to socket failed in SetEvent() */ );
      }
    }
    return TRUE;
  }
  WDL_ASSERT(false);
  return FALSE;
}
BOOL ResetEvent(HANDLE hand)
{
  SWELL_InternalObjectHeader_Event *evt=(SWELL_InternalObjectHeader_Event*)hand;
  if (WDL_NOT_NORMALLY(!evt)) return FALSE;
  if (evt->hdr.type == INTERNAL_OBJECT_EVENT) 
  {
    evt->isSignal=false;
    return TRUE;
  }
  if (evt->hdr.type == INTERNAL_OBJECT_SOCKETEVENT) 
  {
    SWELL_InternalObjectHeader_SocketEvent *se=(SWELL_InternalObjectHeader_SocketEvent*)hand;
    if (se->socket[0]>=0)
    {
      char buf[128];
      if (read(se->socket[0],buf,sizeof(buf)) < 0)
      {
        WDL_ASSERT( false /* read from socket failed in ResetEvent() */ );
      }
    }
    return TRUE;
  }
  WDL_ASSERT(false);
  return FALSE;
}

BOOL WinOffsetRect(LPRECT lprc, int dx, int dy)
{
  if(!lprc) return 0;
  lprc->left+=dx;
  lprc->top+=dy;
  lprc->right+=dx;
  lprc->bottom+=dy;
  return TRUE;
}

BOOL WinSetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom)
{
  if(!lprc) return 0;
  lprc->left = xLeft;
  lprc->top = yTop;
  lprc->right = xRight;
  lprc->bottom = yBottom;
  return TRUE;
}


int WinIntersectRect(RECT *out, const RECT *in1, const RECT *in2)
{
  RECT tmp = *in1; in1 = &tmp;
  memset(out,0,sizeof(RECT));
  if (in1->right <= in1->left) return false;
  if (in2->right <= in2->left) return false;
  if (in1->bottom <= in1->top) return false;
  if (in2->bottom <= in2->top) return false;
  
  // left is maximum of minimum of right edges and max of left edges
  out->left = wdl_max(in1->left,in2->left);
  out->right = wdl_min(in1->right,in2->right);
  out->top=wdl_max(in1->top,in2->top);
  out->bottom = wdl_min(in1->bottom,in2->bottom);
  
  return out->right>out->left && out->bottom>out->top;
}
void WinUnionRect(RECT *out, const RECT *in1, const RECT *in2)
{
  if (in1->left == in1->right && in1->top == in1->bottom) 
  {
    *out = *in2;
  }
  else if (in2->left == in2->right && in2->top == in2->bottom) 
  {
    *out = *in1;
  }
  else
  {
    out->left = wdl_min(in1->left,in2->left);
    out->top = wdl_min(in1->top,in2->top);
    out->right=wdl_max(in1->right,in2->right);
    out->bottom=wdl_max(in1->bottom,in2->bottom);
  }
}


typedef struct
{
  int sz;
  int refcnt;
} GLOBAL_REC;


void *GlobalLock(HANDLE h)
{
  if (!h) return 0;
  GLOBAL_REC *rec=((GLOBAL_REC*)h)-1;
  rec->refcnt++;
  return h;
}
int GlobalSize(HANDLE h)
{
  if (!h) return 0;
  GLOBAL_REC *rec=((GLOBAL_REC*)h)-1;
  return rec->sz;
}

void GlobalUnlock(HANDLE h)
{
  if (!h) return;
  GLOBAL_REC *rec=((GLOBAL_REC*)h)-1;
  rec->refcnt--;
}
void GlobalFree(HANDLE h)
{
  if (!h) return;
  GLOBAL_REC *rec=((GLOBAL_REC*)h)-1;
  if (rec->refcnt)
  {
    // note error freeing locked ram
  }
  free(rec);
  
}
HANDLE GlobalAlloc(int flags, int sz)
{
  if (sz<0)sz=0;
  GLOBAL_REC *rec=(GLOBAL_REC*)malloc(sizeof(GLOBAL_REC)+sz);
  if (!rec) return 0;
  rec->sz=sz;
  rec->refcnt=0;
  if (flags&GMEM_FIXED) memset(rec+1,0,sz);
  return rec+1;
}

char *lstrcpyn(char *dest, const char *src, int l)
{
  if (l<1) return dest;

  char *dsrc=dest;
  while (--l > 0)
  {
    char p=*src++;
    if (!p) break;
    *dest++=p;
  }
  *dest++=0;

  return dsrc;
}

static WDL_Mutex s_libraryMutex;
static int libkeycomp(void **p1, void **p2)
{
  INT_PTR a=(INT_PTR)(*p1) - (INT_PTR)(*p2);
  if (a<0)return -1;
  if (a>0) return 1;
  return 0;
}
static WDL_AssocArray<void *, SWELL_HINSTANCE *> s_loadedLibs(libkeycomp); // index by OS-provided handle (rather than filename since filenames could be relative etc)

HINSTANCE LoadLibrary(const char *fn)
{
  return LoadLibraryGlobals(fn,false);
}

#ifndef SWELL_TARGET_OSX
extern "C" {
  void *SWELLAPI_GetFunc(const char *name);
};
#endif
      

HINSTANCE LoadLibraryGlobals(const char *fn, bool symbolsAsGlobals)
{
  if (!fn || !*fn) return NULL;
  
  void *inst = NULL, *bundleinst=NULL;

#ifdef SWELL_TARGET_OSX
  struct stat ss;
  if (stat(fn,&ss) || (ss.st_mode&S_IFDIR))
  {
    CFStringRef str=(CFStringRef)SWELL_CStringToCFString(fn); 
    CFURLRef r=CFURLCreateWithFileSystemPath(NULL,str,kCFURLPOSIXPathStyle,true);
    CFRelease(str);
  
    bundleinst=(void *)CFBundleCreate(NULL,r);
    CFRelease(r);
    
    if (bundleinst)
    {
      if (!CFBundleLoadExecutable((CFBundleRef)bundleinst))
      {
        CFRelease((CFBundleRef)bundleinst);
        bundleinst=NULL;
      }
    }      
  }
#endif

#ifdef SWELL_TARGET_OSX
  if (!bundleinst)
#endif
  {
    inst=dlopen(fn,RTLD_NOW|(symbolsAsGlobals?RTLD_GLOBAL:RTLD_LOCAL));
    if (!inst) 
    {
#ifndef SWELL_TARGET_OSX
      struct stat ss;
      if (fn[0] == '/' && !stat(fn,&ss) && !(ss.st_mode&S_IFDIR))
      {
        const char *err = dlerror();
        printf("swell: dlopen() failed: %s\n",err ? err : fn);
      }
#endif
      return 0;
    }
  }

  WDL_MutexLock lock(&s_libraryMutex);
  
  SWELL_HINSTANCE *rec = s_loadedLibs.Get(bundleinst ? bundleinst : inst);
  if (!rec) 
  { 
    rec = (SWELL_HINSTANCE *)calloc(sizeof(SWELL_HINSTANCE),1);
    rec->instptr = inst;
#ifdef __APPLE__
    rec->bundleinstptr =  bundleinst;
#endif
    rec->refcnt = 1;
    s_loadedLibs.Insert(bundleinst ? bundleinst : inst,rec);
  
#ifndef SWELL_EXTRA_MINIMAL
    int (*SWELL_dllMain)(HINSTANCE, DWORD, LPVOID) = 0;
    BOOL (*dllMain)(HINSTANCE, DWORD, LPVOID) = 0;
    *(void **)&SWELL_dllMain = GetProcAddress(rec,"SWELL_dllMain");
    if (SWELL_dllMain)
    {
      if (!SWELL_dllMain(rec,DLL_PROCESS_ATTACH,
#ifdef SWELL_TARGET_OSX
            NULL
#else
            (void*)SWELLAPI_GetFunc
#endif
            ))
      {
        FreeLibrary(rec);
        return 0;
      }
      *(void **)&dllMain = GetProcAddress(rec,"DllMain");
      if (dllMain)
      {
        if (!dllMain(rec,DLL_PROCESS_ATTACH,NULL))
        { 
          SWELL_dllMain(rec,DLL_PROCESS_DETACH,(void*)NULL);
          FreeLibrary(rec);
          return 0;
        }
      }
    }
    rec->SWELL_dllMain = SWELL_dllMain;
    rec->dllMain = dllMain;
#endif
  }
  else rec->refcnt++;

  return rec;
}

void *GetProcAddress(HINSTANCE hInst, const char *procName)
{
  if (WDL_NOT_NORMALLY(!hInst)) return 0;

  SWELL_HINSTANCE *rec=(SWELL_HINSTANCE*)hInst;

  void *ret = NULL;
#ifdef SWELL_TARGET_OSX
  if (rec->bundleinstptr)
  {
    CFStringRef str=(CFStringRef)SWELL_CStringToCFString(procName); 
    ret = (void *)CFBundleGetFunctionPointerForName((CFBundleRef)rec->bundleinstptr, str);
    if (ret) rec->lastSymbolRequested=ret;
    CFRelease(str);
    return ret;
  }
#endif
  if (rec->instptr)  ret=(void *)dlsym(rec->instptr, procName);
  if (ret) rec->lastSymbolRequested=ret;
  return ret;
}

BOOL FreeLibrary(HINSTANCE hInst)
{
  if (WDL_NOT_NORMALLY(!hInst)) return FALSE;

  WDL_MutexLock lock(&s_libraryMutex);

  bool dofree=false;
  SWELL_HINSTANCE *rec=(SWELL_HINSTANCE*)hInst;
  if (--rec->refcnt<=0) 
  {
    dofree=true;
#ifdef SWELL_TARGET_OSX
    s_loadedLibs.Delete(rec->bundleinstptr ? rec->bundleinstptr : rec->instptr); 
#else
    s_loadedLibs.Delete(rec->instptr); 
#endif
    
#ifndef SWELL_EXTRA_MINIMAL
    if (rec->SWELL_dllMain) 
    {
      rec->SWELL_dllMain(rec,DLL_PROCESS_DETACH,NULL);
      if (rec->dllMain) rec->dllMain(rec,DLL_PROCESS_DETACH,NULL);
    }
#endif
  }

#ifdef SWELL_TARGET_OSX
  if (rec->bundleinstptr)
  {
    CFRelease((CFBundleRef)rec->bundleinstptr);
  }
#endif
  if (rec->instptr) dlclose(rec->instptr); 
  
  if (dofree) free(rec);
  return TRUE;
}

void* SWELL_GetBundle(HINSTANCE hInst)
{
  SWELL_HINSTANCE* rec=(SWELL_HINSTANCE*)hInst;
  WDL_ASSERT(rec!=NULL);
#ifdef SWELL_TARGET_OSX
  if (rec) return rec->bundleinstptr;
#else
  if (rec) return rec->instptr;
#endif
  return NULL;
}

DWORD GetModuleFileName(HINSTANCE hInst, char *fn, DWORD nSize)
{
  *fn=0;

  void *instptr = NULL, *lastSymbolRequested=NULL;
#ifdef SWELL_TARGET_OSX
  void *bundleinstptr=NULL;
#endif
  if (hInst)
  {
    SWELL_HINSTANCE *p = (SWELL_HINSTANCE*)hInst;
    instptr = p->instptr;
#ifdef SWELL_TARGET_OSX
    bundleinstptr = p->bundleinstptr;
#endif
    lastSymbolRequested=p->lastSymbolRequested;
  }
#ifdef SWELL_TARGET_OSX
  if (!instptr || bundleinstptr)
  {
    CFBundleRef bund=bundleinstptr ? (CFBundleRef)bundleinstptr : CFBundleGetMainBundle();
    if (bund) 
    {
      CFURLRef url=CFBundleCopyBundleURL(bund);
      if (url)
      {
        char buf[8192];
        if (CFURLGetFileSystemRepresentation(url,true,(UInt8*)buf,sizeof(buf))) lstrcpyn(fn,buf,nSize);
        CFRelease(url);
      }
    }
    return (DWORD)strlen(fn);
  }
#elif defined(__linux__)
  if (!instptr) // get exe file name
  {
    int sz=readlink("/proc/self/exe",fn,nSize);
    if (sz<1)
    {
       static char tmp;
       // this will likely not work if the program was launched with a relative path 
       // and the cwd has changed, but give it a try anyway
       Dl_info inf={0,};
       if (dladdr(&tmp,&inf) && inf.dli_fname)
         sz = (int) strlen(inf.dli_fname);
       else
         sz=0;
    }
    if ((DWORD)sz>=nSize)sz=nSize-1;
    fn[sz]=0;
    return sz;
  }
#endif

  if (instptr && lastSymbolRequested)
  {
    Dl_info inf={0,};
    dladdr(lastSymbolRequested,&inf);
    if (inf.dli_fname)
    {
      lstrcpyn(fn,inf.dli_fname,nSize);
      return (DWORD)strlen(fn);
    }
  }
  return 0;
}


bool SWELL_GenerateGUID(void *g)
{
#ifdef SWELL_TARGET_OSX
  CFUUIDRef r = CFUUIDCreate(NULL);
  if (!r) return false;
  CFUUIDBytes a = CFUUIDGetUUIDBytes(r);
  if (g) memcpy(g,&a,16);
  CFRelease(r);
  return true;
#else
  int f = open("/dev/urandom",O_RDONLY);
  if (f<0) return false;

  int v = read(f,g,sizeof(GUID));
  close(f);
  return v == sizeof(GUID);
#endif
}



void GetTempPath(int bufsz, char *buf)
{
  if (bufsz<2)
  {
    if (bufsz>0) *buf=0;
    return;
  }

#ifdef __APPLE__
  const char *p = getenv("TMPDIR");
#else
  const char *p = getenv("TEMP");
#endif
  if (!p || !*p) p="/tmp/";
  lstrcpyn(buf, p, bufsz);

  size_t len = strlen(buf);
  if (!len || buf[len-1] != '/')
  {
    if (len > (size_t)bufsz-2) len = bufsz-2;

    buf[len] = '/';
    buf[len+1]=0;
  }
}

const char *g_swell_appname;
char *g_swell_defini;
const char *g_swell_fontpangram;

void *SWELL_ExtendedAPI(const char *key, void *v)
{
  if (!strcmp(key,"APPNAME")) g_swell_appname = (const char *)v;
  else if (!strcmp(key,"INIFILE"))
  {
    free(g_swell_defini);
    g_swell_defini = v ? strdup((const char *)v) : NULL;

    #ifndef SWELL_TARGET_OSX
    char buf[1024];
    GetPrivateProfileString(".swell","max_open_files","",buf,sizeof(buf),"");
    if (!buf[0])
      WritePrivateProfileString(".swell","max_open_files","auto // (default is min of default or 16384)","");

    struct rlimit rl = {0,};
    getrlimit(RLIMIT_NOFILE,&rl); 

    const int orig_n = atoi(buf);
    rlim_t n = orig_n > 0 ? (rlim_t) orig_n : 16384;
    if (n > rl.rlim_max) n = rl.rlim_max;
    if (orig_n > 0 ? (n != rl.rlim_cur) : (n > rl.rlim_cur))
    {
      rl.rlim_cur = n;
      setrlimit(RLIMIT_NOFILE,&rl); 
      #ifdef _DEBUG
        getrlimit(RLIMIT_NOFILE,&rl); 
        printf("applied rlimit %d/%d\n",(int)rl.rlim_cur,(int)rl.rlim_max);
      #endif
    }
    #endif

    #ifdef SWELL_TARGET_GDK
      if (g_swell_defini)
      {
        void swell_load_color_theme(const char *fn);
        lstrcpyn_safe(buf,g_swell_defini,sizeof(buf));
        WDL_remove_filepart(buf);
        if (buf[0])
        {
          lstrcatn(buf,"/libSwell.colortheme",sizeof(buf));
          swell_load_color_theme(buf);
          WDL_remove_fileext(buf);
          lstrcatn(buf,"-user.colortheme",sizeof(buf));
          swell_load_color_theme(buf);
        }
      }

      GetPrivateProfileString(".swell","ui_scale","",buf,sizeof(buf),"");
      if (buf[0])
      {
        double sc = atof(buf);
        if (sc > 0.01 && sc < 10.0 && sc != 1.0)
        {
          g_swell_ui_scale = (int) (256 * sc + 0.5);
        }
      }
      else
      {
        WritePrivateProfileString(".swell","ui_scale","1.0 // scales the sizes in libSwell.colortheme","");
      }

      bool no_auto_hidpi=false;
      GetPrivateProfileString(".swell","ui_scale_auto","",buf,sizeof(buf),"");
      if (buf[0])
      {
        const char *p = buf;
        while (*p == ' ') p++;
        if (*p == '0' && atoi(p) == 0)
          no_auto_hidpi=true;
      }
      else
      {
        WritePrivateProfileString(".swell","ui_scale_auto","1 // set to 0 to disable system DPI detection (only used when ui_scale=1)","");
      }
     
      swell_scaling_init(no_auto_hidpi);

      if (g_swell_ui_scale != 256)
      {
        const double sc = g_swell_ui_scale * (1.0 / 256.0);
        if (sc>0) g_swell_ctheme.default_font_size--;
        #define __scale(x,c) g_swell_ctheme.x = (int) (g_swell_ctheme.x * sc + 0.5);
          SWELL_GENERIC_THEMESIZEDEFS(__scale,__scale)
        #undef __scale
        if (sc>0) g_swell_ctheme.default_font_size++;
      }
    #endif
  }
  else if (!strcmp(key,"FONTPANGRAM"))
  {
    g_swell_fontpangram = (const char *)v;
  }
#ifndef SWELL_TARGET_OSX
#ifndef SWELL_EXTRA_MINIMAL
  else if (!strcmp(key,"FULLSCREEN") || !strcmp(key,"-FULLSCREEN"))
  {
    int swell_fullscreenWindow(HWND, BOOL);
    return (void*)(INT_PTR)swell_fullscreenWindow((HWND)v, key[0] != '-');
  }
#endif
#endif
#ifdef SWELL_TARGET_GDK
  else if (!strcmp(key,"activate_app"))
  {
    void swell_gdk_reactivate_app(void);
    swell_gdk_reactivate_app();
  }
#endif
  return NULL;
}


#endif
