/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* A driver for various tests. The idea is that instead of having a separate
   executable and related makefile additions for each test, we have one test
   driver which dispatches desired test based on cmd-line arguments.
   Currently it only does one test: mobi file parsing. */

#include <stdio.h>
#include <stdlib.h>

#include "DirIter.h"
#include "MobiParse.h"
#include "FileUtil.h"
#include "MobiHtmlParse.h"

static int Usage()
{
    printf("Tester.exe\n");
    printf("  -mobi dirOrFile : run mobi tests in a given directory or for a given file\n");
    return 1;
}

static void TestMobiFile(TCHAR *path)
{
    _tprintf(_T("Testing file '%s'\n"), path);
    MobiParse *mb = MobiParse::ParseFile(path);
    if (!mb) {
        printf(" error: failed to parse the file\n");
        return;
    }
    size_t sLen;
    char *s = mb->GetBookHtmlData(sLen);
    size_t forLayoutLen;
    uint8_t *forLayout = MobiHtmlToDisplay((uint8_t*)s, sLen, forLayoutLen);
    free(forLayout);
    delete mb;
}

static bool IsMobiFile(TCHAR *f)
{
    // TODO: also .prc and .pdb ?
    return str::EndsWithI(f, _T(".mobi")) ||
           str::EndsWithI(f, _T(".azw")) ||
           str::EndsWithI(f, _T(".azw1"));
}

static void MobiTestDir(TCHAR *dir)
{
    _tprintf(_T("Testing mobi files in '%s'\n"), dir);
    DirIter di(true);
    if (!di.Start(dir)) {
        _tprintf(_T("Error: invalid directory '%s'\n"), dir);
        return;
    }

    for (;;) {
        TCHAR *p = di.Next();
        if (NULL == p)
            break;
        if (IsMobiFile(p))
            TestMobiFile(p);
    }
}

static void MobiTest(char *dirOrFile)
{
    ScopedMem<TCHAR> tmp(str::conv::FromAnsi(dirOrFile));

    if (file::Exists(tmp) && IsMobiFile(tmp))
        TestMobiFile(tmp);
    else
        MobiTestDir(tmp);
}

extern "C"
int main(int argc, char **argv)
{
    int i = 1;
    int left = argc - 1;

    if (left < 2)
        return Usage();

    if (str::Eq(argv[i], "-mobi")) {
        ++i; --left;
        if (left != 1)
            return Usage();
        MobiTest(argv[i]);
        return 0;
    }

    return Usage();
}
