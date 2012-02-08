/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "DebugLog.h"

namespace dbglog {

void lf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ScopedMem<char> s(str::FmtV(fmt, args));
    OutputDebugStringA(s.Get());
    va_end(args);
    OutputDebugStringA("\n");
}

void lf(const WCHAR *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ScopedMem<WCHAR> s(str::FmtV(fmt, args));
    OutputDebugStringW(s.Get());
    va_end(args);
    OutputDebugStringA("\n");
}

} // namespace dbglog
