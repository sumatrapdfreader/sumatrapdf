/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/UtAssert.h"

static int g_nTotal = 0;
static int g_nFailed = 0;

#define MAX_FAILED_ASSERTS 32

struct FailedAssert {
    const char* exprStr;
    const char* file;
    int lineNo;
};

static FailedAssert g_failedAssert[MAX_FAILED_ASSERTS];

void utassert_func(bool ok, const char* exprStr, const char* file, int lineNo) {
    ++g_nTotal;
    if (ok) {
        return;
    }
    if (g_nFailed < MAX_FAILED_ASSERTS) {
        g_failedAssert[g_nFailed].exprStr = exprStr;
        g_failedAssert[g_nFailed].file = file;
        g_failedAssert[g_nFailed].lineNo = lineNo;
    }
    ++g_nFailed;
    OutputDebugStringA("Assertion failed: ");
    OutputDebugStringA(exprStr);
    OutputDebugStringA("\n");
    OutputDebugStringA(file);
    OutputDebugStringA("\n");
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
}

int utassert_print_results() {
    if (0 == g_nFailed) {
        printf("Passed all %d tests\n", g_nTotal);
        return 0;
    }

    fprintf(stderr, "Failed %d (of %d) tests\n", g_nFailed, g_nTotal);
    for (int i = 0; i < g_nFailed && i < MAX_FAILED_ASSERTS; i++) {
        FailedAssert* a = &(g_failedAssert[i]);
        fprintf(stderr, "'%s' %s@%d\n", a->exprStr, a->file, a->lineNo);
    }
    return g_nFailed;
}
