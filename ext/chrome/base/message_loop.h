// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_H_
#define BASE_MESSAGE_LOOP_H_
#pragma once

#include <queue>
#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_helpers.h"
#include "base/message_loop_proxy.h"
#include "base/message_pump.h"
#include "base/observer_list.h"
#include "base/pending_task.h"
#include "base/synchronization/lock.h"
#include "base/tracking_info.h"
#include "base/time.h"

#if defined(OS_WIN)
// We need this to declare base::MessagePumpWin::Dispatcher, which we should
// really just eliminate.
#include "base/message_pump_win.h"
#elif defined(OS_POSIX)
#include "base/message_pump_libevent.h"
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)

#if defined(USE_WAYLAND)
#include "base/message_pump_wayland.h"
#elif defined(USE_AURA)
#include "base/message_pump_x.h"
#else
#include "base/message_pump_gtk.h"
#endif

#endif
#endif

namespace base {
class Histogram;
}

// A MessageLoop is used to process events for a particular thread.  There is
// at most one MessageLoop instance per thread.
//
// Events include at a minimum Task instances submitted to PostTask or those
// managed by TimerManager.  Depending on the type of message pump used by the
// MessageLoop other events such as UI messages may be processed.  On Windows
// APC calls (as time permits) and signals sent to a registered set of HANDLEs
// may also be processed.
//
// NOTE: Unless otherwise specified, a MessageLoop's methods may only be called
// on the thread where the MessageLoop's Run method executes.
//
// NOTE: MessageLoop has task reentrancy protection.  This means that if a
// task is being processed, a second task cannot start until the first task is
// finished.  Reentrancy can happen when processing a task, and an inner
// message pump is created.  That inner pump then processes native messages
// which could implicitly start an inner task.  Inner message pumps are created
// with dialogs (DialogBox), common dialogs (GetOpenFileName), OLE functions
// (DoDragDrop), printer functions (StartDoc) and *many* others.
//
// Sample workaround when inner task processing is needed:
//   bool old_state = MessageLoop::current()->NestableTasksAllowed();
//   MessageLoop::current()->SetNestableTasksAllowed(true);
//   HRESULT hr = DoDragDrop(...); // Implicitly runs a modal message loop here.
//   MessageLoop::current()->SetNestableTasksAllowed(old_state);
//   // Process hr  (the result returned by DoDragDrop().
//
// Please be SURE your task is reentrant (nestable) and all global variables
// are stable and accessible before calling SetNestableTasksAllowed(true).
//
class BASE_EXPORT MessageLoop : public base::MessagePump::Delegate {
 public:
#if defined(OS_WIN)
  typedef base::MessagePumpWin::Dispatcher Dispatcher;
  typedef base::MessagePumpObserver Observer;
#elif !defined(OS_MACOSX) && !defined(OS_ANDROID)
  typedef base::MessagePumpDispatcher Dispatcher;
  typedef base::MessagePumpObserver Observer;
#endif

  // A MessageLoop has a particular type, which indicates the set of
  // asynchronous events it may process in addition to tasks and timers.
  //
  // TYPE_DEFAULT
  //   This type of ML only supports tasks and timers.
  //
  // TYPE_UI
  //   This type of ML also supports native UI events (e.g., Windows messages).
  //   See also MessageLoopForUI.
  //
  // TYPE_IO
  //   This type of ML also supports asynchronous IO.  See also
  //   MessageLoopForIO.
  //
  enum Type {
    TYPE_DEFAULT,
    TYPE_UI,
    TYPE_IO
  };

  // Normally, it is not necessary to instantiate a MessageLoop.  Instead, it
  // is typical to make use of the current thread's MessageLoop instance.
  explicit MessageLoop(Type type = TYPE_DEFAULT);
  virtual ~MessageLoop();

  // Returns the MessageLoop object for the current thread, or null if none.
  static MessageLoop* current();

