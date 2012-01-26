// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OneShotTimer and RepeatingTimer provide a simple timer API.  As the names
// suggest, OneShotTimer calls you back once after a time delay expires.
// RepeatingTimer on the other hand calls you back periodically with the
// prescribed time interval.
//
// OneShotTimer and RepeatingTimer both cancel the timer when they go out of
// scope, which makes it easy to ensure that you do not get called when your
// object has gone out of scope.  Just instantiate a OneShotTimer or
// RepeatingTimer as a member variable of the class for which you wish to
// receive timer events.
//
// Sample RepeatingTimer usage:
//
//   class MyClass {
//    public:
//     void StartDoingStuff() {
//       timer_.Start(FROM_HERE, TimeDelta::FromSeconds(1),
//                    this, &MyClass::DoStuff);
//     }
//     void StopDoingStuff() {
//       timer_.Stop();
//     }
//    private:
//     void DoStuff() {
//       // This method is called every second to do stuff.
//       ...
//     }
//     base::RepeatingTimer<MyClass> timer_;
//   };
//
// Both OneShotTimer and RepeatingTimer also support a Reset method, which
// allows you to easily defer the timer event until the timer delay passes once
// again.  So, in the above example, if 0.5 seconds have already passed,
// calling Reset on timer_ would postpone DoStuff by another 1 second.  In
// other words, Reset is shorthand for calling Stop and then Start again with
// the same arguments.

#ifndef BASE_TIMER_H_
#define BASE_TIMER_H_
#pragma once

// IMPORTANT: If you change timer code, make sure that all tests (including
// disabled ones) from timer_unittests.cc pass locally. Some are disabled
// because they're flaky on the buildbot, but when you run them locally you
// should be able to tell the difference.

#include "base/base_export.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time.h"

class MessageLoop;

namespace base {

//-----------------------------------------------------------------------------
// This class is an implementation detail of OneShotTimer and RepeatingTimer.
// Please do not use this class directly.
//
// This class exists to share code between BaseTimer<T> template instantiations.
//
class BASE_EXPORT BaseTimer_Helper {
 public:
  // Stops the timer.
  ~BaseTimer_Helper() {
    OrphanDelayedTask();
  }

  // Returns true if the timer is running (i.e., not stopped).
  bool IsRunning() const {
    return delayed_task_ != NULL;
  }

  // Returns the current delay for this timer.  May only call this method when
  // the timer is running!
  TimeDelta GetCurrentDelay() const {
    DCHECK(IsRunning());
    return delayed_task_->delay_;
  }

 protected:
  BaseTimer_Helper() : delayed_task_(NULL) {}

  // We have access to the timer_ member so we can orphan this task.
  class TimerTask {
   public:
    TimerTask(const tracked_objects::Location& posted_from,
              TimeDelta delay)
        : posted_from_(posted_from),
          timer_(NULL),
          delay_(delay) {
    }
    virtual ~TimerTask() {}
    virtual void Run() = 0;
    tracked_objects::Location posted_from_;
    BaseTimer_Helper* timer_;
    TimeDelta delay_;
  };

  // Used to orphan delayed_task_ so that when it runs it does nothing.
  void OrphanDelayedTask();

  // Used to initiated a new delayed task.  This has the side-effect of
  // orphaning delayed_task_ if it is non-null.
  void InitiateDelayedTask(TimerTask* timer_task);

  TimerTask* delayed_task_;

  DISALLOW_COPY_AND_ASSIGN(BaseTimer_Helper);
};

//-----------------------------------------------------------------------------
// This class is an implementation detail of OneShotTimer and RepeatingTimer.
// Please do not use this class directly.
template <class Receiver, bool kIsRepeating>
class BaseTimer : public BaseTimer_Helper {
 public:
  typedef void (Receiver::*ReceiverMethod)();

  // Call this method to start the timer.  It is an error to call this method
  // while the timer is already running.
  void Start(const tracked_objects::Location& posted_from,
             TimeDelta delay,
             Receiver* receiver,
             ReceiverMethod method) {
    DCHECK(!IsRunning());
    InitiateDelayedTask(new TimerTask(posted_from, delay, receiver, method));
  }

  // Call this method to stop the timer.  It is a no-op if the timer is not
  // running.
  void Stop() {
    OrphanDelayedTask();
  }

  // Call this method to reset the timer delay of an already running timer.
  void Reset() {
    DCHECK(IsRunning());
    InitiateDelayedTask(static_cast<TimerTask*>(delayed_task_)->Clone());
  }

