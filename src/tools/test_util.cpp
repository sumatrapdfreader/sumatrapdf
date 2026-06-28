#include "utils/BaseUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/DbgHelpDyn.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

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
extern void HtmlPrettyPrintTest();
extern void HtmlPullParser_UnitTests();
extern void JsonTest();
extern void RefHoverTest();
extern void SettingsUtilTest();
extern void SimpleLogTest();
extern void SquareTreeTest();
extern void StrFormatTest();
extern void StrTest();
extern void TrivialHtmlParser_UnitTests();
extern void VecTest();
extern void WinUtilTest();
extern void StrFormatTest();
extern void StrVecTest();

void GetPrintersInfo(struct StrBuilder&) {
    /* stub: do nothing */
}

void MaybeDelayedWarningNotification(Str, ...) {
    // a stub to make this compile
}

static void PrintStdout(Str s) {
    if (!s.s) {
        return;
    }
    printf("%.*s", s.len, s.s);
}

static WStr GetExeDir() {
    static WCHAR buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, dimof(buf));
    if (len == 0 || len >= dimof(buf)) {
        return {};
    }
    while (len > 0 && buf[len - 1] != L'\\' && buf[len - 1] != L'/') {
        --len;
    }
    if (len > 0) {
        --len;
    }
    buf[len] = 0;
    return WStr(buf, (int)len);
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
    StrBuilder s;
    dbghelp::GetExceptionInfo(s, exceptionInfo);
    PrintStdout(s.Get());
    fflush(stdout);
    ExitProcess(7);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv) { // str-port: C main argv
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
    HtmlPrettyPrintTest();
    HtmlPullParser_UnitTests();
    JsonTest();
    RefHoverTest();
    SettingsUtilTest();
    SimpleLogTest();
    SquareTreeTest();
    StrFormatTest();
    StrTest();
    StrVecTest();
    TrivialHtmlParser_UnitTests();
    VecTest();
    WinUtilTest();
    SumatraPDF_UnitTests();

    int res = utassert_print_results();
    DestroyTempArena();
    return res;
}
