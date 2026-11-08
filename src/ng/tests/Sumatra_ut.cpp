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
#include "AIChatTypes.h"
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

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/ucdn.h>
}
#include "mupdf/noto_sumatra.h"
#include "EmbeddedResources.h"

// must be last to over-write assert()
#include "base/tests/UtAssert.h"

// in src/shared/tests/SumatraCommon_ut.cpp
void BenchRangeTest();
void versioncheck_test();
void hexstrTest();
void colorTest();
void PngSizeTest();
void parseCommandsTest();
void DocPropertiesTest();
void ParseFileArgsTest();
void ParseCommandLineTest();
void HexViewTest();
void RestrictedUseConfigTest();
void ControlFrameActionTest();
void OpenDdeCmdTest();
void UpdateCheckDecisionTest();

void CachedObjects_UnitTests();
void CommandPaletteModel_UnitTests();
void ImageReader_UnitTests();
bool RenderCache_UnitTestCookieUnlocked();
bool RenderCache_UnitTestFullOfOneVisiblePage();
bool RenderCache_UnitTestFullOfOldZoom();

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

#if OS_DARWIN
// Body text in a markdown file is CSS sans-serif, which loads a system face.
static void OpenMarkdownWithSystemFonts(fz_context* ctx) {
    TempStr bare = GetTempFilePathTemp(StrL("sumd"));
    utassert(bare.s);
    TempStr mdPath = fmt("%s.md", bare);
    utassert(file::WriteFile(mdPath, StrL("# Hello\n\nplain **bold** and `code`.\n")));

    fz_document* doc = nullptr;
    int nPages = 0;
    int failed = 0;
    fz_var(doc);
    fz_var(nPages);
    fz_var(failed);
    fz_try(ctx) {
        fz_register_document_handlers(ctx);
        doc = fz_open_document(ctx, CStrTemp(mdPath));
        fz_layout_document(ctx, doc, 450, 600, 12);
        nPages = fz_count_pages(ctx, doc);
    }
    fz_always(ctx) {
        fz_drop_document(ctx, doc);
    }
    fz_catch(ctx) {
        failed = 1;
    }
    file::Delete(mdPath);
    file::Delete(bare);
    utassert(!failed);
    utassert(nPages > 0);
}
#endif

// which font file the built-in font table asks the loader for
static const char* gAskedFont = nullptr;
static bool gHaveSourceHan = true;

static const unsigned char* StubFontLoader(const char* fileName, int* size) {
    static const unsigned char kData[] = {0};
    if (!gHaveSourceHan && str::StartsWith(Str(fileName), StrL("SourceHan"))) {
        *size = 0;
        return nullptr;
    }
    gAskedFont = fileName;
    *size = (int)sizeof(kData);
    return kData;
}

static bool AskedFor(fz_context* ctx, int script, int lang, Str fileName, int subfont) {
    int size = 0;
    int sub = -1;
    gAskedFont = nullptr;
    const unsigned char* data = fz_lookup_noto_font(ctx, script, lang, &size, &sub);
    return data && gAskedFont && str::Eq(Str(gAskedFont), fileName) && sub == subfont;
}

