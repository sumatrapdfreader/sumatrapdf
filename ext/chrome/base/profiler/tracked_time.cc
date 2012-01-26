// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/tracked_time.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <mmsystem.h>  // Declare timeGetTime()... after including build_config.
#endif

namespace tracked_objects {

#if defined(USE_FAST_TIME_CLASS_FOR_DURATION_CALCULATIONS)

Duration::Duration() : ms_(0) {}
Duration::Duration(int32 duration) : ms_(duration) {}

Duration& Duration::operator+=(const Duration& other) {
  ms_ += other.ms_;
  return *this;
}

Duration Duration::operator+(const Duration& other) const {
  return Duration(ms_ + other.ms_);
}

bool Duration::operator==(const Duration& other) const {
  return ms_ == other.ms_;
}

bool Duration::operator!=(const Duration& other) const {
  return ms_ != other.ms_;
}

bool Duration::operator>(const Duration& other) const {
  return ms_ > other.ms_;
}

// static
Duration Duration::FromMilliseconds(int ms) { return Duration(ms); }

int32 Duration::InMilliseconds() const { return ms_; }

//------------------------------------------------------------------------------

TrackedTime::TrackedTime() : ms_(0) {}
TrackedTime::TrackedTime(int32 ms) : ms_(ms) {}
TrackedTime::TrackedTime(const base::TimeTicks& time)
    : ms_((time - base::TimeTicks()).InMilliseconds()) {
}

// static
TrackedTime TrackedTime::Now() {
#if defined(OS_WIN)
  // Use lock-free accessor to 32 bit time.
  // Note that TimeTicks::Now() is built on this, so we have "compatible"
  // times when we down-convert a TimeTicks sample.
  // TODO(jar): Surface this interface via something in base/time.h.
  return TrackedTime(static_cast<int32>(timeGetTime()));
#else
  // Posix has nice cheap 64 bit times, so we just down-convert it.
  return TrackedTime(base::TimeTicks::Now());
#endif  // OS_WIN
}

Duration TrackedTime::operator-(const TrackedTime& other) const {
  return Duration(ms_ - other.ms_);
}

TrackedTime TrackedTime::operator+(const Duration& other) const {
  return TrackedTime(ms_ + other.ms_);
}

bool TrackedTime::is_null() const { return ms_ == 0; }

#endif  // USE_FAST_TIME_CLASS_FOR_DURATION_CALCULATIONS

}  // namespace tracked_objects
