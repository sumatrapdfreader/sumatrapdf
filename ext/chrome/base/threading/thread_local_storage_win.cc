// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_local_storage.h"

#include <windows.h>

#include "base/logging.h"


namespace base {

namespace {
// The maximum number of 'slots' in our thread local storage stack.
const int kThreadLocalStorageSize = 64;

// The maximum number of times to try to clear slots by calling destructors.
// Use pthread naming convention for clarity.
const int kMaxDestructorIterations = kThreadLocalStorageSize;

// An array of destructor function pointers for the slots.  If a slot has a
// destructor, it will be stored in its corresponding entry in this array.
// The elements are volatile to ensure that when the compiler reads the value
// to potentially call the destructor, it does so once, and that value is tested
// for null-ness and then used. Yes, that would be a weird de-optimization,
// but I can imagine some register machines where it was just as easy to
// re-fetch an array element, and I want to be sure a call to free the key
// (i.e., null out the destructor entry) that happens on a separate thread can't
// hurt the racy calls to the destructors on another thread.
volatile ThreadLocalStorage::TLSDestructorFunc
    g_tls_destructors[kThreadLocalStorageSize];

}  // namespace anonymous

// In order to make TLS destructors work, we need to keep function
// pointers to the destructor for each TLS that we allocate.
// We make this work by allocating a single OS-level TLS, which
// contains an array of slots for the application to use.  In
// parallel, we also allocate an array of destructors, which we
// keep track of and call when threads terminate.

// tls_key_ is the one native TLS that we use.  It stores our
// table.
long ThreadLocalStorage::tls_key_ = TLS_OUT_OF_INDEXES;

// tls_max_ is the high-water-mark of allocated thread local storage.
// We intentionally skip 0 so that it is not confused with an
// unallocated TLS slot.
long ThreadLocalStorage::tls_max_ = 1;

void** ThreadLocalStorage::Initialize() {
  if (tls_key_ == TLS_OUT_OF_INDEXES) {
    long value = TlsAlloc();
    DCHECK(value != TLS_OUT_OF_INDEXES);

    // Atomically test-and-set the tls_key.  If the key is TLS_OUT_OF_INDEXES,
    // go ahead and set it.  Otherwise, do nothing, as another
    // thread already did our dirty work.
    if (InterlockedCompareExchange(&tls_key_, value, TLS_OUT_OF_INDEXES) !=
            TLS_OUT_OF_INDEXES) {
      // We've been shortcut. Another thread replaced tls_key_ first so we need
      // to destroy our index and use the one the other thread got first.
      TlsFree(value);
    }
  }
  DCHECK(!TlsGetValue(tls_key_));

  // Some allocators, such as TCMalloc, make use of thread local storage.
  // As a result, any attempt to call new (or malloc) will lazily cause such a
  // system to initialize, which will include registering for a TLS key.  If we
  // are not careful here, then that request to create a key will call new back,
  // and we'll have an infinite loop.  We avoid that as follows:
  // Use a stack allocated vector, so that we don't have dependence on our
  // allocator until our service is in place.  (i.e., don't even call new until
  // after we're setup)
  void* stack_allocated_tls_data[kThreadLocalStorageSize];
  memset(stack_allocated_tls_data, 0, sizeof(stack_allocated_tls_data));
  // Ensure that any rentrant calls change the temp version.
  TlsSetValue(tls_key_, stack_allocated_tls_data);

  // Allocate an array to store our data.
  void** tls_data = new void*[kThreadLocalStorageSize];
  memcpy(tls_data, stack_allocated_tls_data, sizeof(stack_allocated_tls_data));
  TlsSetValue(tls_key_, tls_data);
  return tls_data;
}

ThreadLocalStorage::Slot::Slot(TLSDestructorFunc destructor)
    : initialized_(false),
      slot_(0) {
  Initialize(destructor);
}

bool ThreadLocalStorage::Slot::Initialize(TLSDestructorFunc destructor) {
  if (tls_key_ == TLS_OUT_OF_INDEXES || !TlsGetValue(tls_key_))
    ThreadLocalStorage::Initialize();

  // Grab a new slot.
  slot_ = InterlockedIncrement(&tls_max_) - 1;
  DCHECK_GT(slot_, 0);
  if (slot_ >= kThreadLocalStorageSize) {
    NOTREACHED();
    return false;
  }

  // Setup our destructor.
  g_tls_destructors[slot_] = destructor;
  initialized_ = true;
  return true;
}

void ThreadLocalStorage::Slot::Free() {
  // At this time, we don't reclaim old indices for TLS slots.
  // So all we need to do is wipe the destructor.
  DCHECK_GT(slot_, 0);
  DCHECK_LT(slot_, kThreadLocalStorageSize);
  g_tls_destructors[slot_] = NULL;
  slot_ = 0;
  initialized_ = false;
}

void* ThreadLocalStorage::Slot::Get() const {
  void** tls_data = static_cast<void**>(TlsGetValue(tls_key_));
  if (!tls_data)
    tls_data = ThreadLocalStorage::Initialize();
  DCHECK_GT(slot_, 0);
  DCHECK_LT(slot_, kThreadLocalStorageSize);
  return tls_data[slot_];
}

void ThreadLocalStorage::Slot::Set(void* value) {
  void** tls_data = static_cast<void**>(TlsGetValue(tls_key_));
  if (!tls_data)
    tls_data = ThreadLocalStorage::Initialize();
  DCHECK_GT(slot_, 0);
  DCHECK_LT(slot_, kThreadLocalStorageSize);
  tls_data[slot_] = value;
}

void ThreadLocalStorage::ThreadExit() {
  if (tls_key_ == TLS_OUT_OF_INDEXES)
    return;

  void** tls_data = static_cast<void**>(TlsGetValue(tls_key_));
  // Maybe we have never initialized TLS for this thread.
  if (!tls_data)
    return;

  // Some allocators, such as TCMalloc, use TLS.  As a result, when a thread
  // terminates, one of the destructor calls we make may be to shut down an
  // allocator.  We have to be careful that after we've shutdown all of the
  // known destructors (perchance including an allocator), that we don't call
  // the allocator and cause it to resurrect itself (with no possibly destructor
  // call to follow).  We handle this problem as follows:
  // Switch to using a stack allocated vector, so that we don't have dependence
  // on our allocator after we have called all g_tls_destructors.  (i.e., don't
  // even call delete[] after we're done with destructors.)
  void* stack_allocated_tls_data[kThreadLocalStorageSize];
  memcpy(stack_allocated_tls_data, tls_data, sizeof(stack_allocated_tls_data));
  // Ensure that any re-entrant calls change the temp version.
  TlsSetValue(tls_key_, stack_allocated_tls_data);
  delete[] tls_data;  // Our last dependence on an allocator.

  int remaining_attempts = kMaxDestructorIterations;
  bool need_to_scan_destructors = true;
  while (need_to_scan_destructors) {
    need_to_scan_destructors = false;
    // Try to destroy the first-created-slot (which is slot 1) in our last
    // destructor call.  That user was able to function, and define a slot with
    // no other services running, so perhaps it is a basic service (like an
    // allocator) and should also be destroyed last.  If we get the order wrong,
    // then we'll itterate several more times, so it is really not that
    // critical (but it might help).
    for (int slot = tls_max_ - 1; slot > 0; --slot) {
      void* value = stack_allocated_tls_data[slot];
      if (value == NULL)
        continue;
      TLSDestructorFunc destructor = g_tls_destructors[slot];
      if (destructor == NULL)
        continue;
      stack_allocated_tls_data[slot] = NULL;  // pre-clear the slot.
      destructor(value);
      // Any destructor might have called a different service, which then set
      // a different slot to a non-NULL value.  Hence we need to check
      // the whole vector again.  This is a pthread standard.
      need_to_scan_destructors = true;
    }
    if (--remaining_attempts <= 0) {
      NOTREACHED();  // Destructors might not have been called.
      break;
    }
  }

  // Remove our stack allocated vector.
  TlsSetValue(tls_key_, NULL);
}

}  // namespace base

