// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple struct with tracking information that is stored
// with a PendingTask (when message_loop is handling the task).
// Only the information that is shared with the profiler in tracked_objects
// are included in this structure.


#ifndef BASE_TRACKING_INFO_H_
#define BASE_TRACKING_INFO_H_

#include "base/time.h"

namespace tracked_objects {
class Location;
class Births;
}

namespace base {

// This structure is copied around by value.
struct BASE_EXPORT TrackingInfo {
  TrackingInfo(const tracked_objects::Location& posted_from,
               base::TimeTicks delayed_run_time);
  ~TrackingInfo();

  // Record of location and thread that the task came from.
  tracked_objects::Births* birth_tally;

  // Time when the related task was posted.
  base::TimeTicks time_posted;

  // The time when the task should be run.
  base::TimeTicks delayed_run_time;
};

}  // namespace base

#endif  // BASE_TRACKING_INFO_H_
