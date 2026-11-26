/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Http.h"

#if IS_DEBUG

#include <ktmw32.h>

#include "base/AutoWin.h"

#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/GuessFileType.h"
#include "base/SettingsUtil.h"
#include "base/Win.h"
#include "base/CrashHandler.h"
#include "base/Zip.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "AppTools.h"
#include "Version.h"
#include "Installer.h"
#include "Settings.h"
#include "DocController.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "AppSettings.h"
#include "AIChatTypes.h"
#include "AIChatCommon.h"
#include "WindowTab.h"
#include "Flags.h"
#include "Commands.h"
#include "Annotation.h"
#include "CommandAvailability.h"
#include "CommandPalette.h"
#include "CommandPaletteCommon.h"
#include "SvgIcons.h"
#include "StripGroups.h"
#include "AppUnitTests.h"
#include "Theme.h"
#include "PDFScript.h"
#include "RegistryPreview.h"
#include "MainWindow.h"
#include "HomePage.h"
#include "HomePageCommon.h"
#include "UpdateCheck.h"
#include "UpdateCheckCommon.h"
#include "shared/PdfToolsCommon.h"

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
void JsMenuTest();
void HtmlImageCacheTest();
void HtmlFormatterNarrowPageTest();
void PrintAdvancedDefaultsTest();
void ParseFileArgsTest();
void ParseCommandLineTest();
void RestrictedUseConfigTest();
void CrashReportSettingsTest();
void ControlFrameActionTest();
void HexViewTest();
void SumatraPDFWebTest();
void HttpTransferTest();
void OpenDdeCmdTest();
void UpdateCheckDecisionTest();

// in src/base/tests/
void ArchiveTest();
void ZipDirTest();
void ArchiveWarnTest();
void ArchiveTarLazyTest();
void ArchiveRarHugeEntryTest();
void ArchiveTarHugeEntryTest();
void BaseUtilTest();
void ByteOrderTests();
void ClipboardImageTest();
void CryptoUtilTest();
void CssParser_UnitTests();
void DictTest();
void DirRemoveAllTest();
void UncExistsTest();
void NonVirtualPathTest();
void FileWatcherOverflowTest();
void FileUtilTest();
void UpdateTempFileTests();
void ExifTest();
void GuessFileTypeTest();
void JsonTest();
void RefHoverTest();
void SettingsUtilTest();
void SquareTreeTest();
void StrFormatTest();
void StrTest();
void StrVecTest();
void VecTest();
void WinUtilTest();

// in src/tests/*_ut.cpp
void ChapterTable_UnitTests();
void LitDoc_UnitTests();
void RtfDoc_UnitTests();
void MobiDoc_UnitTests();
void Ofd_UnitTests();
void PagePosition_UnitTests();
void PdfSync_UnitTests();
void CachedObjects_UnitTests();
bool PdfDarkModeImageStats_UnitTest();

void CommandPaletteModel_UnitTests();
void TextSelection_UnitTests();
void EngineDjvuDec_UnitTests();
void Layout_UnitTests();
void VirtCtrl_UnitTests();
bool PlatformTextCache_UnitTest();
bool TableOfContents_UnitTestSnapshotNamedDest();
bool Translations_UnitTestUnescape();
bool Translations_UnitTestLocale();
bool Translations_UnitTestLookup();
bool Eutl_UnitTestListedCache();
bool MarkdownModel_UnitTestBrowserNavigationUrl();
bool MarkdownToc_UnitTestHtmlLinks();
bool MarkdownToc_UnitTestHtmlHeadings();
bool MarkdownToc_UnitTestMermaid();
bool MarkdownToc_UnitTestHighlightWikilinks();
bool MarkdownToc_TestLineBreaks();
bool EbookDoc_UnitTestNormalizeURL();
bool EbookDoc_UnitTestLoading();
bool EbookDoc_UnitTestLinearNo();
bool EbookDoc_UnitTestUtf16BeTwice();
bool GumboHtmlParser_UnitTest();
bool DisplayMode_UnitTestZoom();
bool ExternalViewers_UnitTestPDFXChangePaths();
bool ExternalViewers_UnitTestExtInList();
bool Canvas_UnitTestScrollLineAmount();
bool EngineMupdf_UnitTestEbookLineSpacingCss();
bool EngineMupdf_UnitTestFormatUnixTime();
bool EngineMupdf_UnitTestEbookFontFamilyCss();
bool EngineMupdf_UnitTestEbookMarginCss();
bool EngineMupdf_UnitTestMergeEBookUI();
bool EngineMupdf_UnitTestPageLabels();
bool EngineBase_UnitTestTextOutlivesInvalidate();
bool EngineMupdf_UnitTestDoi();
bool Accelerators_UnitTestFolderNavIsSafe();
bool Accelerators_UnitTestTreeTakesLetters();
bool Accelerators_UnitTestCreateAnnotEdit();
bool Accelerators_UnitTestCustomShortcutShown();
bool AnnotSearch_UnitTests();
void ReadAloudHighlight_UnitTests();
bool RenderCache_UnitTestCookieUnlocked();
bool RenderCache_UnitTestFullOfOneVisiblePage();
bool RenderCache_UnitTestFullOfOldZoom();
bool RenderCache_UnitTestReduceTileSize();
bool DisplayModel_UnitTestZoomFallback();
bool EngineEbook_UnitTestUrlLink();
bool DisplayModel_UnitTestFreePanFirstPage();
bool DocController_UnitTestDestroyHook();
bool Commands_UnitTestIdLimit();
bool PathFromFileUriTemp(Str uri, TempStr* pathOut, Str* fragmentOut);

