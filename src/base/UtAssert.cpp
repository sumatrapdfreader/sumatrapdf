/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#if OS_WIN
#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"
#endif
#include "base/UtAssert.h"

static int g_nTotal = 0;
static int g_nFailed = 0;
static bool gForAi = false;

constexpr int kMaxFailedAsserts = 32;

struct FailedAssert {
    Str exprStr;
    Str file;
    int lineNo;
};

static FailedAssert g_failedAssert[kMaxFailedAsserts];

void utassert_set_for_ai(bool enabled) {
    gForAi = enabled;
}

static void OutputDebugString(Str s) {
    if (str::IsNull(s)) {
        return;
    }
#if OS_WIN
    OutputDebugStringA(CStrTemp(s));
#else
    fprintf(stderr, "%.*s", s.len, s.s);
#endif
}

static void OutputDebugString(const char* s) {
    OutputDebugString(Str(s));
}

static void PrintStdout(Str s) {
    if (str::IsNull(s)) {
        return;
    }
    printf("%.*s", s.len, s.s);
}

/* This is assert for unit tests that can be used in non-interactive usage.
Instead of showing a UI to the user, like regular assert(), it simply
remembers number of failed asserts. */
void utassert_func(bool ok, Str exprStr, Str file, int lineNo) {
    ++g_nTotal;
    if (ok) {
        return;
    }
    if (g_nFailed < kMaxFailedAsserts) {
        g_failedAssert[g_nFailed].exprStr = exprStr;
        g_failedAssert[g_nFailed].file = file;
        g_failedAssert[g_nFailed].lineNo = lineNo;
    }
    ++g_nFailed;
    OutputDebugString("Assertion failed: ");
    OutputDebugString(exprStr);
    OutputDebugString("\n");
    OutputDebugString(file);
    OutputDebugString("\n");
    if (gForAi) {
        printf("Assertion failed: %.*s\n%.*s@%d\n", exprStr.len, exprStr.s, file.len, file.s, lineNo);
#if OS_WIN
        str::Builder s;
        if (dbghelp::GetCurrentThreadCallstack(s)) {
            PrintStdout(ToStr(s));
        } else {
            printf("failed to get callstack\n");
        }
#endif
        fflush(stdout);
        return;
    }
#if OS_WIN
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#endif
}

int utassert_print_results() {
    if (0 == g_nFailed) {
        printf("Passed all %d tests\n", g_nTotal);
        return 0;
    }

    fprintf(stderr, "Failed %d (of %d) tests\n", g_nFailed, g_nTotal);
    for (int i = 0; i < g_nFailed && i < kMaxFailedAsserts; i++) {
        FailedAssert* a = &(g_failedAssert[i]);
        fprintf(stderr, "'%.*s' %.*s@%d\n", a->exprStr.len, a->exprStr.s, a->file.len, a->file.s, a->lineNo);
    }
    return g_nFailed;
}
