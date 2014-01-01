/* Copyright 2014 the ucrt project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

extern void OnExit();
extern void OnStart();

extern "C" int __cdecl main(int, char **, char **);

extern "C" void __cdecl mainCRTStartup()
{
    // TODO: set argc/argv
    //int argc = _ConvertCommandLineToArgcArgv( );

    OnStart();
    int ret = main(0, NULL, 0);
    OnExit();
    ExitProcess(ret);
}

