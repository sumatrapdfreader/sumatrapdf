/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: orig dispatches tasks through a hidden win32 message-only window. Our UI
// is gpui on every platform, so this is a plain mutex-protected queue that the
// UI thread drains. SetWakeupFn() lets the UI ask to be woken when a worker
// posts (step 6 hooks it to gpui); without it DrainQueue() still runs
// everything on the next UI pass.

#include "base/Base.h"
#include "base/Timer.h"
#include "base/UITask.h"

namespace uitask {

static ThreadId gMainUIThreadId = 0;
static bool gInitialized = false;
static bool gWasDestroyed = false;
static void (*gWakeupFn)() = nullptr;

// What Post() hands to the dispatcher. Bundling these leaves somewhere to
// record when the task was queued.
struct TaskInfo {
    Func0 f;
    Kind kind = nullptr;
    TimeStamp queueTime{};
};

static Mutex gQueueMutex;
static Vec<TaskInfo*>* gQueue = nullptr;

// A task that sat in the queue this long is worth reporting even for the kinds
// that are too frequent to log every time.
#if IS_ASAN || IS_DEBUG
constexpr double kSlowTaskDispatchMs = 300.0;
#else
constexpr double kSlowTaskDispatchMs = 50.0;
#endif

static SeqStrings gSkipLogNames =
    "TaskFindCountProgress\0CopyProgress\0RenderFinished\0FrameUpdateUi\0(no "
    "kind)\0Repaint\0SaveSettings\0ShowSelectedAnnot\0GoToFindMatch\0HangDetectorPing\0";

static void ExecuteTask(TaskInfo* ti) {
    Kind kind = ti->kind;
    // how long the task waited between Post() and getting here
    double queuedMs = TimeSinceInMs(ti->queueTime);
    Str kindName = kind ? Str(kind) : StrL("(no kind)");
    bool shouldLog = SeqStrIndex(gSkipLogNames, kindName) < 0;
    if (shouldLog) {
        logf("uitask::ExecuteTask: will execute '%s', 0x%p, queued for %.3f ms\n", kindName, (void*)ti, queuedMs);
    } else if (queuedMs >= kSlowTaskDispatchMs) {
        logf("uitask::ExecuteTask: slow dispatch of '%s', queued for %.3f ms\n", kindName, queuedMs);
    }
    ti->f.Call();
    if (shouldLog) {
        logf("uitask::ExecuteTask: did execute 0x%p\n", (void*)ti);
    }
    delete ti;
}

// Call Initialize() at program startup and Destroy() at the end
void Initialize() {
    gMainUIThreadId = GetCurrentThreadId();
    gWasDestroyed = false;
    ReportIf(gQueue);
    gQueue = new Vec<TaskInfo*>();
    gInitialized = true;
}

// The UI thread calls this when it gets a chance to run queued work.
// Tasks posted by a task run on the next drain, as on win32.
void DrainQueue() {
    if (!gInitialized) {
        return;
    }
    Vec<TaskInfo*> toRun;
    gQueueMutex.Lock();
    VecAppendVec(toRun, *gQueue);
    VecClear(*gQueue);
    gQueueMutex.Unlock();
    for (TaskInfo* ti : toRun) {
        ExecuteTask(ti);
    }
}

void Destroy() {
    DrainQueue();
    gInitialized = false;
    gWasDestroyed = true;
    gQueueMutex.Lock();
    Vec<TaskInfo*>* q = gQueue;
    gQueue = nullptr;
    gQueueMutex.Unlock();
    if (q) {
        for (TaskInfo* ti : *q) {
            delete ti;
        }
        delete q;
    }
}

// ng: called by the gpui shell so a task posted from a worker wakes the UI
void SetWakeupFn(void (*fn)()) {
    gWakeupFn = fn;
}

void Post(const Func0& f, Kind kind) {
    if (!gInitialized) {
        // After Destroy() this is a worker that outlived the UI finishing its
        // work (the file-existence checker is the usual one). Nothing can run
        // the task any more, so drop it - quietly, because the process is on
        // its way out and a debug report here would race the rest of shutdown.
        // Before Initialize() it is a real bug: the task would never run.
        ReportIf(!gWasDestroyed);
        return;
    }
    TaskInfo* ti = new TaskInfo();
    ti->f = f;
    ti->kind = kind;
    ti->queueTime = TimeGet();
    gQueueMutex.Lock();
    VecAppend(*gQueue, ti);
    gQueueMutex.Unlock();
    if (gWakeupFn) {
        gWakeupFn();
    }
} // NOLINT

bool IsMainUIThread() {
    return GetCurrentThreadId() == gMainUIThreadId;
}

void PostOptimized(const Func0& f, Kind kind) {
    if (IsMainUIThread()) {
        // if we're already on ui thread, execute immediately
        // faster and easier to debug
        f.Call();
        return;
    }
    Post(f, kind);
} // NOLINT

} // namespace uitask
