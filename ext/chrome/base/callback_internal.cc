// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_internal.h"

#include "base/logging.h"

namespace base {
namespace internal {

bool CallbackBase::is_null() const {
  return bind_state_.get() == NULL;
}

void CallbackBase::Reset() {
  bind_state_ = NULL;
  polymorphic_invoke_ = NULL;
}

bool CallbackBase::Equals(const CallbackBase& other) const {
  return bind_state_.get() == other.bind_state_.get() &&
         polymorphic_invoke_ == other.polymorphic_invoke_;
}

CallbackBase::CallbackBase(BindStateBase* bind_state)
    : bind_state_(bind_state),
      polymorphic_invoke_(NULL) {
  DCHECK(!bind_state_ || bind_state_->HasOneRef());
}

CallbackBase::~CallbackBase() {
}

}  // namespace base
}  // namespace internal
