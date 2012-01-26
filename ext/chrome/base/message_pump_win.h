// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_WIN_H_
#define BASE_MESSAGE_PUMP_WIN_H_
#pragma once

#include <windows.h>

#include <list>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/message_pump.h"
#include "base/message_pump_observer.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/win/scoped_handle.h"

namespace base {

// MessagePumpWin serves as the base for specialized versions of the MessagePump
// for Windows. It provides basic functionality like handling of observers and
// controlling the lifetime of the message pump.
class BASE_EXPORT MessagePumpWin : public MessagePump {
 public:

  // Dispatcher is used during a nested invocation of Run to dispatch events.
  // If Run is invoked with a non-NULL Dispatcher, MessageLoop does not
  // dispatch events (or invoke TranslateMessage), rather every message is
  // passed to Dispatcher's Dispatch method for dispatch. It is up to the
  // Dispatcher to dispatch, or not, the event.
  //
  // The nested loop is exited by either posting a quit, or returning false
  // from Dispatch.
  class BASE_EXPORT Dispatcher {
   public:
    virtual ~Dispatcher() {}
    // Dispatches the event. If true is returned processing continues as
    // normal. If false is returned, the nested loop exits immediately.
    virtual bool Dispatch(const MSG& msg) = 0;
  };

  MessagePumpWin() : have_work_(0), state_(NULL) {}
  virtual ~MessagePumpWin() {}

  // Add an Observer, which will start receiving notifications immediately.
  void AddObserver(MessagePumpObserver* observer);

  // Remove an Observer.  It is safe to call this method while an Observer is
  // receiving a notification callback.
  void RemoveObserver(MessagePumpObserver* observer);

  // Give a chance to code processing additional messages to notify the
  // message loop observers that another message has been processed.
  void WillProcessMessage(const MSG& msg);
  void DidProcessMessage(const MSG& msg);

  // Like MessagePump::Run, but MSG objects are routed through dispatcher.
  void RunWithDispatcher(Delegate* delegate, Dispatcher* dispatcher);

  // MessagePump methods:
  virtual void Run(Delegate* delegate) { RunWithDispatcher(delegate, NULL); }
  virtual void Quit();

 protected:
  struct RunState {
    Delegate* delegate;
    Dispatcher* dispatcher;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit;

    // Used to count how many Run() invocations are on the stack.
    int run_depth;
  };

  virtual void DoRunLoop() = 0;
  int GetCurrentDelay() const;

  ObserverList<MessagePumpObserver> observers_;

  // The time at which delayed work should run.
  TimeTicks delayed_work_time_;

  // A boolean value used to indicate if there is a kMsgDoWork message pending
  // in the Windows Message queue.  There is at most one such message, and it
  // can drive execution of tasks when a native message pump is running.
  LONG have_work_;

  // State for the current invocation of Run.
  RunState* state_;
};

//-----------------------------------------------------------------------------
// MessagePumpForUI extends MessagePumpWin with methods that are particular to a
// MessageLoop instantiated with TYPE_UI.
//
// MessagePumpForUI implements a "traditional" Windows message pump. It contains
// a nearly infinite loop that peeks out messages, and then dispatches them.
// Intermixed with those peeks are callouts to DoWork for pending tasks, and
// DoDelayedWork for pending timers. When there are no events to be serviced,
// this pump goes into a wait state. In most cases, this message pump handles
// all processing.
//
// However, when a task, or windows event, invokes on the stack a native dialog
// box or such, that window typically provides a bare bones (native?) message
// pump.  That bare-bones message pump generally supports little more than a
// peek of the Windows message queue, followed by a dispatch of the peeked
// message.  MessageLoop extends that bare-bones message pump to also service
// Tasks, at the cost of some complexity.
//
// The basic structure of the extension (refered to as a sub-pump) is that a
// special message, kMsgHaveWork, is repeatedly injected into the Windows
// Message queue.  Each time the kMsgHaveWork message is peeked, checks are
// made for an extended set of events, including the availability of Tasks to
// run.
//
// After running a task, the special message kMsgHaveWork is again posted to
// the Windows Message queue, ensuring a future time slice for processing a
// future event.  To prevent flooding the Windows Message queue, care is taken
// to be sure that at most one kMsgHaveWork message is EVER pending in the
// Window's Message queue.
//
// There are a few additional complexities in this system where, when there are
// no Tasks to run, this otherwise infinite stream of messages which drives the
// sub-pump is halted.  The pump is automatically re-started when Tasks are
// queued.
//
// A second complexity is that the presence of this stream of posted tasks may
// prevent a bare-bones message pump from ever peeking a WM_PAINT or WM_TIMER.
// Such paint and timer events always give priority to a posted message, such as
// kMsgHaveWork messages.  As a result, care is taken to do some peeking in
// between the posting of each kMsgHaveWork message (i.e., after kMsgHaveWork
// is peeked, and before a replacement kMsgHaveWork is posted).
//
// NOTE: Although it may seem odd that messages are used to start and stop this
// flow (as opposed to signaling objects, etc.), it should be understood that
// the native message pump will *only* respond to messages.  As a result, it is
// an excellent choice.  It is also helpful that the starter messages that are
// placed in the queue when new task arrive also awakens DoRunLoop.
//
class BASE_EXPORT MessagePumpForUI : public MessagePumpWin {
 public:
  // The application-defined code passed to the hook procedure.
  static const int kMessageFilterCode = 0x5001;

