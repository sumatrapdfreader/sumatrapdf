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
#include "RestrictedUseConfig.h"
#include "MainWindow.h"
#include "SumatraControlCommon.h"
#include "PdfSync.h"
#include "UpdateCheck.h"
#include "UpdateCheckCommon.h"

struct OpenDdeArgs {
    bool parsed = false;
    TempStr filePath;
    int newWindow = 0;
    int setFocus = 0;
    int forceRefresh = 0;
    int inCurrentTab = 0;
    Str next;
};
OpenDdeArgs ParseOpenDdeCmd(Str cmd);
int ApplyOpenNewWindow(int newWindow, int inCurrentTab);
int FindDebounceDelayMs(int nChars);
TempStr ForwardSearchFailTextTemp(int ret, Str fileName, int line, int page);

// must be last to over-write assert()
#include "base/tests/UtAssert.h"

// App-level unit tests that run in both orig and ng. Each app's Sumatra_ut.cpp
// calls them from its driver.

void RestrictedUseConfigTest() {
    RestrictedUseConfig cfg;
    LoadRestrictedUseConfig(cfg, {}, RestrictedUseFile::Missing);
    for (int i = 0; i < RestrictedUseFeatureCount(); i++) {
        utassert(cfg.items[i].enabled);
        utassert(!cfg.items[i].changed);
    }

    Str data = StrL("[Policies]\nInternetAccess = 1\nPrinterAccess = 1\nLinkProtocols = https\n");
    LoadRestrictedUseConfig(cfg, data, RestrictedUseFile::Exists);
    utassert(cfg.items[0].enabled);
    utassert(!cfg.items[1].enabled);
    utassert(cfg.items[4].enabled);
    utassert(str::Eq(cfg.linkProtocols, StrL("https")));

    cfg.items[1].enabled = true;
    UpdateRestrictedUseChanged(cfg);
    utassert(cfg.items[1].changed);
    utassert(RestrictedUseConfigChanged(cfg));

    TempStr saved = SerializeRestrictedUseConfigTemp(cfg);
    RestrictedUseConfig roundTrip;
    LoadRestrictedUseConfig(roundTrip, saved, RestrictedUseFile::Exists);
    for (int i = 0; i < RestrictedUseFeatureCount(); i++) {
        utassert(roundTrip.items[i].enabled == cfg.items[i].enabled);
    }
    utassert(str::Eq(roundTrip.linkProtocols, StrL("https")));
}

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

void OpenDdeCmdTest() {
    OpenDdeArgs plain = ParseOpenDdeCmd(StrL("[Open(\"docs/file.pdf\")]"));
    utassert(plain.parsed);
    utassert(str::Eq(plain.filePath, StrL("docs/file.pdf")));
    utassert(plain.newWindow == 0 && plain.setFocus == 0 && plain.forceRefresh == 0 && plain.inCurrentTab == 0);

    OpenDdeArgs three = ParseOpenDdeCmd(StrL("[Open(\"docs/file.pdf\",1,0,1)]"));
    utassert(three.parsed);
    utassert(str::Eq(three.filePath, StrL("docs/file.pdf")));
    utassert(three.newWindow == 1 && three.setFocus == 0 && three.forceRefresh == 1 && three.inCurrentTab == 0);

    OpenDdeArgs four = ParseOpenDdeCmd(StrL("[Open(\"docs/file.pdf\",1,1,0,1)]"));
    utassert(four.parsed);
    utassert(four.newWindow == 1 && four.setFocus == 1 && four.forceRefresh == 0 && four.inCurrentTab == 1);
    utassert(ApplyOpenNewWindow(four.newWindow, four.inCurrentTab) == 0);
    utassert(ApplyOpenNewWindow(0, 1) == 1);

    utassert(!ParseOpenDdeCmd(StrL("[Search(\"x\",\"y\")]")).parsed);

    utassert(FindDebounceDelayMs(0) == 1000);
    utassert(FindDebounceDelayMs(2) == 1000);
    utassert(FindDebounceDelayMs(3) == 500);

    TempStr page = ForwardSearchFailTextTemp(PDFSYNCERR_INVALID_PAGE_NUMBER, StrL("a.tex"), 12, 7);
    utassert(str::Eq(page, StrL("Page 7 does not exist")));
    TempStr around = ForwardSearchFailTextTemp(PDFSYNCERR_NORECORD_FOR_THATLINE, StrL("a.tex"), 12, 7);
    utassert(str::Eq(around, StrL("No result found around line 12 in file a.tex")));
    TempStr point = ForwardSearchFailTextTemp(PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD, StrL("a.tex"), 12, 7);
    utassert(str::Eq(point, around));
    utassert(len(ForwardSearchFailTextTemp(PDFSYNCERR_SYNCFILE_NOTFOUND, StrL("a.tex"), 12, 7)) == 0);
    utassert(len(ForwardSearchFailTextTemp(PDFSYNCERR_SUCCESS, StrL("a.tex"), 12, 7)) == 0);
}

