// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_STACK_TRACE_H_
#define BASE_DEBUG_STACK_TRACE_H_
#pragma once

#include <iosfwd>
#include <string>

#include "base/base_export.h"
#include "build/build_config.h"

#if defined(OS_WIN)
struct _EXCEPTION_POINTERS;
#endif

namespace base {
namespace debug {

// A stacktrace can be helpful in debugging. For example, you can include a
// stacktrace member in a object (probably around #ifndef NDEBUG) so that you
// can later see where the given object was created from.
class BASE_EXPORT StackTrace {
 public:
  // Creates a stacktrace from the current location.
  StackTrace();

  // Creates a stacktrace from an existing array of instruction
  // pointers (such as returned by Addresses()).  |count| will be
  // trimmed to |kMaxTraces|.
  StackTrace(const void* const* trace, size_t count);

#if defined(OS_WIN)
  // Creates a stacktrace for an exception.
  // Note: this function will throw an import not found (StackWalk64) exception
  // on system without dbghelp 5.1.
  StackTrace(_EXCEPTION_POINTERS* exception_pointers);
#endif

  // Copying and assignment are allowed with the default functions.

  ~StackTrace();

  // Gets an array of instruction pointer values. |*count| will be set to the
  // number of elements in the returned array.
  const void* const* Addresses(size_t* count) const;

  // Prints a backtrace to stderr
  void PrintBacktrace() const;

  // Resolves backtrace to symbols and write to stream.
  void OutputToStream(std::ostream* os) const;

  // Resolves backtrace to symbols and returns as string.
  std::string ToString() const;

 private:
  // From http://msdn.microsoft.com/en-us/library/bb204633.aspx,
  // the sum of FramesToSkip and FramesToCapture must be less than 63,
  // so set it to 62. Even if on POSIX it could be a larger value, it usually
  // doesn't give much more information.
  static const int kMaxTraces = 62;

  void* trace_[kMaxTraces];
  int count_;
};

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_STACK_TRACE_H_
