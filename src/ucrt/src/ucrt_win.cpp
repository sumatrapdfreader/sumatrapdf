/* Copyright 2014 the ucrt project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

extern void OnExit();
extern void OnStart();

extern "C" void __cdecl WinMainCRTStartup()
{
    STARTUPINFO StartupInfo = {0};
    GetStartupInfo(&StartupInfo);

    OnStart();
    int ret = WinMain(GetModuleHandle(NULL), NULL, NULL,
                      StartupInfo.dwFlags & STARTF_USESHOWWINDOW
                            ? StartupInfo.wShowWindow : SW_SHOWDEFAULT );
    OnExit();
    ExitProcess(ret);
}

