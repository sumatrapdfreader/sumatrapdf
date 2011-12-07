//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
// FILE: CRT0TCON.CPP
//==========================================
#include "libctiny.h"
#include <windows.h>
#include "argcargv.h"
#include "initterm.h"

// Force the linker to include KERNEL32.LIB
#pragma comment(linker, "/defaultlib:kernel32.lib")

//#pragma comment(linker, "/nodefaultlib:libc.lib")
//#pragma comment(linker, "/nodefaultlib:libcmt.lib")

extern "C" int __cdecl main(int, char **, char **);    // In user's code

extern "C" void DoInitialization();
extern "C" void DoCleanup();

//
// Modified version of the Visual C++ startup code.  Simplified to
// make it easier to read.  Only supports ANSI programs.
//
extern "C" void __cdecl mainCRTStartup() {
    int mainret, argc;

    argc = _ConvertCommandLineToArgcArgv( );

    // set up our minimal cheezy atexit table
    _atexit_init();

    // Call C++ constructors
    _initterm( __xc_a, __xc_z );

    // DoInitialization();

    mainret = main( argc, _ppszArgv, 0 );

    _DoExit();
    // DoCleanup();

    ExitProcess(mainret);
}
