/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/UITask.h"

namespace uitask {

static HWND gTaskDispatchHwnd = nullptr;

#define UITASK_CLASS_NAME L"UITask_Wnd_Class"
#define WM_EXECUTE_TASK (WM_USER + 104)

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_EXECUTE_TASK == msg) {
        auto func = (std::function<void()>*)lp;
        (*func)();
        delete func;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void Initialize() {
    WNDCLASSEX wcex;
    FillWndClassEx(wcex, UITASK_CLASS_NAME, WndProcTaskDispatch);
    RegisterClassEx(&wcex);

    CrashIf(gTaskDispatchHwnd);
    gTaskDispatchHwnd = CreateWindow(UITASK_CLASS_NAME, L"UITask Dispatch Window", WS_OVERLAPPED, 0, 0, 0, 0,
                                     HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
}

void DrainQueue() {
    CrashIf(!gTaskDispatchHwnd);
    MSG msg;
    while (PeekMessage(&msg, gTaskDispatchHwnd, WM_EXECUTE_TASK, WM_EXECUTE_TASK, PM_REMOVE)) {
        DispatchMessage(&msg);
    }
}

void Destroy() {
    DrainQueue();
    DestroyWindow(gTaskDispatchHwnd);
    gTaskDispatchHwnd = nullptr;
}

void Post(const std::function<void()>& f) {
    auto func = new std::function<void()>(f);
    PostMessageW(gTaskDispatchHwnd, WM_EXECUTE_TASK, 0, (LPARAM)func);
} // NOLINT

void PostOptimized(const std::function<void()>& f) {
    if (IsGUIThread(FALSE)) {
        // if we're already on ui thread, execute immediately
        // faster and easier to debug
        f();
        return;
    }
    auto func = new std::function<void()>(f);
    PostMessageW(gTaskDispatchHwnd, WM_EXECUTE_TASK, 0, (LPARAM)func);
} // NOLINT

} // namespace uitask
