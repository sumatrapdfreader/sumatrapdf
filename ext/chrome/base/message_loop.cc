// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy_impl.h"
#include "base/message_pump_default.h"
#include "base/metrics/histogram.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread_local.h"
#include "base/time.h"
#include "base/tracked_objects.h"

#if defined(OS_MACOSX)
#include "base/message_pump_mac.h"
#endif
#if defined(OS_POSIX)
#include "base/message_pump_libevent.h"
#endif
#if defined(OS_ANDROID)
#include "base/message_pump_android.h"
#endif
#if defined(TOOLKIT_USES_GTK)
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

using base::PendingTask;
using base::TimeDelta;
using base::TimeTicks;

namespace {

// A lazily created thread local storage for quick access to a thread's message
// loop, if one exists.  This should be safe and free of static constructors.
base::LazyInstance<base::ThreadLocalPointer<MessageLoop> > lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

// Logical events for Histogram profiling. Run with -message-loop-histogrammer
// to get an accounting of messages and actions taken on each thread.
const int kTaskRunEvent = 0x1;
const int kTimerEvent = 0x2;

// Provide range of message IDs for use in histogramming and debug display.
const int kLeastNonZeroMessageId = 1;
const int kMaxMessageId = 1099;
const int kNumberOfDistinctMessagesDisplayed = 1100;

// Provide a macro that takes an expression (such as a constant, or macro
// constant) and creates a pair to initalize an array of pairs.  In this case,
// our pair consists of the expressions value, and the "stringized" version
// of the expression (i.e., the exrpression put in quotes).  For example, if
// we have:
//    #define FOO 2
//    #define BAR 5
// then the following:
//    VALUE_TO_NUMBER_AND_NAME(FOO + BAR)
// will expand to:
//   {7, "FOO + BAR"}
// We use the resulting array as an argument to our histogram, which reads the
// number as a bucket identifier, and proceeds to use the corresponding name
// in the pair (i.e., the quoted string) when printing out a histogram.
#define VALUE_TO_NUMBER_AND_NAME(name) {name, #name},

const base::LinearHistogram::DescriptionPair event_descriptions_[] = {
  // Provide some pretty print capability in our histogram for our internal
  // messages.

  // A few events we handle (kindred to messages), and used to profile actions.
  VALUE_TO_NUMBER_AND_NAME(kTaskRunEvent)
  VALUE_TO_NUMBER_AND_NAME(kTimerEvent)

  {-1, NULL}  // The list must be null terminated, per API to histogram.
};

bool enable_histogrammer_ = false;

MessageLoop::MessagePumpFactory* message_pump_for_ui_factory_ = NULL;

}  // namespace

//------------------------------------------------------------------------------

#if defined(OS_WIN)

// Upon a SEH exception in this thread, it restores the original unhandled
// exception filter.
static int SEHFilter(LPTOP_LEVEL_EXCEPTION_FILTER old_filter) {
  ::SetUnhandledExceptionFilter(old_filter);
  return EXCEPTION_CONTINUE_SEARCH;
}

// Retrieves a pointer to the current unhandled exception filter. There
// is no standalone getter method.
static LPTOP_LEVEL_EXCEPTION_FILTER GetTopSEHFilter() {
  LPTOP_LEVEL_EXCEPTION_FILTER top_filter = NULL;
  top_filter = ::SetUnhandledExceptionFilter(0);
  ::SetUnhandledExceptionFilter(top_filter);
  return top_filter;
}

#endif  // defined(OS_WIN)

//------------------------------------------------------------------------------

MessageLoop::TaskObserver::TaskObserver() {
}

MessageLoop::TaskObserver::~TaskObserver() {
}

MessageLoop::DestructionObserver::~DestructionObserver() {
}

//------------------------------------------------------------------------------

MessageLoop::MessageLoop(Type type)
    : type_(type),
      nestable_tasks_allowed_(true),
      exception_restoration_(false),
      message_histogram_(NULL),
      state_(NULL),
      should_leak_tasks_(true),
#ifdef OS_WIN
      os_modal_loop_(false),
