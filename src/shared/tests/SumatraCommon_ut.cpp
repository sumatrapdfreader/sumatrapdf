/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/File.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "Flags.h"
#include "Commands.h"

// must be last to over-write assert()
#include "base/tests/UtAssert.h"

// App-level unit tests that run in both orig and ng. Each app's Sumatra_ut.cpp
// calls them from its driver.

void BenchRangeTest() {
    utassert(IsBenchPagesInfo(StrL("1")));
    utassert(IsBenchPagesInfo(StrL("2-4")));
    utassert(IsBenchPagesInfo(StrL("5,7")));
    utassert(IsBenchPagesInfo(StrL("6,8,")));
    utassert(IsBenchPagesInfo(StrL("1-3,4,6-9,13")));
    utassert(IsBenchPagesInfo(StrL("2-")));
    utassert(IsBenchPagesInfo(StrL("loadonly")));

    utassert(!IsBenchPagesInfo(StrL("")));
    utassert(!IsBenchPagesInfo(StrL("-2")));
    utassert(!IsBenchPagesInfo(StrL("2--4")));
    utassert(!IsBenchPagesInfo(StrL("4-2")));
    utassert(!IsBenchPagesInfo(StrL("1-3,loadonly")));
    utassert(!IsBenchPagesInfo({}));
}

void versioncheck_test() {
    utassert(IsValidProgramVersion(StrL("1")));
    utassert(IsValidProgramVersion(StrL("1.1")));
    utassert(IsValidProgramVersion(StrL("1.1.1\r\n")));
    utassert(IsValidProgramVersion(StrL("2662")));

    utassert(!IsValidProgramVersion(StrL("1.1b")));
    utassert(!IsValidProgramVersion(StrL("1..1")));
    utassert(!IsValidProgramVersion(StrL("1.1\r\n.1")));

    utassert(CompareProgramVersion(StrL("0.9.3.900"), StrL("0.9.3")) > 0);
    utassert(CompareProgramVersion(StrL("1.09.300"), StrL("1.09.3")) > 0);
    utassert(CompareProgramVersion(StrL("1.9.1"), StrL("1.09.3")) < 0);
    utassert(CompareProgramVersion(StrL("1.2.0"), StrL("1.2")) == 0);
    utassert(CompareProgramVersion(StrL("1.3.0"), StrL("2662")) < 0);
}

void hexstrTest() {
    u8 buf[6] = {1, 2, 33, 255, 0, 18};
    u8 buf2[6]{};
    TempStr s = str::MemToHexTemp(Str((const char*)buf, dimofi(buf)));
    utassert(str::Eq(s, StrL("010221ff0012")));
    bool ok = str::HexToMem(s, Str((char*)buf2, dimofi(buf2)));
    utassert(ok);
    utassert(MemEq(buf, buf2, dimofi(buf)));

    FILETIME ft1{123, 456}, ft2;
    s = str::MemToHexTemp(Str((const char*)&ft1, sizeofi(ft1)));
    str::HexToMem(s, Str((char*)&ft2, sizeofi(ft2)));
    DWORD diff = FileTimeDiffInSecs(ft1, ft2);
    utassert(0 == diff);
    utassert(FileTimeEq(ft1, ft2));

    s = str::MemToHexTemp(Str());
    utassert(str::Eq(s, StrL("")));
    ok = str::HexToMem(s, Str());
    utassert(ok);
}

static void assertSerializedColor(Color c, Str s) {
    TempStr s2 = SerializeColorTemp(c);
    utassert(str::Eq(s2, s));
}

void colorTest() {
    Color c = 0;
    bool ok = ParseColor(&c, StrL("0x01020304"));
    utassert(ok);
    assertSerializedColor(c, StrL("#01020304"));

    ok = ParseColor(&c, StrL("#01020304"));
    utassert(ok);
    assertSerializedColor(c, StrL("#01020304"));

    Color c2 = MkRgba(2, 3, 4, 1);
    assertSerializedColor(c2, StrL("#01020304"));
    utassert(c == c2);

    c2 = MkRgba(5, 7, 6, 8);
    assertSerializedColor(c2, StrL("#08050706"));
    ok = ParseColor(&c, StrL("#08050706"));
    utassert(ok);
    utassert(c == c2);
}

static void assertGoToNextPage3(int cmdId) {
    auto* cmd = FindCustomCommand(cmdId);
    utassert(cmd->origId == CmdGoToNextPage);
    auto* arg = GetCommandArg(cmd, kCmdArgN);
    utassert(arg->intVal == 3);
}