// promos from the update check arrive after the home page parsed its tips
static void ServerPromoTest() {
    EnsureTipsParsed();
    int n = len(gPromoLines);
    Str d = StrL(
        "[SumatraPDF]\nLatest: 3.6\n[Promo]\n"
        "[\n  Name = Foo\n  URL = https://foo.example\n  Info = Bar\n]\n"
        "[\n  Name = Edna\n  URL = https://edna.arslexis.io\n  Info = Notes\n]\n");
    delete ParseUpdateInfo(d);
    utassert(len(gPromoLines) == n + 1 && str::Eq(gPromoLines[n], StrL("Try [Foo](https://foo.example): Bar")));
    FreeHomePageTips();
}

static void FileUriTest() {
    struct {
        Str uri;
        Str path;
    } cases[] = {
        {StrL("file:///C:/a%20b/x.pdf"), StrL("C:\\a b\\x.pdf")},
        {StrL("file://server/share/x.pdf"), StrL("\\\\server\\share\\x.pdf")},
        {StrL("file://localhost/C:/x.pdf"), StrL("C:\\x.pdf")},
        {StrL("file:C:/x.pdf"), StrL("C:\\x.pdf")},
    };
    for (auto& c : cases) {
        TempStr path;
        utassert(PathFromFileUriTemp(c.uri, &path, nullptr));
        utassert(str::Eq(path, c.path));
    }
    TempStr path;
    Str frag;
    utassert(PathFromFileUriTemp(StrL("file:///C:/x.pdf#page=2"), &path, &frag));
    utassert(str::Eq(path, StrL("C:\\x.pdf")) && str::Eq(frag, StrL("page=2")));
    utassert(!PathFromFileUriTemp(StrL("https://x.com/a.pdf"), &path, nullptr));
}

// every SidebarView the app saves must survive loading; the settings enum used
// to drop signatures / layers, which came back as bookmarks
static void SidebarViewSettingTest() {
    for (int i = 0; i < kSidebarViewCount; i++) {
        SidebarView v = (SidebarView)i;
        TempStr data =
            fmt("FileStates [\n\t[\n\t\tFilePath = C:\\a.pdf\n\t\tSidebarView = %s\n\t]\n]\n", SidebarViewToStr(v));
        Settings* settings = NewSettings(data);
        utassert(settings && settings->fileStates && len(*settings->fileStates) == 1);
        Str got = (*settings->fileStates)[0]->sidebarView;
        utassert(SidebarViewFromStr(got, SidebarView::Bookmarks) == v);
        DeleteSettings(settings);
    }
}

// a color setting's row gets a swatch of its value, or of the value being typed
static void PaletteRowColorTest() {
    Settings* prevSettings = gSettings;
    if (!gSettings) {
        gSettings = NewSettings({});
    }

    StrVecCP rows;
    CollectSettingRows(rows);
    // an unset (empty) color has no swatch
    int nSwatches = 0;
    Color col;
    for (int i = 0; i < len(rows); i++) {
        ItemDataCP* d = rows.AtData(i);
        if (!PaletteRowColor(d, rows[i], &col)) {
            continue;
        }
        utassert(d->settingType == SettingType::Color);
        nSwatches++;
    }
    utassert(nSwatches > 0);

    ItemDataCP typed;
    typed.settingType = SettingType::Color;
    typed.settingPath = StrL("FixedPageUI.TextColor");
    utassert(PaletteRowColor(&typed, StrL("#ff0000"), &col));
    utassert(col == kColRed);
    utassert(!PaletteRowColor(&typed, StrL("not a color"), &col));

    if (!prevSettings) {
        DeleteSettings(gSettings);
        gSettings = nullptr;
    }
}

// a disabled ghost button (Merge PDF's Remove) must look different from an
// enabled one in every built-in theme (#6478)
static void GhostButtonDisabledTest() {
    Settings* prevSettings = gSettings;
    if (!gSettings) {
        gSettings = NewSettings({});
    }
    CreateThemeCommands();

    int nBuiltIn = ThemeGetCount() - len(*gSettings->themes);
    for (int i = 0; i < nBuiltIn; i++) {
        SetThemeByIndex(i);
        VirtButton b(StrL("Remove"), nullptr);
        SetVirtButtonStyle(&b, VirtButtonStyle::Ghost);
        Color bg = b.GetColor(kColBtnBg);
        Color enabled = b.TextColor(bg);
        b.SetFlag(vwfEnabled, false);
        Color disabled = b.TextColor(bg);
        // Solarized Dark's own text and disabled colors are only ~24 apart
        float delta = GetLightness(enabled) - GetLightness(disabled);
        utassert(delta > 20 || delta < -20);

        // secondary text (dialog labels, hints) and disabled text stay readable;
        // neither has to beat the theme's own text (Solarized Dark's is 4.1)
        Color ctlBg = ThemeWindowControlBackgroundColor();
        float textRatio = ContrastRatio(ThemeWindowTextColor(), ctlBg);
        utassert(ContrastRatio(ThemeWindowDarkerTextColor(), ctlBg) >= std::min(4.5f, textRatio) - 0.05f);
        utassert(ContrastRatio(ThemeWindowTextDisabledColor(), ctlBg) >= std::min(3.f, textRatio) - 0.05f);
    }
    utassert(ContrastRatio(kColBlack, kColWhite) > 20.9f);

    SetThemeByIndex(0);
    if (!prevSettings) {
        FreeThemes();
        DeleteSettings(gSettings);
        gSettings = nullptr;
    }
}

