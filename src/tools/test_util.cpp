#include "utils/BaseUtil.h"
#include "utils/WinDynCalls.h"

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
extern void SettingsUtilTest();
extern void SimpleLogTest();
extern void SquareTreeTest();
extern void StrFormatTest();
extern void StrTest();
extern void TrivialHtmlParser_UnitTests();
extern void VecTest();
extern void WinUtilTest();
extern void StrFormatTest();

void _submitDebugReportIfFunc(__unused bool cond, __unused const char* condStr) {
    // no-op implementation to satisfy SubmitBugReport()
}

int main(__unused int argc, __unused char** argv) {
    printf("Running unit tests\n");

    InitDynCalls();
    BaseUtilTest();
    ByteOrderTests();
    CryptoUtilTest();
    CssParser_UnitTests();
    DictTest();
    FileUtilTest();
    HtmlPrettyPrintTest();
    HtmlPullParser_UnitTests();
    JsonTest();
    SettingsUtilTest();
    SimpleLogTest();
    SquareTreeTest();
    StrTest();
    TrivialHtmlParser_UnitTests();
    VecTest();
    WinUtilTest();
    SumatraPDF_UnitTests();
    StrFormatTest();

    int res = utassert_print_results();
    DestroyTempAllocator();
    return res;
}
