/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "UiMsg.h"

#include "Scoped.h"
#include "Vec.h"

namespace uimsg {

static Vec<UiMsg*> *        gUiMsgQueue;
static CRITICAL_SECTION     gUiMsgCs;
static DWORD                gUiMsgThreadId;

void Initialize()
{
    gUiMsgQueue = new Vec<UiMsg*>();
    InitializeCriticalSection(&gUiMsgCs);
    gUiMsgThreadId = GetCurrentThreadId();
}

void Destroy()
{
    delete gUiMsgQueue;
    DeleteCriticalSection(&gUiMsgCs);
}

void Post(UiMsg *msg)
{
    CrashIf(!msg);
    ScopedCritSec cs(&gUiMsgCs);
    gUiMsgQueue->Append(msg);
    if (gUiMsgQueue->Count() == 1) {
        // make sure that the message queue isn't empty
        PostThreadMessage(gUiMsgThreadId, WM_NULL, 0, 0);
    }
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

}