static void ExpandPDFScriptTest() {
    StrVec none;
    utassert(str::Eq(ExpandPDFScript(StrL("print(1);\n"), none), StrL("print(1);\n")));

    StrVec inputOnly;
    inputOnly.Append(StrL("C:\\docs\\a \"b\".pdf"));
    utassert(str::Eq(ExpandPDFScript(StrL("print(1);\n"), inputOnly), StrL("print(1);\n")));
    utassert(str::Eq(ExpandPDFScript(StrL("\"<INPUT>\" \"<OUTPUT>\""), inputOnly),
                     StrL("\"C:\\\\docs\\\\a \\\"b\\\".pdf\" \"\"")));

    StrVec emptyOut;
    emptyOut.Append(StrL("in.pdf"));
    emptyOut.Append(Str());
    utassert(str::Eq(ExpandPDFScript(StrL("print(\"ok\");\n"), emptyOut), StrL("print(\"ok\");\n")));

    StrVec both;
    both.Append(StrL("in.pdf"));
    both.Append(StrL("out.pdf"));
    utassert(str::Eq(ExpandPDFScript(StrL("\"<INPUT>\" \"<OUTPUT>\""), both), StrL("\"in.pdf\" \"out.pdf\"")));
}

static void PDFScriptArgHintTest() {
    Str script = StrL(
        "// Arg: <DIR>/<BASE>-[page].png : Output filename template (folder must exist)\n"
        "// Arg: <DIR>/<BASE>-[page].png : Image filename template (.png or .jpg; folder must exist)\n"
        "// Arg: Page %page / %lastpage : Footer text (empty = none)\n"
        "// Arg: x : Keep (this) word\n");
    StrVec values;
    StrVec hints;
    GetPDFScriptArgs(script, StrL("C:\\docs\\a.pdf"), values, hints);
    utassert(len(hints) == 4);
    utassert(str::Eq(hints[0], StrL("Output filename template")));
    utassert(str::Eq(hints[1], StrL("Image filename template")));
    utassert(str::Eq(hints[2], StrL("Footer text")));
    utassert(str::Eq(hints[3], StrL("Keep (this) word")));
    utassert(str::EndsWith(values[0], StrL("a-[page].png")));
}

#define kUtThumbnailIid "{e357fccd-a995-4576-b01f-234630154e96}"
#define kUtPreviewIid "{8895b1c6-b41f-4c1c-a562-0d564250836f}"
#define kUtForeignClsid "{11111111-2222-3333-4444-555555555555}"
#define kUtStaleClsid "{66666666-7777-8888-9999-000000000000}"

static TempStr UtShellExTemp(Str ext, Str iid) {
    return ReadRegStrTemp(HKEY_CURRENT_USER, fmt("Software\\Classes\\%s\\shellex\\%s", ext, iid), {});
}

// PdfPreview.dll (un)registration leaves other apps' handlers alone (#6442).
// HKCR, HKCU and HKLM are redirected to scratch keys; HKCR doesn't merge the
// HKCU one, so a handler Explorer would see is written to both.
static void PreviewHandlersTest() {
    Str root = StrL("Software\\SumatraPDF-unit-tests\\preview-handlers");
    DeleteRegKey(HKEY_CURRENT_USER, root);
    HKEY predefs[3] = {HKEY_CLASSES_ROOT, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};
    Str names[3] = {StrL("cr"), StrL("cu"), StrL("lm")};
    HKEY keys[3]{};
    for (int i = 0; i < 3; i++) {
        WCHAR* path = CWStrTemp(fmt("%s\\%s", root, names[i]));
        LSTATUS st =
            RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &keys[i], nullptr);
        utassert(st == ERROR_SUCCESS);
    }
    bool overridden = true;
    for (int i = 0; i < 3; i++) {
        overridden &= RegOverridePredefKey(predefs[i], keys[i]) == ERROR_SUCCESS;
    }
    utassert(overridden);
    if (!overridden) {
        goto Exit;
    }

    {
        // another app's thumbnailer for .epub, a stale one for .mobi
        Str epubThumb = StrL(".epub\\shellex\\" kUtThumbnailIid);
        WriteRegStr(HKEY_CLASSES_ROOT, epubThumb, {}, StrL(kUtForeignClsid));
        CreateRegKey(HKEY_CLASSES_ROOT, StrL("CLSID\\" kUtForeignClsid "\\InProcServer32"));
        WriteRegStr(HKEY_CURRENT_USER, fmt("Software\\Classes\\%s", epubThumb), {}, StrL(kUtForeignClsid));
        WriteRegStr(HKEY_CLASSES_ROOT, StrL(".mobi\\shellex\\" kUtThumbnailIid), {}, StrL(kUtStaleClsid));

        PreviewHandlers foreign = GetForeignPreviewHandlers();
        utassert(foreign.preview == 0);
        utassert(foreign.thumbnails != 0 && (foreign.thumbnails & (foreign.thumbnails - 1)) == 0);

        PreviewHandlers which{kAllPreviewExts, kAllPreviewExts & ~foreign.thumbnails};
        utassert(InstallPreviewDll(StrL("C:\\SumatraPDF\\PdfPreview.dll"), false, which));
        utassert(str::Eq(UtShellExTemp(StrL(".epub"), StrL(kUtThumbnailIid)), StrL(kUtForeignClsid)));
        utassert(str::Eq(UtShellExTemp(StrL(".epub"), StrL(kUtPreviewIid)), StrL(kEpubPreviewClsid)));
        utassert(str::Eq(UtShellExTemp(StrL(".pdf"), StrL(kUtThumbnailIid)), StrL(kPdfPreviewClsid)));
        utassert(str::Eq(UtShellExTemp(StrL(".mobi"), StrL(kUtThumbnailIid)), StrL(kMobiPreviewClsid)));

        // preview pane only
        UninstallPreviewDll();
        utassert(InstallPreviewDll(StrL("C:\\SumatraPDF\\PdfPreview.dll"), false, {kAllPreviewExts, 0}));
        utassert(len(UtShellExTemp(StrL(".pdf"), StrL(kUtThumbnailIid))) == 0);
        utassert(str::Eq(UtShellExTemp(StrL(".pdf"), StrL(kUtPreviewIid)), StrL(kPdfPreviewClsid)));

        UninstallPreviewDll();
        utassert(str::Eq(UtShellExTemp(StrL(".epub"), StrL(kUtThumbnailIid)), StrL(kUtForeignClsid)));
        utassert(len(UtShellExTemp(StrL(".pdf"), StrL(kUtPreviewIid))) == 0);
        utassert(len(UtShellExTemp(StrL(".epub"), StrL(kUtPreviewIid))) == 0);
    }