  static void EnableHistogrammer(bool enable_histogrammer);

  typedef base::MessagePump* (MessagePumpFactory)();
  // Using the given base::MessagePumpForUIFactory to override the default
  // MessagePump implementation for 'TYPE_UI'.
  static void InitMessagePumpForUIFactory(MessagePumpFactory* factory);

  // A DestructionObserver is notified when the current MessageLoop is being
  // destroyed.  These observers are notified prior to MessageLoop::current()
  // being changed to return NULL.  This gives interested parties the chance to
  // do final cleanup that depends on the MessageLoop.
  //
  // NOTE: Any tasks posted to the MessageLoop during this notification will
  // not be run.  Instead, they will be deleted.
  //
  class BASE_EXPORT DestructionObserver {
   public:
    virtual void WillDestroyCurrentMessageLoop() = 0;

   protected:
    virtual ~DestructionObserver();
  };

  // Add a DestructionObserver, which will start receiving notifications
  // immediately.
  void AddDestructionObserver(DestructionObserver* destruction_observer);

  // Remove a DestructionObserver.  It is safe to call this method while a
  // DestructionObserver is receiving a notification callback.
  void RemoveDestructionObserver(DestructionObserver* destruction_observer);

  // The "PostTask" family of methods call the task's Run method asynchronously
  // from within a message loop at some point in the future.
  //
  // With the PostTask variant, tasks are invoked in FIFO order, inter-mixed
  // with normal UI or IO event processing.  With the PostDelayedTask variant,
  // tasks are called after at least approximately 'delay_ms' have elapsed.
  //
  // The NonNestable variants work similarly except that they promise never to
  // dispatch the task from a nested invocation of MessageLoop::Run.  Instead,
  // such tasks get deferred until the top-most MessageLoop::Run is executing.
  //
  // The MessageLoop takes ownership of the Task, and deletes it after it has
  // been Run().
  //
  // NOTE: These methods may be called on any thread.  The Task will be invoked
  // on the thread that executes MessageLoop::Run().
  void PostTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task);

  void PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task, int64 delay_ms);

  void PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay);

  void PostNonNestableTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task);

  void PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task, int64 delay_ms);

  void PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay);

  // A variant on PostTask that deletes the given object.  This is useful
  // if the object needs to live until the next run of the MessageLoop (for
  // example, deleting a RenderProcessHost from within an IPC callback is not
  // good).
  //
  // NOTE: This method may be called on any thread.  The object will be deleted
  // on the thread that executes MessageLoop::Run().  If this is not the same
  // as the thread that calls PostDelayedTask(FROM_HERE, ), then T MUST inherit
  // from RefCountedThreadSafe<T>!
  template <class T>
  void DeleteSoon(const tracked_objects::Location& from_here, const T* object) {
    base::subtle::DeleteHelperInternal<T, void>::DeleteOnMessageLoop(
        this, from_here, object);
  }

  // A variant on PostTask that releases the given reference counted object
  // (by calling its Release method).  This is useful if the object needs to
  // live until the next run of the MessageLoop, or if the object needs to be
  // released on a particular thread.
  //
  // NOTE: This method may be called on any thread.  The object will be
  // released (and thus possibly deleted) on the thread that executes
  // MessageLoop::Run().  If this is not the same as the thread that calls
  // PostDelayedTask(FROM_HERE, ), then T MUST inherit from
  // RefCountedThreadSafe<T>!
  template <class T>
  void ReleaseSoon(const tracked_objects::Location& from_here,
                   const T* object) {
    base::subtle::ReleaseHelperInternal<T, void>::ReleaseOnMessageLoop(
        this, from_here, object);
  }

  // Run the message loop.
  void Run();

  // Process all pending tasks, windows messages, etc., but don't wait/sleep.
  // Return as soon as all items that can be run are taken care of.
  void RunAllPending();

  // Signals the Run method to return after it is done processing all pending
  // messages.  This method may only be called on the same thread that called
  // Run, and Run must still be on the call stack.
  //
  // Use QuitClosure if you need to Quit another thread's MessageLoop, but note
  // that doing so is fairly dangerous if the target thread makes nested calls
  // to MessageLoop::Run.  The problem being that you won't know which nested
  // run loop you are quitting, so be careful!
  void Quit();

  // This method is a variant of Quit, that does not wait for pending messages
  // to be processed before returning from Run.
  void QuitNow();

  // Invokes Quit on the current MessageLoop when run. Useful to schedule an
  // arbitrary MessageLoop to Quit.
  static base::Closure QuitClosure();

  // Returns the type passed to the constructor.
  Type type() const { return type_; }

  // Optional call to connect the thread name with this loop.
  void set_thread_name(const std::string& thread_name) {
    DCHECK(thread_name_.empty()) << "Should not rename this thread!";
    thread_name_ = thread_name;
  }
  const std::string& thread_name() const { return thread_name_; }

  // Gets the message loop proxy associated with this message loop.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy() {
    return message_loop_proxy_.get();
  }

  // Enables or disables the recursive task processing. This happens in the case
  // of recursive message loops. Some unwanted message loop may occurs when
  // using common controls or printer functions. By default, recursive task
  // processing is disabled.
  //
  // The specific case where tasks get queued is:
  // - The thread is running a message loop.
  // - It receives a task #1 and execute it.
  // - The task #1 implicitly start a message loop, like a MessageBox in the
  //   unit test. This can also be StartDoc or GetSaveFileName.
  // - The thread receives a task #2 before or while in this second message
  //   loop.
  // - With NestableTasksAllowed set to true, the task #2 will run right away.
  //   Otherwise, it will get executed right after task #1 completes at "thread
  //   message loop level".
  void SetNestableTasksAllowed(bool allowed);
  bool NestableTasksAllowed() const;

  // Enables nestable tasks on |loop| while in scope.
  class ScopedNestableTaskAllower {
   public:
    explicit ScopedNestableTaskAllower(MessageLoop* loop)
        : loop_(loop),
          old_state_(loop_->NestableTasksAllowed()) {
      loop_->SetNestableTasksAllowed(true);
    }
    ~ScopedNestableTaskAllower() {
      loop_->SetNestableTasksAllowed(old_state_);
    }

   private:
    MessageLoop* loop_;
    bool old_state_;
  };

  // Enables or disables the restoration during an exception of the unhandled
  // exception filter that was active when Run() was called. This can happen
  // if some third party code call SetUnhandledExceptionFilter() and never
  // restores the previous filter.
  void set_exception_restoration(bool restore) {
    exception_restoration_ = restore;
  }

  // Returns true if we are currently running a nested message loop.
  bool IsNested();

  // A TaskObserver is an object that receives task notifications from the
  // MessageLoop.
  //
  // NOTE: A TaskObserver implementation should be extremely fast!
  class BASE_EXPORT TaskObserver {
   public:
    TaskObserver();

    // This method is called before processing a task.
    virtual void WillProcessTask(base::TimeTicks time_posted) = 0;

    // This method is called after processing a task.
    virtual void DidProcessTask(base::TimeTicks time_posted) = 0;

   protected:
    virtual ~TaskObserver();
  };

  // These functions can only be called on the same thread that |this| is
  // running on.
  void AddTaskObserver(TaskObserver* task_observer);
  void RemoveTaskObserver(TaskObserver* task_observer);

  // Returns true if the message loop has high resolution timers enabled.
  // Provided for testing.
  bool high_resolution_timers_enabled() {
#if defined(OS_WIN)
    return !high_resolution_timer_expiration_.is_null();
#else
    return true;
#endif
  }

  // When we go into high resolution timer mode, we will stay in hi-res mode
  // for at least 1s.
  static const int kHighResolutionTimerModeLeaseTimeMs = 1000;

  // Asserts that the MessageLoop is "idle".
  void AssertIdle() const;

