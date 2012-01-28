// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop_proxy.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/threading/post_task_and_reply_impl.h"

namespace base {

namespace {

class PostTaskAndReplyMessageLoopProxy : public internal::PostTaskAndReplyImpl {
 public:
  PostTaskAndReplyMessageLoopProxy(MessageLoopProxy* destination)
     : destination_(destination) {
  }

 private:
  virtual bool PostTask(const base::Closure& task) OVERRIDE {
    return destination_->PostTask(task);
  }

  // Non-owning.
  MessageLoopProxy* destination_;
};

}  // namespace

MessageLoopProxy::MessageLoopProxy() {
}

MessageLoopProxy::~MessageLoopProxy() {
}

bool MessageLoopProxy::PostTaskAndReply(
    const Closure& task,
    const Closure& reply) {
  return PostTaskAndReplyMessageLoopProxy(this).PostTaskAndReply(
      task, reply);
}

void MessageLoopProxy::OnDestruct() const {
  delete this;
}

bool MessageLoopProxy::DeleteSoonInternal(
    void(*deleter)(const void*),
    const void* object) {
  return PostNonNestableTask(base::Bind(deleter, object));
}

bool MessageLoopProxy::ReleaseSoonInternal(
    void(*releaser)(const void*),
    const void* object) {
  return PostNonNestableTask(base::Bind(releaser, object));
}

}  // namespace base
