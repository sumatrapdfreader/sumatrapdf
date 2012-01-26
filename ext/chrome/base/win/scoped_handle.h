// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_HANDLE_H_
#define BASE_WIN_SCOPED_HANDLE_H_
#pragma once

#include <windows.h>

#include "base/basictypes.h"
#include "base/logging.h"

namespace base {
namespace win {

// Used so we always remember to close the handle.
// The class interface matches that of ScopedStdioHandle in  addition to an
// IsValid() method since invalid handles on windows can be either NULL or
// INVALID_HANDLE_VALUE (-1).
//
// Example:
//   ScopedHandle hfile(CreateFile(...));
//   if (!hfile.Get())
//     ...process error
//   ReadFile(hfile.Get(), ...);
//
// To sqirrel the handle away somewhere else:
//   secret_handle_ = hfile.Take();
//
// To explicitly close the handle:
//   hfile.Close();
class ScopedHandle {
 public:
  ScopedHandle() : handle_(NULL) {
  }

  explicit ScopedHandle(HANDLE h) : handle_(NULL) {
    Set(h);
  }

  ~ScopedHandle() {
    Close();
  }

  // Use this instead of comparing to INVALID_HANDLE_VALUE to pick up our NULL
  // usage for errors.
  bool IsValid() const {
    return handle_ != NULL;
  }

  void Set(HANDLE new_handle) {
    Close();

    // Windows is inconsistent about invalid handles, so we always use NULL
    if (new_handle != INVALID_HANDLE_VALUE)
      handle_ = new_handle;
  }

  HANDLE Get() {
    return handle_;
  }

  operator HANDLE() { return handle_; }

  HANDLE Take() {
    // transfers ownership away from this object
    HANDLE h = handle_;
    handle_ = NULL;
    return h;
  }

  void Close() {
    if (handle_) {
      if (!::CloseHandle(handle_)) {
        NOTREACHED();
      }
      handle_ = NULL;
    }
  }

 private:
  HANDLE handle_;
  DISALLOW_COPY_AND_ASSIGN(ScopedHandle);
};

}  // namespace win
}  // namespace base

#endif  // BASE_SCOPED_HANDLE_WIN_H_
