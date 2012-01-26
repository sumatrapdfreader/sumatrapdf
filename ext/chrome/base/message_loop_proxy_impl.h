// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_PROXY_IMPL_H_
#define BASE_MESSAGE_LOOP_PROXY_IMPL_H_
#pragma once

#include "base/base_export.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"

namespace base {

// A stock implementation of MessageLoopProxy that is created and managed by a
// MessageLoop. For now a MessageLoopProxyImpl can only be created as part of a
// MessageLoop.
class BASE_EXPORT MessageLoopProxyImpl
    : public MessageLoopProxy {
 public:
  virtual ~MessageLoopProxyImpl();

  // MessageLoopProxy implementation
  virtual bool PostTask(const tracked_objects::Location& from_here,
                        const base::Closure& task) OVERRIDE;
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               int64 delay_ms) OVERRIDE;
  virtual bool PostNonNestableTask(const tracked_objects::Location& from_here,
                                   const base::Closure& task) OVERRIDE;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int64 delay_ms) OVERRIDE;
  virtual bool BelongsToCurrentThread() OVERRIDE;

 protected:
  // Override OnDestruct so that we can delete the object on the target message
  // loop if it still exists.
  virtual void OnDestruct() const OVERRIDE;

 private:
  MessageLoopProxyImpl();

  // Called directly by MessageLoop::~MessageLoop.
  virtual void WillDestroyCurrentMessageLoop();


  bool PostTaskHelper(const tracked_objects::Location& from_here,
                      const base::Closure& task,
                      int64 delay_ms,
                      bool nestable);

  // Allow the messageLoop to create a MessageLoopProxyImpl.
  friend class ::MessageLoop;

  // The lock that protects access to target_message_loop_.
  mutable base::Lock message_loop_lock_;
  MessageLoop* target_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopProxyImpl);
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_PROXY_IMPL_H_
