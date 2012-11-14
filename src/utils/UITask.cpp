#include "BaseUtil.h"
#include "UITask.h"

namespace uitask {

static Vec<UIThreadWorkItem*> * gUiTaskQueue;
static CRITICAL_SECTION         gUiTaskCs;
static HANDLE                   gUiTaskEvent;

void Initialize()
{
    gUiTaskQueue = new Vec<UIThreadWorkItem*>();
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

void Post(UIThreadWorkItem *msg)
{
    CrashIf(!msg);
    ScopedCritSec cs(&gUiTaskCs);
    gUiTaskQueue->Append(msg);
    SetEvent(gUiTaskEvent);
}

UIThreadWorkItem *RetrieveNext()
{
    ScopedCritSec cs(&gUiTaskCs);
    if (0 == gUiTaskQueue->Count())
        return NULL;
    UIThreadWorkItem *res = gUiTaskQueue->At(0);
    CrashIf(!res);
    gUiTaskQueue->RemoveAt(0);
    return res;
}

HANDLE  GetQueueEvent()
{
    return gUiTaskEvent;
}

}

void QueueWorkItem(UIThreadWorkItem *wi)
{
    uitask::Post(wi);
}

void ExecuteUITasks()
{
    UIThreadWorkItem *wi;
    for (;;) {
        wi = uitask::RetrieveNext();
        if (!wi)
            return;
        wi->Execute();
        delete wi;
    }
}

