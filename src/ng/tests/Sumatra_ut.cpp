/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's src/tests/Sumatra_ut.cpp is also the unit test driver
// (RunAppUnitTests). src/tools/test_util.cpp is the driver here, so this file
// keeps only the test bodies. ParseTip_UnitTests and SvgTextIcon_UnitTests are
// left out: they need VirtCtrl / SvgIcons (step 12).

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
#include "AIChatCommon.h"
#include "SystemFonts.h"

#if OS_LINUX || OS_DARWIN
extern "C" {
#include <mupdf/fitz.h>
#if OS_LINUX
void install_load_linux_font_funcs(fz_context* ctx);
#else
void install_load_mac_font_funcs(fz_context* ctx);
#endif
}
#endif

// must be last to over-write assert()
#include "base/tests/UtAssert.h"

void CachedObjects_UnitTests();
void CommandPaletteModel_UnitTests();
void ImageReader_UnitTests();
bool RenderCache_UnitTestCookieUnlocked();

#if OS_POSIX && !OS_WASM
static void AIChatProcessTest() {
    u8 rnd1[16];
    u8 rnd2[16];
    memset(rnd1, 0xff, sizeof(rnd1));
    memset(rnd2, 0, sizeof(rnd2));
    TempStr id1 = AIChatFormatSessionIdTemp(rnd1);
    TempStr id2 = AIChatFormatSessionIdTemp(rnd2);
    utassert(len(id1) == 36);
    utassert(id1.s[8] == '-' && id1.s[13] == '-' && id1.s[18] == '-' && id1.s[23] == '-');
    utassert(id1.s[14] == '4');
    utassert(str::Contains(StrL("89ab"), Str(id1.s + 19, 1)));
    utassert(!str::Eq(id1, id2));

    StrVec candidates;
    TempStr shell = AIChatFindExecutableTemp(candidates, StrL("sh"), StrL("sh"));
    utassert(len(shell) > 0);

    TempStr payload = StrL("portable process: \\\"ok\\\"");
    TempStr shellCmd = fmt("printf '%%s' %s", QuoteCmdLineArgTemp(payload));
    TempStr cmdLine = fmt("%s -c %s", QuoteCmdLineArgTemp(shell), QuoteCmdLineArgTemp(shellCmd));
    AIChatProcessLaunchResult launch;
    utassert(AIChatLaunchProcessWithStdoutPipe(cmdLine, {}, &launch));
    utassert(launch.ok);
    utassert(launch.processId > 0);

    str::Builder output;
    AIChatReadPipeToEnd(launch.hReadPipe, output);
    launch.hReadPipe = nullptr;
    utassert(AIChatWaitForProcess(launch.hProcess, 1000));
    AIChatCloseProcess(&launch.hProcess, false);
    utassert(str::Eq(ToStr(output), payload));

    cmdLine = fmt("%s -c %s", QuoteCmdLineArgTemp(shell), QuoteCmdLineArgTemp(StrL("sleep 1")));
    utassert(AIChatLaunchProcessWithStdoutPipe(cmdLine, {}, &launch));
    utassert(!AIChatWaitForProcess(launch.hProcess, 0));
    AIChatTerminateProcess(launch.hProcess);
    AIChatCloseProcess(&launch.hProcess, false);
}
#endif

#if OS_LINUX || OS_DARWIN
static void PlatformSystemFontTest() {
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    utassert(ctx);
#if OS_LINUX
    install_load_linux_font_funcs(ctx);
    fz_font* font = fz_load_system_font(ctx, "sans-serif", 0, 0, 0);
#else
    install_load_mac_font_funcs(ctx);
    fz_font* font = fz_load_system_font(ctx, "Helvetica", 0, 0, 0);
#endif
    utassert(font);
    fz_drop_font(ctx, font);
    font = fz_load_system_font(ctx, "SumatraMissingFont-7FA26D", 0, 0, 1);
    utassert(!font);
    fz_drop_context(ctx);
}
#endif

#if OS_WIN || OS_LINUX || defined(__APPLE__)
static void SystemFontNamesTest() {
    StrVec names;
    GetInstalledFontNames(names);
    utassert(len(names) > 0);
    for (int i = 1; i < len(names); i++) {
        utassert(!StrLessNoCase(names[i], names[i - 1]));
        utassert(!str::EqI(names[i], names[i - 1]));
    }
}
#endif

