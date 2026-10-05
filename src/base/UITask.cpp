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
static bool gInitialized = false;
static bool gWasDestroyed = false;
static Dispatch gDispatch = Dispatch::Queue;
static void (*gWakeupFn)() = nullptr;
#if OS_WIN
static HWND gTaskDispatchHwnd = nullptr;
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

#if OS_WIN
static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (gExecuteTaskMessage != msg) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    auto* ti = (TaskInfo*)lp;
    ExecuteTask(ti);
    return 0;
}
constexpr const WCHAR* kUiTaskClassName = L"UITask_Wnd_Class";
#endif

void Initialize(Dispatch dispatch) {
    gMainUIThreadId = GetCurrentThreadId();
    gWasDestroyed = false;
#if OS_WIN
    gDispatch = dispatch;
    if (gDispatch == Dispatch::Native) {
        ReportIf(gExecuteTaskMessage != 0);
        gExecuteTaskMessage = RegisterWindowMessageA("UITask_Msg_StdFunction");
        WNDCLASSEX wcex;
        FillWndClassEx(wcex, kUiTaskClassName, WndProcTaskDispatch);
        RegisterClassEx(&wcex);

        ReportIf(gTaskDispatchHwnd);
        const auto* cls = kUiTaskClassName;
        const auto* title = L"UITask Dispatch Window";
        auto* m = GetModuleHandleW(nullptr);
        DWORD style = WS_OVERLAPPED;
        gTaskDispatchHwnd = CreateWindowExW(0, cls, title, style, 0, 0, 0, 0, HWND_MESSAGE, nullptr, m, nullptr);

        gInitialized = gTaskDispatchHwnd != nullptr;
        return;
    }
#else
    (void)dispatch;
#endif
    ReportIf(gQueue);
    gQueue = new Vec<TaskInfo*>();
    gInitialized = true;
}

void DrainQueue() {
    if (!gInitialized) return;
#if OS_WIN
    if (gDispatch == Dispatch::Native) {
        MSG msg;
        UINT wmExecTask = gExecuteTaskMessage;
        while (PeekMessage(&msg, gTaskDispatchHwnd, wmExecTask, wmExecTask, PM_REMOVE)) {
            DispatchMessage(&msg);
        }
        return;
    }
#endif
    Vec<TaskInfo*> toRun;
    {
        AutoUnlockMutex lock(&gQueueMutex);
        VecAppendVec(toRun, *gQueue);
        VecClear(*gQueue);
    }
    for (TaskInfo* ti : toRun) ExecuteTask(ti);
}

void Destroy() {
    DrainQueue();
    gInitialized = false;
    gWasDestroyed = true;
#if OS_WIN
    if (gDispatch == Dispatch::Native) {
        DestroyWindow(gTaskDispatchHwnd);
        gTaskDispatchHwnd = nullptr;
    }
#endif
    Vec<TaskInfo*>* q;
    {
        AutoUnlockMutex lock(&gQueueMutex);
        q = gQueue;
        gQueue = nullptr;
    }
    if (q) {
        for (TaskInfo* ti : *q) FreeTaskInfo(ti);
        delete q;
    }
    delete (TaskInfo*)AtomicPtrExchange(&gTaskInfoCache, nullptr);
    gWakeupFn = nullptr;
}

void SetWakeupFn(void (*fn)()) {
    gWakeupFn = fn;
}

void Post(const Func0& f, Kind kind) {
    if (!gInitialized) {
        ReportIf(!gWasDestroyed);
        return;
    }
    TaskInfo* ti = AllocTaskInfo();
    ti->f = f;
    ti->kind = kind;
    ti->queueTime = TimeGet();
#if OS_WIN
    if (gDispatch == Dispatch::Native) {
        if (!PostMessageW(gTaskDispatchHwnd, gExecuteTaskMessage, 0, (LPARAM)ti)) FreeTaskInfo(ti);
        return;
    }
#endif
    {
        AutoUnlockMutex lock(&gQueueMutex);
        VecAppend(*gQueue, ti);
    }
    if (gWakeupFn) gWakeupFn();
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
