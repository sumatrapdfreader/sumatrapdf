/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

// TODO: merge this into tools/tests/UnitMain.cpp?

extern void BaseUtilTest();
extern void BencTest();
extern void ByteOrderTests();
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

void BaseUtils_UnitTests()
{
    printf("Running BaseUtils unit tests\n");
    BaseUtilTest();
    BencTest();
    ByteOrderTests();
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
}