Exit:
    for (int i = 0; i < 3; i++) {
        RegOverridePredefKey(predefs[i], nullptr);
        RegCloseKey(keys[i]);
    }
    DeleteRegKey(HKEY_CURRENT_USER, StrL("Software\\SumatraPDF-unit-tests"));
}

static void OnlineURLAllowedTest() {
    utassert(OnlineURLAllowed(StrL("https://web.sumatrapdfreader.org/api/files")));
    utassert(OnlineURLAllowed(StrL("http://localhost:9332/desktop-link")));
    utassert(OnlineURLAllowed(StrL("http://127.0.0.1:9332/api/files")));
    utassert(OnlineURLAllowed(StrL("https://web.sumatrapdfreader.org")));
    utassert(OnlineURLAllowed(StrL("http://localhost:9332")));
    utassert(OnlineURLAllowed(StrL("http://127.0.0.1:9332")));
    utassert(OnlineURLAllowed(StrL("https://web.sumatrapdfreader.org:443/api/files")));
    utassert(!OnlineURLAllowed(StrL("https://web.sumatrapdfreader.org.evil.example/api/files")));
    utassert(!OnlineURLAllowed(StrL("http://localhost")));
    utassert(!OnlineURLAllowed(StrL("http://localhost:80")));
    utassert(!OnlineURLAllowed(StrL("https://user@web.sumatrapdfreader.org/api/files")));
}

static bool OrderIs(const Vec<int>& order, std::initializer_list<int> want) {
    if (len(order) != (int)want.size()) {
        return false;
    }
    int i = 0;
    for (int v : want) {
        if (order[i++] != v) {
            return false;
        }
    }
    return true;
}

// a group's tabs gather where its first tab is
static void StripGroupOrderTest() {
    Vec<int> ids;
    Vec<int> order;
    utassert(!StripGroupOrder(ids, order));
    utassert(len(order) == 0);

    for (int id : {0, 1, 1, 0, 2, 2}) {
        VecAppend(ids, id);
    }
    utassert(!StripGroupOrder(ids, order));
    utassert(OrderIs(order, {0, 1, 2, 3, 4, 5}));

    // ids [0 A 0 A B]
    VecReset(ids);
    for (int id : {0, 7, 0, 7, 9}) {
        VecAppend(ids, id);
    }
    utassert(StripGroupOrder(ids, order));
    utassert(OrderIs(order, {0, 1, 3, 2, 4}));

    // ids [A B A B]
    VecReset(ids);
    for (int id : {3, 4, 3, 4}) {
        VecAppend(ids, id);
    }
    utassert(StripGroupOrder(ids, order));
    utassert(OrderIs(order, {0, 2, 1, 3}));
}

// a CJK UI language on a Windows whose UI font is Segoe UI gets that script's UI font
static void GuiFontLangTest() {
    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    if (!str::EqI(ToUtf8Temp(WStr(ncm.lfMessageFont.lfFaceName)), StrL("Segoe UI"))) {
        return;
    }
    struct {
        Str lang;
        Str face;
    } cases[] = {
        {StrL("cn"), StrL("Microsoft YaHei UI")}, {StrL("tw"), StrL("Microsoft JhengHei UI")},
        {StrL("ja"), StrL("Yu Gothic UI")},       {StrL("kr"), StrL("Malgun Gothic")},
        {StrL("en"), StrL("Segoe UI")},
    };
    for (auto& c : cases) {
        SetGuiFontLang(c.lang);
        PlatformFont* font = GetUserGuiFont(StrL("auto"), 17);
        LOGFONTW lf{};
        GetObjectW(font->GetHFont(), sizeof(lf), &lf);
        utassert(str::EqI(ToUtf8Temp(WStr(lf.lfFaceName)), c.face));
    }
    SetGuiFontLang({});
}

// a child that keeps its output open must not hold the reader past the timeout
static void AIChatReadUntilExitTest() {
    AIChatProcessLaunchResult launch;
    utassert(AIChatLaunchProcessWithStdoutPipe(StrL("cmd /c echo done"), {}, &launch));
    str::Builder out;
    utassert(AIChatReadUntilExit(launch.hReadPipe, launch.hProcess, 10 * 1000, out));
    AIChatCloseProcess(&launch.hProcess, true);
    utassert(str::Contains(ToStr(out), StrL("done")));

    utassert(AIChatLaunchProcessWithStdoutPipe(StrL("cmd /c \"echo hi & ping -n 30 127.0.0.1\""), {}, &launch));
    str::Builder out2;
    u64 start = GetTickCount64();
    bool exited = AIChatReadUntilExit(launch.hReadPipe, launch.hProcess, 1000, out2);
    u64 elapsed = GetTickCount64() - start;
    AIChatCloseProcess(&launch.hProcess, true);
    utassert(!exited);
    utassert(elapsed < 10 * 1000);
    utassert(str::Contains(ToStr(out2), StrL("hi")));
}

