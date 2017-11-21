/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ScopedWin.h"
#include "SimpleLog.h"

// must be last due to assert() over-write
#include "UtAssert.h"

void SimpleLogTest() {
    {
        slog::MemoryLogger log;
        log.Log(L"Test1");
        log.Log(L"ML");
        log.LogFmt(L"%s : %d", L"filen\xE4me.pdf", 25);

        utassert(str::Eq(log.GetData(), L"Test1\r\nML\r\nfilen\xE4me.pdf : 25\r\n"));
    }
}
