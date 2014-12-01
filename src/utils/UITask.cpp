/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "WinUtil.h"
#include "UITask.h"

namespace uitask {

static HWND gTaskDispatchHwnd = NULL;

#define UITASK_CLASS_NAME L"UITask_Wnd_Class"
#define WM_EXECUTE_TASK (WM_USER + 1)

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (WM_EXECUTE_TASK == msg) {
        auto func = (std::function<void()> *)lParam;
        (*func)();
        delete func;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Initialize() {
    WNDCLASSEX wcex;
    FillWndClassEx(wcex, UITASK_CLASS_NAME, WndProcTaskDispatch);
    RegisterClassEx(&wcex);

    CrashIf(gTaskDispatchHwnd);
    gTaskDispatchHwnd = CreateWindow(UITASK_CLASS_NAME, L"UITask Dispatch Window", WS_OVERLAPPED, 0,
                                     0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);
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
    gTaskDispatchHwnd = NULL;
}

void Post(const std::function<void()> &f) {
    auto func = new std::function<void()>(f);
    PostMessage(gTaskDispatchHwnd, WM_EXECUTE_TASK, 0, (LPARAM)func);
}
}
