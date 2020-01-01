/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Log.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void SimpleLogTest() {
    {
        log(L"Test1\n");
        log(L"ML\n");
        logf(L"%s : %d\n", L"filename.pdf", 25);

        char* got = gLogBuf->Get();
        char* exp = "Test1\nML\nfilename.pdf : 25\n";
        utassert(str::Eq(got, exp));
    }
}
