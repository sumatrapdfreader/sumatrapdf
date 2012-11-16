/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "UITask.h"
#include "WinUtil.h"

#define NOLOG 1
#include "DebugLog.h"

namespace uitask {

static HWND  gTaskDispatchHwnd;
static UINT  gTaskMsg;

#define WND_CLASS _T("UITask_Wnd_Class")

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UITask *task;
    if (msg == gTaskMsg) {
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
    gTaskMsg = RegisterWindowMessageA("UITask_Process_Msg");

    WNDCLASSEX  wcex = { 0 };
    HINSTANCE hinst = GetModuleHandle(NULL);
    FillWndClassEx(wcex, hinst);
    wcex.lpfnWndProc    = WndProcTaskDispatch;
    wcex.lpszClassName  = WND_CLASS;
    wcex.hIcon          = NULL;
    wcex.hIconSm        = NULL;
    RegisterClassEx(&wcex);

    gTaskDispatchHwnd = CreateWindow(
            WND_CLASS, _T("UITask Dispatch Window"),
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
    PostMessage(gTaskDispatchHwnd, gTaskMsg, 0, (LPARAM)task);
}

}