// Thread Termination Callbacks.
// Windows doesn't support a per-thread destructor with its
// TLS primitives.  So, we build it manually by inserting a
// function to be called on each thread's exit.
// This magic is from http://www.codeproject.com/threads/tls.asp
// and it works for VC++ 7.0 and later.

// Force a reference to _tls_used to make the linker create the TLS directory
// if it's not already there.  (e.g. if __declspec(thread) is not used).
// Force a reference to p_thread_callback_base to prevent whole program
// optimization from discarding the variable.
#ifdef _WIN64

#pragma comment(linker, "/INCLUDE:_tls_used")
#pragma comment(linker, "/INCLUDE:p_thread_callback_base")

#else  // _WIN64

#pragma comment(linker, "/INCLUDE:__tls_used")
#pragma comment(linker, "/INCLUDE:_p_thread_callback_base")

#endif  // _WIN64

// Static callback function to call with each thread termination.
void NTAPI OnThreadExit(PVOID module, DWORD reason, PVOID reserved) {
  // On XP SP0 & SP1, the DLL_PROCESS_ATTACH is never seen. It is sent on SP2+
  // and on W2K and W2K3. So don't assume it is sent.
  if (DLL_THREAD_DETACH == reason || DLL_PROCESS_DETACH == reason)
    base::ThreadLocalStorage::ThreadExit();
}

// .CRT$XLA to .CRT$XLZ is an array of PIMAGE_TLS_CALLBACK pointers that are
// called automatically by the OS loader code (not the CRT) when the module is
// loaded and on thread creation. They are NOT called if the module has been
// loaded by a LoadLibrary() call. It must have implicitly been loaded at
// process startup.
// By implicitly loaded, I mean that it is directly referenced by the main EXE
// or by one of its dependent DLLs. Delay-loaded DLL doesn't count as being
// implicitly loaded.
//
// See VC\crt\src\tlssup.c for reference.

// extern "C" suppresses C++ name mangling so we know the symbol name for the
// linker /INCLUDE:symbol pragma above.
extern "C" {
// The linker must not discard p_thread_callback_base.  (We force a reference
// to this variable with a linker /INCLUDE:symbol pragma to ensure that.) If
// this variable is discarded, the OnThreadExit function will never be called.
#ifdef _WIN64

// .CRT section is merged with .rdata on x64 so it must be constant data.
#pragma const_seg(".CRT$XLB")
// When defining a const variable, it must have external linkage to be sure the
// linker doesn't discard it.
extern const PIMAGE_TLS_CALLBACK p_thread_callback_base;
const PIMAGE_TLS_CALLBACK p_thread_callback_base = OnThreadExit;

// Reset the default section.
#pragma const_seg()

#else  // _WIN64

#pragma data_seg(".CRT$XLB")
PIMAGE_TLS_CALLBACK p_thread_callback_base = OnThreadExit;

// Reset the default section.
#pragma data_seg()

#endif  // _WIN64
}  // extern "C"
