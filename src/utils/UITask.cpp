/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/UITask.h"

#include "utils/Log.h"

namespace uitask {

static HWND gTaskDispatchHwnd = nullptr;

UINT gExecuteTaskMessage = 0;

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (gExecuteTaskMessage == msg) {
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

constexpr const WCHAR* UITASK_CLASS_NAME = L"UITask_Wnd_Class";

void Initialize() {
    ReportIf(gExecuteTaskMessage != 0);
    gExecuteTaskMessage = RegisterWindowMessageA("UITask_Msg_StdFunction");
    WNDCLASSEX wcex;
    FillWndClassEx(wcex, UITASK_CLASS_NAME, WndProcTaskDispatch);
    RegisterClassEx(&wcex);

    ReportIf(gTaskDispatchHwnd);
    auto cls = UITASK_CLASS_NAME;
    auto title = L"UITask Dispatch Window";
    auto m = GetModuleHandleW(nullptr);
    DWORD style = WS_OVERLAPPED;
    gTaskDispatchHwnd = CreateWindowExW(0, cls, title, style, 0, 0, 0, 0, HWND_MESSAGE, nullptr, m, nullptr);
}

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
}

void Post(const Func0& f, Kind kind) {
    auto func = new Func0(f);
    PostMessageW(gTaskDispatchHwnd, gExecuteTaskMessage, (WPARAM)kind, (LPARAM)func);
} // NOLINT

void PostOptimized(const Func0& f, Kind kind) {
    if (IsGUIThread(FALSE)) {
        // if we're already on ui thread, execute immediately
        // faster and easier to debug
        f.Call();
        return;
    }
    Post(f, kind);
} // NOLINT

} // namespace uitask
