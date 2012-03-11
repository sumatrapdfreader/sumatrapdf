/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
memtrace.dll is system for tracing memory allocations in arbitrary programs.
It hooks RtlFreeHeap etc. APIs in the process and sends collected information
(allocation address/size, address of freed memory, callstacks if possible etc.)
to an external collection and visualization process via named pipe.

The dll can either be injected into arbitrary processes (to trace arbitrary
processes) or an app can specifically load it to activate memory tracing.

If the collection process doesn't run when memtrace.dll is initialized, it does
nothing.
*/

#include "BaseUtil.h"
#include "MemTraceDll.h"

static HINSTANCE gInst;

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    gInst = hInstance;
    return TRUE;
}
