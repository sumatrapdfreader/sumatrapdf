/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "UITask.h"
#include "WinUtil.h"

#define NOLOG 1
#include "DebugLog.h"

namespace uitask {

static HWND  gTaskDispatchHwnd;

#define UITASK_CLASS_NAME   _T("UITask_Wnd_Class")
#define WM_QUEUE_TASK       (WM_USER + 1)

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UITask *task;
    if (WM_QUEUE_TASK == msg) {
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
            UITASK_CLASS_NAME, _T("UITask Dispatch Window"),
            WS_OVERLAPPED,
            0, 0, 0, 0,
            NULL, NULL,
            hinst, NULL);
}

void Destroy()
{
}

void Post(UITask *task)
{
    CrashIf(!task);
    lf("posting %s", task->name);
    PostMessage(gTaskDispatchHwnd, WM_QUEUE_TASK, 0, (LPARAM)task);
}

}
