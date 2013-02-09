/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "UITask.h"
#include "WinUtil.h"

#define NOLOG 1
#include "DebugLog.h"

class UITaskFunc : public UITask {
    UITaskFuncPtr func;
    void *        arg;
public:
    UITaskFunc(UITaskFuncPtr func, void *arg) : func(func), arg(arg) {
        name = "UITaskFunc";
    }

    virtual ~UITaskFunc() {}

    virtual void Execute() {
        func(arg);
    }
};

namespace uitask {

static HWND  gTaskDispatchHwnd;

#define UITASK_CLASS_NAME   L"UITask_Wnd_Class"
#define WM_EXECUTE_TASK       (WM_USER + 1)

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UITask *task;
    if (WM_EXECUTE_TASK == msg) {
        task = (UITask*)lParam;
        lf("executing %s", task->name);
        task->Execute();
        delete task;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Initialize()
{
    WNDCLASSEX  wcex;
    HINSTANCE hinst = GetModuleHandle(NULL);
    FillWndClassEx(wcex, hinst, UITASK_CLASS_NAME, WndProcTaskDispatch);
    RegisterClassEx(&wcex);

    gTaskDispatchHwnd = CreateWindow(
            UITASK_CLASS_NAME, L"UITask Dispatch Window",
            WS_OVERLAPPED,
            0, 0, 0, 0,
            NULL, NULL,
            hinst, NULL);
}

// note: it's possible (but highly unlikely) that we might leak UITask
// objects that were sent to the window but not executed/destroyed.
// There's nothing we can do about it.
void Destroy()
{
}

void Post(UITask *task)
{
    CrashIf(!task);
    lf("posting %s", task->name);
    PostMessage(gTaskDispatchHwnd, WM_EXECUTE_TASK, 0, (LPARAM)task);
}

// arg can be NULL
void Post(UITaskFuncPtr func, void *arg)
{
    CrashIf(!func);
    Post(new UITaskFunc(func, arg));
}

}
