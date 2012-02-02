/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

// a critical section for everything that needs protecting in mui
// we use only one for simplicity as long as contention is not a problem
CRITICAL_SECTION gMuiCs;

void InitMuiCriticalSection()
{
    InitializeCriticalSection(&gMuiCs);
}

void DeleteMuiCriticalSection()
{
    DeleteCriticalSection(&gMuiCs);
}

void EnterMuiCriticalSection()
{
    EnterCriticalSection(&gMuiCs);
}

void LeaveMuiCriticalSection()
{
    LeaveCriticalSection(&gMuiCs);
}



}
