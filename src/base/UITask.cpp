/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#if OS_WIN
#include "base/Win.h"
#endif
#include "base/Timer.h"
#include "base/UITask.h"

namespace uitask {
static ThreadId gMainUIThreadId = 0;
static AtomicBool gInitialized = 0;
static AtomicBool gWasDestroyed = 0;
static bool gDestroying = false;
static Dispatch gDispatch = Dispatch::Queue;
// protected by gQueueMutex
static void (*gWakeupFn)() = nullptr;
#if OS_WIN
static AtomicPtr gTaskDispatchHwnd = nullptr;
static UINT gExecuteTaskMessage = 0;
#endif

struct TaskInfo {
    Func0 f;
    Kind kind = nullptr;
    TimeStamp queueTime{};
};

// Post() runs on worker threads and the dispatcher frees on the ui thread, so
// hand the finished TaskInfo back through a one-slot cache rather than going to
// the allocator for every task. An exchange is all it takes: taking swaps in
// nullptr, returning swaps the pointer in and deletes whatever it displaced, so
// at most one is ever parked here and no thread can see a half-published one.
static AtomicPtr gTaskInfoCache = nullptr;

static TaskInfo* AllocTaskInfo() {
    auto* ti = (TaskInfo*)AtomicPtrExchange(&gTaskInfoCache, nullptr);
    if (!ti) {
        ti = new TaskInfo();
    }
    return ti;
}

static void FreeTaskInfo(TaskInfo* ti) {
    if (!ti) {
        return;
    }
    *ti = TaskInfo{};
    auto* prev = (TaskInfo*)AtomicPtrExchange(&gTaskInfoCache, ti);
    delete prev;
}

// A task that sat in the queue this long is worth reporting even for the kinds
// that are too frequent to log every time.
#if IS_ASAN || IS_DEBUG
constexpr double kSlowTaskDispatchMs = 300.0;
#else
constexpr double kSlowTaskDispatchMs = 50.0;
#endif

static SeqStrings gSkipLogNames =
    "TaskFindCountProgress\0CopyProgress\0RenderFinished\0FrameUpdateUi\0(no "
    "kind)\0Repaint\0SaveSettings\0ShowSelectedAnnot\0GoToFindMatch\0";

static Mutex gQueueMutex;
// Dispatch::Queue's queue; with Dispatch::Native it holds the tasks whose
// PostMessage failed (a full message queue), run with the next dispatch
static Vec<TaskInfo*>* gQueue = nullptr;

static void ExecuteTask(TaskInfo* ti) {
    Kind kind = ti->kind;
    // how long the task waited between Post() and getting here
    double queuedMs = TimeSinceInMs(ti->queueTime);
    Str kindName = kind ? Str(kind) : StrL("(no kind)");
    bool shouldLog = SeqStrIndex(gSkipLogNames, kindName) < 0;
    if (shouldLog) {
        logf("uitask::WndProcTaskDispatch: will execute '%s', 0x%p, queued for %.3f ms\n", kindName, (void*)ti,
             queuedMs);
    } else if (queuedMs >= kSlowTaskDispatchMs) {
        logf("uitask::WndProcTaskDispatch: slow dispatch of '%s', queued for %.3f ms\n", kindName, queuedMs);
    }
    ti->f.Call();
    if (shouldLog) {
        logf("uitask::WndProcTaskDispatch: did execute 0x%p\n", (void*)ti);
    }
    FreeTaskInfo(ti);
}

static void RunQueuedTasks() {
    Vec<TaskInfo*> toRun;
    {
        AutoUnlockMutex lock(&gQueueMutex);
        if (!gQueue || !VecTransferAppend(toRun, *gQueue)) {
            return;
        }
    }
    for (TaskInfo* ti : toRun) {
        ExecuteTask(ti);
    }
}

#if OS_WIN
static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (gExecuteTaskMessage != msg) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    auto* ti = (TaskInfo*)lp;
    ExecuteTask(ti);
    // the ones PostMessage couldn't take
    RunQueuedTasks();
    return 0;
}
constexpr const WCHAR* kUiTaskClassName = L"UITask_Wnd_Class";
#endif

