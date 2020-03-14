/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */
#include "utils/BaseUtil.h"

// useful for 
bool gDisableDbgLog = false;

void dbglogf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AutoFree s = str::FmtV(fmt, args);
    OutputDebugStringA(s.Get());
    va_end(args);
}