static void InstallRegistryRollbackTest() {
    TempStr testPath = fmt("Software\\SumatraPDF-unit-test-%u-%llu", (uint)GetCurrentProcessId(), GetTickCount64());
    HKEY root;
    DWORD disposition;
    LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, CWStrTemp(testPath), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr,
                                     &root, &disposition);
    utassert(status == ERROR_SUCCESS && disposition == REG_CREATED_NEW_KEY);
    if (status != ERROR_SUCCESS) {
        return;
    }
    defer {
        RegCloseKey(root);
    };
    if (disposition != REG_CREATED_NEW_KEY) {
        return;
    }
    defer {
        RegDeleteTreeW(HKEY_CURRENT_USER, CWStrTemp(testPath));
    };

    TempStr uninstallKey = GetRegPathUninstTemp(StrL(kAppName));
    TempStr pdfProgId = str::JoinTemp(StrL("Software\\Classes\\"), StrL(kAppName), StrL(".pdf"));
    Str mimeKey = StrL(R"(Software\Classes\MIME\Database\Content Type\application/pdf)");
    Str valueName = StrL("InstallLocation");
    Str previousDir = StrL("C:\\previous");
    Str friendlyName = StrL("Previous friendly name");
    Str previousExt = StrL(".previous");
    utassert(WriteRegStr(root, uninstallKey, valueName, previousDir));
    utassert(WriteRegStr(root, pdfProgId, {}, friendlyName));
    utassert(WriteRegStr(root, mimeKey, StrL("Extension"), previousExt));

    TempStr installDir = GetTempDirTemp();
    {
        // Hold a write on the last association key so earlier writes must roll back.
        AutoCloseHandle blocker(CreateTransaction(nullptr, nullptr, TRANSACTION_DO_NOT_PROMOTE, 0, 0, 0, nullptr));
        utassert(blocker.IsValid());
        HKEY held;
        status = RegOpenKeyTransactedW(root, CWStrTemp(mimeKey), 0, KEY_SET_VALUE, &held, blocker, nullptr);
        utassert(status == ERROR_SUCCESS);
        if (status != ERROR_SUCCESS) {
            return;
        }
        DWORD data = 1;
        utassert(RegSetValueExW(held, L"Extension", 0, REG_DWORD, (BYTE*)&data, sizeof(data)) == ERROR_SUCCESS);
        RegCloseKey(held);
        auto result = InstallRegistryInfo(root, installDir, DesktopShortcut::Omit);
        utassert(result == InstallRegResult::AssociationsFailed);
    }

    DWORD shortcut;
    utassert(str::Eq(ReadRegStrTemp(root, uninstallKey, valueName), previousDir));
    utassert(!ReadRegDWORD(root, uninstallKey, StrL(kRegDesktopShortcut), shortcut));
    utassert(str::Eq(ReadRegStrTemp(root, pdfProgId, {}), friendlyName));
    utassert(!RegKeyExists(root, str::JoinTemp(pdfProgId, StrL("\\shell"))));
    utassert(str::Eq(ReadRegStrTemp(root, mimeKey, StrL("Extension")), previousExt));

    utassert(InstallRegistryInfo(root, installDir, DesktopShortcut::Omit) == InstallRegResult::Complete);
    utassert(str::Eq(ReadRegStrTemp(root, uninstallKey, valueName), installDir));
    utassert(ReadRegDWORD(root, uninstallKey, StrL(kRegDesktopShortcut), shortcut) && shortcut == 0);
    utassert(RegKeyExists(root, str::JoinTemp(pdfProgId, StrL("\\shell\\open\\command"))));
    utassert(str::Eq(ReadRegStrTemp(root, mimeKey, StrL("Extension")), StrL(".pdf")));
}

static void SumatraPDF_UnitTests() {
    GuiFontLangTest();
    StripGroupOrderTest();
    OnlineURLAllowedTest();
    RestrictedUseConfigTest();
    CrashReportSettingsTest();
    ControlFrameActionTest();
    HexViewTest();
    SumatraPDFWebTest();
    HttpTransferTest();
    OpenDdeCmdTest();
    InstallRegistryRollbackTest();
    utassert(PlatformTextCache_UnitTest());
    UpdateCheckDecisionTest();
    CachedObjects_UnitTests();
    CommandPaletteModel_UnitTests();
    PaletteRowColorTest();
    SidebarViewSettingTest();
    FileUriTest();
    ServerPromoTest();
    AIChatReadUntilExitTest();
    GhostButtonDisabledTest();
    DocPropertiesTest();
    JsMenuTest();
    HtmlImageCacheTest();
    HtmlFormatterNarrowPageTest();
    PrintAdvancedDefaultsTest();
    parseCommandsTest();
    colorTest();
    PngSizeTest();
    BenchRangeTest();
    ParseCommandLineTest();
    ParseFileArgsTest();
    versioncheck_test();
    hexstrTest();
    ExpandPDFScriptTest();
    PDFScriptArgHintTest();
    PreviewHandlersTest();
}

static void ParseTipExpectWordsLinks(Str input, int expWords, int expLinks) {
    VirtRichText* tip = ParseTip(input);
    utassert(TipWordCount(tip) == expWords);
    utassert(TipLinkCount(tip) == expLinks);
    delete tip;
}

static void ParseTipExpectPlainContains(Str input, Str needle) {
    VirtRichText* tip = ParseTip(input);
    TempStr plain = tip->PlainTextTemp();
    utassert(plain && str::Contains(plain, needle));
    delete tip;
}

static void ParseTipExpectLinkCmd(Str input, Str expCmd) {
    VirtRichText* tip = ParseTip(input);
    utassert(TipLinkCount(tip) == 1);
    utassert(str::Eq(tip->links.next->cmd, expCmd));
    delete tip;
}

static int CountPaintedPixels(Pixmap* px) {
    int n = 0;
    for (int y = 0; y < px->height; y++) {
        u8* d = px->data + ((size_t)px->stride * (size_t)y);
        for (int x = 0; x < px->width; x++) {
            if (d[3] != 0) {
                n++;
            }
            d += 4;
        }
    }
    return n;
}

