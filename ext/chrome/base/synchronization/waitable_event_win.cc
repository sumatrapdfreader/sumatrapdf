// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"

#include <math.h>
#include <windows.h>

#include "base/logging.h"
#include "base/time.h"

namespace base {

WaitableEvent::WaitableEvent(bool manual_reset, bool signaled)
    : handle_(CreateEvent(NULL, manual_reset, signaled, NULL)) {
  // We're probably going to crash anyways if this is ever NULL, so we might as
  // well make our stack reports more informative by crashing here.
  CHECK(handle_);
}

WaitableEvent::WaitableEvent(HANDLE handle)
    : handle_(handle) {
  CHECK(handle) << "Tried to create WaitableEvent from NULL handle";
}

WaitableEvent::~WaitableEvent() {
  CloseHandle(handle_);
}

HANDLE WaitableEvent::Release() {
  HANDLE rv = handle_;
  handle_ = INVALID_HANDLE_VALUE;
  return rv;
}

void WaitableEvent::Reset() {
  ResetEvent(handle_);
}

void WaitableEvent::Signal() {
  SetEvent(handle_);
}

bool WaitableEvent::IsSignaled() {
  return TimedWait(TimeDelta::FromMilliseconds(0));
}

void WaitableEvent::Wait() {
  DWORD result = WaitForSingleObject(handle_, INFINITE);
  // It is most unexpected that this should ever fail.  Help consumers learn
  // about it if it should ever fail.
  DCHECK_EQ(WAIT_OBJECT_0, result) << "WaitForSingleObject failed";
}

bool WaitableEvent::TimedWait(const TimeDelta& max_time) {
  DCHECK(max_time >= TimeDelta::FromMicroseconds(0));
  // Be careful here.  TimeDelta has a precision of microseconds, but this API
  // is in milliseconds.  If there are 5.5ms left, should the delay be 5 or 6?
  // It should be 6 to avoid returning too early.
  double timeout = ceil(max_time.InMillisecondsF());
  DWORD result = WaitForSingleObject(handle_, static_cast<DWORD>(timeout));
  switch (result) {
    case WAIT_OBJECT_0:
      return true;
    case WAIT_TIMEOUT:
      return false;
  }
  // It is most unexpected that this should ever fail.  Help consumers learn
  // about it if it should ever fail.
  NOTREACHED() << "WaitForSingleObject failed";
  return false;
}

// static
size_t WaitableEvent::WaitMany(WaitableEvent** events, size_t count) {
  HANDLE handles[MAXIMUM_WAIT_OBJECTS];
  CHECK_LE(count, MAXIMUM_WAIT_OBJECTS)
      << "Can only wait on " << MAXIMUM_WAIT_OBJECTS << " with WaitMany";

  for (size_t i = 0; i < count; ++i)
    handles[i] = events[i]->handle();

  // The cast is safe because count is small - see the CHECK above.
  DWORD result =
      WaitForMultipleObjects(static_cast<DWORD>(count),
                             handles,
                             FALSE,      // don't wait for all the objects
                             INFINITE);  // no timeout
  if (result >= WAIT_OBJECT_0 + count) {
    NOTREACHED() << "WaitForMultipleObjects failed: " << GetLastError();
    return 0;
  }

  return result - WAIT_OBJECT_0;
}

}  // namespace base