#endif  // OS_WIN
      next_sequence_num_(0) {
  DCHECK(!current()) << "should only have one message loop per thread";
  lazy_tls_ptr.Pointer()->Set(this);

  message_loop_proxy_ = new base::MessageLoopProxyImpl();

// TODO(rvargas): Get rid of the OS guards.
#if defined(OS_WIN)
#define MESSAGE_PUMP_UI new base::MessagePumpForUI()
#define MESSAGE_PUMP_IO new base::MessagePumpForIO()
#elif defined(OS_MACOSX)
#define MESSAGE_PUMP_UI base::MessagePumpMac::Create()
#define MESSAGE_PUMP_IO new base::MessagePumpLibevent()
#elif defined(OS_NACL)
// Currently NaCl doesn't have a UI or an IO MessageLoop.
// TODO(abarth): Figure out if we need these.
#define MESSAGE_PUMP_UI NULL
#define MESSAGE_PUMP_IO NULL
#elif defined(OS_POSIX)  // POSIX but not MACOSX.
#define MESSAGE_PUMP_UI new base::MessagePumpForUI()
#define MESSAGE_PUMP_IO new base::MessagePumpLibevent()
#else
#error Not implemented
#endif

  if (type_ == TYPE_UI) {
    if (message_pump_for_ui_factory_)
      pump_ = message_pump_for_ui_factory_();
    else
      pump_ = MESSAGE_PUMP_UI;
  } else if (type_ == TYPE_IO) {
    pump_ = MESSAGE_PUMP_IO;
  } else {
    DCHECK_EQ(TYPE_DEFAULT, type_);
    pump_ = new base::MessagePumpDefault();
  }
}

MessageLoop::~MessageLoop() {
  DCHECK_EQ(this, current());

  DCHECK(!state_);

  // Clean up any unprocessed tasks, but take care: deleting a task could
  // result in the addition of more tasks (e.g., via DeleteSoon).  We set a
  // limit on the number of times we will allow a deleted task to generate more
  // tasks.  Normally, we should only pass through this loop once or twice.  If
  // we end up hitting the loop limit, then it is probably due to one task that
  // is being stubborn.  Inspect the queues to see who is left.
  bool did_work;
  for (int i = 0; i < 100; ++i) {
    DeletePendingTasks();
    ReloadWorkQueue();
    // If we end up with empty queues, then break out of the loop.
    did_work = DeletePendingTasks();
    if (!did_work)
      break;
  }
  DCHECK(!did_work);

  // Let interested parties have one last shot at accessing this.
  FOR_EACH_OBSERVER(DestructionObserver, destruction_observers_,
                    WillDestroyCurrentMessageLoop());

  // Tell the message_loop_proxy that we are dying.
  static_cast<base::MessageLoopProxyImpl*>(message_loop_proxy_.get())->
      WillDestroyCurrentMessageLoop();
  message_loop_proxy_ = NULL;

  // OK, now make it so that no one can find us.
  lazy_tls_ptr.Pointer()->Set(NULL);

#if defined(OS_WIN)
  // If we left the high-resolution timer activated, deactivate it now.
  // Doing this is not-critical, it is mainly to make sure we track
  // the high resolution timer activations properly in our unit tests.
  if (!high_resolution_timer_expiration_.is_null()) {
    base::Time::ActivateHighResolutionTimer(false);
    high_resolution_timer_expiration_ = base::TimeTicks();
  }
#endif
}

// static
MessageLoop* MessageLoop::current() {
  // TODO(darin): sadly, we cannot enable this yet since people call us even
  // when they have no intention of using us.
  // DCHECK(loop) << "Ouch, did you forget to initialize me?";
  return lazy_tls_ptr.Pointer()->Get();
}

// static
void MessageLoop::EnableHistogrammer(bool enable) {
  enable_histogrammer_ = enable;
}

// static
void MessageLoop::InitMessagePumpForUIFactory(MessagePumpFactory* factory) {
  DCHECK(!message_pump_for_ui_factory_);
  message_pump_for_ui_factory_ = factory;
}

void MessageLoop::AddDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK_EQ(this, current());
  destruction_observers_.AddObserver(destruction_observer);
}

void MessageLoop::RemoveDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK_EQ(this, current());
  destruction_observers_.RemoveObserver(destruction_observer);
}

void MessageLoop::PostTask(
    const tracked_objects::Location& from_here, const base::Closure& task) {
  DCHECK(!task.is_null()) << from_here.ToString();
  PendingTask pending_task(from_here, task, CalculateDelayedRuntime(0), true);
  AddToIncomingQueue(&pending_task);
}

void MessageLoop::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  DCHECK(!task.is_null()) << from_here.ToString();
  PendingTask pending_task(from_here, task,
                           CalculateDelayedRuntime(delay_ms), true);
  AddToIncomingQueue(&pending_task);
}

void MessageLoop::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  PostDelayedTask(from_here, task, delay.InMillisecondsRoundedUp());
}

