/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include "base/Base.h"

// stubs for what SumatraPDF.cpp provides, for the tools that don't link it
void CrashHandlerSetSettings(Str) {}

void _uploadDebugReport(Str /*condStr*/, Str /*fileLine*/, bool /*isCrash*/) {
#if OS_WIN
    // outside of SumatraPDF binary, this only breaks if running under debugger
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#endif
}