#if OS_WIN
static void ParseCommandLineTest() {
    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -install -no-desktop-shortcut", i);
        utassert(i.install);
        utassert(i.noDesktopShortcut);
        utassert(0 == len(i.fileNames));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -start-perf-log -log-perf-file c:\\tmp\\perf.txt foo.pdf", i);
        utassert(i.startPerfLog);
        utassert(str::Eq(i.perfLogFile, StrL("c:\\tmp\\perf.txt")));
        utassert(1 == len(i.fileNames));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bench foo.pdf", i);
        utassert(2 == len(i.pathsToBenchmark));
        utassert(str::Eq(StrL("foo.pdf"), i.pathsToBenchmark[0]));
        utassert(len(i.pathsToBenchmark[1]) == 0);
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bench foo.pdf -fwdsearch-width 5", i);
        utassert(len(i.globalPrefArgs) == 2);
        Str s = i.globalPrefArgs[0];
        utassert(str::Eq(s, StrL("-fwdsearch-width")));
        s = i.globalPrefArgs[1];
        utassert(str::Eq(s, StrL("5")));
        utassert(2 == len(i.pathsToBenchmark));
        utassert(str::Eq(StrL("foo.pdf"), i.pathsToBenchmark[0]));
        utassert(len(i.pathsToBenchmark[1]) == 0);
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bench bar.pdf loadonly", i);
        utassert(2 == len(i.pathsToBenchmark));
        utassert(str::Eq(StrL("bar.pdf"), i.pathsToBenchmark[0]));
        utassert(str::Eq(StrL("loadonly"), i.pathsToBenchmark[1]));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bench bar.pdf 1 -set-color-range 0x123456 #abCDef", i);
        utassert(len(i.globalPrefArgs) == 3);
        utassert(2 == len(i.pathsToBenchmark));
        utassert(str::Eq(StrL("bar.pdf"), i.pathsToBenchmark[0]));
        utassert(str::Eq(StrL("1"), i.pathsToBenchmark[1]));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bench bar.pdf 1-5,3   -bench some.pdf 1,3,8-34", i);
        utassert(4 == len(i.pathsToBenchmark));
        utassert(str::Eq(StrL("bar.pdf"), i.pathsToBenchmark[0]));
        utassert(str::Eq(StrL("1-5,3"), i.pathsToBenchmark[1]));
        utassert(str::Eq(StrL("some.pdf"), i.pathsToBenchmark[2]));
        utassert(str::Eq(StrL("1,3,8-34"), i.pathsToBenchmark[3]));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -presentation -bgcolor 0xaa0c13 foo.pdf -invert-colors bar.pdf", i);
        utassert(true == i.enterPresentation);
        utassert(true == i.invertColors);
        utassert(2 == len(i.fileNames));
        utassert(0 == i.fileNames.Find(StrL("foo.pdf")));
        utassert(1 == i.fileNames.Find(StrL("bar.pdf")));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -bg-color 0xaa0c13 -invertcolors rosanna.pdf", i);
        utassert(true == i.invertColors);
        utassert(1 == len(i.fileNames));
        utassert(0 == i.fileNames.Find(StrL("rosanna.pdf")));
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), LR"(SumatraPDF.exe "foo \" bar \\.pdf" un\"quoted.pdf)", i);
        utassert(2 == len(i.fileNames));
        utassert(0 == i.fileNames.Find(StrL(R"(foo " bar \\.pdf)")));
        utassert(1 == i.fileNames.Find(StrL(R"(un"quoted.pdf)")));
    }

    {
        Flags i;
        ParseFlags(
            GetPermArena(),
            L"SumatraPDF.exe -page 37 -view continuousfacing -zoom fitcontent -scroll 45,1234         -reuse-instance",
            i);
        utassert(0 == len(i.fileNames));
        utassert(i.pageNumber == 37);
        utassert(i.startView == DisplayMode::ContinuousFacing);
        utassert(i.startZoom == kZoomFitContent);
        utassert(i.startScroll.x == 45 && i.startScroll.y == 1234);
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), LR"(SumatraPDF.exe -view "single page" -zoom 237.45 -scroll -21,-1)", i);
        utassert(0 == len(i.fileNames));
        utassert(i.startView == DisplayMode::SinglePage);
        utassert(i.startZoom == 237.45f);
        utassert(i.startScroll.x == -21 && i.startScroll.y == -1);
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -zoom 35%", i);
        utassert(0 == len(i.fileNames));
        utassert(i.startZoom == 35.f);
    }

    {
        Flags i;
        ParseFlags(GetPermArena(), L"SumatraPDF.exe -zoom fit-content", i);
        utassert(i.startZoom == kZoomFitContent);
        utassert(0 == len(i.fileNames));
    }
}

static void ParseFileArgsTest() {
    FileArgs* fa = ParseFileArgs(StrL("C:\\foo.pdf?page=4"));
    utassert(fa && str::Eq(fa->cleanPath, StrL("C:\\foo.pdf")) && fa->pageNumber == 4);
    delete fa;
    utassert(!ParseFileArgs(StrL("C:\\foo.pdf")));
    utassert(!ParseFileArgs(StrL("\\\\?\\C:\\foo.pdf")));
    utassert(!ParseFileArgs(StrL("?:\\foo.pdf")));
}
#endif

static void BenchRangeTest() {
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

static void versioncheck_test() {
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

static void hexstrTest() {
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

static void colorTest() {
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

static void parseCommandsTest() {
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

static void DocPropertiesTest() {
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

static void PageAspectViewTest() {
    DisplayMode mode = DisplayMode::Automatic;
    float zoom = kInvalidZoom;
    utassert(GetPageAspectView(RectF(0, 0, 800, 600), &mode, &zoom));
    utassert(mode == DisplayMode::SinglePage);
    utassert(zoom == kZoomFitPage);
    utassert(GetPageAspectView(RectF(0, 0, 600, 800), &mode, &zoom));
    utassert(mode == DisplayMode::Continuous);
    utassert(zoom == kZoomFitWidth);
    utassert(!GetPageAspectView(RectF(), &mode, &zoom));
}

void SumatraPDF_UnitTests() {
    CachedObjects_UnitTests();
    CommandPaletteModel_UnitTests();
    ImageReader_UnitTests();
    utassert(RenderCache_UnitTestCookieUnlocked());
#if OS_POSIX && !OS_WASM
    AIChatProcessTest();
#endif
#if OS_LINUX || OS_DARWIN
    PlatformSystemFontTest();
#endif
#if OS_WIN || OS_LINUX || defined(__APPLE__)
    SystemFontNamesTest();
#endif
    DocPropertiesTest();
    PageAspectViewTest();
    parseCommandsTest();
    colorTest();
    BenchRangeTest();
#if OS_WIN
    ParseCommandLineTest();
    ParseFileArgsTest();
#endif
    versioncheck_test();
    hexstrTest();
}