#if defined(OS_WIN)
  void set_os_modal_loop(bool os_modal_loop) {
    os_modal_loop_ = os_modal_loop;
  }

  bool os_modal_loop() const {
    return os_modal_loop_;
  }
#endif  // OS_WIN

  // Can only be called from the thread that owns the MessageLoop.
  bool is_running() const;

  //----------------------------------------------------------------------------
 protected:
  struct RunState {
    // Used to count how many Run() invocations are on the stack.
    int run_depth;

    // Used to record that Quit() was called, or that we should quit the pump
    // once it becomes idle.
    bool quit_received;

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
    Dispatcher* dispatcher;
#endif
  };

#if defined(OS_ANDROID)
  // Android Java process manages the UI thread message loop. So its
  // MessagePumpForUI needs to keep the RunState.
 public:
#endif
  class BASE_EXPORT AutoRunState : RunState {
   public:
    explicit AutoRunState(MessageLoop* loop);
    ~AutoRunState();
   private:
    MessageLoop* loop_;
    RunState* previous_state_;
  };
#if defined(OS_ANDROID)
 protected:
#endif

#if defined(OS_WIN)
  base::MessagePumpWin* pump_win() {
    return static_cast<base::MessagePumpWin*>(pump_.get());
  }
#elif defined(OS_POSIX)
  base::MessagePumpLibevent* pump_libevent() {
    return static_cast<base::MessagePumpLibevent*>(pump_.get());
  }
