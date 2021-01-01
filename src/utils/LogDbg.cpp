/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */
#include "utils/BaseUtil.h"

// is set to true in debug build in SumatraStartup.cpp
// not in release builds as it spams the debug output
bool gEnableDbgLog = false;

void dbglog(const char* msg) {
    OutputDebugStringA(msg);
}

void dbglogf(const char* fmt, ...) {
    if (!gEnableDbgLog) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    AutoFree s = str::FmtV(fmt, args);
    OutputDebugStringA(s.Get());
    va_end(args);
}

void dbglog(const WCHAR* msg) {
    OutputDebugStringW(msg);
}

void dbglogf(const WCHAR* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AutoFreeWstr s = str::FmtV(fmt, args);
    OutputDebugStringW(s);
    va_end(args);
}