// issue #6186: an icon whose only paint is a <text> needs a base14 font, which
// mupdf only has once the embedded font loader is installed
static void SvgTextIcon_UnitTests() {
    const char* svgFmt =
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24"><text x="-1" y="24" font-size="32" font-family="sans-serif" fill="currentColor">%s</text></svg>)";

    // "A" is in Helvetica; the reporter's glyph needs the fallback font
    Str glyphs[] = {StrL("A"), StrL("\xE2\x96\xA6")};
    for (Str glyph : glyphs) {
        TempStr svg = fmt(svgFmt, glyph);
        Pixmap* px = GetCachedPixmapForSvg(svg, 24, 24, kColBlack, kColWhite);
        utassert(px != nullptr);
        utassert(CountPaintedPixels(px) > 0);
    }
}

// a black triangle on white; its diagonal edge is anti-aliased on screen
static const char* kTrianglePdf =
    "%PDF-1.4\n"
    "1 0 obj <</Type/Catalog/Pages 2 0 R>> endobj\n"
    "2 0 obj <</Type/Pages/Kids[3 0 R]/Count 1>> endobj\n"
    "3 0 obj <</Type/Page/Parent 2 0 R/MediaBox[0 0 100 100]/Contents 4 0 R>> endobj\n"
    "4 0 obj <</Length 32>> stream\n"
    "0 g 10 10 m 90 10 l 50 87.3 l f\n"
    "endstream endobj\n"
    "trailer <</Root 1 0 R>>\n"
    "%%EOF\n";

static int CountGrayPixels(Pixmap* px) {
    int bpp = PixmapBytesPerPixel(px->format);
    int n = 0;
    for (int y = 0; y < px->height; y++) {
        u8* d = px->data + ((size_t)px->stride * (size_t)y);
        for (int x = 0; x < px->width; x++, d += bpp) {
            if (d[0] != 0 && d[0] != 255) {
                n++;
            }
        }
    }
    return n;
}

static int RenderGrayPixels(EngineBase* engine, RenderTarget target) {
    RenderPageArgs args(1, 2.f, 0, nullptr, target);
    Pixmap* rendered = engine->RenderPage(args);
    // a few-color page renders to a palette DIB, readable only through GDI
    Pixmap* px = rendered ? PixmapCopyAs32bppDIB(rendered) : nullptr;
    FreePixmap(rendered);
    utassert(px && px->data);
    int n = px ? CountGrayPixels(px) : -1;
    FreePixmap(px);
    return n;
}

// #6384: printers halftone anti-aliased edge grays into toothy edges
static void PrintNoAntiAlias_UnitTest() {
    EngineBase* engine = CreateEngineMupdfFromData(Str(kTrianglePdf), StrL("triangle.pdf"), nullptr);
    utassert(engine);
    if (!engine) {
        return;
    }
    utassert(RenderGrayPixels(engine, RenderTarget::View) > 0);
    utassert(RenderGrayPixels(engine, RenderTarget::Print) == 0);
    SafeEngineRelease(&engine);
}

static void ParseTip_UnitTests() {
    // issue #5752: brackets in filenames must not hang
    ParseTipExpectPlainContains(StrL("Loading Apocalypse Bringer Mynoghra_01 [CIW].pdf ..."), StrL("[CIW]"));

    // empty link text must not create a zero-word link (DrawTipWords crash)
    ParseTipExpectWordsLinks(StrL("[](CmdFoo)"), 1, 0);
    ParseTipExpectPlainContains(StrL("[](CmdFoo)"), StrL("[](CmdFoo)"));

    // URLs may contain balanced parentheses
    ParseTipExpectLinkCmd(StrL("[text](https://example.com/foo(bar))"), StrL("https://example.com/foo(bar)"));
    ParseTipExpectWordsLinks(StrL("[text](https://example.com/foo(bar))"), 1, 1);

    // Help/ link followed by trailing punctuation: the resolved URL must stop at
    // the link's ')' and not pull in the following ")." (the link cmd is a
    // non-NUL-terminated view into the tip line)
    ParseTipExpectLinkCmd(StrL("You can [extract text from PDF file](Help/Tool-x-extract-text-from-pdf)."),
                          StrL("https://www.sumatrapdfreader.org/docs/Tool-x-extract-text-from-pdf"));

    // nested brackets in link text
    ParseTipExpectWordsLinks(StrL("[foo [bar]](CmdFoo)"), 2, 1);
    ParseTipExpectPlainContains(StrL("[foo [bar]](CmdFoo)"), StrL("foo"));
    ParseTipExpectPlainContains(StrL("[foo [bar]](CmdFoo)"), StrL("[bar]"));

    // (Key/...) only expands for real commands
    ParseTipExpectPlainContains(StrL("file (Key/foo).pdf"), StrL("(Key/foo).pdf"));
    ParseTipExpectPlainContains(StrL("(Key/CmdCommandPalette)"), StrL("Ctrl"));
    ParseTipExpectPlainContains(StrL("(Key/CmdToggleKeyboardHelp)"), StrL("?"));

    // (Kbd/...) draws as a key-cap word; nests with (Key/...)
    {
        VirtRichText* tip = ParseTip(StrL("(Kbd/Cmd+Shift)"));
        utassert(TipWordCount(tip) == 1);
        utassert(tip->words.next->isKbd);
        utassert(str::Eq(tip->words.next->text, StrL("Cmd+Shift")));
        utassert(tip->HasRichContent());
        delete tip;
    }
    {
        VirtRichText* tip = ParseTip(StrL("(Kbd/(Key/CmdCommandPalette)): go"));
        utassert(TipWordCount(tip) >= 2);
        TipWord* w0 = tip->words.next;
        TipWord* w1 = w0->next;
        utassert(w0->isKbd);
        // expanded shortcut contains Ctrl (default binding)
        utassert(str::Contains(w0->text, StrL("Ctrl")));
        // ':' abuts the key-cap with no space
        utassert(w1->noSpaceBefore);
        utassert(str::Eq(w1->text, StrL(":")));
        delete tip;
    }

    // whitespace: tab and newline break words
    ParseTipExpectWordsLinks(StrL("line1\nline2"), 2, 0);
    ParseTipExpectWordsLinks(StrL("tab\there"), 2, 0);

    // ordinary tips still work
    ParseTipExpectWordsLinks(StrL("before [valid](CmdFoo)"), 2, 1);
    ParseTipExpectWordsLinks(StrL("[valid](CmdFoo) after"), 2, 1);

    // GHSA-2wv2-qm2f-vmxh: a file name can contain the markup, so text from
    // outside the app must never become a link. AddPlainText / AddPlainLink are
    // how such text gets in
    {
        Str evil = StrL("a[b](CmdExec calc.exe)c");
        VirtRichText* tip = new VirtRichText();
        tip->AddPlainText(evil);
        utassert(TipLinkCount(tip) == 0);
        utassert(str::Contains(tip->PlainTextTemp(), StrL("(CmdExec")));
        delete tip;

        // the same text as a link: exactly one link, and to our command
        tip = new VirtRichText();
        tip->AddPlainLink(evil, StrL("CmdOpenNextFileInFolder"));
        utassert(TipLinkCount(tip) == 1);
        utassert(str::Eq(tip->links.next->cmd, StrL("CmdOpenNextFileInFolder")));
        delete tip;

        // mixing our markup with outside text keeps them apart
        tip = new VirtRichText();
        ParseTipInto(tip, StrL("open"));
        tip->AddPlainLink(evil, StrL("CmdOpenNextFileInFolder"));
        ParseTipInto(tip, StrL("[browse](CmdNavigateFilesInFolder)"));
        utassert(TipLinkCount(tip) == 2);
        utassert(str::Eq(tip->links.next->cmd, StrL("CmdOpenNextFileInFolder")));
        utassert(str::Eq(tip->links.next->next->cmd, StrL("CmdNavigateFilesInFolder")));
        delete tip;
    }
}

