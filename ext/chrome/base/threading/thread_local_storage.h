// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_THREAD_LOCAL_STORAGE_H_
#define BASE_THREADING_THREAD_LOCAL_STORAGE_H_
#pragma once

#include "base/base_export.h"
#include "base/basictypes.h"

#if defined(OS_POSIX)
#include <pthread.h>
#endif

namespace base {

// Wrapper for thread local storage.  This class doesn't do much except provide
// an API for portability.
class BASE_EXPORT ThreadLocalStorage {
 public:

  // Prototype for the TLS destructor function, which can be optionally used to
  // cleanup thread local storage on thread exit.  'value' is the data that is
  // stored in thread local storage.
  typedef void (*TLSDestructorFunc)(void* value);

  // A key representing one value stored in TLS.
  class BASE_EXPORT Slot {
   public:
    explicit Slot(TLSDestructorFunc destructor = NULL);

    // This constructor should be used for statics.
    // It returns an uninitialized Slot.
    explicit Slot(base::LinkerInitialized x) {}

    // Set up the TLS slot.  Called by the constructor.
    // 'destructor' is a pointer to a function to perform per-thread cleanup of
    // this object.  If set to NULL, no cleanup is done for this TLS slot.
    // Returns false on error.
    bool Initialize(TLSDestructorFunc destructor);

    // Free a previously allocated TLS 'slot'.
    // If a destructor was set for this slot, removes
    // the destructor so that remaining threads exiting
    // will not free data.
    void Free();

    // Get the thread-local value stored in slot 'slot'.
    // Values are guaranteed to initially be zero.
    void* Get() const;

    // Set the thread-local value stored in slot 'slot' to
    // value 'value'.
    void Set(void* value);

    bool initialized() const { return initialized_; }

   private:
    // The internals of this struct should be considered private.
    bool initialized_;
#if defined(OS_WIN)
    int slot_;
#elif defined(OS_POSIX)
    pthread_key_t key_;
#endif

    DISALLOW_COPY_AND_ASSIGN(Slot);
  };

#if defined(OS_WIN)
  // Function called when on thread exit to call TLS
  // destructor functions.  This function is used internally.
  static void ThreadExit();

 private:
  // Function to lazily initialize our thread local storage.
  static void **Initialize();

  static long tls_key_;
  static long tls_max_;
#endif  // OS_WIN

  DISALLOW_COPY_AND_ASSIGN(ThreadLocalStorage);
};

}  // namespace base

#endif  // BASE_THREADING_THREAD_LOCAL_STORAGE_H_
