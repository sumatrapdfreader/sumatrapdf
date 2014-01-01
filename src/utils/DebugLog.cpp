/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "DebugLog.h"

namespace dbglog {

// TODO: as an optimization (to avoid allocations) and to prevent potential problems
// when formatting, when there are no args just output fmt
void LogF(const char *fmt, ...)
{
    char buf[512] = { 0 };
    va_list args;
    va_start(args, fmt);
    str::BufFmtV(buf, dimof(buf), fmt, args);
    str::BufAppend(buf, dimof(buf), "\n");
    OutputDebugStringA(buf);
    va_end(args);
}

// TODO: as an optimization (to avoid allocations) and to prevent potential problems
// when formatting, when there are no args just output fmt
void LogF(const WCHAR *fmt, ...)
{
    WCHAR buf[256] = { 0 };
    va_list args;
    va_start(args, fmt);
    str::BufFmtV(buf, dimof(buf), fmt, args);
    str::BufAppend(buf, dimof(buf), L"\n");
    OutputDebugStringW(buf);
    va_end(args);
}

static str::Str<char> *gCrashLog = NULL;

void CrashLogF(const char *fmt, ...)
{
    if (!gCrashLog) {
        // this is never freed, so only call CrashLogF before a crash
        gCrashLog = new str::Str<char>(4096);
    }

    va_list args;
    va_start(args, fmt);
    gCrashLog->AppendAndFree(str::FmtV(fmt, args));
    gCrashLog->Append("\r\n");
    va_end(args);
}

const char *GetCrashLog()
{
    if (!gCrashLog)
        return NULL;
    return gCrashLog->LendData();
}

} // namespace dbglog