#endif

  // A function to encapsulate all the exception handling capability in the
  // stacks around the running of a main message loop.  It will run the message
  // loop in a SEH try block or not depending on the set_SEH_restoration()
  // flag invoking respectively RunInternalInSEHFrame() or RunInternal().
  void RunHandler();

#if defined(OS_WIN)
  __declspec(noinline) void RunInternalInSEHFrame();
#endif

  // A surrounding stack frame around the running of the message loop that
  // supports all saving and restoring of state, as is needed for any/all (ugly)
  // recursive calls.
  void RunInternal();

  // Called to process any delayed non-nestable tasks.
  bool ProcessNextDelayedNonNestableTask();

  // Runs the specified PendingTask.
  void RunTask(const base::PendingTask& pending_task);

  // Calls RunTask or queues the pending_task on the deferred task list if it
  // cannot be run right now.  Returns true if the task was run.
  bool DeferOrRunPendingTask(const base::PendingTask& pending_task);

  // Adds the pending task to delayed_work_queue_.
  void AddToDelayedWorkQueue(const base::PendingTask& pending_task);

  // Adds the pending task to our incoming_queue_.
  //
  // Caller retains ownership of |pending_task|, but this function will
  // reset the value of pending_task->task.  This is needed to ensure
  // that the posting call stack does not retain pending_task->task
  // beyond this function call.
  void AddToIncomingQueue(base::PendingTask* pending_task);

  // Load tasks from the incoming_queue_ into work_queue_ if the latter is
  // empty.  The former requires a lock to access, while the latter is directly
  // accessible on this thread.
  void ReloadWorkQueue();

  // Delete tasks that haven't run yet without running them.  Used in the
  // destructor to make sure all the task's destructors get called.  Returns
  // true if some work was done.
  bool DeletePendingTasks();

  // Calculates the time at which a PendingTask should run.
  base::TimeTicks CalculateDelayedRuntime(int64 delay_ms);

  // Start recording histogram info about events and action IF it was enabled
  // and IF the statistics recorder can accept a registration of our histogram.
  void StartHistogrammer();

  // Add occurrence of event to our histogram, so that we can see what is being
  // done in a specific MessageLoop instance (i.e., specific thread).
  // If message_histogram_ is NULL, this is a no-op.
  void HistogramEvent(int event);

  // base::MessagePump::Delegate methods:
  virtual bool DoWork() OVERRIDE;
  virtual bool DoDelayedWork(base::TimeTicks* next_delayed_work_time) OVERRIDE;
  virtual bool DoIdleWork() OVERRIDE;

  Type type_;

  // A list of tasks that need to be processed by this instance.  Note that
  // this queue is only accessed (push/pop) by our current thread.
  base::TaskQueue work_queue_;

  // Contains delayed tasks, sorted by their 'delayed_run_time' property.
  base::DelayedTaskQueue delayed_work_queue_;

  // A recent snapshot of Time::Now(), used to check delayed_work_queue_.
  base::TimeTicks recent_time_;

  // A queue of non-nestable tasks that we had to defer because when it came
  // time to execute them we were in a nested message loop.  They will execute
  // once we're out of nested message loops.
  base::TaskQueue deferred_non_nestable_work_queue_;

  scoped_refptr<base::MessagePump> pump_;

  ObserverList<DestructionObserver> destruction_observers_;

  // A recursion block that prevents accidentally running additional tasks when
  // insider a (accidentally induced?) nested message pump.
  bool nestable_tasks_allowed_;

  bool exception_restoration_;

  std::string thread_name_;
  // A profiling histogram showing the counts of various messages and events.
  base::Histogram* message_histogram_;

  // A null terminated list which creates an incoming_queue of tasks that are
  // acquired under a mutex for processing on this instance's thread. These
  // tasks have not yet been sorted out into items for our work_queue_ vs items
  // that will be handled by the TimerManager.
  base::TaskQueue incoming_queue_;
  // Protect access to incoming_queue_.
  mutable base::Lock incoming_queue_lock_;

  RunState* state_;

  // The need for this variable is subtle. Please see implementation comments
  // around where it is used.
  bool should_leak_tasks_;

