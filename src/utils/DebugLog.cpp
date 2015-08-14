/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "DebugLog.h"

namespace dbglog {

  // TODO: as an optimization (to avoid allocations) and to prevent potential problems
  // when formatting, when there are no args just output fmt
void LogFV(const char *fmt, va_list args)
{
  char buf[512] = { 0 };
  str::BufFmtV(buf, dimof(buf), fmt, args);
  str::BufAppend(buf, dimof(buf), "\n");
  OutputDebugStringA(buf);
}

void LogF(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogFV(fmt, args);
    va_end(args);
}

// TODO: as an optimization (to avoid allocations) and to prevent potential problems
// when formatting, when there are no args just output fmt
void LogFV(const WCHAR *fmt, va_list args)
{
    WCHAR buf[256] = { 0 };
    str::BufFmtV(buf, dimof(buf), fmt, args);
    str::BufAppend(buf, dimof(buf), L"\n");
    OutputDebugStringW(buf);
}

void LogF(const WCHAR *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogFV(fmt, args);
    va_end(args);
}

static str::Str<char> *gCrashLog = nullptr;

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
        return nullptr;
    return gCrashLog->LendData();
}

} // namespace dbglog
