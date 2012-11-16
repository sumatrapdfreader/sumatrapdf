/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "UITask.h"
#include "WinUtil.h"

#define NOLOG 1
#include "DebugLog.h"

namespace uitask {

static Vec<UITask*> *           gUiTaskQueue;
static CRITICAL_SECTION         gUiTaskCs;
static HWND                     gTaskDispatchHwnd;

#define WND_CLASS _T("UITask_Wnd_Class")

static UITask *RetrieveNext()
{
    ScopedCritSec cs(&gUiTaskCs);
    if (0 == gUiTaskQueue->Count())
        return NULL;
    UITask *res = gUiTaskQueue->At(0);
    CrashIf(!res);
    gUiTaskQueue->RemoveAt(0);
    return res;
}

static void ExecuteAll()
{
    UITask *task;
    for (;;) {
        task = uitask::RetrieveNext();
        if (!task)
            return;
        lf("executing %s", task->name);
        task->Execute();
        delete task;
    }
}

static LRESULT CALLBACK WndProcTaskDispatch(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    uitask::ExecuteAll();
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Initialize()
{
    gUiTaskQueue = new Vec<UITask*>();
    InitializeCriticalSection(&gUiTaskCs);

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
    DeleteVecMembers(*gUiTaskQueue);
    delete gUiTaskQueue;
    DeleteCriticalSection(&gUiTaskCs);
}

void Post(UITask *task)
{
    CrashIf(!task);
    ScopedCritSec cs(&gUiTaskCs);
    gUiTaskQueue->Append(task);
    lf("posting %s", task->name);
    PostMessage(gTaskDispatchHwnd, WM_NULL, 0, 0);
}

}