#if defined(OS_WIN)
  base::TimeTicks high_resolution_timer_expiration_;
  // Should be set to true before calling Windows APIs like TrackPopupMenu, etc
  // which enter a modal message loop.
  bool os_modal_loop_;
#endif

  // The next sequence number to use for delayed tasks.
  int next_sequence_num_;

  ObserverList<TaskObserver> task_observers_;

  // The message loop proxy associated with this message loop, if one exists.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

 private:
  template <class T, class R> friend class base::subtle::DeleteHelperInternal;
  template <class T, class R> friend class base::subtle::ReleaseHelperInternal;

  void DeleteSoonInternal(const tracked_objects::Location& from_here,
                          void(*deleter)(const void*),
                          const void* object);
  void ReleaseSoonInternal(const tracked_objects::Location& from_here,
                           void(*releaser)(const void*),
                           const void* object);


  DISALLOW_COPY_AND_ASSIGN(MessageLoop);
};

//-----------------------------------------------------------------------------
// MessageLoopForUI extends MessageLoop with methods that are particular to a
// MessageLoop instantiated with TYPE_UI.
//
// This class is typically used like so:
//   MessageLoopForUI::current()->...call some method...
//
class BASE_EXPORT MessageLoopForUI : public MessageLoop {
 public:
  MessageLoopForUI() : MessageLoop(TYPE_UI) {
  }

  // Returns the MessageLoopForUI of the current thread.
  static MessageLoopForUI* current() {
    MessageLoop* loop = MessageLoop::current();
    DCHECK_EQ(MessageLoop::TYPE_UI, loop->type());
    return static_cast<MessageLoopForUI*>(loop);
  }

#if defined(OS_WIN)
  void DidProcessMessage(const MSG& message);
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
  // On Android, the UI message loop is handled by Java side. So Run() should
  // never be called. Instead use Start(), which will forward all the native UI
  // events to the Java message loop.
  void Start();
#elif !defined(OS_MACOSX)
  // Please see message_pump_win/message_pump_glib for definitions of these
  // methods.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void RunWithDispatcher(Dispatcher* dispatcher);
  void RunAllPendingWithDispatcher(Dispatcher* dispatcher);