void MessageLoop::PostNonNestableTask(
    const tracked_objects::Location& from_here, const base::Closure& task) {
  DCHECK(!task.is_null()) << from_here.ToString();
  PendingTask pending_task(from_here, task, CalculateDelayedRuntime(0), false);
  AddToIncomingQueue(&pending_task);
}

void MessageLoop::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here, const base::Closure& task,
    int64 delay_ms) {
  DCHECK(!task.is_null()) << from_here.ToString();
  PendingTask pending_task(from_here, task,
                           CalculateDelayedRuntime(delay_ms), false);
  AddToIncomingQueue(&pending_task);
}

void MessageLoop::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  PostNonNestableDelayedTask(from_here, task, delay.InMillisecondsRoundedUp());
}

void MessageLoop::Run() {
  AutoRunState save_state(this);
  RunHandler();
}

void MessageLoop::RunAllPending() {
  AutoRunState save_state(this);
  state_->quit_received = true;  // Means run until we would otherwise block.
  RunHandler();
}

void MessageLoop::Quit() {
  DCHECK_EQ(this, current());
  if (state_) {
    state_->quit_received = true;
  } else {
    NOTREACHED() << "Must be inside Run to call Quit";
  }
}

void MessageLoop::QuitNow() {
  DCHECK_EQ(this, current());
  if (state_) {
    pump_->Quit();
  } else {
    NOTREACHED() << "Must be inside Run to call Quit";
  }
}

static void QuitCurrent() {
  MessageLoop::current()->Quit();
}

// static
base::Closure MessageLoop::QuitClosure() {
  return base::Bind(&QuitCurrent);
}

void MessageLoop::SetNestableTasksAllowed(bool allowed) {
  if (nestable_tasks_allowed_ != allowed) {
    nestable_tasks_allowed_ = allowed;
    if (!nestable_tasks_allowed_)
      return;
    // Start the native pump if we are not already pumping.
    pump_->ScheduleWork();
  }
}

bool MessageLoop::NestableTasksAllowed() const {
  return nestable_tasks_allowed_;
}

bool MessageLoop::IsNested() {
  return state_->run_depth > 1;
}

void MessageLoop::AddTaskObserver(TaskObserver* task_observer) {
  DCHECK_EQ(this, current());
  task_observers_.AddObserver(task_observer);
}

void MessageLoop::RemoveTaskObserver(TaskObserver* task_observer) {
  DCHECK_EQ(this, current());
  task_observers_.RemoveObserver(task_observer);
}

void MessageLoop::AssertIdle() const {
  // We only check |incoming_queue_|, since we don't want to lock |work_queue_|.
  base::AutoLock lock(incoming_queue_lock_);
  DCHECK(incoming_queue_.empty());
}

bool MessageLoop::is_running() const {
  DCHECK_EQ(this, current());
  return state_ != NULL;
}

//------------------------------------------------------------------------------

// Runs the loop in two different SEH modes:
// enable_SEH_restoration_ = false : any unhandled exception goes to the last
// one that calls SetUnhandledExceptionFilter().
// enable_SEH_restoration_ = true : any unhandled exception goes to the filter
// that was existed before the loop was run.
void MessageLoop::RunHandler() {
#if defined(OS_WIN)
  if (exception_restoration_) {
    RunInternalInSEHFrame();
    return;
  }
#endif

  RunInternal();
}

#if defined(OS_WIN)
__declspec(noinline) void MessageLoop::RunInternalInSEHFrame() {
  LPTOP_LEVEL_EXCEPTION_FILTER current_filter = GetTopSEHFilter();
  __try {
    RunInternal();
  } __except(SEHFilter(current_filter)) {
  }
  return;
}
#endif

void MessageLoop::RunInternal() {
  DCHECK_EQ(this, current());

  StartHistogrammer();

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  if (state_->dispatcher && type() == TYPE_UI) {
    static_cast<base::MessagePumpForUI*>(pump_.get())->
        RunWithDispatcher(this, state_->dispatcher);
    return;
  }
#endif

  pump_->Run(this);
}

bool MessageLoop::ProcessNextDelayedNonNestableTask() {
  if (state_->run_depth != 1)
    return false;

  if (deferred_non_nestable_work_queue_.empty())
    return false;

  PendingTask pending_task = deferred_non_nestable_work_queue_.front();
  deferred_non_nestable_work_queue_.pop();

  RunTask(pending_task);
  return true;
}

