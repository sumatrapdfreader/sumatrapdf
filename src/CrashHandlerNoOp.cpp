/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include "base/Base.h"

void _uploadDebugReport(Str, Str, bool, bool) {
#if OS_WIN
    // outside of SumatraPDF binary, this only breaks if running under debugger
    // for the benefit of test_util
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#endif
}
