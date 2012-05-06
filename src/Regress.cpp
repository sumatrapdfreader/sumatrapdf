/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A regression test suite. The idea is to add tests for bugs we fix that
are too time consuming to be part of unit tests. The tests can rely
on presence of shared test files.

*/

#include "BaseUtil.h"
#include "DirIter.h"
#include "FileUtil.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "HtmlFormatter.h"
#include "HtmlPrettyPrint.h"
#include "MobiDoc.h"
#include "Mui.h"
#include "Timer.h"
#include "WinUtil.h"

#include "DebugLog.h"


static int Usage()
{
    printf("regress.exe\n");
    printf("Error: didn't find test files on this computer!\n");
    return 1;
}

/* Auto-detect the location of test files. Ultimately we might add a cmd-line
option to specify this directory, for now just add your location(s) to the list */
static TCHAR *FindTestFilesDir()
{
    TCHAR *dirsToCheck[] = {
        _T("C:\\Documents and Settings\\kkowalczyk\\My Documents\\Google Drive\\Sumatra")
    };
    for (size_t i = 0; i < dimof(dirsToCheck); i++) {
        TCHAR *dir = dirsToCheck[i];
        if (dir::Exists(dir))
            return dir;
    }
    return NULL;
}

static void RunTests()
{
    // TODO: write me!!!
}

extern "C"
int main(int argc, char **argv)
{
    TCHAR *testFilesDir = FindTestFilesDir();
    if (!testFilesDir)
        return Usage();

    InitAllCommonControls();
    ScopedGdiPlus gdi;
    mui::Initialize();

    RunTests();
    printf("All tests completed successfully!\n");
    mui::Destroy();
    return 0;
}
