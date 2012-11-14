/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "UITask.h"

namespace uitask {

static Vec<UITask*> *           gUiTaskQueue;
static CRITICAL_SECTION         gUiTaskCs;
static HANDLE                   gUiTaskEvent;

void Initialize()
{
    gUiTaskQueue = new Vec<UITask*>();
    InitializeCriticalSection(&gUiTaskCs);
    gUiTaskEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

void Destroy()
{
    DeleteVecMembers(*gUiTaskQueue);
    delete gUiTaskQueue;
    DeleteCriticalSection(&gUiTaskCs);
    CloseHandle(gUiTaskEvent);
}

void Post(UITask *task)
{
    CrashIf(!task);
    ScopedCritSec cs(&gUiTaskCs);
    gUiTaskQueue->Append(task);
    SetEvent(gUiTaskEvent);
}

UITask *RetrieveNext()
{
    ScopedCritSec cs(&gUiTaskCs);
    if (0 == gUiTaskQueue->Count())
        return NULL;
    UITask *res = gUiTaskQueue->At(0);
    CrashIf(!res);
    gUiTaskQueue->RemoveAt(0);
    return res;
}

void ExecuteAll()
{
    UITask *wi;
    for (;;) {
        wi = uitask::RetrieveNext();
        if (!wi)
            return;
        wi->Execute();
        delete wi;
    }
}

HANDLE  GetQueueEvent()
{
    return gUiTaskEvent;
}

}

