/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "DebugLog.h"
#include "Scoped.h"
#include "StrUtil.h"

namespace dbglog {

// TODO: as an optimization (to avoid allocations) and to prevent potential problems
// when formatting, when there are no args just output fmt
void LogF(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ScopedMem<char> s(str::FmtV(fmt, args));
    OutputDebugStringA(s.Get());
    va_end(args);
    OutputDebugStringA("\n");
}

// TODO: as an optimization (to avoid allocations) and to prevent potential problems
// when formatting, when there are no args just output fmt
void LogF(const WCHAR *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ScopedMem<WCHAR> s(str::FmtV(fmt, args));
    OutputDebugStringW(s.Get());
    va_end(args);
    OutputDebugStringA("\n");
}

} // namespace dbglog
