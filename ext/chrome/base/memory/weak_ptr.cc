// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"

namespace base {
namespace internal {

WeakReference::Flag::Flag() : is_valid_(true) {
}

void WeakReference::Flag::Invalidate() {
  // The flag being invalidated with a single ref implies that there are no
  // weak pointers in existence. Allow deletion on other thread in this case.
  DCHECK(thread_checker_.CalledOnValidThread() || HasOneRef());
  is_valid_ = false;
}

bool WeakReference::Flag::IsValid() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return is_valid_;
}

WeakReference::Flag::~Flag() {
}

WeakReference::WeakReference() {
}

WeakReference::WeakReference(const Flag* flag) : flag_(flag) {
}

WeakReference::~WeakReference() {
}

bool WeakReference::is_valid() const {
  return flag_ && flag_->IsValid();
}

WeakReferenceOwner::WeakReferenceOwner() {
}

WeakReferenceOwner::~WeakReferenceOwner() {
  Invalidate();
}

WeakReference WeakReferenceOwner::GetRef() const {
  // We also want to reattach to the current thread if all previous references
  // have gone away.
  if (!HasRefs())
    flag_ = new WeakReference::Flag();
  return WeakReference(flag_);
}

void WeakReferenceOwner::Invalidate() {
  if (flag_) {
    flag_->Invalidate();
    flag_ = NULL;
  }
}

WeakPtrBase::WeakPtrBase() {
}

WeakPtrBase::~WeakPtrBase() {
}

WeakPtrBase::WeakPtrBase(const WeakReference& ref) : ref_(ref) {
}

}  // namespace internal
}  // namespace base