namespace {
int gTestCrashCommentCalls = 0;

Str TestCrashComment(Arena* a, Str, Str, bool) {
    gTestCrashCommentCalls++;
    return str::Dup(a, StrL("test report\n"));
}

void InstallTestCrashHandler() {
    CrashHandlerConfig cfg{};
    cfg.crashDumpPath = path::JoinTemp(GetTempDirTemp(), StrL("sumatra-ut-crash.dmp"));
    cfg.getCrashComment = TestCrashComment;
    InstallCrashHandler(cfg);
}
} // namespace

// tears down the app's crash handler: run last
static void CrashHandler_UnitTests() {
    LPTOP_LEVEL_EXCEPTION_FILTER saved = SetUnhandledExceptionFilter(nullptr);
    UninstallCrashHandler();

    // uninstall restores the filter it replaced
    SetUnhandledExceptionFilter(nullptr);
    InstallTestCrashHandler();
    UninstallCrashHandler();
    LPTOP_LEVEL_EXCEPTION_FILTER cur = SetUnhandledExceptionFilter(nullptr);
    utassert(cur == nullptr);

    // a debug report that isn't uploaded builds its full text once per session
    InstallTestCrashHandler();
    gTestCrashCommentCalls = 0;
    _uploadDebugReport(StrL("test cond"), StrL("Sumatra_ut.cpp:1"), false);
    _uploadDebugReport(StrL("test cond"), StrL("Sumatra_ut.cpp:1"), false);
    utassert(gTestCrashCommentCalls <= 1);
    UninstallCrashHandler();

    SetUnhandledExceptionFilter(saved);
}

// a tool main run in-process must not read the previous run's argv: those
// strings are gone (they were in the job thread's temp arena)
static void PdfToolRerunTest() {
    char arg0[] = "clean";
    char argZ[] = {'-', 'Z', 0, 0};
    char* argv1[] = {arg0, argZ};
    // no input file: prints usage
    utassert(RunPdfToolMain(pdfclean_main, dimofi(argv1), argv1) != 0);
    // the first run's "-Z" string, freed and reused
    argZ[2] = 'Q';

    TempStr dir = GetTempDirTemp();
    TempStr src = path::JoinTemp(dir, StrL("sumatra-ut-rerun.pdf"));
    TempStr dst = path::JoinTemp(dir, StrL("sumatra-ut-rerun-out.pdf"));
    Str pdf = StrL(
        "%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 10 10]>>endobj\n"
        "trailer<</Root 1 0 R>>\n%%EOF\n");
    utassert(file::WriteFile(src, pdf));
    char argZ2[] = "-Z";
    char* argv2[] = {arg0, argZ2, CStrTemp(src), CStrTemp(dst)};
    utassert(RunPdfToolMain(pdfclean_main, dimofi(argv2), argv2) == 0);
    utassert(file::Exists(dst));
    file::Delete(src);
    file::Delete(dst);
}

static void TestCbxSourceData() {
    TempStr dir = GetTempFilePathTemp(StrL("cbx"));
    file::Delete(dir);
    utassert(dir::Create(dir));
    defer {
        dir::RemoveAll(dir);
    };

    utassert(file::WriteFile(path::JoinTemp(dir, StrL("page.png")), StrL("image bytes")));
    Str input = ZipDirToData(dir);
    utassert(len(input) > 0);
    Str expected = str::DupTemp(input);
    EngineBase* engine = CreateEngineCbxFromData(input);
    str::Free(input);
    utassert(engine);
    if (!engine) {
        return;
    }
    defer {
        engine->Release();
    };

    Str data = engine->GetFileData();
    utassert(str::Eq(data, expected));
    str::Free(data);

    TempStr dst = path::JoinTemp(dir, StrL("copy.cbz"));
    utassert(engine->SaveFileAs(dst));
    Str saved = file::ReadFile(dst);
    utassert(str::Eq(saved, expected));
    str::Free(saved);
}

