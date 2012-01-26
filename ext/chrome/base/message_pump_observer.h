// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_OBSERVER_H
#define BASE_MESSAGE_PUMP_OBSERVER_H
#pragma once

#include "base/event_types.h"

namespace base {

enum EventStatus {
  EVENT_CONTINUE,    // The event should be dispatched as normal.
#if defined(USE_X11)
  EVENT_HANDLED      // The event should not be processed any farther.
#endif
};

// A MessagePumpObserver is an object that receives global
// notifications from the UI MessageLoop with MessagePumpWin or
// MessagePumpX.
//
// NOTE: An Observer implementation should be extremely fast!
//
// For use with MessagePumpX, please see message_pump_glib.h for more
// info about how this is invoked in this environment.
class BASE_EXPORT MessagePumpObserver {
 public:
  // This method is called before processing a NativeEvent. If the
  // method returns EVENT_HANDLED, it indicates the event has already
  // been handled, so the event is not processed any farther. If the
  // method returns EVENT_CONTINUE, the event dispatching proceeds as
  // normal.
  virtual EventStatus WillProcessEvent(const NativeEvent& event) = 0;

  // This method is called after processing a message. This method
  // will not be called if WillProcessEvent returns EVENT_HANDLED.
  virtual void DidProcessEvent(const NativeEvent& event) = 0;

 protected:
  virtual ~MessagePumpObserver() {}
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_OBSERVER_VIEWS_H