void parseCommandsTest() {
    CommandArg* arg;

    {
        // names match case-insensitively, so a re-cased name keeps old shortcuts working
        utassert(GetCommandIdByName(StrL("CmdOpenWithFoxit")) == CmdOpenWithFoxit);
        utassert(GetCommandIdByName(StrL("CmdOpenWithFoxIt")) == CmdOpenWithFoxit);
        utassert(GetCommandIdByName(StrL("cmdopenwithfoxitphantom")) == CmdOpenWithFoxitPhantom);
    }
    {
        auto* cmd = CreateCommandFromDefinition(StrL(" CmdCreateAnnotHighlight   #00ff00 openEdit copytoclipboard"));
        utassert(cmd->origId == CmdCreateAnnotHighlight);

        arg = GetCommandArg(cmd, kCmdArgColor);
        utassert(arg != nullptr);
        arg = GetCommandArg(cmd, kCmdArgOpenEdit);
        utassert(arg != nullptr);
        utassert(GetCommandBoolArg(cmd, kCmdArgOpenEdit, false) == true);
        utassert(GetCommandBoolArg(cmd, kCmdArgCopyToClipboard, false) == true);
    }
    {
        auto* cmd = CreateCommandFromDefinition(StrL(" CmdCreateAnnotHighlight   #00ff00 OpenEdit=yes"));
        utassert(cmd->origId == CmdCreateAnnotHighlight);

        utassert(GetCommandArg(cmd, kCmdArgColor) != nullptr);
        utassert(GetCommandArg(cmd, kCmdArgOpenEdit) != nullptr);
        utassert(GetCommandBoolArg(cmd, kCmdArgOpenEdit, false) == true);
    }
    {
        auto* cmd = CreateCommandFromDefinition(StrL(" CmdCreateAnnotHighlight   #00ff00 OpenEdit=no"));
        utassert(cmd->origId == CmdCreateAnnotHighlight);

        utassert(GetCommandArg(cmd, kCmdArgColor) != nullptr);
        utassert(GetCommandArg(cmd, kCmdArgOpenEdit) != nullptr);
        utassert(GetCommandBoolArg(cmd, kCmdArgOpenEdit, true) == false);
    }
    {
        auto* cmd = CreateCommandFromDefinition(StrL("CmdCreateAnnotHighlight OpenEdit=bogus"));
        utassert(cmd == nullptr);
    }
    {
        auto* cmd = CreateCommandFromDefinition(StrL("CmdCreateAnnotHighlight OpenEdit: bogus"));
        utassert(cmd == nullptr);
    }
    {
        {
            auto* cmd = CreateCommandFromDefinition(StrL("CmdGoToNextPage 3"));
            assertGoToNextPage3(cmd->id);
        }
        {
            auto* cmd = CreateCommandFromDefinition(StrL("CmdGoToNextPage n 3"));
            assertGoToNextPage3(cmd->id);
        }
        {
            auto* cmd = CreateCommandFromDefinition(StrL("CmdGoToNextPage n: 3"));
            assertGoToNextPage3(cmd->id);
        }
        {
            auto* cmd = CreateCommandFromDefinition(StrL("CmdGoToNextPage n=3"));
            assertGoToNextPage3(cmd->id);
        }
    }
    {
        Str argStr = StrL(R"("C:\Program Files\FoxitReader\FoxitReader.exe" /A page=%p "%1)");
        Str s = str::JoinTemp(StrL("CmdExec   "), argStr);
        auto* cmd = CreateCommandFromDefinition(s);
        utassert(cmd->origId == CmdExec);
        auto* cmd2 = FindCustomCommand(cmd->id);
        utassert(cmd == cmd2);
        arg = GetCommandArg(cmd, kCmdArgExe);
        utassert(str::Eq(arg->strVal, argStr));
    }
    {
        Str argStr = StrL(R"("C:\Program Files\FoxitReader\FoxitReader.exe" /A page=%p "%1)");
        Str s = str::JoinTemp(StrL("CmdExec  filter: *.jpeg "), argStr);
        auto* cmd = CreateCommandFromDefinition(s);
        utassert(cmd->origId == CmdExec);
        auto* cmd2 = FindCustomCommand(cmd->id);
        utassert(cmd == cmd2);
        arg = GetCommandArg(cmd, kCmdArgExe);
        utassert(str::Eq(arg->strVal, argStr));
        arg = GetCommandArg(cmd, kCmdArgFilter);
        utassert(str::Eq(arg->strVal, StrL("*.jpeg")));
    }
}

void DocPropertiesTest() {
    // gPropNames round-trips: first (Title=1), a middle one (FocalLength35mm=27)
    // and the last property (ImagePath=53), both directions.
    utassert(str::Eq(PropNameTemp(DocProp::Title), StrL("title")));
    utassert(str::Eq(PropNameTemp(DocProp::FocalLength35mm), StrL("focalLength35mm")));
    utassert(str::Eq(PropNameTemp(DocProp::ImagePath), StrL("imagePath")));
    utassert(PropFromName(StrL("title")) == DocProp::Title);
    utassert(PropFromName(StrL("focalLength35mm")) == DocProp::FocalLength35mm);
    utassert(PropFromName(StrL("imagePath")) == DocProp::ImagePath);
    // a couple more, plus unknown/None
    utassert(str::Eq(PropNameTemp(DocProp::CreationDate), StrL("creationDate")));
    utassert(PropFromName(StrL("modDate")) == DocProp::ModificationDate);
    utassert(PropFromName(StrL("bogusPropName")) == DocProp::None);
}
