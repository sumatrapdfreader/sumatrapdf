/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "utils/Log.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void SimpleLogTest() {
    {
        log("Test1\n");
        log("ML\n");
        logf("%s : %d\n", "filename.pdf", 25);

        Str got = gLogBuf->Get();
        Str exp = "Test1\nML\nfilename.pdf : 25\n";
        utassert(str::Eq(got, exp));
    }
}