void Initialize(Dispatch dispatch) {
    gMainUIThreadId = GetCurrentThreadId();
    AtomicBoolSet(&gWasDestroyed, false);
#if OS_WIN
    gDispatch = dispatch;
    if (gDispatch == Dispatch::Native) {
        ReportIf(gExecuteTaskMessage != 0);
        gExecuteTaskMessage = RegisterWindowMessageA("UITask_Msg_StdFunction");
        WNDCLASSEX wcex;
        FillWndClassEx(wcex, kUiTaskClassName, WndProcTaskDispatch);
        RegisterClassEx(&wcex);

        ReportIf(AtomicPtrGet(&gTaskDispatchHwnd) != nullptr);
        const auto* cls = kUiTaskClassName;
        const auto* title = L"UITask Dispatch Window";
        auto* m = GetModuleHandleW(nullptr);
        DWORD style = WS_OVERLAPPED;
        HWND hwnd = CreateWindowExW(0, cls, title, style, 0, 0, 0, 0, HWND_MESSAGE, nullptr, m, nullptr);
        if (!hwnd) {
            gExecuteTaskMessage = 0;
            AtomicBoolSet(&gInitialized, false);
            return;
        }
        AtomicPtrSet(&gTaskDispatchHwnd, hwnd);

        {
            AutoUnlockMutex lock(&gQueueMutex);
            gQueue = new Vec<TaskInfo*>();
        }
        AtomicBoolSet(&gInitialized, true);
        return;
    }
#else
    (void)dispatch;
#endif
    {
        AutoUnlockMutex lock(&gQueueMutex);
        ReportIf(gQueue);
        gQueue = new Vec<TaskInfo*>();
    }
    AtomicBoolSet(&gInitialized, true);
}

void DrainQueue() {
    if (!AtomicBoolGet(&gInitialized) || ReportIf(!IsMainUIThread())) {
        return;
    }
#if OS_WIN
    if (gDispatch == Dispatch::Native) {
        HWND hwnd = (HWND)AtomicPtrGet(&gTaskDispatchHwnd);
        if (hwnd) {
            MSG msg;
            UINT wmExecTask = gExecuteTaskMessage;
            while (PeekMessage(&msg, hwnd, wmExecTask, wmExecTask, PM_REMOVE)) {
                DispatchMessage(&msg);
            }
        }
    }
#endif
    RunQueuedTasks();
}

void Destroy() {
    if (ReportIf(!IsMainUIThread()) || gDestroying) {
        return;
    }
    gDestroying = true;
    DrainQueue();
    {
        AutoUnlockMutex lock(&gQueueMutex);
        AtomicBoolSet(&gWasDestroyed, true);
        AtomicBoolSet(&gInitialized, false);
    }
#if OS_WIN
    if (gDispatch == Dispatch::Native) {
        HWND hwnd = (HWND)AtomicPtrExchange(&gTaskDispatchHwnd, nullptr);
        if (hwnd) {
            MSG msg;
            UINT wmExecTask = gExecuteTaskMessage;
            while (PeekMessage(&msg, hwnd, wmExecTask, wmExecTask, PM_REMOVE)) {
                DispatchMessage(&msg);
            }
            DestroyWindow(hwnd);
        }
        gExecuteTaskMessage = 0;
    }
#endif
    // A worker may have queued a task during the first drain.
    RunQueuedTasks();
    Vec<TaskInfo*>* q;
    {
        AutoUnlockMutex lock(&gQueueMutex);
        q = gQueue;
        gQueue = nullptr;
        gWakeupFn = nullptr;
    }
    delete q;
    delete (TaskInfo*)AtomicPtrExchange(&gTaskInfoCache, nullptr);
    gDestroying = false;
}

void SetWakeupFn(void (*fn)()) {
    AutoUnlockMutex lock(&gQueueMutex);
    gWakeupFn = fn;
}

bool Post(const Func0& f, Kind kind) {
    if (!AtomicBoolGet(&gInitialized)) {
        ReportIf(!AtomicBoolGet(&gWasDestroyed));
        return false;
    }
    TaskInfo* ti = AllocTaskInfo();
    ti->f = f;
    ti->kind = kind;
    ti->queueTime = TimeGet();
    void (*wakeup)() = nullptr;
    {
        AutoUnlockMutex lock(&gQueueMutex);
        if (!AtomicBoolGet(&gInitialized) || !gQueue) {
            delete ti;
            return false;
        }
#if OS_WIN
        if (gDispatch == Dispatch::Native) {
            HWND hwnd = (HWND)AtomicPtrGet(&gTaskDispatchHwnd);
            if (hwnd && PostMessageW(hwnd, gExecuteTaskMessage, 0, (LPARAM)ti)) {
                return true;
            }
            // Keep it for the next dispatch if the native queue is full.
        }
#endif
        if (!VecAppend(*gQueue, ti)) {
            delete ti;
            return false;
        }
        wakeup = gWakeupFn;
    }
    if (wakeup) wakeup();
    return true;
}

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
}
} // namespace uitask