void MessageLoop::RunTask(const PendingTask& pending_task) {
  UNSHIPPED_TRACE_EVENT2("task", "MessageLoop::RunTask",
                         "src_file", pending_task.posted_from.file_name(),
                         "src_func", pending_task.posted_from.function_name());
  DCHECK(nestable_tasks_allowed_);
  // Execute the task and assume the worst: It is probably not reentrant.
  nestable_tasks_allowed_ = false;

  // Before running the task, store the program counter where it was posted
  // and deliberately alias it to ensure it is on the stack if the task
  // crashes. Be careful not to assume that the variable itself will have the
  // expected value when displayed by the optimizer in an optimized build.
  // Look at a memory dump of the stack.
  const void* program_counter =
      pending_task.posted_from.program_counter();
  base::debug::Alias(&program_counter);

  HistogramEvent(kTaskRunEvent);

  tracked_objects::TrackedTime start_time =
      tracked_objects::ThreadData::NowForStartOfRun(pending_task.birth_tally);

  FOR_EACH_OBSERVER(TaskObserver, task_observers_,
                    WillProcessTask(pending_task.time_posted));
  pending_task.task.Run();
  FOR_EACH_OBSERVER(TaskObserver, task_observers_,
                    DidProcessTask(pending_task.time_posted));

  tracked_objects::ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      start_time, tracked_objects::ThreadData::NowForEndOfRun());

  nestable_tasks_allowed_ = true;
}

bool MessageLoop::DeferOrRunPendingTask(const PendingTask& pending_task) {
  if (pending_task.nestable || state_->run_depth == 1) {
    RunTask(pending_task);
    // Show that we ran a task (Note: a new one might arrive as a
    // consequence!).
    return true;
  }

  // We couldn't run the task now because we're in a nested message loop
  // and the task isn't nestable.
  deferred_non_nestable_work_queue_.push(pending_task);
  return false;
}

void MessageLoop::AddToDelayedWorkQueue(const PendingTask& pending_task) {
  // Move to the delayed work queue.  Initialize the sequence number
  // before inserting into the delayed_work_queue_.  The sequence number
  // is used to faciliate FIFO sorting when two tasks have the same
  // delayed_run_time value.
  PendingTask new_pending_task(pending_task);
  new_pending_task.sequence_num = next_sequence_num_++;
  delayed_work_queue_.push(new_pending_task);
}

void MessageLoop::ReloadWorkQueue() {
  // We can improve performance of our loading tasks from incoming_queue_ to
  // work_queue_ by waiting until the last minute (work_queue_ is empty) to
  // load.  That reduces the number of locks-per-task significantly when our
  // queues get large.
  if (!work_queue_.empty())
    return;  // Wait till we *really* need to lock and load.

  // Acquire all we can from the inter-thread queue with one lock acquisition.
  {
    base::AutoLock lock(incoming_queue_lock_);
    if (incoming_queue_.empty())
      return;
    incoming_queue_.Swap(&work_queue_);  // Constant time
    DCHECK(incoming_queue_.empty());
  }
}

bool MessageLoop::DeletePendingTasks() {
  bool did_work = !work_queue_.empty();
  // TODO(darin): Delete all tasks once it is safe to do so.
  // Until it is totally safe, just do it when running Valgrind.
  //
  // See http://crbug.com/61131
  //
#if defined(USE_HEAPCHECKER)
  should_leak_tasks_ = false;
#else
      if (RunningOnValgrind())
        should_leak_tasks_ = false;
#endif  // defined(OS_POSIX)
  while (!work_queue_.empty()) {
    PendingTask pending_task = work_queue_.front();
    work_queue_.pop();
    if (!pending_task.delayed_run_time.is_null()) {
      // We want to delete delayed tasks in the same order in which they would
      // normally be deleted in case of any funny dependencies between delayed
      // tasks.
      AddToDelayedWorkQueue(pending_task);
    }
  }
  did_work |= !deferred_non_nestable_work_queue_.empty();
  while (!deferred_non_nestable_work_queue_.empty()) {
    deferred_non_nestable_work_queue_.pop();
  }
  did_work |= !delayed_work_queue_.empty();

  // Historically, we always delete the task regardless of valgrind status. It's
  // not completely clear why we want to leak them in the loops above.  This
  // code is replicating legacy behavior, and should not be considered
  // absolutely "correct" behavior.  See TODO above about deleting all tasks
  // when it's safe.
  should_leak_tasks_ = false;
  while (!delayed_work_queue_.empty()) {
    delayed_work_queue_.pop();
  }
  should_leak_tasks_ = true;
  return did_work;
}