 private:
  typedef BaseTimer<Receiver, kIsRepeating> SelfType;

  class TimerTask : public BaseTimer_Helper::TimerTask {
   public:
    TimerTask(const tracked_objects::Location& posted_from,
              TimeDelta delay,
              Receiver* receiver,
              ReceiverMethod method)
        : BaseTimer_Helper::TimerTask(posted_from, delay),
          receiver_(receiver),
          method_(method) {
    }

    virtual ~TimerTask() {
      // This task may be getting cleared because the MessageLoop has been
      // destructed.  If so, don't leave the Timer with a dangling pointer
      // to this now-defunct task.
      ClearBaseTimer();
    }

    virtual void Run() {
      if (!timer_)  // timer_ is null if we were orphaned.
        return;
      if (kIsRepeating)
        ResetBaseTimer();
      else
        ClearBaseTimer();
      (receiver_->*method_)();
    }

    TimerTask* Clone() const {
      return new TimerTask(posted_from_, delay_, receiver_, method_);
    }

   private:
    // Inform the Base that the timer is no longer active.
    void ClearBaseTimer() {
      if (timer_) {
        SelfType* self = static_cast<SelfType*>(timer_);
        // It is possible that the Timer has already been reset, and that this
        // Task is old.  So, if the Timer points to a different task, assume
        // that the Timer has already taken care of properly setting the task.
        if (self->delayed_task_ == this)
          self->delayed_task_ = NULL;
        // By now the delayed_task_ in the Timer does not point to us anymore.
        // We should reset our own timer_ because the Timer can not do this
        // for us in its destructor.
        timer_ = NULL;
      }
    }

    // Inform the Base that we're resetting the timer.
    void ResetBaseTimer() {
      DCHECK(timer_);
      DCHECK(kIsRepeating);
      SelfType* self = static_cast<SelfType*>(timer_);
      self->Reset();
    }

    Receiver* receiver_;
    ReceiverMethod method_;
  };
};

//-----------------------------------------------------------------------------
// A simple, one-shot timer.  See usage notes at the top of the file.
template <class Receiver>
class OneShotTimer : public BaseTimer<Receiver, false> {};

//-----------------------------------------------------------------------------
// A simple, repeating timer.  See usage notes at the top of the file.
template <class Receiver>
class RepeatingTimer : public BaseTimer<Receiver, true> {};

//-----------------------------------------------------------------------------
// A Delay timer is like The Button from Lost. Once started, you have to keep
// calling Reset otherwise it will call the given method in the MessageLoop
// thread.
//
// Once created, it is inactive until Reset is called. Once |delay| seconds have
// passed since the last call to Reset, the callback is made. Once the callback
// has been made, it's inactive until Reset is called again.
//
// If destroyed, the timeout is canceled and will not occur even if already
// inflight.
template <class Receiver>
class DelayTimer {
 public:
  typedef void (Receiver::*ReceiverMethod)();

  DelayTimer(const tracked_objects::Location& posted_from,
             TimeDelta delay,
             Receiver* receiver,
             ReceiverMethod method)
      : posted_from_(posted_from),
        receiver_(receiver),
        method_(method),
        delay_(delay) {
  }

  void Reset() {
    DelayFor(delay_);
  }

 private:
  void DelayFor(TimeDelta delay) {
    trigger_time_ = TimeTicks::Now() + delay;

    // If we already have a timer that will expire at or before the given delay,
    // then we have nothing more to do now.
    if (timer_.IsRunning() && timer_.GetCurrentDelay() <= delay)
      return;

    // The timer isn't running, or will expire too late, so restart it.
    timer_.Stop();
    timer_.Start(posted_from_, delay, this, &DelayTimer<Receiver>::Check);
  }

  void Check() {
    if (trigger_time_.is_null())
      return;

    // If we have not waited long enough, then wait some more.
    const TimeTicks now = TimeTicks::Now();
    if (now < trigger_time_) {
      DelayFor(trigger_time_ - now);
      return;
    }

    (receiver_->*method_)();
  }

  tracked_objects::Location posted_from_;
  Receiver *const receiver_;
  const ReceiverMethod method_;
  const TimeDelta delay_;

  OneShotTimer<DelayTimer<Receiver> > timer_;
  TimeTicks trigger_time_;
};

}  // namespace base

#endif  // BASE_TIMER_H_
