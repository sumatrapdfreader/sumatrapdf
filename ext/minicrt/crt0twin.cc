//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
// FILE: CRT0TWIN.CPP
//==========================================
#include "libctiny.h"
#include <windows.h>
#include "initterm.h"

// Force the linker to include KERNEL32.LIB
#pragma comment(linker, "/defaultlib:kernel32.lib")

#pragma comment(linker, "/nodefaultlib:libc.lib")
#pragma comment(linker, "/nodefaultlib:libcmt.lib")

// Modified version of the Visual C++ startup code.  Simplified to
// make it easier to read.

extern int WINAPI WinMain(HINSTANCE hinstExe, HINSTANCE hinstPrev,
                   LPSTR pszCmdLine, int nCmdShow);

extern "C" void __cdecl WinMainCRTStartup() {
    int mainret;
    char *lpszCommandLine;
    STARTUPINFO StartupInfo;

    lpszCommandLine = GetCommandLine();

    // Skip past program name (first token in command line).

    if (*lpszCommandLine == '"')  // Check for and handle quoted program name
    {
        lpszCommandLine++;  // Get past the first quote

        // Now, scan, and skip over, subsequent characters until  another
        // double-quote or a null is encountered
        while (*lpszCommandLine && (*lpszCommandLine != '"'))
            lpszCommandLine++;

        // If we stopped on a double-quote (usual case), skip over it.

        if (*lpszCommandLine == '"')
            lpszCommandLine++;
    }
    else    // First token wasn't a quote
    {
        while (*lpszCommandLine > ' ')
            lpszCommandLine++;
    }

    // Skip past any white space preceeding the second token.

    while (*lpszCommandLine && (*lpszCommandLine <= ' '))
        lpszCommandLine++;

    StartupInfo.dwFlags = 0;
    GetStartupInfo( &StartupInfo );

    // set up our minimal cheezy atexit table
    _atexit_init();

    // Call C++ constructors
    _initterm( __xc_a, __xc_z );

    mainret = WinMain( GetModuleHandle(NULL),
                       NULL,
                       lpszCommandLine,
                       StartupInfo.dwFlags & STARTF_USESHOWWINDOW
                            ? StartupInfo.wShowWindow : SW_SHOWDEFAULT );

    _DoExit();

    ExitProcess(mainret);
}

