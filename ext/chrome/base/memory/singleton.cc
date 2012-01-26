// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "base/threading/platform_thread.h"

namespace base {
namespace internal {

subtle::AtomicWord WaitForInstance(subtle::AtomicWord* instance) {
  // Handle the race. Another thread beat us and either:
  // - Has the object in BeingCreated state
  // - Already has the object created...
  // We know value != NULL.  It could be kBeingCreatedMarker, or a valid ptr.
  // Unless your constructor can be very time consuming, it is very unlikely
  // to hit this race.  When it does, we just spin and yield the thread until
  // the object has been created.
  subtle::AtomicWord value;
  while (true) {
    value = subtle::NoBarrier_Load(instance);
    if (value != kBeingCreatedMarker)
      break;
    PlatformThread::YieldCurrentThread();
  }
  return value;
}

}  // namespace internal
}  // namespace base

