#include "BaseUtil.h"

// must be last due to assert() over-write
#include "UtAssert.h"

// in src/util/tests/UtilTests.cpp
extern void BaseUtils_UnitTests();

// in src/UnitTests.cpp
extern void SumatraPDF_UnitTests();

// in src/mui/SvgPath_ut.cpp
extern void SvgPath_UnitTests();

extern void BaseUtilTest();
extern void BencTest();
extern void ByteOrderTests();
extern void CmdLineParserTest();
extern void CssParser_UnitTests();
extern void DictTest();
extern void FileUtilTest();
extern void HtmlPullParser_UnitTests();
extern void JsonTest();
extern void SettingsUtilTest();
extern void SigSlotTest();
extern void SimpleLogTest();
extern void SquareTreeTest();
extern void StrFormatTest();
extern void StrTest();
extern void TrivialHtmlParser_UnitTests();
extern void VarintGobTest();
extern void VecTest();
extern void WinUtilTest();

int main(int argc, char **argv)
{
    printf("Running unit tests\n");
    BaseUtilTest();
    BencTest();
    ByteOrderTests();
    CmdLineParserTest();
    CssParser_UnitTests();
    DictTest();
    FileUtilTest();
    HtmlPullParser_UnitTests();
    JsonTest();
    SettingsUtilTest();
    SigSlotTest();
    SimpleLogTest();
    SquareTreeTest();
    StrFormatTest();
    StrTest();
    TrivialHtmlParser_UnitTests();
    VarintGobTest();
    VecTest();
    WinUtilTest();
    SumatraPDF_UnitTests();
    SvgPath_UnitTests();
    int res = utassert_print_results();
    return res;
}