  MessagePumpForUI();
  virtual ~MessagePumpForUI();

  // MessagePump methods:
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time);

  // Applications can call this to encourage us to process all pending WM_PAINT
  // messages.  This method will process all paint messages the Windows Message
  // queue can provide, up to some fixed number (to avoid any infinite loops).
  void PumpOutPendingPaintMessages();

 private:
  static LRESULT CALLBACK WndProcThunk(
      HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  virtual void DoRunLoop();
  void InitMessageWnd();
  void WaitForWork();
  void HandleWorkMessage();
  void HandleTimerMessage();
  bool ProcessNextWindowsMessage();
  bool ProcessMessageHelper(const MSG& msg);
  bool ProcessPumpReplacementMessage();

  // A hidden message-only window.
  HWND message_hwnd_;
};

//-----------------------------------------------------------------------------
// MessagePumpForIO extends MessagePumpWin with methods that are particular to a
// MessageLoop instantiated with TYPE_IO. This version of MessagePump does not
// deal with Windows mesagges, and instead has a Run loop based on Completion
// Ports so it is better suited for IO operations.
//
class BASE_EXPORT MessagePumpForIO : public MessagePumpWin {
 public:
  struct IOContext;

  // Clients interested in receiving OS notifications when asynchronous IO
  // operations complete should implement this interface and register themselves
  // with the message pump.
  //
  // Typical use #1:
  //   // Use only when there are no user's buffers involved on the actual IO,
  //   // so that all the cleanup can be done by the message pump.
  //   class MyFile : public IOHandler {
  //     MyFile() {
  //       ...
  //       context_ = new IOContext;
  //       context_->handler = this;
  //       message_pump->RegisterIOHandler(file_, this);
  //     }
  //     ~MyFile() {
  //       if (pending_) {
  //         // By setting the handler to NULL, we're asking for this context
  //         // to be deleted when received, without calling back to us.
  //         context_->handler = NULL;
  //       } else {
  //         delete context_;
  //      }
  //     }
  //     virtual void OnIOCompleted(IOContext* context, DWORD bytes_transfered,
  //                                DWORD error) {
  //         pending_ = false;
  //     }
  //     void DoSomeIo() {
  //       ...
  //       // The only buffer required for this operation is the overlapped
  //       // structure.
  //       ConnectNamedPipe(file_, &context_->overlapped);
  //       pending_ = true;
  //     }
  //     bool pending_;
  //     IOContext* context_;
  //     HANDLE file_;
  //   };
  //
  // Typical use #2:
  //   class MyFile : public IOHandler {
  //     MyFile() {
  //       ...
  //       message_pump->RegisterIOHandler(file_, this);
  //     }
  //     // Plus some code to make sure that this destructor is not called
  //     // while there are pending IO operations.
  //     ~MyFile() {
  //     }
  //     virtual void OnIOCompleted(IOContext* context, DWORD bytes_transfered,
  //                                DWORD error) {
  //       ...
  //       delete context;
  //     }
  //     void DoSomeIo() {
  //       ...
  //       IOContext* context = new IOContext;
  //       // This is not used for anything. It just prevents the context from
  //       // being considered "abandoned".
  //       context->handler = this;
  //       ReadFile(file_, buffer, num_bytes, &read, &context->overlapped);
  //     }
  //     HANDLE file_;
  //   };
  //
  // Typical use #3:
  // Same as the previous example, except that in order to deal with the
  // requirement stated for the destructor, the class calls WaitForIOCompletion
  // from the destructor to block until all IO finishes.
  //     ~MyFile() {
  //       while(pending_)
  //         message_pump->WaitForIOCompletion(INFINITE, this);
  //     }
  //
  class IOHandler {
   public:
    virtual ~IOHandler() {}
    // This will be called once the pending IO operation associated with
    // |context| completes. |error| is the Win32 error code of the IO operation
    // (ERROR_SUCCESS if there was no error). |bytes_transfered| will be zero
    // on error.
    virtual void OnIOCompleted(IOContext* context, DWORD bytes_transfered,
                               DWORD error) = 0;
  };