 protected:
  // TODO(rvargas): Make this platform independent.
  base::MessagePumpForUI* pump_ui() {
    return static_cast<base::MessagePumpForUI*>(pump_.get());
  }
#endif  // !defined(OS_MACOSX)
};

// Do not add any member variables to MessageLoopForUI!  This is important b/c
// MessageLoopForUI is often allocated via MessageLoop(TYPE_UI).  Any extra
// data that you need should be stored on the MessageLoop's pump_ instance.
COMPILE_ASSERT(sizeof(MessageLoop) == sizeof(MessageLoopForUI),
               MessageLoopForUI_should_not_have_extra_member_variables);

//-----------------------------------------------------------------------------
// MessageLoopForIO extends MessageLoop with methods that are particular to a
// MessageLoop instantiated with TYPE_IO.
//
// This class is typically used like so:
//   MessageLoopForIO::current()->...call some method...
//
class BASE_EXPORT MessageLoopForIO : public MessageLoop {
 public:
#if defined(OS_WIN)
  typedef base::MessagePumpForIO::IOHandler IOHandler;
  typedef base::MessagePumpForIO::IOContext IOContext;
  typedef base::MessagePumpForIO::IOObserver IOObserver;
#elif defined(OS_POSIX)
  typedef base::MessagePumpLibevent::Watcher Watcher;
  typedef base::MessagePumpLibevent::FileDescriptorWatcher
      FileDescriptorWatcher;
  typedef base::MessagePumpLibevent::IOObserver IOObserver;

  enum Mode {
    WATCH_READ = base::MessagePumpLibevent::WATCH_READ,
    WATCH_WRITE = base::MessagePumpLibevent::WATCH_WRITE,
    WATCH_READ_WRITE = base::MessagePumpLibevent::WATCH_READ_WRITE
  };

#endif

  MessageLoopForIO() : MessageLoop(TYPE_IO) {
  }

  // Returns the MessageLoopForIO of the current thread.
  static MessageLoopForIO* current() {
    MessageLoop* loop = MessageLoop::current();
    DCHECK_EQ(MessageLoop::TYPE_IO, loop->type());
    return static_cast<MessageLoopForIO*>(loop);
  }

  void AddIOObserver(IOObserver* io_observer) {
    pump_io()->AddIOObserver(io_observer);
  }

  void RemoveIOObserver(IOObserver* io_observer) {
    pump_io()->RemoveIOObserver(io_observer);
  }

#if defined(OS_WIN)
  // Please see MessagePumpWin for definitions of these methods.
  void RegisterIOHandler(HANDLE file_handle, IOHandler* handler);
  bool WaitForIOCompletion(DWORD timeout, IOHandler* filter);

 protected:
  // TODO(rvargas): Make this platform independent.
  base::MessagePumpForIO* pump_io() {
    return static_cast<base::MessagePumpForIO*>(pump_.get());
  }

#elif defined(OS_POSIX)
  // Please see MessagePumpLibevent for definition.
  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           Mode mode,
                           FileDescriptorWatcher* controller,
                           Watcher* delegate);

 private:
  base::MessagePumpLibevent* pump_io() {
    return static_cast<base::MessagePumpLibevent*>(pump_.get());
  }
#endif  // defined(OS_POSIX)
};

// Do not add any member variables to MessageLoopForIO!  This is important b/c
// MessageLoopForIO is often allocated via MessageLoop(TYPE_IO).  Any extra
// data that you need should be stored on the MessageLoop's pump_ instance.
COMPILE_ASSERT(sizeof(MessageLoop) == sizeof(MessageLoopForIO),
               MessageLoopForIO_should_not_have_extra_member_variables);

#endif  // BASE_MESSAGE_LOOP_H_
