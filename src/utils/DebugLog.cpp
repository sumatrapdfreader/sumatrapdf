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
    // DbgView displays one line per OutputDebugString call
    s.Set(str::Join(s, "\n"));
    OutputDebugStringA(s.Get());
    va_end(args);
}

// TODO: as an optimization (to avoid allocations) and to prevent potential problems
// when formatting, when there are no args just output fmt
void LogF(const WCHAR *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ScopedMem<WCHAR> s(str::FmtV(fmt, args));
    // DbgView displays one line per OutputDebugString call
    s.Set(str::Join(s, L"\n"));
    OutputDebugStringW(s.Get());
    va_end(args);
}

} // namespace dbglog
