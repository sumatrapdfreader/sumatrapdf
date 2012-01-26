// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FLOAT_UTIL_H_
#define BASE_FLOAT_UTIL_H_
#pragma once

#include "build/build_config.h"

#include <float.h>
#include <math.h>

#if defined(OS_SOLARIS)
#include <ieeefp.h>
#endif

namespace base {

inline bool IsFinite(const double& number) {
#if defined(OS_POSIX)
  return finite(number) != 0;
#elif defined(OS_WIN)
  return _finite(number) != 0;
#endif
}

}  // namespace base

#endif  // BASE_FLOAT_UTIL_H_
