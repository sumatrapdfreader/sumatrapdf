// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/wrapped_window_proc.h"

#include "base/atomicops.h"

namespace {

base::win::WinProcExceptionFilter s_exception_filter = NULL;

}  // namespace.

namespace base {
namespace win {

WinProcExceptionFilter SetWinProcExceptionFilter(
    WinProcExceptionFilter filter) {
  subtle::AtomicWord rv = subtle::NoBarrier_AtomicExchange(
      reinterpret_cast<subtle::AtomicWord*>(&s_exception_filter),
      reinterpret_cast<subtle::AtomicWord>(filter));
  return reinterpret_cast<WinProcExceptionFilter>(rv);
}

int CallExceptionFilter(EXCEPTION_POINTERS* info) {
  return s_exception_filter ? s_exception_filter(info) :
                              EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace win
}  // namespace base