TimeTicks MessageLoop::CalculateDelayedRuntime(int64 delay_ms) {
  TimeTicks delayed_run_time;
  if (delay_ms > 0) {
    delayed_run_time =
        TimeTicks::Now() + TimeDelta::FromMilliseconds(delay_ms);

#if defined(OS_WIN)
    if (high_resolution_timer_expiration_.is_null()) {
      // Windows timers are granular to 15.6ms.  If we only set high-res
      // timers for those under 15.6ms, then a 18ms timer ticks at ~32ms,
      // which as a percentage is pretty inaccurate.  So enable high
      // res timers for any timer which is within 2x of the granularity.
      // This is a tradeoff between accuracy and power management.
      bool needs_high_res_timers =
          delay_ms < (2 * base::Time::kMinLowResolutionThresholdMs);
      if (needs_high_res_timers) {
        if (base::Time::ActivateHighResolutionTimer(true)) {
          high_resolution_timer_expiration_ = TimeTicks::Now() +
              TimeDelta::FromMilliseconds(kHighResolutionTimerModeLeaseTimeMs);
        }
      }
    }
#endif
  } else {
    DCHECK_EQ(delay_ms, 0) << "delay should not be negative";
  }

#if defined(OS_WIN)
  if (!high_resolution_timer_expiration_.is_null()) {
    if (TimeTicks::Now() > high_resolution_timer_expiration_) {
      base::Time::ActivateHighResolutionTimer(false);
      high_resolution_timer_expiration_ = TimeTicks();
    }
  }
#endif

  return delayed_run_time;
}

// Possibly called on a background thread!
void MessageLoop::AddToIncomingQueue(PendingTask* pending_task) {
  // Warning: Don't try to short-circuit, and handle this thread's tasks more
  // directly, as it could starve handling of foreign threads.  Put every task
  // into this queue.

  scoped_refptr<base::MessagePump> pump;
  {
    base::AutoLock locked(incoming_queue_lock_);

    bool was_empty = incoming_queue_.empty();
    incoming_queue_.push(*pending_task);
    pending_task->task.Reset();
    if (!was_empty)
      return;  // Someone else should have started the sub-pump.

    pump = pump_;
  }
  // Since the incoming_queue_ may contain a task that destroys this message
  // loop, we cannot exit incoming_queue_lock_ until we are done with |this|.
  // We use a stack-based reference to the message pump so that we can call
  // ScheduleWork outside of incoming_queue_lock_.

  pump->ScheduleWork();
}

//------------------------------------------------------------------------------
// Method and data for histogramming events and actions taken by each instance
// on each thread.

void MessageLoop::StartHistogrammer() {
  if (enable_histogrammer_ && !message_histogram_
      && base::StatisticsRecorder::IsActive()) {
    DCHECK(!thread_name_.empty());
    message_histogram_ = base::LinearHistogram::FactoryGet(
        "MsgLoop:" + thread_name_,
        kLeastNonZeroMessageId, kMaxMessageId,
        kNumberOfDistinctMessagesDisplayed,
        message_histogram_->kHexRangePrintingFlag);
    message_histogram_->SetRangeDescriptions(event_descriptions_);
  }
}

void MessageLoop::HistogramEvent(int event) {
  if (message_histogram_)
    message_histogram_->Add(event);
}

bool MessageLoop::DoWork() {
  if (!nestable_tasks_allowed_) {
    // Task can't be executed right now.
    return false;
  }

  for (;;) {
    ReloadWorkQueue();
    if (work_queue_.empty())
      break;

    // Execute oldest task.
    do {
      PendingTask pending_task = work_queue_.front();
      work_queue_.pop();
      if (!pending_task.delayed_run_time.is_null()) {
        AddToDelayedWorkQueue(pending_task);
        // If we changed the topmost task, then it is time to reschedule.
        if (delayed_work_queue_.top().task.Equals(pending_task.task))
          pump_->ScheduleDelayedWork(pending_task.delayed_run_time);
      } else {
        if (DeferOrRunPendingTask(pending_task))
          return true;
      }
    } while (!work_queue_.empty());
  }

  // Nothing happened.
  return false;
}

