// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"

#include "base/callback.h"

namespace base {

void DoNothing() {
}

ScopedClosureRunner::ScopedClosureRunner(const Closure& closure)
    : closure_(closure) {
}

ScopedClosureRunner::~ScopedClosureRunner() {
  if (!closure_.is_null())
    closure_.Run();
}

Closure ScopedClosureRunner::Release() {
  Closure result = closure_;
  closure_.Reset();
  return result;
}

}  // namespace base
