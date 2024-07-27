/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD */

#include "utils/BaseUtil.h"

void _uploadDebugReport(const char*, bool, bool) {
    // outside of SumatraPDF binary, this only breaks if running under debugger
    // for the benefit of test_util
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
}
