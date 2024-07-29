/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/UITask.h"

#include "utils/Log.h"

namespace uitask {

static HWND gTaskDispatchHwnd = nullptr;

static UINT GetExecuteTaskMessage() {
    static UINT gExecuteTaskMessage = 0;
    if (!gExecuteTaskMessage) {
        gExecuteTaskMessage = RegisterWindowMessageW(L"UITask_Msg_StdFunction");
    }
    return gExecuteTaskMessage;
}

static UINT GetExecuteTask2Message() {
    static UINT gExecuteTask2Message = 0;
    if (!gExecuteTask2Message) {
        gExecuteTask2Message = RegisterWindowMessageW(L"UITask_Msg_Func0");
    }
    return gExecuteTask2Message;
}

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UINT wmExecTask = GetExecuteTaskMessage();
    if (wmExecTask == msg) {
        Kind kind = (Kind)wp;
        auto func = (std::function<void()>*)lp;
        if (kind != nullptr) {
            logf("uitask::WndProcTaskDispatch: will execute '%s', func 0x%p\n", kind, (void*)func);
        }
        (*func)();
        if (kind != nullptr) {
            logf("uitask::WndProcTaskDispatch: did execute, will delete func 0x%p\n", (void*)func);
        }
        delete func;
        return 0;
    }

    UINT wmExecTask2 = GetExecuteTask2Message();
    if (wmExecTask2 == msg) {
        Kind kind = (Kind)wp;
        auto func = (Func0*)lp;
        if (kind != nullptr) {
            logf("uitask::WndProcTaskDispatch: will execute '%s', func 0x%p\n", kind, (void*)func);
        }
        func->Call();
        if (kind != nullptr) {
            logf("uitask::WndProcTaskDispatch: did execute, will delete func 0x%p\n", (void*)func);
        }
        delete func;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

#define UITASK_CLASS_NAME L"UITask_Wnd_Class"

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
    {
        UINT wmExecTask = GetExecuteTaskMessage();
        while (PeekMessage(&msg, gTaskDispatchHwnd, wmExecTask, wmExecTask, PM_REMOVE)) {
            DispatchMessage(&msg);
        }
    }
    {
        UINT wmExecTask = GetExecuteTask2Message();
        while (PeekMessage(&msg, gTaskDispatchHwnd, wmExecTask, wmExecTask, PM_REMOVE)) {
            DispatchMessage(&msg);
        }
    }
}

void Destroy() {
    DrainQueue();
    DestroyWindow(gTaskDispatchHwnd);
    gTaskDispatchHwnd = nullptr;
}

void Post(Kind kind, const std::function<void()>& f) {
    auto func = new std::function<void()>(f);
    // logf("uitask::Post: allocated func 0x%p\n", (void*)func);
    UINT wmExecTask = GetExecuteTaskMessage();
    PostMessageW(gTaskDispatchHwnd, wmExecTask, (WPARAM)kind, (LPARAM)func);
} // NOLINT

void Post(const Func0& f, Kind kind) {
    auto func = new Func0(f);
    // logf("uitask::Post: allocated func 0x%p\n", (void*)func);
    UINT wmExecTask = GetExecuteTask2Message();
    PostMessageW(gTaskDispatchHwnd, wmExecTask, (WPARAM)kind, (LPARAM)func);
} // NOLINT

#if 0
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
#endif

} // namespace uitask
