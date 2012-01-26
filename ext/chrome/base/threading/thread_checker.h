// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_THREAD_CHECKER_H_
#define BASE_THREADING_THREAD_CHECKER_H_
#pragma once

// Apart from debug builds, we also enable the thread checker in
// builds with DCHECK_ALWAYS_ON so that trybots and waterfall bots
// with this define will get the same level of thread checking as
// debug bots.
//
// Note that this does not perfectly match situations where DCHECK is
// enabled.  For example a non-official release build may have
// DCHECK_ALWAYS_ON undefined (and therefore ThreadChecker would be
// disabled) but have DCHECKs enabled at runtime.
#define ENABLE_THREAD_CHECKER (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))

#if ENABLE_THREAD_CHECKER
#include "base/threading/thread_checker_impl.h"
#endif

namespace base {

// Do nothing implementation, for use in release mode.
//
// Note: You should almost always use the ThreadChecker class to get the
// right version for your build configuration.
class ThreadCheckerDoNothing {
 public:
  bool CalledOnValidThread() const {
    return true;
  }

  void DetachFromThread() {}
};

// Before using this class, please consider using NonThreadSafe as it
// makes it much easier to determine the nature of your class.
//
// ThreadChecker is a helper class used to help verify that some methods of a
// class are called from the same thread.  One can inherit from this class and
// use CalledOnValidThread() to verify.
//
// Inheriting from class indicates that one must be careful when using the
// class with multiple threads. However, it is up to the class document to
// indicate how it can be used with threads.
//
// Example:
// class MyClass : public ThreadChecker {
//  public:
//   void Foo() {
//     DCHECK(CalledOnValidThread());
//     ... (do stuff) ...
//   }
// }
//
// In Release mode, CalledOnValidThread will always return true.
#if ENABLE_THREAD_CHECKER
class ThreadChecker : public ThreadCheckerImpl {
};
#else
class ThreadChecker : public ThreadCheckerDoNothing {
};
#endif  // ENABLE_THREAD_CHECKER

#undef ENABLE_THREAD_CHECKER

}  // namespace base

#endif  // BASE_THREADING_THREAD_CHECKER_H_
