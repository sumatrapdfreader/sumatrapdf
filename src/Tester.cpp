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

static int usage()
{
    printf("st.exe\n");
    printf("  -mobi dir : run mobi tests in a given directory\n");
    return 1;
}

static void testmobifile(TCHAR *path)
{
    _tprintf(_T("Testing file '%s'\n"), path);
}

static void mobitest(char *dir)
{
    printf("Testing mobi files in '%s'\n", dir);
    DirIter di(true);
    ScopedMem<TCHAR> tmp(str::conv::FromAnsi(dir));
    if (!di.Start(tmp)) {
        printf("Error: invalid directory '%s'\n", dir);
        return;
    }

    for (;;) {
        TCHAR *p = di.Next();
        if (NULL == p)
            break;
        if (str::EndsWithI(p, _T(".mobi")))
            testmobifile(p);
    }
}

extern "C"
int main(int argc, char **argv)
{
    int i = 1;
    int left = argc - 1;

    if (left < 2)
        return usage();

    if (str::Eq(argv[i], "-mobi")) {
        ++i; --left;
        if (left != 1)
            return usage();
        mobitest(argv[i]);
        return 0;
    }

    return usage();
}
