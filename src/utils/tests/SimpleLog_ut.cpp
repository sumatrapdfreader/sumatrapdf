/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ScopedWin.h"
#include "SimpleLog.h"

// must be last due to assert() over-write
#include "UtAssert.h"

void SimpleLogTest() {
    slog::MultiLogger log;
    log.LogAndFree(str::Dup(L"Don't leak me!"));

    slog::MemoryLogger logAll;
    log.AddLogger(&logAll);

    {
        slog::MemoryLogger ml;
        log.AddLogger(&ml);
        log.Log(L"Test1");
        ml.Log(L"ML");
        ml.LogFmt(L"%s : %d", L"filen\xE4me.pdf", 25);
        log.RemoveLogger(&ml);

        utassert(str::Eq(ml.GetData(), L"Test1\r\nML\r\nfilen\xE4me.pdf : 25\r\n"));
    }

    utassert(str::Eq(logAll.GetData(), L"Test1\r\nTest2\r\nfilen\xE4me.pdf : 25\r\n"));
    log.RemoveLogger(&logAll);

    // don't leak the logger, don't crash on logging nullptr
    log.AddLogger(new slog::DebugLogger());
    log.Log(nullptr);
}