  // An IOObserver is an object that receives IO notifications from the
  // MessagePump.
  //
  // NOTE: An IOObserver implementation should be extremely fast!
  class IOObserver {
   public:
    IOObserver() {}

    virtual void WillProcessIOEvent() = 0;
    virtual void DidProcessIOEvent() = 0;

   protected:
    virtual ~IOObserver() {}
  };

  // The extended context that should be used as the base structure on every
  // overlapped IO operation. |handler| must be set to the registered IOHandler
  // for the given file when the operation is started, and it can be set to NULL
  // before the operation completes to indicate that the handler should not be
  // called anymore, and instead, the IOContext should be deleted when the OS
  // notifies the completion of this operation. Please remember that any buffers
  // involved with an IO operation should be around until the callback is
  // received, so this technique can only be used for IO that do not involve
  // additional buffers (other than the overlapped structure itself).
  struct IOContext {
    OVERLAPPED overlapped;
    IOHandler* handler;
  };

  MessagePumpForIO();
  virtual ~MessagePumpForIO() {}

  // MessagePump methods:
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time);

  // Register the handler to be used when asynchronous IO for the given file
  // completes. The registration persists as long as |file_handle| is valid, so
  // |handler| must be valid as long as there is pending IO for the given file.
  void RegisterIOHandler(HANDLE file_handle, IOHandler* handler);

  // Waits for the next IO completion that should be processed by |filter|, for
  // up to |timeout| milliseconds. Return true if any IO operation completed,
  // regardless of the involved handler, and false if the timeout expired. If
  // the completion port received any message and the involved IO handler
  // matches |filter|, the callback is called before returning from this code;
  // if the handler is not the one that we are looking for, the callback will
  // be postponed for another time, so reentrancy problems can be avoided.
  // External use of this method should be reserved for the rare case when the
  // caller is willing to allow pausing regular task dispatching on this thread.
  bool WaitForIOCompletion(DWORD timeout, IOHandler* filter);

  void AddIOObserver(IOObserver* obs);
  void RemoveIOObserver(IOObserver* obs);

 private:
  struct IOItem {
    IOHandler* handler;
    IOContext* context;
    DWORD bytes_transfered;
    DWORD error;
  };

  virtual void DoRunLoop();
  void WaitForWork();
  bool MatchCompletedIOItem(IOHandler* filter, IOItem* item);
  bool GetIOItem(DWORD timeout, IOItem* item);
  bool ProcessInternalIOItem(const IOItem& item);
  void WillProcessIOEvent();
  void DidProcessIOEvent();

  // The completion port associated with this thread.
  win::ScopedHandle port_;
  // This list will be empty almost always. It stores IO completions that have
  // not been delivered yet because somebody was doing cleanup.
  std::list<IOItem> completed_io_;

  ObserverList<IOObserver> io_observers_;
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_WIN_H_
