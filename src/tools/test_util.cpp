#include "utils/BaseUtil.h"
#include "utils/WinDynCalls.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

// in src/util/tests/UtilTests.cpp
extern void BaseUtils_UnitTests();

// in src/UnitTests.cpp
extern void SumatraPDF_UnitTests();

// in src/mui/SvgPath_ut.cpp
extern void SvgPath_UnitTests();

extern void BaseUtilTest();
extern void ByteOrderTests();
extern void CmdLineParserTest();
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
// extern void VarintGobTest();
extern void VecTest();
extern void WinUtilTest();
extern void StrFormatTest();

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    printf("Running unit tests\n");
    InitDynCalls();
    BaseUtilTest();
    ByteOrderTests();
    CmdLineParserTest();
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
    // VarintGobTest();
    VecTest();
    WinUtilTest();
    SumatraPDF_UnitTests();
    SvgPath_UnitTests();
    StrFormatTest();

    int res = utassert_print_results();
    return res;
}
