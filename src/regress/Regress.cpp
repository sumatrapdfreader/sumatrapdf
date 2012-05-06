/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
A regression test suite. The idea is to add tests for bugs we fix that
are too time consuming to be part of unit tests. The tests can rely
on presence of shared test files.

Note: because it can be run as both release and debug, we can't use
assert() or CrashIf() but CrashAlwaysIf().

To write new regression test:
- add a file src/regress/Regress${NN}.cpp with Regress${NN} function
- #include "Regress${NN}.cpp" right before RunTest() function
- call Regress${NN} function from RunTests()
*/

#include "BaseUtil.h"
#include "DirIter.h"
#include "Doc.h"
#include "FileUtil.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "HtmlFormatter.h"
#include "Mui.h"
#include "WinUtil.h"

#include "DebugLog.h"

/*
TODO: install a crash handler and dump crash info to stdout so that we can tell
a crash has happened
*/

static TCHAR *gTestFilesDir;

TCHAR *TestFilesDir()
{
    return gTestFilesDir;
}

static int Usage()
{
    printf("regress.exe\n");
    printf("Error: didn't find test files on this computer!\n");
    return 1;
}

/* Auto-detect the location of test files. Ultimately we might add a cmd-line
option to specify this directory, for now just add your location(s) to the list */
static bool FindTestFilesDir()
{
    TCHAR *dirsToCheck[] = {
        _T("C:\\Documents and Settings\\kkowalczyk\\My Documents\\Google Drive\\Sumatra")
    };
    for (size_t i = 0; i < dimof(dirsToCheck); i++) {
        TCHAR *dir = dirsToCheck[i];
        if (dir::Exists(dir)) {
            gTestFilesDir = dir;
            return true;
        }
    }
    return false;
}

void VerifyFileExists(const TCHAR *filePath)
{
    if (!file::Exists(filePath)) {
        _tprintf(_T("File '%s' doesn't exist!\n"), filePath);
        exit(1);
    }
}

#include "Regress00.cpp"

static void RunTests()
{
    Regress00();
}

extern "C"
int main(int argc, char **argv)
{
    if (!FindTestFilesDir())
        return Usage();

    InitAllCommonControls();
    ScopedGdiPlus gdi;
    mui::Initialize();

    RunTests();
    printf("All tests completed successfully!\n");
    mui::Destroy();
    return 0;
}
