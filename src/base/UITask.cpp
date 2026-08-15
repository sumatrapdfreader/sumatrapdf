/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/Timer.h"
#include "base/UITask.h"

namespace uitask {

static HWND gTaskDispatchHwnd = nullptr;

// set by Destroy(). From then on gTaskDispatchHwnd is null because the app is
// shutting down, which is a different situation from it never having been
// created - see Post()
static bool gWasDestroyed = false;

static UINT gExecuteTaskMessage = 0;

static ThreadId gMainUIThreadId = 0;

// What Post() hands to the dispatcher. Bundling these means one allocation per
// task instead of one for the Func0 plus smuggling the kind through wparam, and
// it leaves somewhere to record when the task was queued.
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
constexpr double kSlowTaskDispatchMs = 50.0;

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (gExecuteTaskMessage == msg) {
        auto* ti = (TaskInfo*)lp;
        Kind kind = ti->kind;
        // how long the task waited between Post() and getting here
        double queuedMs = TimeSinceInMs(ti->queueTime);
        Str kindName = kind ? Str(kind) : StrL("(no kind)");
        bool shouldLog =
            (kind != nullptr) && !str::Eq(kindName, StrL("RenderFinished")) && !str::Eq(kindName, StrL("CopyProgress"));
        if (shouldLog) {
            logf("uitask::WndProcTaskDispatch: will execute '%s', task 0x%p, queued for %.3f ms\n", kindName, (void*)ti,
                 queuedMs);
        } else if (queuedMs >= kSlowTaskDispatchMs) {
            logf("uitask::WndProcTaskDispatch: slow dispatch of '%s', queued for %.3f ms\n", kindName, queuedMs);
        }
        ti->f.Call();
        if (shouldLog) {
            logf("uitask::WndProcTaskDispatch: did execute task 0x%p\n", (void*)ti);
        }
        FreeTaskInfo(ti);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

constexpr const WCHAR* UITASK_CLASS_NAME = L"UITask_Wnd_Class";

// Call Initialize() at program startup and Destroy() at the end
void Initialize() {
    gMainUIThreadId = GetCurrentThreadId();
    gWasDestroyed = false;

    ReportIf(gExecuteTaskMessage != 0);
    gExecuteTaskMessage = RegisterWindowMessageA("UITask_Msg_StdFunction");
    WNDCLASSEX wcex;
    FillWndClassEx(wcex, UITASK_CLASS_NAME, WndProcTaskDispatch);
    RegisterClassEx(&wcex);

    ReportIf(gTaskDispatchHwnd);
    const auto* cls = UITASK_CLASS_NAME;
    const auto* title = L"UITask Dispatch Window";
    auto* m = GetModuleHandleW(nullptr);
    DWORD style = WS_OVERLAPPED;
    gTaskDispatchHwnd = CreateWindowExW(0, cls, title, style, 0, 0, 0, 0, HWND_MESSAGE, nullptr, m, nullptr);
}

// call only from the same thread as Initialize() and Destroy()
void DrainQueue() {
    ReportIf(!gTaskDispatchHwnd);
    MSG msg;
    UINT wmExecTask = gExecuteTaskMessage;
    while (PeekMessage(&msg, gTaskDispatchHwnd, wmExecTask, wmExecTask, PM_REMOVE)) {
        DispatchMessage(&msg);
    }
}

void Destroy() {
    DrainQueue();
    DestroyWindow(gTaskDispatchHwnd);
    gTaskDispatchHwnd = nullptr;
    gWasDestroyed = true;
    delete (TaskInfo*)AtomicPtrExchange(&gTaskInfoCache, nullptr);
}

void Post(const Func0& f, Kind kind) {
    if (!gTaskDispatchHwnd) {
        // After Destroy() this is a worker that outlived the UI finishing its
        // work (the file-existence checker is the usual one). Nothing can run
        // the task any more, so drop it - quietly, because the process is on
        // its way out and a debug report here would race the rest of shutdown.
        // Before Initialize() it is a real bug: PostMessageW() with a null hwnd
        // posts a *thread* message, which succeeds but is never routed to a
        // window proc, so the task would silently never run.
        ReportIf(!gWasDestroyed);
        return;
    }
    TaskInfo* ti = AllocTaskInfo();
    ti->f = f;
    ti->kind = kind;
    ti->queueTime = TimeGet();
    if (!PostMessageW(gTaskDispatchHwnd, gExecuteTaskMessage, 0, (LPARAM)ti)) {
        // nothing will dispatch it, so don't lose the allocation
        FreeTaskInfo(ti);
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
