#include "base/Base.h"
#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

// in src/util/tests/UtilTests.cpp
extern void BaseUtils_UnitTests();

// in src/UnitTests.cpp
extern void SumatraPDF_UnitTests();

extern void BaseUtilTest();
extern void ByteOrderTests();
extern void CryptoUtilTest();
extern void CssParser_UnitTests();
extern void DictTest();
extern void FileUtilTest();
extern void JsonTest();
extern void RefHoverTest();
extern void SettingsUtilTest();
extern void SimpleLogTest();
extern void SquareTreeTest();
extern void StrFormatTest();
extern void StrTest();
extern void VecTest();
extern void WinUtilTest();
extern void StrVecTest();

void GetPrintersInfo(struct str::Builder&) {
    /* stub: do nothing */
}

void MaybeDelayedWarningNotification(Str) {
    // a stub to make this compile
}

static void PrintStdout(Str s) {
    if (str::IsNull(s)) {
        return;
    }
    printf("%.*s", s.len, s.s);
}

static WStr GetExeDir() {
    static WCHAR buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, dimof(buf));
    if (n == 0 || n >= dimof(buf)) {
        return {};
    }
    while (n > 0 && buf[n - 1] != L'\\' && buf[n - 1] != L'/') {
        --n;
    }
    if (n > 0) {
        --n;
    }
    buf[n] = 0;
    return WStr(buf, (int)n);
}

static bool InitSymbolsForAi() {
    WStr exeDir = GetExeDir();
    if (!dbghelp::Initialize(exeDir, true)) {
        printf("failed to initialize dbghelp symbols\n");
        return false;
    }
    return true;
}

static LONG WINAPI ForAiCrashHandler(EXCEPTION_POINTERS* exceptionInfo) {
    printf("test_util crash\n");
    str::Builder s;
    dbghelp::GetExceptionInfo(s, exceptionInfo);
    PrintStdout(ToStr(s));
    fflush(stdout);
    ExitProcess(7);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv) {
    bool forAi = false;
    for (int i = 1; i < argc; i++) {
        if (str::Eq(argv[i], "-for-ai")) {
            forAi = true;
        }
    }
    if (forAi) {
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
    }
    printf("Running unit tests\n");

    InitDynCalls();
    if (forAi) {
        utassert_set_for_ai(true);
        InitSymbolsForAi();
        SetUnhandledExceptionFilter(ForAiCrashHandler);
    }
    BaseUtilTest();
    ByteOrderTests();
    CryptoUtilTest();
    CssParser_UnitTests();
    DictTest();
    FileUtilTest();
    JsonTest();
    RefHoverTest();
    SettingsUtilTest();
    SimpleLogTest();
    SquareTreeTest();
    StrFormatTest();
    StrTest();
    StrVecTest();
    VecTest();
    WinUtilTest();
    SumatraPDF_UnitTests();

    int res = utassert_print_results();
    DestroyTempArena();
    return res;
}
