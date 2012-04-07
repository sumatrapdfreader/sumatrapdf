/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "UiMsg.h"

namespace uimsg {

static Vec<UiMsg*> *        gUiMsgQueue;
static CRITICAL_SECTION     gUiMsgCs;
static HANDLE               gUiMsgEvent;

void Initialize()
{
    gUiMsgQueue = new Vec<UiMsg*>();
    InitializeCriticalSection(&gUiMsgCs);
    gUiMsgEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

void Destroy()
{
    delete gUiMsgQueue;
    DeleteCriticalSection(&gUiMsgCs);
    CloseHandle(gUiMsgEvent);
}

void Post(UiMsg *msg)
{
    CrashIf(!msg);
    ScopedCritSec cs(&gUiMsgCs);
    gUiMsgQueue->Append(msg);
    SetEvent(gUiMsgEvent);
}

UiMsg *RetrieveNext()
{
    ScopedCritSec cs(&gUiMsgCs);
    if (0 == gUiMsgQueue->Count())
        return NULL;
    UiMsg *res = gUiMsgQueue->At(0);
    CrashIf(!res);
    gUiMsgQueue->RemoveAt(0);
    return res;
}

HANDLE  GetQueueEvent()
{
    return gUiMsgEvent;
}

}
