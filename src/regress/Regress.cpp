/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
A regression test suite. The idea is to add tests for bugs we fix that
are too time consuming to be part of unit tests. The tests can rely
on presence of shared test files.

Note: because it can be run as both release and debug, we can't use
assert() or CrashIf() but CrashAlwaysIf().
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

// http://code.google.com/p/sumatrapdf/issues/detail?id=1926
static void Regress00()
{
    TCHAR *filePath = path::Join(TestFilesDir(), _T("epub\\widget-figure-gallery-20120405.epub"));
    VerifyFileExists(filePath);
    Doc doc(Doc::CreateFromFile(filePath));
    CrashAlwaysIf(doc.LoadingFailed());
    CrashAlwaysIf(Doc_Epub != doc.GetDocType());

    PoolAllocator   textAllocator;
    HtmlFormatterArgs *args = CreateFormatterArgsDoc(doc, 820, 920, &textAllocator);
    HtmlPage *pages[3];
    HtmlFormatter *formatter = CreateFormatter(doc, args);
    int page = 0;
    for (HtmlPage *pd = formatter->Next(); pd; pd = formatter->Next()) {
        pages[page++] = pd;
        if (page == dimof(pages))
            break;
    }
    delete formatter;
    delete args;
    CrashAlwaysIf(page != 3);

    args = CreateFormatterArgsDoc(doc, 820, 920, &textAllocator);
    args->reparseIdx = pages[2]->reparseIdx;
    formatter = CreateFormatter(doc, args);
    // if bug is present, this will crash in formatter->Next()
    for (HtmlPage *pd = formatter->Next(); pd; pd = formatter->Next()) {
        delete pd;
    }
    delete formatter;
    delete args;
}

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
