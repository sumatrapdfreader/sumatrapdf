// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/tracked_objects.h"

#include "base/win/windows_version.h"

namespace base {

namespace {

static ThreadLocalPointer<char> current_thread_name;

// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD kVCThreadNameException = 0x406D1388;

typedef struct tagTHREADNAME_INFO {
  DWORD dwType;  // Must be 0x1000.
  LPCSTR szName;  // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;  // Reserved for future use, must be zero.
} THREADNAME_INFO;

// This function has try handling, so it is separated out of its caller.
void SetNameInternal(PlatformThreadId thread_id, const char* name) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = thread_id;
  info.dwFlags = 0;

  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info)/sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR*>(&info));
  } __except(EXCEPTION_CONTINUE_EXECUTION) {
  }
}

struct ThreadParams {
  PlatformThread::Delegate* delegate;
  bool joinable;
};

DWORD __stdcall ThreadFunc(void* params) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(params);
  PlatformThread::Delegate* delegate = thread_params->delegate;
  if (!thread_params->joinable)
    base::ThreadRestrictions::SetSingletonAllowed(false);
  delete thread_params;
  delegate->ThreadMain();
  return NULL;
}

// CreateThreadInternal() matches PlatformThread::Create(), except that
// |out_thread_handle| may be NULL, in which case a non-joinable thread is
// created.
bool CreateThreadInternal(size_t stack_size,
                          PlatformThread::Delegate* delegate,
                          PlatformThreadHandle* out_thread_handle) {
  PlatformThreadHandle thread_handle;
  unsigned int flags = 0;
  if (stack_size > 0 && base::win::GetVersion() >= base::win::VERSION_XP) {
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
  } else {
    stack_size = 0;
  }

  ThreadParams* params = new ThreadParams;
  params->delegate = delegate;
  params->joinable = out_thread_handle != NULL;

  // Using CreateThread here vs _beginthreadex makes thread creation a bit
  // faster and doesn't require the loader lock to be available.  Our code will
  // have to work running on CreateThread() threads anyway, since we run code
  // on the Windows thread pool, etc.  For some background on the difference:
  //   http://www.microsoft.com/msj/1099/win32/win321099.aspx
  thread_handle = CreateThread(
      NULL, stack_size, ThreadFunc, params, flags, NULL);
  if (!thread_handle) {
    delete params;
    return false;
  }

  if (out_thread_handle)
    *out_thread_handle = thread_handle;
  else
    CloseHandle(thread_handle);
  return true;
}

}  // namespace

// static
PlatformThreadId PlatformThread::CurrentId() {
  return GetCurrentThreadId();
}

// static
void PlatformThread::YieldCurrentThread() {
  ::Sleep(0);
}

// static
void PlatformThread::Sleep(int duration_ms) {
  ::Sleep(duration_ms);
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  ::Sleep(duration.InMillisecondsRoundedUp());
}

// static
void PlatformThread::SetName(const char* name) {
  current_thread_name.Set(const_cast<char*>(name));
  tracked_objects::ThreadData::InitializeThreadContext(name);

  // The debugger needs to be around to catch the name in the exception.  If
  // there isn't a debugger, we are just needlessly throwing an exception.
  if (!::IsDebuggerPresent())
    return;

  SetNameInternal(CurrentId(), name);
}

// static
const char* PlatformThread::GetName() {
  return current_thread_name.Get();
}

// static
bool PlatformThread::Create(size_t stack_size, Delegate* delegate,
                            PlatformThreadHandle* thread_handle) {
  DCHECK(thread_handle);
  return CreateThreadInternal(stack_size, delegate, thread_handle);
}

// static
bool PlatformThread::CreateNonJoinable(size_t stack_size, Delegate* delegate) {
  return CreateThreadInternal(stack_size, delegate, NULL);
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  DCHECK(thread_handle);
  // TODO(willchan): Enable this check once I can get it to work for Windows
  // shutdown.
  // Joining another thread may block the current thread for a long time, since
  // the thread referred to by |thread_handle| may still be running long-lived /
  // blocking tasks.
#if 0
  base::ThreadRestrictions::AssertIOAllowed();
#endif

  // Wait for the thread to exit.  It should already have terminated but make
  // sure this assumption is valid.
  DWORD result = WaitForSingleObject(thread_handle, INFINITE);
  DCHECK_EQ(WAIT_OBJECT_0, result);

  CloseHandle(thread_handle);
}

// static
void PlatformThread::SetThreadPriority(PlatformThreadHandle handle,
                                       ThreadPriority priority) {
  switch (priority) {
    case kThreadPriority_Normal:
      ::SetThreadPriority(handle, THREAD_PRIORITY_NORMAL);
      break;
    case kThreadPriority_RealtimeAudio:
      ::SetThreadPriority(handle, THREAD_PRIORITY_TIME_CRITICAL);
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
}

}  // namespace base
