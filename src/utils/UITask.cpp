/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/UITask.h"

#include "utils/Log.h"

#define DEF_NAME(id) #id "\0"
static SeqStrings gTaskNames = TASK_NAMES(DEF_NAME) "\0";
#undef DEF_NAME

namespace uitask {

static HWND gTaskDispatchHwnd = nullptr;

#define UITASK_CLASS_NAME L"UITask_Wnd_Class"

static UINT gExecuteTaskMessage = 0;

static UINT GetExecuteTaskMessage() {
    if (!gExecuteTaskMessage) {
        gExecuteTaskMessage = RegisterWindowMessageW(UITASK_CLASS_NAME);
    }
    return gExecuteTaskMessage;
}

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UINT wmExecTask = GetExecuteTaskMessage();
    if (wmExecTask == msg) {
        auto func = (std::function<void()>*)lp;
        int taskId = (int)wp;
        // logf("uitask::WndPorcTaskDispatch: about to free func 0x%p\n", (void*)func);
        // don't log most frequent for less spammy logs
        bool skipLog = taskId == TaskRepaintAsync;
        if (!skipLog) {
            auto name = seqstrings::IdxToStr(gTaskNames, taskId);
            if (!name) {
                name = "not found";
            }
            logf("uitask::WndProcTaskDispatch: will execute taskID: %d name: %s\n", taskId, name);
        }
        (*func)();
        if (!skipLog) {
            logf("uitask::WndProcTaskDispatch: did execute, will delete func 0x%p\n", (void*)func);
        }
        delete func;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void Initialize() {
    WNDCLASSEX wcex;
    FillWndClassEx(wcex, UITASK_CLASS_NAME, WndProcTaskDispatch);
    RegisterClassEx(&wcex);

    ReportIf(gTaskDispatchHwnd);
    gTaskDispatchHwnd = CreateWindow(UITASK_CLASS_NAME, L"UITask Dispatch Window", WS_OVERLAPPED, 0, 0, 0, 0,
                                     HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
}

void DrainQueue() {
    ReportIf(!gTaskDispatchHwnd);
    MSG msg;
    UINT wmExecTask = GetExecuteTaskMessage();
    while (PeekMessage(&msg, gTaskDispatchHwnd, wmExecTask, wmExecTask, PM_REMOVE)) {
        DispatchMessage(&msg);
    }
}

void Destroy() {
    DrainQueue();
    DestroyWindow(gTaskDispatchHwnd);
    gTaskDispatchHwnd = nullptr;
}

void Post(int taskId, const std::function<void()>& f) {
    auto func = new std::function<void()>(f);
    // logf("uitask::Post: allocated func 0x%p\n", (void*)func);
    UINT wmExecTask = GetExecuteTaskMessage();
    PostMessageW(gTaskDispatchHwnd, wmExecTask, (WPARAM)taskId, (LPARAM)func);
} // NOLINT

void PostOptimized(int taskId, const std::function<void()>& f) {
    if (IsGUIThread(FALSE)) {
        // if we're already on ui thread, execute immediately
        // faster and easier to debug
        f();
        return;
    }
    auto func = new std::function<void()>(f);
    // logf("uitask::PostOptimized: allocated func 0x%p\n", (void*)func);
    UINT wmExecTask = GetExecuteTaskMessage();
    PostMessageW(gTaskDispatchHwnd, wmExecTask, (WPARAM)taskId, (LPARAM)func);
} // NOLINT

} // namespace uitask
