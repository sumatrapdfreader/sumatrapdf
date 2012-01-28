// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PENDING_TASK_H_
#define PENDING_TASK_H_
#pragma once

#include <queue>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/time.h"

namespace base {

// Contains data about a pending task. Stored in TaskQueue and DelayedTaskQueue
// for use by classes that queue and execute tasks.
struct BASE_EXPORT PendingTask {
  PendingTask(const Closure& task);
  PendingTask(const Closure& task,
              TimeTicks delayed_run_time,
              bool nestable);
  ~PendingTask();

  // Used to support sorting.
  bool operator<(const PendingTask& other) const;

  // The task to run.
  Closure task;

  // Time when the related task was posted.
  base::TimeTicks time_posted;

  // The time when the task should be run.
  base::TimeTicks delayed_run_time;

  // Secondary sort key for run time.
  int sequence_num;

  // OK to dispatch from a nested loop.
  bool nestable;
};

// Wrapper around std::queue specialized for PendingTask which adds a Swap
// helper method.
class BASE_EXPORT TaskQueue : public std::queue<PendingTask> {
 public:
  void Swap(TaskQueue* queue);
};

// PendingTasks are sorted by their |delayed_run_time| property.
typedef std::priority_queue<base::PendingTask> DelayedTaskQueue;

}  // namespace base

#endif  // PENDING_TASK_H_
