/* Copyright 2011-2012 the ucrt project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

extern void OnExit();
extern void OnStart();

extern "C" BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID reserved);

extern "C"
BOOL WINAPI _DllMainCRTStartup(HANDLE hDllHandle, DWORD dwReason, LPVOID reserved)
{
    if (DLL_PROCESS_ATTACH == dwReason)
        OnStart();

    BOOL ret = DllMain(hDllHandle, dwReason, reserved);

    if (DLL_PROCESS_DETACH == dwReason)
        OnExit();

    return ret;
}

