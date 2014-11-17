/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "WinUtil.h"
#include "UITask.h"
#define NOLOG 1
#include "DebugLog.h"

class UITaskFunction : public UITask {
    const std::function<void()> f;

  public:
    UITaskFunction(const std::function<void()> f) : f(f) { name = "UITaskFunction"; }

    UITaskFunction &operator=(const UITaskFunction &) = delete;

    virtual ~UITaskFunction() override {}

    virtual void Execute() override { f(); }
};

namespace uitask {

static HWND gTaskDispatchHwnd = NULL;

#define UITASK_CLASS_NAME L"UITask_Wnd_Class"
#define WM_EXECUTE_TASK (WM_USER + 1)

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    UITask *task;
    if (WM_EXECUTE_TASK == msg) {
        task = (UITask *)lParam;
        lf("executing %s", task->name);
        task->Execute();
        delete task;
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

void Post(UITask *task) {
    CrashIf(!task || !gTaskDispatchHwnd);
    lf("posting %s", task->name);
    PostMessage(gTaskDispatchHwnd, WM_EXECUTE_TASK, 0, (LPARAM)task);
}

void Post(const std::function<void()> &f) {
    auto task = new UITaskFunction(f);
    PostMessage(gTaskDispatchHwnd, WM_EXECUTE_TASK, 0, (LPARAM)task);
}
}