void UpdateCheckDecisionTest() {
    UpdateCheckFacts busy;
    busy.inProgress = true;
    busy.userInitiated = true;
    busy.hasInternet = true;
    utassert(DecideUpdateCheck(busy) == UpdateCheckChoice::Skip);

    UpdateCheckFacts offline;
    offline.userInitiated = true;
    utassert(DecideUpdateCheck(offline) == UpdateCheckChoice::Skip);

    UpdateCheckFacts asked;
    asked.userInitiated = true;
    asked.hasInternet = true;
    utassert(DecideUpdateCheck(asked) == UpdateCheckChoice::Check);

    UpdateCheckFacts noPrefs;
    noPrefs.hasInternet = true;
    noPrefs.checkForUpdates = true;
    utassert(DecideUpdateCheck(noPrefs) == UpdateCheckChoice::Skip);

    UpdateCheckFacts optedOut;
    optedOut.hasInternet = true;
    optedOut.canSavePrefs = true;
    utassert(DecideUpdateCheck(optedOut) == UpdateCheckChoice::Skip);

    UpdateCheckFacts first;
    first.hasInternet = true;
    first.canSavePrefs = true;
    first.checkForUpdates = true;
    first.firstStart = true;
    utassert(DecideUpdateCheck(first) == UpdateCheckChoice::Skip);

    UpdateCheckFacts pre;
    pre.hasInternet = true;
    pre.canSavePrefs = true;
    pre.checkForUpdates = true;
    pre.isPreRelease = true;
    utassert(DecideUpdateCheck(pre) == UpdateCheckChoice::Check);

    constexpr int kSecondsInDay = 60 * 60 * 24;
    UpdateCheckFacts daily;
    daily.hasInternet = true;
    daily.canSavePrefs = true;
    daily.checkForUpdates = true;
    daily.secsSinceLast = kSecondsInDay;
    utassert(DecideUpdateCheck(daily) == UpdateCheckChoice::Skip);
    daily.secsSinceLast = kSecondsInDay + 1;
    utassert(DecideUpdateCheck(daily) == UpdateCheckChoice::Check);

    UpdateCheckFacts forced = daily;
    forced.forcePreReleaseOnly = true;
    utassert(DecideUpdateCheck(forced) == UpdateCheckChoice::Skip);
    forced.userInitiated = true;
    utassert(DecideUpdateCheck(forced) == UpdateCheckChoice::Check);

    utassert(!ShouldOfferProgramUpdate(StrL("3.8"), StrL("3.7"), true));
    utassert(ShouldOfferProgramUpdate(StrL("3.8"), StrL("3.7"), false));
    utassert(!ShouldOfferProgramUpdate(StrL("3.7"), StrL("3.7"), false));
    utassert(!ShouldOfferProgramUpdate(StrL("3.6"), StrL("3.7"), false));

    Str allowed[] = {StrL("https://www.sumatrapdfreader.org/")};
    Str url = StrL("https://www.sumatrapdfreader.org/update-check-rel.txt");
    utassert(ClassifyUpdateInfoFetch(url, 0, 200, 4, allowed, 1) == UpdateInfoFetch::Ok);
    Str evil = StrL("https://evil.example/update-check-rel.txt");
    utassert(ClassifyUpdateInfoFetch(evil, 0, 200, 4, allowed, 1) == UpdateInfoFetch::UnlistedUrl);
    utassert(ClassifyUpdateInfoFetch(url, 5, 200, 4, allowed, 1) == UpdateInfoFetch::TransportError);
    utassert(ClassifyUpdateInfoFetch(url, 0, 404, 4, allowed, 1) == UpdateInfoFetch::BadStatus);
    utassert(ClassifyUpdateInfoFetch(url, 0, 200, 0, allowed, 1) == UpdateInfoFetch::EmptyBody);

    utassert(str::Eq(InstallerUpgradeArgsTemp(true, true, StrL("C:/SumatraPDF.exe")), StrL(" -fast-install")));
    utassert(str::Eq(InstallerUpgradeArgsTemp(true, false, StrL("C:/SumatraPDF.exe")), StrL(" -install")));
    TempStr portable = InstallerUpgradeArgsTemp(false, true, StrL("C:/SumatraPDF.exe"));
    utassert(str::Eq(portable, StrL(" -sleep-ms 2000 -exit-when-done -update-self-to \"C:/SumatraPDF.exe\"")));
    utassert(str::Eq(portable, InstallerUpgradeArgsTemp(false, false, StrL("C:/SumatraPDF.exe"))));
}

void ControlFrameActionTest() {
    utassert(ControlFrameActionFor((u16)ControlFrameCmd::WaitRenderIdle) == ControlFrameAction::WaitRenderIdle);
    utassert(ControlFrameActionFor((u16)ControlFrameCmd::WaitSessionRestored) ==
             ControlFrameAction::WaitSessionRestored);
    utassert(ControlFrameActionFor((u16)ControlFrameCmd::Quit) == ControlFrameAction::RunOnUi);
    utassert(ControlFrameActionFor((u16)ControlFrameCmd::CrashMe) == ControlFrameAction::RunOnUi);
    utassert(ControlFrameActionFor(1) == ControlFrameAction::RunOnUi);
    utassert(ControlCmdEndsListener((u16)ControlFrameCmd::Quit));
    utassert(!ControlCmdEndsListener((u16)ControlFrameCmd::CrashMe));
    utassert(!ControlCmdEndsListener(11));
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

void ParseFileArgsTest() {
    FileArgs* fa = ParseFileArgs(StrL("C:\\foo.pdf?page=4"));
    utassert(fa && str::Eq(fa->cleanPath, StrL("C:\\foo.pdf")) && fa->pageNumber == 4);
    delete fa;
    utassert(!ParseFileArgs(StrL("C:\\foo.pdf")));
    utassert(!ParseFileArgs(StrL("\\\\?\\C:\\foo.pdf")));
    // a garbled drive letter: no file before the '?'
    utassert(!ParseFileArgs(StrL("?:\\foo.pdf")));
}
