// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"

namespace base {

void BaseTimer_Helper::OrphanDelayedTask() {
  if (delayed_task_) {
    delayed_task_->timer_ = NULL;
    delayed_task_ = NULL;
  }
}

void BaseTimer_Helper::InitiateDelayedTask(TimerTask* timer_task) {
  OrphanDelayedTask();

  delayed_task_ = timer_task;
  delayed_task_->timer_ = this;
  MessageLoop::current()->PostDelayedTask(
      timer_task->posted_from_,
      base::Bind(&TimerTask::Run, base::Owned(timer_task)),
      static_cast<int>(timer_task->delay_.InMillisecondsRoundedUp()));
}

}  // namespace base
