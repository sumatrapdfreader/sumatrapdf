#ifndef _WDL_SETTHREADNAME_H_
#define _WDL_SETTHREADNAME_H_

#include "wdltypes.h"

#if defined(_WIN32) && (defined(_DEBUG) || defined(WDL_SETTHREADNAME_WIN32_RELEASE))
// thread names only work on win32 when running in a debugger, so they are not enabled
// by default for release builds (defined WDL_SETTHREADNAME_WIN32_RELEASE if you wish to use
// them)

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

static void WDL_SetThreadName(const char* threadName) {
  const DWORD MS_VC_EXCEPTION = 0x406D1388;
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = threadName;
  info.dwThreadID = (DWORD)-1;
  info.dwFlags = 0;
#pragma warning(push)
#if _MSC_VER < 1300 && !defined(_WIN64)
#define ULONG_PTR ULONG
#else
#pragma warning(disable: 6320 6322)
#endif
  __try {
    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
  }
  __except (EXCEPTION_EXECUTE_HANDLER) {
  }
#pragma warning(pop)
}

#elif defined(__APPLE__)

#include <pthread.h>
#include <CoreServices/CoreServices.h>
#include <AvailabilityMacros.h>

extern "C" 
{
  void *objc_getClass(const char *p);
  void *sel_getUid(const char *p);
  void objc_msgSend(void);
};

static void WDL_STATICFUNC_UNUSED WDL_SetThreadName(const char* threadName) {
  void *(*send_msg)(void *, void *) = (void *(*)(void *, void *))objc_msgSend;
  void (*send_msg_cfstring)(void *, void *, CFStringRef) = (void (*)(void *, void *, CFStringRef))objc_msgSend;

  void *ct=send_msg( objc_getClass("NSThread"), sel_getUid("currentThread"));
  CFStringRef tn=CFStringCreateWithCString(NULL,threadName,kCFStringEncodingUTF8);
  send_msg_cfstring(ct,sel_getUid("setName:"), tn);
  CFRelease(tn);
#ifdef MAC_OS_X_VERSION_10_6
  pthread_setname_np(threadName);
#endif
}

#elif defined(__linux__)

#include <pthread.h>

static void WDL_STATICFUNC_UNUSED WDL_SetThreadName(const char* threadName) {
  char tmp[16];
  int x;
  for (x=0;*threadName && x<15; threadName++) if (*threadName != ' ') tmp[x++]=*threadName;
  tmp[x]=0;
  pthread_setname_np(pthread_self(),tmp);
}

#else

#define WDL_SetThreadName(x) do { } while (0)

#endif

#endif
