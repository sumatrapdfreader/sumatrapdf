// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"

namespace trace_event_internal {

void TraceEndOnScopeClose::Initialize(const unsigned char* category_enabled,
                                      const char* name) {
  data_.category_enabled = category_enabled;
  data_.name = name;
  p_data_ = &data_;
}

void TraceEndOnScopeClose::AddEventIfEnabled() {
  // Only called when p_data_ is non-null.
  if (*p_data_->category_enabled) {
    TRACE_EVENT_API_ADD_TRACE_EVENT(
        TRACE_EVENT_PHASE_END,
        p_data_->category_enabled,
        p_data_->name, kNoEventId,
        kZeroNumArgs, NULL, NULL, NULL,
        kNoThreshholdBeginId, kNoThresholdValue, TRACE_EVENT_FLAG_NONE);
  }
}

void TraceEndOnScopeCloseThreshold::Initialize(
                                      const unsigned char* category_enabled,
                                      const char* name,
                                      int threshold_begin_id,
                                      long long threshold) {
  data_.category_enabled = category_enabled;
  data_.name = name;
  data_.threshold_begin_id = threshold_begin_id;
  data_.threshold = threshold;
  p_data_ = &data_;
}

void TraceEndOnScopeCloseThreshold::AddEventIfEnabled() {
  // Only called when p_data_ is non-null.
  if (*p_data_->category_enabled) {
    TRACE_EVENT_API_ADD_TRACE_EVENT(
        TRACE_EVENT_PHASE_END,
        p_data_->category_enabled,
        p_data_->name, kNoEventId,
        kZeroNumArgs, NULL, NULL, NULL,
        p_data_->threshold_begin_id, p_data_->threshold,
        TRACE_EVENT_FLAG_NONE);
  }
}

}  // namespace trace_event_internal