// CJK text tagged with a language gets that language's Source Han Serif face;
// untagged text, or a failed download, gets Droid Sans Fallback
static void BuiltinFontTableTest() {
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    utassert(ctx);
    fz_set_builtin_font_loader(StubFontLoader);

    Str han = StrL("SourceHanSerif-Regular.ttc");
    Str droid = StrL("DroidSansFallbackFull.ttf");
    utassert(AskedFor(ctx, UCDN_SCRIPT_HAN, FZ_LANG_ja, han, 0));
    utassert(AskedFor(ctx, UCDN_SCRIPT_HIRAGANA, FZ_LANG_ja, han, 0));
    utassert(AskedFor(ctx, UCDN_SCRIPT_HAN, FZ_LANG_ko, han, 1));
    utassert(AskedFor(ctx, UCDN_SCRIPT_HANGUL, FZ_LANG_ko, han, 1));
    utassert(AskedFor(ctx, UCDN_SCRIPT_HAN, FZ_LANG_zh_Hans, han, 2));
    utassert(AskedFor(ctx, UCDN_SCRIPT_HAN, FZ_LANG_zh_Hant, han, 3));
    utassert(AskedFor(ctx, UCDN_SCRIPT_HAN, FZ_LANG_UNSET, droid, 0));
    utassert(AskedFor(ctx, UCDN_SCRIPT_HANGUL, FZ_LANG_UNSET, droid, 0));

    // fonts mupdf leaves out of its table by default for size
    utassert(AskedFor(ctx, UCDN_SCRIPT_TANGUT, FZ_LANG_UNSET, StrL("NotoSerifTangut-Regular.otf"), 0));
    utassert(AskedFor(ctx, UCDN_SCRIPT_SIGNWRITING, FZ_LANG_UNSET, StrL("NotoSansSignWriting-Regular.otf"), 0));

    gHaveSourceHan = false;
    utassert(AskedFor(ctx, UCDN_SCRIPT_HAN, FZ_LANG_ja, droid, 0));
    gHaveSourceHan = true;

    InstallEmbeddedFontLoader();
    fz_drop_context(ctx);
}

// a downloaded font must match the sha1 prefix in its file name
static void FontMatchesFileNameTest() {
    Str abc = StrL("abc"); // sha1 a9993e364706816aba3e25717850c26c9cd0d89d
    utassert(FontMatchesFileName(StrL("Font-a999.ttf"), abc));
    utassert(FontMatchesFileName(StrL("Font-Regular-A9993E36.otf"), abc));
    utassert(!FontMatchesFileName(StrL("Font-a998.ttf"), abc));
    utassert(!FontMatchesFileName(StrL("Font-Regular-a9993e.cff"), StrL("<html>")));
    // nothing to check against
    utassert(FontMatchesFileName(StrL("Font-Regular.otf"), abc));
    utassert(FontMatchesFileName(StrL("Font.ttf"), abc));
}

#if OS_LINUX || OS_DARWIN
static void PlatformSystemFontTest() {
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    utassert(ctx);
#if OS_LINUX
    install_load_linux_font_funcs(ctx);
    fz_font* font = fz_load_system_font(ctx, "sans-serif", 0, 0, 0);
    utassert(font);
    fz_drop_font(ctx, font);
#else
    install_load_mac_font_funcs(ctx);
    fz_font* font = fz_load_system_font(ctx, "Helvetica", 0, 0, 0);
    utassert(font);
    utassert(fz_encode_character(ctx, font, 'A') > 0);
    fz_drop_font(ctx, font);
    font = fz_load_system_font(ctx, "sans-serif", 1, 1, 0);
    utassert(font);
    fz_drop_font(ctx, font);
    font = fz_load_system_font(ctx, "serif", 0, 1, 0);
    utassert(font);
    fz_drop_font(ctx, font);
    font = fz_load_system_font(ctx, "monospace", 1, 0, 0);
    utassert(font);
    fz_drop_font(ctx, font);
    OpenMarkdownWithSystemFonts(ctx);
#endif
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
    RestrictedUseConfigTest();
    ControlFrameActionTest();
    OpenDdeCmdTest();
    UpdateCheckDecisionTest();
    CachedObjects_UnitTests();
    CommandPaletteModel_UnitTests();
    ImageReader_UnitTests();
    utassert(RenderCache_UnitTestCookieUnlocked());
    utassert(RenderCache_UnitTestFullOfOneVisiblePage());
    utassert(RenderCache_UnitTestFullOfOldZoom());
#if OS_POSIX && !OS_WASM
    AIChatProcessTest();
#endif
    BuiltinFontTableTest();
    FontMatchesFileNameTest();
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
    PngSizeTest();
    BenchRangeTest();
    HexViewTest();
#if OS_WIN
    ParseCommandLineTest();
    ParseFileArgsTest();
#endif
    versioncheck_test();
    hexstrTest();
}
