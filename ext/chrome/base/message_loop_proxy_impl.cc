// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop_proxy_impl.h"

#include "base/location.h"
#include "base/threading/thread_restrictions.h"

namespace base {

MessageLoopProxyImpl::~MessageLoopProxyImpl() {
}

bool MessageLoopProxyImpl::PostTask(const tracked_objects::Location& from_here,
                                    const base::Closure& task) {
  return PostTaskHelper(from_here, task, 0, true);
}

bool MessageLoopProxyImpl::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  return PostTaskHelper(from_here, task, delay_ms, true);
}

bool MessageLoopProxyImpl::PostNonNestableTask(
    const tracked_objects::Location& from_here, const base::Closure& task) {
  return PostTaskHelper(from_here, task, 0, false);
}

bool MessageLoopProxyImpl::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  return PostTaskHelper(from_here, task, delay_ms, false);
}

bool MessageLoopProxyImpl::BelongsToCurrentThread() {
  // We shouldn't use MessageLoop::current() since it uses LazyInstance which
  // may be deleted by ~AtExitManager when a WorkerPool thread calls this
  // function.
  // http://crbug.com/63678
  base::ThreadRestrictions::ScopedAllowSingleton allow_singleton;
  AutoLock lock(message_loop_lock_);
  return (target_message_loop_ &&
          (MessageLoop::current() == target_message_loop_));
}

// MessageLoop::DestructionObserver implementation
void MessageLoopProxyImpl::WillDestroyCurrentMessageLoop() {
  AutoLock lock(message_loop_lock_);
  target_message_loop_ = NULL;
}

void MessageLoopProxyImpl::OnDestruct() const {
  // We shouldn't use MessageLoop::current() since it uses LazyInstance which
  // may be deleted by ~AtExitManager when a WorkerPool thread calls this
  // function.
  // http://crbug.com/63678
  base::ThreadRestrictions::ScopedAllowSingleton allow_singleton;
  bool delete_later = false;
  {
    AutoLock lock(message_loop_lock_);
    if (target_message_loop_ &&
        (MessageLoop::current() != target_message_loop_)) {
      target_message_loop_->DeleteSoon(FROM_HERE, this);
      delete_later = true;
    }
  }
  if (!delete_later)
    delete this;
}

MessageLoopProxyImpl::MessageLoopProxyImpl()
    : target_message_loop_(MessageLoop::current()) {
}

bool MessageLoopProxyImpl::PostTaskHelper(
    const tracked_objects::Location& from_here, const base::Closure& task,
    int64 delay_ms, bool nestable) {
  AutoLock lock(message_loop_lock_);
  if (target_message_loop_) {
    if (nestable) {
      target_message_loop_->PostDelayedTask(from_here, task, delay_ms);
    } else {
      target_message_loop_->PostNonNestableDelayedTask(from_here, task,
                                                       delay_ms);
    }
    return true;
  }
  return false;
}

scoped_refptr<MessageLoopProxy>
MessageLoopProxy::current() {
  MessageLoop* cur_loop = MessageLoop::current();
  if (!cur_loop)
    return NULL;
  return cur_loop->message_loop_proxy();
}

}  // namespace base