bool MessageLoop::DoDelayedWork(TimeTicks* next_delayed_work_time) {
  if (!nestable_tasks_allowed_ || delayed_work_queue_.empty()) {
    recent_time_ = *next_delayed_work_time = TimeTicks();
    return false;
  }

  // When we "fall behind," there will be a lot of tasks in the delayed work
  // queue that are ready to run.  To increase efficiency when we fall behind,
  // we will only call Time::Now() intermittently, and then process all tasks
  // that are ready to run before calling it again.  As a result, the more we
  // fall behind (and have a lot of ready-to-run delayed tasks), the more
  // efficient we'll be at handling the tasks.

  TimeTicks next_run_time = delayed_work_queue_.top().delayed_run_time;
  if (next_run_time > recent_time_) {
    recent_time_ = TimeTicks::Now();  // Get a better view of Now();
    if (next_run_time > recent_time_) {
      *next_delayed_work_time = next_run_time;
      return false;
    }
  }

  PendingTask pending_task = delayed_work_queue_.top();
  delayed_work_queue_.pop();

  if (!delayed_work_queue_.empty())
    *next_delayed_work_time = delayed_work_queue_.top().delayed_run_time;

  return DeferOrRunPendingTask(pending_task);
}

bool MessageLoop::DoIdleWork() {
  if (ProcessNextDelayedNonNestableTask())
    return true;

  if (state_->quit_received)
    pump_->Quit();

  return false;
}

void MessageLoop::DeleteSoonInternal(const tracked_objects::Location& from_here,
                                     void(*deleter)(const void*),
                                     const void* object) {
  PostNonNestableTask(from_here, base::Bind(deleter, object));
}

void MessageLoop::ReleaseSoonInternal(
    const tracked_objects::Location& from_here,
    void(*releaser)(const void*),
    const void* object) {
  PostNonNestableTask(from_here, base::Bind(releaser, object));
}

//------------------------------------------------------------------------------
// MessageLoop::AutoRunState

MessageLoop::AutoRunState::AutoRunState(MessageLoop* loop) : loop_(loop) {
  // Make the loop reference us.
  previous_state_ = loop_->state_;
  if (previous_state_) {
    run_depth = previous_state_->run_depth + 1;
  } else {
    run_depth = 1;
  }
  loop_->state_ = this;

  // Initialize the other fields:
  quit_received = false;
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  dispatcher = NULL;
#endif
}

MessageLoop::AutoRunState::~AutoRunState() {
  loop_->state_ = previous_state_;
}

//------------------------------------------------------------------------------
// MessageLoopForUI

#if defined(OS_WIN)
void MessageLoopForUI::DidProcessMessage(const MSG& message) {
  pump_win()->DidProcessMessage(message);
}
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
void MessageLoopForUI::Start() {
  // No Histogram support for UI message loop as it is managed by Java side
  static_cast<base::MessagePumpForUI*>(pump_.get())->Start(this);
}
#endif

#if !defined(OS_MACOSX) && !defined(OS_NACL) && !defined(OS_ANDROID)
void MessageLoopForUI::AddObserver(Observer* observer) {
  pump_ui()->AddObserver(observer);
}

void MessageLoopForUI::RemoveObserver(Observer* observer) {
  pump_ui()->RemoveObserver(observer);
}

void MessageLoopForUI::RunWithDispatcher(Dispatcher* dispatcher) {
  AutoRunState save_state(this);
  state_->dispatcher = dispatcher;
  RunHandler();
}

void MessageLoopForUI::RunAllPendingWithDispatcher(Dispatcher* dispatcher) {
  AutoRunState save_state(this);
  state_->dispatcher = dispatcher;
  state_->quit_received = true;  // Means run until we would otherwise block.
  RunHandler();
}

#endif  //  !defined(OS_MACOSX) && !defined(OS_NACL) && !defined(OS_ANDROID)

//------------------------------------------------------------------------------
// MessageLoopForIO

#if defined(OS_WIN)

void MessageLoopForIO::RegisterIOHandler(HANDLE file, IOHandler* handler) {
  pump_io()->RegisterIOHandler(file, handler);
}

bool MessageLoopForIO::WaitForIOCompletion(DWORD timeout, IOHandler* filter) {
  return pump_io()->WaitForIOCompletion(timeout, filter);
}

#elif defined(OS_POSIX) && !defined(OS_NACL)

bool MessageLoopForIO::WatchFileDescriptor(int fd,
                                           bool persistent,
                                           Mode mode,
                                           FileDescriptorWatcher *controller,
                                           Watcher *delegate) {
  return pump_libevent()->WatchFileDescriptor(
      fd,
      persistent,
      static_cast<base::MessagePumpLibevent::Mode>(mode),
      controller,
      delegate);
}

#endif
