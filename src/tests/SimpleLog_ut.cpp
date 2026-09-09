/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/tests/UtAssert.h"
#include "SumatraLog.h"

void SimpleLogTest() {
    {
        log(StrL("Test1\n"));
        log(StrL("ML\n"));
        logf("%s : %d\n", StrL("filename.pdf"), 25);

        // the app logs during startup, so only the tail is ours
        Str got = ToStr(*gLogBuf);
        Str exp = StrL("Test1\nML\nfilename.pdf : 25\n");
        utassert(str::EndsWith(got, exp));
    }
}