static LONG WINAPI ForAiCrashHandler(EXCEPTION_POINTERS* ei) {
    printf("unit tests crash\n");
    str::Builder s;
    dbghelp::GetExceptionInfo(s, ei);
    Str info = ToStr(s);
    printf("%.*s", info.len, info.s);
    fflush(stdout);
    ExitProcess(7);
    return EXCEPTION_EXECUTE_HANDLER;
}

// -for-ai: print assertion and crash callstacks to stdout instead of breaking
// into a debugger, so a script can report the failure
static void SetupForAi() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    utassert_set_for_ai(true);
    dbghelp::Initialize(ToWStrTemp(GetSelfExeDirTemp()), true);
    SetUnhandledExceptionFilter(ForAiCrashHandler);
}

int RunAppUnitTests(bool forAi) {
    if (forAi) {
        SetupForAi();
    }
    printf("Running unit tests\n");

    ArchiveTest();
    TestCbxSourceData();
    ZipDirTest();
    ArchiveWarnTest();
    ArchiveTarLazyTest();
    ArchiveRarHugeEntryTest();
    ArchiveTarHugeEntryTest();
    BaseUtilTest();
    ByteOrderTests();
    ClipboardImageTest();
    CryptoUtilTest();
    CssParser_UnitTests();
    DictTest();
    DirRemoveAllTest();
    UncExistsTest();
    NonVirtualPathTest();
    FileWatcherOverflowTest();
    FileUtilTest();
    UpdateTempFileTests();
    ExifTest();
    GuessFileTypeTest();
    JsonTest();
    RefHoverTest();
    SettingsUtilTest();
    SquareTreeTest();
    StrFormatTest();
    StrTest();
    StrVecTest();
    VecTest();
    WinUtilTest();
    PdfToolRerunTest();

    ChapterTable_UnitTests();
    LitDoc_UnitTests();
    RtfDoc_UnitTests();
    MobiDoc_UnitTests();
    Ofd_UnitTests();
    PagePosition_UnitTests();
    PdfSync_UnitTests();
    utassert(PdfDarkModeImageStats_UnitTest());
    SumatraPDF_UnitTests();

    ParseTip_UnitTests();
    SvgTextIcon_UnitTests();
    PrintNoAntiAlias_UnitTest();
#if IS_DEBUG
    TextSelection_UnitTests();
    EngineDjvuDec_UnitTests();
    Layout_UnitTests();
    LayoutWin_UnitTests();
    VirtCtrl_UnitTests();
    utassert(TableOfContents_UnitTestSnapshotNamedDest());
    utassert(Translations_UnitTestUnescape());
    utassert(Translations_UnitTestLocale());
    utassert(Translations_UnitTestLookup());
    utassert(Eutl_UnitTestListedCache());
    utassert(MarkdownModel_UnitTestBrowserNavigationUrl());
    utassert(MarkdownToc_UnitTestHtmlLinks());
    utassert(MarkdownToc_UnitTestHtmlHeadings());
    utassert(MarkdownToc_UnitTestMermaid());
    utassert(MarkdownToc_UnitTestHighlightWikilinks());
    utassert(MarkdownToc_TestLineBreaks());
    utassert(EbookDoc_UnitTestNormalizeURL());
    utassert(EbookDoc_UnitTestLoading());
    utassert(EbookDoc_UnitTestLinearNo());
    utassert(EbookDoc_UnitTestUtf16BeTwice());
    utassert(GumboHtmlParser_UnitTest());
    utassert(DisplayMode_UnitTestZoom());
    utassert(ExternalViewers_UnitTestPDFXChangePaths());
    utassert(ExternalViewers_UnitTestExtInList());
    utassert(Canvas_UnitTestScrollLineAmount());
    utassert(EngineMupdf_UnitTestEbookLineSpacingCss());
    utassert(EngineMupdf_UnitTestFormatUnixTime());
    utassert(EngineMupdf_UnitTestEbookFontFamilyCss());
    utassert(EngineMupdf_UnitTestEbookMarginCss());
    utassert(EngineMupdf_UnitTestMergeEBookUI());
    utassert(EngineMupdf_UnitTestPageLabels());
    utassert(EngineBase_UnitTestTextOutlivesInvalidate());
    utassert(EngineMupdf_UnitTestDoi());
    utassert(Accelerators_UnitTestFolderNavIsSafe());
    utassert(Accelerators_UnitTestTreeTakesLetters());
    utassert(Accelerators_UnitTestCreateAnnotEdit());
    utassert(Accelerators_UnitTestCustomShortcutShown());
    utassert(AnnotSearch_UnitTests());
    utassert(RenderCache_UnitTestCookieUnlocked());
    utassert(RenderCache_UnitTestFullOfOneVisiblePage());
    utassert(RenderCache_UnitTestFullOfOldZoom());
    utassert(RenderCache_UnitTestReduceTileSize());
    utassert(DisplayModel_UnitTestZoomFallback());
    utassert(EngineEbook_UnitTestUrlLink());
    utassert(DisplayModel_UnitTestFreePanFirstPage());
    utassert(DocController_UnitTestDestroyHook());
    utassert(Commands_UnitTestIdLimit());
    ReadAloudHighlight_UnitTests();
#endif
    CrashHandler_UnitTests();
    return utassert_print_results();
}

#endif
