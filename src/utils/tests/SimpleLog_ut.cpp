/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SimpleLog.h"
#include "UtAssert.h"

void SimpleLogTest()
{
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

        assert(str::Eq(ml.GetData(), L"Test1\r\nML\r\nfilen\xE4me.pdf : 25\r\n"));
    }

    {
        HANDLE hRead, hWrite;
        CreatePipe(&hRead, &hWrite, NULL, 0);
        slog::FileLogger fl(hWrite);
        log.AddLogger(&fl);
        log.Log(L"Test2");
        fl.Log(L"FL");
        log.LogFmt(L"%s : %d", L"filen\xE4me.pdf", 25);
        log.RemoveLogger(&fl);

        char pipeData[32];
        char *expected = "Test2\r\nFL\r\nfilen\xC3\xA4me.pdf : 25\r\n";
        DWORD len;
        BOOL ok = ReadFile(hRead, pipeData, sizeof(pipeData), &len, NULL);
        assert(ok && len == str::Len(expected));
        pipeData[len] = '\0';
        assert(str::Eq(pipeData, expected));
        CloseHandle(hRead);
    }

    assert(str::Eq(logAll.GetData(), L"Test1\r\nTest2\r\nfilen\xE4me.pdf : 25\r\n"));
    log.RemoveLogger(&logAll);

    // don't leak the logger, don't crash on logging NULL
    log.AddLogger(new slog::DebugLogger());
    log.Log(NULL);
}
