/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "SumatraLog.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

void SimpleLogTest() {
    {
        log("Test1\n");
        log("ML\n");
        logf("%s : %d\n", StrL("filename.pdf"), 25);

        Str got = ToStr(*gLogBuf);
        Str exp = "Test1\nML\nfilename.pdf : 25\n";
        utassert(str::Eq(got, exp));
    }
}