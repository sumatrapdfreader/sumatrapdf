/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include "base/Base.h"
#include "CrashHandlerSumatra.h"

void CrashHandlerSetSettings(Str) {}

void _uploadDebugReport(Str /*condStr*/, Str /*fileLine*/, bool /*isCrash*/, bool /*captureCallstack*/) {
#if OS_WIN
    // outside of SumatraPDF binary, this only breaks if running under debugger
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#endif
}
