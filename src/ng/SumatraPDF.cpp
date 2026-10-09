/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's SumatraPDF.cpp is 18.5k lines: the whole win32 application. This
// is the part the gpui shell needs now - startup, loading a document into a
// tab, the command dispatcher, closing - copied from orig and trimmed. The
// rest arrives with the step that needs it.

#include "gui/GpuiBridge.h"
#include "base/File.h"
#include "base/SquareTreeParser.h"
#include "base/Http.h"
#include "base/GuessFileType.h"
#include "base/CmdLineArgs.h"
#include "base/DirScan.h"
#include "base/FileWatcher.h"
#include "base/Launch.h"
#include "base/UITask.h"
#include "base/Timer.h"
#include "base/Archive.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/WinDynCalls.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocProperties.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "MarkdownToc.h"
#include "PngOptimizer.h"
#include "EbookBase.h"
#include "PalmDbReader.h"
#include "EbookDoc.h"
#include "MobiDoc.h"
#include "FileHistory.h"
#include "Version.h"
#include "Flags.h"
#include "ChmDump.h"
#include "ExifDump.h"
#include "Commands.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "Translations.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "CommandAvailability.h"
#include "TextSelection.h"
#include "Menu.h"
#include "TipMarkup.h"
#include "Notifications.h"
#include "ExplorerQuickLook.h"
#include "EmbeddedResources.h"
#include "Installer.h"
#include "VirtKeys.h"
#include "PerfLog.h"
#include "StressTesting.h"
#include "SumatraControl.h"
#include "GlobalHotkeys.h"
#include "HangDetector.h"
#include "UpdateCheck.h"
#include "SumatraCrashHandler.h"
#include "SumatraPDF.h"
#include "SumatraLog.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Tabs.h"
#include "gui/Dpi.h"
#include "gui/AppShell.h"
#include "gui/NativeFileDlg.h"
#include "gui/OleDragDrop.h"
#include "gui/NativeCursors.h"
#include "gui/Sidebar.h"
#include "gui/WasmBridge.h"
#include "gui/DocCanvas.h"
#include "gui/DialogWidgets.h"
#include "gui/TabsUI.h"
#include "Selection.h"
#include "SelectionToolbar.h"
#include "LinkFollow.h"
#include "ProgressUpdateUI.h"
#include "TextSearch.h"
#include "AppTools.h"
#include "PdfSync.h"
#include "SearchAndDDE.h"
#include "FindBar.h"
#include "Toolbar.h"
#include "ReadingBar.h"
#include "ReadingAutoScroll.h"
#include "ReadAloud.h"
#include "SelectTextKeyboard.h"
#include "SumatraDialogs.h"
#include "TableOfContents.h"
#include "Favorites.h"
#include "gui/Sidebar.h"
#include "FileThumbnails.h"
#include "HomePage.h"
#include "DocumentProperties.h"
#include "PdfDarkMode.h"
#include "PagePosition.h"
#include "SessionState.h"
#include "TabGroupsManage.h"
#include "gui/TabSwitcher.h"
#include "gui/BrowserView.h"
#include "CommandPalette.h"
#include "NavFilesInFolder.h"
#include "ExternalViewers.h"
#include "KeyboardHelp.h"
#include "Annotation.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "AnnotPlacement.h"
#include "AnnotTextPopup.h"
#include "RefHover.h"
#include "FormFields.h"
#include "ImageReader.h"
#include "ImageSaveCropResize.h"
#include "Screenshot.h"
#include "SelectionHandlers.h"
#include "SelectionTranslate.h"
#include "GoogleLens.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "SimpleBrowserWindow.h"
#include "PdfTools.h"
#include "Print.h"
#include "PrintWin11.h"
#include "TextViewWnd.h"

RenderCache* gRenderCache = nullptr;
static Flags* gFlags = nullptr;
// what CmdToggleHoverPreview turns the citation hover on to (orig's value)
constexpr int kDefaultCitationHoverDelay = 300;
// -crash-on-open: crash while opening a document, to test the crash handler
static bool gCrashOnOpen = false;

// ng: the plugin host is step 17, so the app is never in plugin mode yet
Str gPluginURL;

constexpr const char* kRestrictionsFileName = "sumatrapdfrestrict.ini";

// in restricted mode, some features can be disabled (such as
// opening files, printing, following URLs), so that SumatraPDF
// can be used as a PDF reader on locked down systems
static Perm gPolicyRestrictions = Perm::All;
// only the listed protocols will be passed to the OS for
// opening in e.g. a browser or an email client (ignored,
// if gPolicyRestrictions doesn't contain Perm::DiskAccess)
static StrVec gAllowedLinkProtocols;
// only files of the listed perceived types will be opened
// externally by LinkHandler::LaunchFile (i.e. when clicking
// on an in-document link); examples: "audio", "video", ...
static StrVec gAllowedFileTypes;

#define kDefaultFilePerceivedTypes "audio,video,webpage"
#define kDefaultLinkProtocols "http,https,mailto,file"

void InitializePolicies(bool restrict) {
    // default configuration should be to restrict everything
    ReportIf(gPolicyRestrictions != Perm::All);
    ReportIf(len(gAllowedLinkProtocols) != 0 || len(gAllowedFileTypes) != 0);

    // the -restrict command line flag overrides any sumatrapdfrestrict.ini configuration
    if (restrict) {
        gPolicyRestrictions = Perm::RestrictedUse;
        return;
    }

    // allow to restrict SumatraPDF's functionality from an INI file in the
    // same directory as SumatraPDF.exe (see ../docs/sumatrapdfrestrict.ini)
    // (if the file isn't there, everything is allowed)
    TempStr restrictPath = GetPathInExeDirTemp(Str(kRestrictionsFileName));
    if (!file::Exists(restrictPath)) {
        Split(&gAllowedLinkProtocols, StrL(kDefaultLinkProtocols), StrL(","));
        Split(&gAllowedFileTypes, StrL(kDefaultFilePerceivedTypes), StrL(","));
        return;
    }

    Str restrictData = file::ReadFile(restrictPath);
    SquareTreeNode* root = ParseSquareTree(restrictData);
    AutoDelete delRoot(root);
    SquareTreeNode* polsec = root ? root->GetChild(StrL("Policies")) : nullptr;
    gPolicyRestrictions = Perm::RestrictedUse;
    // if the restriction file is broken, err on the side of full restriction
    if (!polsec) {
        return;
    }

    static Perm perms[] = {Perm::InternetAccess, Perm::DiskAccess,    Perm::SavePreferences, Perm::RegistryAccess,
                           Perm::PrinterAccess,  Perm::CopySelection, Perm::FullscreenAccess};
    static SeqStrings permNames =
        "InternetAccess\0DiskAccess\0SavePreferences\0RegistryAccess\0PrinterAccess\0CopySelection\0FullscreenAccess\0";

    // enable policies as indicated in sumatrapdfrestrict.ini
    for (int i = 0; i < dimofi(perms); i++) {
        Str name = SeqStrByIndex(permNames, i);
        Str val = polsec->GetValue(name);
        if (val && ParseInt(val) != 0) {
            gPolicyRestrictions = gPolicyRestrictions | perms[i];
        }
    }

    // determine the list of allowed link protocols and perceived file types
    if ((gPolicyRestrictions & Perm::DiskAccess) != (Perm)0) {
        Str value = polsec->GetValue(StrL("LinkProtocols"));
        if (value) {
            TempStr protocols = str::DupTemp(value);
            str::ToLowerInPlace(protocols);
            str::TransCharsInPlace(protocols, StrL(" :;"), StrL(",,,"));
            Split(&gAllowedLinkProtocols, protocols, StrL(","), true);
        }
        value = polsec->GetValue(StrL("SafeFileTypes"));
        if (value) {
            TempStr protocols = str::DupTemp(value);
            str::ToLowerInPlace(protocols);
            str::TransCharsInPlace(protocols, StrL(" :;"), StrL(",,,"));
            Split(&gAllowedFileTypes, protocols, StrL(","), true);
        }
    }
}

static void RestrictPolicies(Perm revokePermission) {
    gPolicyRestrictions = (gPolicyRestrictions | Perm::RestrictedUse) & ~revokePermission;
}

bool HasPermission(Perm permission) {
    return (permission & gPolicyRestrictions) == permission;
}

bool CanAccessDisk() {
    return HasPermission(Perm::DiskAccess);
}

// TODO: could add a setting
bool AnnotationsAreDisabled() {
    if (!CanAccessDisk()) {
        // annotations must be saved back to a file so lack of disk access
        // implies no ability to edit annotations
        return true;
    }
    return false;
}

// lets the shell open a URI for any supported scheme in
// the appropriate application (web browser, mail client, etc.)
bool SumatraLaunchBrowser(Str url) {
#if OS_WIN
    if (gPluginMode) {
        // pass the URI back to the browser
        ReportIf(len(gWindows) == 0);
        if (len(gWindows) == 0) {
            return false;
        }
        HWND plugin = AppShellNativeHwnd(gWindows[0]);
        HWND parent = GetAncestor(plugin, GA_PARENT);
        int urlLen = len(url);
        if (!parent || len(url) == 0 || (urlLen > 4096)) {
            return false;
        }
        TempStr urlZ = str::DupTemp(url);
        COPYDATASTRUCT cds = {0x4C5255 /* URL */, (DWORD)urlZ.len + 1, urlZ.s};
        return SendMessageW(parent, WM_COPYDATA, (WPARAM)plugin, (LPARAM)&cds);
    }
#endif

    if (!CanAccessDisk()) {
        return false;
    }

    // check if this URL's protocol is allowed
    TempStr protocol;
    if (str::IsNull(str::Parse(url, "%S:", &protocol))) {
        return false;
    }
    str::ToLowerInPlace(protocol);
    if (!gAllowedLinkProtocols.Contains(protocol)) {
        logf("SumatraLaunchBrowser: protocol '%s' is not allowed\n", protocol);
        return false;
    }

    // ng: orig's LaunchFileShell(url, {}, "open")
    logf("SumatraLaunchBrowser: '%s'\n", url);
    gp::OpenUrl(ToGpui(url));
    return true;
}

// CmdDebugToggleRtl: force the RTL layout without switching the language
static bool gForceRtl = false;

bool IsUIRtl() {
    if (gForceRtl) {
        return true;
    }
    return trans::IsCurrLangRtl();
}

bool SettingsUseTabs() {
    return gSettings->useTabs;
}

bool SettingsRememberOpenedFiles() {
    return gSettings->rememberOpenedFiles;
}

// ng: Windows keeps a file type's perceived type in the registry; elsewhere
// it comes from a short list of extensions
static TempStr PerceivedTypeTemp(Str path) {
    TempStr ext = path::GetExtTemp(path);
#if OS_WIN
    return ReadRegStrTemp(HKEY_CLASSES_ROOT, ext, StrL("PerceivedType"));
#else
    static SeqStrings audioExts = ".mp3\0.wav\0.ogg\0.oga\0.flac\0.m4a\0.aac\0.opus\0.wma\0.mid\0.midi\0";
    static SeqStrings videoExts = ".mp4\0.m4v\0.mkv\0.webm\0.avi\0.mov\0.mpg\0.mpeg\0.wmv\0.ogv\0";
    TempStr extLower = str::DupTemp(ext);
    str::ToLowerInPlace(extLower);
    if (SeqStrIndex(audioExts, extLower) >= 0) {
        return str::DupTemp(StrL("audio"));
    }
    if (SeqStrIndex(videoExts, extLower) >= 0) {
        return str::DupTemp(StrL("video"));
    }
    return {};
#endif
}

// lets the shell open a file of any supported perceived type
// in the default application for opening such files
bool OpenFileExternally(Str path) {
    if (!CanAccessDisk() || gPluginMode) {
        return false;
    }

    // check if this file's perceived type is allowed
    TempStr perceivedType = PerceivedTypeTemp(path);
    // since we allow following hyperlinks, also allow opening local webpages
    if (str::EndsWithI(path, StrL(".htm")) || str::EndsWithI(path, StrL(".html")) ||
        str::EndsWithI(path, StrL(".xhtml"))) {
        perceivedType = str::DupTemp(StrL("webpage"));
    }
    str::ToLowerInPlace(perceivedType);
    if (gAllowedFileTypes.Contains(StrL("*"))) {
        /* allow all file types (not recommended) */;
    } else if (len(perceivedType) == 0 || !gAllowedFileTypes.Contains(perceivedType)) {
        return false;
    }

    // TODO: only do this for trusted files (cf. IsUntrustedFile)?
    return LaunchFileShell(path);
}

void SetCurrentLang(Str langCode) {
    if (len(langCode) == 0) {
        return;
    }
    str::ReplaceWithCopy(&gSettings->uiLanguage, langCode);
    trans::SetCurrentLangByCode(gSettings->uiLanguage);
}

void SetCurrentLanguageAndRefreshUI(Str langCode) {
    if (len(langCode) == 0 || str::Eq(langCode, trans::GetCurrentLangCode())) {
        return;
    }
    SetCurrentLang(langCode);
    // the home page caches its laid-out rows, which carry translated labels
    HomePageInvalidateLayoutCache();
    for (MainWindow* win : gWindows) {
        RebuildMenuBar(win);
        // ng: orig's UpdateToolbarSidebarText sets the text of its win32
        // controls; every label here is built from Tr() on the next frame
        win->RedrawAll();
    }
    ScheduleSaveSettings();
}

// orig's: the render cache's two colors plus the dark-mode options that also
// affect a rendered page. A change bumps darkModeEpoch, which drops every
// cached bitmap, and re-renders the open documents.
// the theme, or the system palette it follows, changed: rebuild and repaint
// everything that shows a theme color. ng: orig also recreates its win32
// controls (brushes, find bar, toolbar, annotation bars); here every element is
// built from the theme on the next frame, so an invalidation is enough. The
// The gpui theme is already installed before this runs.
void UpdateAfterThemeChange() {
    // ng: LoadSettings() picks the theme before GpuiMain makes the render cache
    if (!gRenderCache) {
        return;
    }
    // the home page's icons are cached in the theme's colors
    HomePageInvalidateLayoutCache();
    for (MainWindow* win : gWindows) {
        RebuildMenuBar(win);
        UpdateToolbarAfterThemeChange(win);
        win->RedrawAll(true);
    }
    UpdateDocumentColors();
}

void UpdateDocumentColors() {
    Color bg;
    Color text = ThemePageRenderColors(bg);
    bool pagesDark = !IsLightColor(bg);
    Color link = pagesDark ? ThemeWindowLinkColor() : 0;

    static bool s_lastPreservePdfImages = false;
    static int s_lastDocumentColorsFollowTheme = -1;
    bool preservePdfImages = pagesDark && GetPreservePdfImagesInDarkMode();
    int documentColorsFollowTheme = (int)GetDocumentColorsFollowTheme();
    bool grayscale = gSettings->fixedPageUI.grayscale;

    if ((text == gRenderCache->textColor) && (bg == gRenderCache->backgroundColor) &&
        (link == gRenderCache->linkColor) && preservePdfImages == s_lastPreservePdfImages &&
        documentColorsFollowTheme == s_lastDocumentColorsFollowTheme &&
        grayscale == AtomicBoolGet(&gRenderCache->grayscalePageColors)) {
        return; // colors didn't change
    }
    s_lastPreservePdfImages = preservePdfImages;
    s_lastDocumentColorsFollowTheme = documentColorsFollowTheme;

    AtomicBoolSet(&gRenderCache->grayscalePageColors, grayscale);
    gRenderCache->textColor = text;
    gRenderCache->backgroundColor = bg;
    gRenderCache->linkColor = link;
    gRenderCache->darkModeEpoch++;

    // ng: orig also regenerates the chm / markdown previews here; their browser
    // host is step 11b
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            DisplayModel* dm = tab->AsFixed();
            if (!dm) {
                continue;
            }
            gRenderCache->AbortRendering(dm);
            EngineMupdfInvalidateDarkMode(dm->GetEngine());
            dm->SyncWithEngineLayout();
            dm->GetEngine()->StartBackgroundChapterLayout();
        }
        win->RedrawAll(true);
    }
}

// ng: the scrollbars are gpui elements the canvas draws from the setting on
// every frame, so only the viewport (which the "windows" mode reserves space
// in) has to be recomputed
void UpdateFixedPageScrollbarsVisibility() {
    for (MainWindow* w : gWindows) {
        DisplayModel* dm = w->AsFixed();
        if (dm) {
            w->lastViewPortSize = Size{};
            dm->SetViewPortSize(w->GetViewPortSize());
        }
        w->RedrawAll(true);
    }
}

static void AppendLayoutFloats(str::Builder& b, Vec<float>* vals) {
    if (!vals) {
        return;
    }
    for (float v : *vals) {
        b.Append(fmt("%g,", v));
    }
}

// font, page size, spacing and CSS: what a reload has to re-paginate
static Str EbookLayoutSnapshot() {
    str::Builder b;
    if (!gSettings) {
        return b.TakeStr();
    }
    b.Append(fmt("dpi=%d\n", gSettings->customScreenDPI));
    EBookUI* g = &gSettings->eBookUI;
    b.Append(fmt("g|%s|%g|%g|%g|%d|%g|", g->fontName, g->fontSize, g->layoutDx, g->layoutDy,
                 g->ignoreDocumentCSS ? 1 : 0, g->lineSpacing));
    AppendLayoutFloats(b, g->margin);
    b.Append(fmt("|%s\n", g->customCSS));
    if (gSettings->fileStates) {
        for (FileState* fs : *gSettings->fileStates) {
            FileEBookUI* f = fs->eBookUI;
            if (!f) {
                continue;
            }
            b.Append(fmt("f|%s|%s|%g|%g|%g|%s|%g|", fs->filePath, f->fontName, f->fontSize, f->layoutDx, f->layoutDy,
                         f->ignoreDocumentCSS, f->lineSpacing));
            AppendLayoutFloats(b, f->margin);
            b.Append(fmt("|%s\n", f->customCSS));
        }
    }
    return b.TakeStr();
}

// reflowable docs, and anything with chapters (MOBI): their page count follows
// the ebook font / page size / CSS
static bool LayoutFollowsEbookSettings(EngineBase* engine) {
    if (!engine) {
        return false;
    }
    if (engine->isReflowable || engine->HasChapters()) {
        return true;
    }
    Kind k = engine->kind;
    return k == kindEngineMobi || k == kindEngineFb2 || k == kindEnginePdb || k == kindEngineHtml ||
           k == kindEngineEpub;
}

static void ReloadEbookLayoutDocs() {
    for (MainWindow* w : gWindows) {
        Vec<WindowTab*> tabs;
        for (WindowTab* tab : w->Tabs()) {
            VecAppend(tabs, tab);
        }
        for (WindowTab* tab : tabs) {
            DisplayModel* dm = tab->AsFixed();
            EngineBase* engine = dm ? dm->GetEngine() : nullptr;
            if (!LayoutFollowsEbookSettings(engine)) {
                continue;
            }
            if (tab == w->CurrentTab()) {
                ReloadDocument(w, false);
            } else {
                tab->reloadOnFocus = true;
            }
        }
    }
}

// ng: AppSettings.cpp is built without MainWindow (its NG_HAS_UI blocks), so
// the window half of orig's ApplySettingsToOpenWindows() is here
void ApplySettingsToWindowsUi() {
    for (MainWindow* win : gWindows) {
        // WindowMargin / PageSpacing are copied into DisplayModel at SetUiDpi;
        // pick up the reloaded prefs before the relayout below (issue #6018)
        if (DisplayModel* dm = win->AsFixed()) {
            dm->SetUiDpi(dm->uiDpi);
        }
        // LoadSettings re-creates custom commands (themes, external viewers,
        // selection handlers, shortcuts) with fresh command ids. Menus still
        // hold the old ids unless rebuilt - without this, e.g. "Set theme '...'"
        // does nothing until restart (issue #5822).
        RebuildMenuBar(win);
        ReCreateToolbar(win);
        ToolbarUpdateStateForWindow(win, true);
        UpdateFindbox(win);
        for (WindowTab* tab : win->Tabs()) {
            UpdateTabPageText(tab);
        }
        // force the relayout
        win->lastViewPortSize = Size{};
        win->RedrawAll(true);
    }
}

// ng: the window half of orig's ReloadSettings(), after LoadSettings().
// UpdateControlsColors has no counterpart: every element reads the theme on
// the next frame. SetUiDpi and the relayout are in ApplySettingsToWindowsUi().
void ReloadSettingsUpdateWindows(bool showToolbarBefore) {
    // its favorites and file history rows point into the old gSettings
    CommandPaletteOnSettingsReloaded();
    for (MainWindow* win : gWindows) {
        if (gSettings->showToolbar != showToolbarBefore) {
            ShowOrHideToolbar(win);
        }
        UpdateFavoritesTree(win);
        win->RedrawAll(true);
    }
}

SettingsApplyState GetSettingsApplyState() {
    Settings* p = gSettings;
    SettingsApplyState s;
    s.useTabs = p->useTabs;
    s.showMenubar = p->showMenubar;
    s.showMenubarWithTabs = p->showMenubarWithTabs;
    s.disableAntiAlias = p->disableAntiAlias;
    s.chmUseFixedPageUI = p->chmUI.useFixedPageUI;
    s.markdownUseFixedPageUI = p->markdownUI.useFixedPageUI;
    s.ebookLayout = EbookLayoutSnapshot();
    return s;
}

// orig's, minus what this port has no service for: ExplorerQuickLook (step 14)
// and the UseTabs <-> windows transition (it closes and reopens every window)
void ApplyChangedSettingsAndRelayout(const SettingsApplyState& before) {
    Settings* p = gSettings;

    if (before.disableAntiAlias != p->disableAntiAlias) {
        for (MainWindow* w : gWindows) {
            DisplayModel* dm = w->AsFixed();
            if (dm && dm->GetEngine()) {
                dm->GetEngine()->disableAntiAlias = p->disableAntiAlias;
            }
            gRenderCache->darkModeEpoch++;
            w->RedrawAll(true);
        }
    }
    if (before.chmUseFixedPageUI != p->chmUI.useFixedPageUI) {
        for (MainWindow* w : gWindows) {
            if (w->CurrentTab() && w->CurrentTab()->AsChm()) {
                ReloadDocument(w, false);
            }
        }
    }
    if (before.markdownUseFixedPageUI != p->markdownUI.useFixedPageUI) {
        for (MainWindow* w : gWindows) {
            if (w->CurrentTab() && w->CurrentTab()->AsMarkdown()) {
                ReloadDocument(w, false);
            }
        }
    }
    bool menubarChanged =
        (before.showMenubar != p->showMenubar) || (before.showMenubarWithTabs != p->showMenubarWithTabs);
    if (menubarChanged) {
        for (MainWindow* w : gWindows) {
            w->isMenuBarVisible = SettingsUseTabs() ? p->showMenubarWithTabs : p->showMenubar;
        }
    }
    ApplySettingsToOpenWindows();

    Str prevLayout = before.ebookLayout;
    Str nowLayout = EbookLayoutSnapshot();
    bool ebookLayoutChanged = !str::Eq(prevLayout, nowLayout);
    str::Free(prevLayout);
    str::Free(nowLayout);
    if (ebookLayoutChanged) {
        ReloadEbookLayoutDocs();
    }

    for (MainWindow* w : gWindows) {
        RebuildMenuBar(w);
        w->lastViewPortSize = Size{};
        w->RedrawAll(true);
    }
}

// --- the in-app manual -------------------------------------------------------
// orig packs docs/md/** plus the client-side renderer into IDR_EMBEDDED_PAK and
// serves them to a WebView2 from a virtual host; here they are archive entries
// named "<file>" (cmd/gen-docs.ts) and the host is
// the SimpleBrowserWindow's. Falls back to the website in the user's browser.

constexpr const char* kManualDefaultDocURI = "/SumatraPDF-documentation";
constexpr const char* kManualVirtualHost = "https://sumatrapdf.manual/";

// unpacked once each and kept: the WebView reads them after we return
struct ManualFile {
    ManualFile* next;
    Str name;
    Str data;
};

static ManualFile* gManualFiles = nullptr;

static Str ManualFileData(Str name) {
    for (ManualFile* f = gManualFiles; f; f = f->next) {
        if (str::Eq(f->name, name)) {
            return f->data;
        }
    }
    int size = 0;
    u8* data = GetEmbeddedFileData(name, &size);
    auto* f = AllocStruct<ManualFile>();
    f->name = str::Dup(name);
    f->data = Str((char*)data, size);
    f->next = gManualFiles;
    gManualFiles = f;
    return f->data;
}

static const char* kHelpThemeValues[] = {"app", "light", "dark"};

// HelpTheme setting, "app" unless it holds one of the known values
static Str HelpThemePref() {
    for (const char* v : kHelpThemeValues) {
        if (str::EqI(gSettings->helpTheme, Str(v))) {
            return Str(v);
        }
    }
    return StrL("app");
}

// theme.js reports a click on the manual's switch via
// window.__sumatra__.notify("manualTheme", "<system|light|dark>")
static void ManualOnJsNotify(void*, Str method, Str paramsJson) {
    if (!str::Eq(method, StrL("manualTheme"))) {
        return;
    }
    // params is a JSON array with one string, e.g. ["dark"]; "system" is what
    // theme.js calls the follow-the-app option
    Str v{};
    if (str::Contains(paramsJson, StrL("\"system\""))) {
        v = StrL("app");
    }
    for (const char* known : kHelpThemeValues) {
        if (str::Contains(paramsJson, fmt("\"%s\"", Str(known)))) {
            v = Str(known);
        }
    }
    if (len(v) == 0 || str::Eq(gSettings->helpTheme, v)) {
        return;
    }
    str::ReplaceWithCopy(&gSettings->helpTheme, v);
    ScheduleSaveSettings();
}

// The manual's theme switch (docs/theme.js) has a third option that follows the
// app: announce the app's scheme and the HelpTheme setting before the script
// runs, and hand the exact window colors to manual.css so "app" mode matches
// the native window.
static Str ManualInjectThemeCss(Str html) {
    TempStr bg = SerializeColorTemp(ThemeWindowBackgroundColor());
    TempStr fg = SerializeColorTemp(ThemeWindowTextColor());
    Str scheme = IsLightColor(ThemeWindowBackgroundColor()) ? StrL("light") : StrL("dark");
    // theme.js calls the follow-the-app option "system"
    Str pref = HelpThemePref();
    if (str::Eq(pref, StrL("app"))) {
        pref = StrL("system");
    }
    TempStr script =
        fmt("<script>window.SumatraAppTheme=\"%s\";window.SumatraManualTheme=\"%s\"</script>", scheme, pref);
    TempStr css =
        fmt("<style id=\"sumatra-manual-theme\">"
            "html[data-theme-pref=\"system\"]{--bg-primary:%s;--bg-elevated:%s;--text-primary:%s;--link-color:%s}"
            "</style>",
            bg, bg, fg, fg);

    int scriptAt = str::IndexOfI(html, StrL("<head>"));
    scriptAt = scriptAt < 0 ? 0 : scriptAt + len(StrL("<head>"));
    int cssAt = str::IndexOfI(html, StrL("</head>"));
    if (cssAt < scriptAt) {
        cssAt = scriptAt;
    }
    str::Builder result;
    result.Append(Str(html.s, scriptAt));
    result.Append(script);
    result.Append(Str(html.s + scriptAt, cssAt - scriptAt));
    result.Append(css);
    result.Append(Str(html.s + cssAt, len(html) - cssAt));
    return result.TakeStr();
}

static bool IsManualDocHtmlPage(Str path) {
    if (len(path) == 0 || !str::EndsWithI(path, StrL(".html"))) {
        return false;
    }
    return !str::EqI(path, StrL("manual.shell.html"));
}

// the shell page is what renders every doc page from its .md source, so every
// <name>.html request is answered with it
static Str ManualGetData(void*, Str path) {
    if (len(path) == 0) {
        return {};
    }
    if (IsManualDocHtmlPage(path)) {
        // themed on every request, as orig does: the app theme and HelpTheme
        // can change while the manual is open. The WebView reads the buffer
        // after we return, so it is kept until the next request.
        static Str themedShell;
        Str shell = ManualFileData(StrL("manual.shell.html"));
        if (len(shell) == 0) {
            return {};
        }
        str::Free(themedShell);
        themedShell = ManualInjectThemeCss(shell);
        return themedShell;
    }
    return ManualFileData(path);
}

// orig's DocURIToLocalManualUrlTemp
static TempStr DocURIToLocalManualUrlTemp(Str docURI) {
    if (len(docURI) == 0) {
        docURI = Str(kManualDefaultDocURI);
    }
    Str fragment = str::SliceFromChar(docURI, '#');
    Str pathStart = docURI;
    if (len(pathStart) > 0 && pathStart.s[0] == '/') {
        pathStart = Str(pathStart.s + 1, pathStart.len - 1);
    }
    int pathLen = fragment ? (int)(fragment.s - pathStart.s) : pathStart.len;
    if (pathLen <= 0) {
        pathStart = Str(kManualDefaultDocURI + 1);
        pathLen = pathStart.len;
        fragment = {};
    }

    TempStr htmlFile = str::DupTemp(Str(pathStart.s, pathLen));
    if (!str::EndsWithI(htmlFile, StrL(".html"))) {
        htmlFile = str::JoinTemp(htmlFile, StrL(".html"));
    }
    TempStr url = str::JoinTemp(Str(kManualVirtualHost), htmlFile);
    if (fragment) {
        url = str::JoinTemp(url, fragment);
    }
    return url;
}

static TempStr DocURIToWebUrlTemp(Str docURI) {
    if (len(docURI) == 0) {
        docURI = Str(kManualDefaultDocURI);
    }
    if (docURI.s[0] == '/') {
        return fmt("https://www.sumatrapdfreader.org/docs%s", docURI);
    }
    return fmt("https://www.sumatrapdfreader.org/docs/%s", docURI);
}

// Shrink to the work area if needed (saved size from a bigger monitor, or a
// resolution change) and shift so the window is fully visible.
static Rect ClampHelpWindowRect(Rect r, MainWindow* win) {
    if (r.dx <= 0 || r.dy <= 0) {
        return r;
    }
    Rect work = AppShellWorkArea(win);
    if (!work.IsEmpty()) {
        r.dx = std::min(r.dx, work.dx);
        r.dy = std::min(r.dy, work.dy);
    }
    return AppShellShiftToWorkArea(r, win, true);
}

// First open: upper half of the parent, on the side with more leftover space.
// Wide enough for the manual's table-of-contents sidebar, which the page CSS
// shows only from a 950px viewport (docs/manual.shell.html).
static Rect DefaultHelpWindowRect(MainWindow* win) {
    int dpi = std::max(AppShellWindowDpi(win), 96);
    Size size{MulDiv(1000, dpi, 96), MulDiv(860, dpi, 96)};
    Rect frame = AppShellWindowScreenRect(win);
    Rect work = AppShellWorkArea(win);
    if (work.IsEmpty()) {
        work = {0, 0, std::max(size.dx, 1920), std::max(size.dy, 1080)};
    }
    size.dx = std::min(size.dx, work.dx);
    size.dy = std::min(size.dy, work.dy);
    int rightSpace = work.Right() - frame.Right();
    int leftSpace = frame.x - work.x;
    int x = (rightSpace >= leftSpace) ? frame.Right() : frame.x - size.dx;
    int y = frame.y;
    return ClampHelpWindowRect({x, y, size.dx, size.dy}, win);
}

static Rect ManualBrowserPlacementRect(MainWindow* win) {
    Rect saved = gSettings->helpWindowPos;
    if (!saved.IsEmpty()) {
        // null: the primary work area. A saved rect from a disconnected
        // display is shifted back on screen.
        return ClampHelpWindowRect(saved, nullptr);
    }
    return DefaultHelpWindowRect(win);
}

// orig's SaveManualBrowserPos
static void SaveManualBrowserPos(Rect r) {
    if (r.IsEmpty()) {
        return;
    }
    gSettings->helpWindowPos = r;
    ScheduleSaveSettings();
}

// orig's ManualBrowserParentFrame: the window the user is in
static MainWindow* ManualBrowserParentWindow() {
    for (MainWindow* win : gWindows) {
        if (win->gpuiWin && win->gpuiWin->active) {
            return win;
        }
    }
    return len(gWindows) > 0 ? gWindows[0] : nullptr;
}

void LaunchDocumentation(Str docURI) {
#if OS_WASM
    // the wasm build embeds no manual (cmd/ng-build.ts)
    SumatraLaunchBrowser(DocURIToWebUrlTemp(docURI));
    return;
#endif
    MainWindow* win = ManualBrowserParentWindow();
    bool haveManual = len(ManualFileData(StrL("manual.shell.html"))) > 0;
    if (win && BrowserViewAvailable() && haveManual) {
        SimpleBrowserCreateArgs args;
        args.win = win;
        args.pos = ManualBrowserPlacementRect(win);
        args.onPosChanged = SaveManualBrowserPos;
        args.title = StrL("SumatraPDF Documentation");
        args.url = DocURIToLocalManualUrlTemp(docURI);
        args.resourceUriPrefix = Str(kManualVirtualHost);
        args.resourceProvider.getData = ManualGetData;
        args.jsNotify = ManualOnJsNotify;
        SimpleBrowserWindowShow(args);
        return;
    }
    SumatraLaunchBrowser(DocURIToWebUrlTemp(docURI));
}

// orig's ToggleDocumentationWindow: F1 opens the documentation window and F1
// again dismisses it, the way ? toggles the keyboard shortcuts window
static void ToggleDocumentationWindow() {
    if (IsSimpleBrowserWindowOpen()) {
        SimpleBrowserWindowClose();
        return;
    }
    LaunchDocumentation({});
}

void MaybeRedrawHomePage() {
    for (MainWindow* win : gWindows) {
        if (win->IsCurrentTabAbout()) {
            AppShellInvalidate(win);
        }
    }
}

// removes a file from the Frequently Read list on the home page. Files with
// favorites are only hidden (so the favorites aren't lost). orig has this in
// Menu.cpp, next to the home page's context menu (issue #283).
void ForgetFileFromFrequentlyRead(MainWindow* win, Str filePath) {
    FileState* fs = FileHistoryFindByPath(filePath);
    if (!fs) {
        return;
    }
    TempStr path = str::DupTemp(fs->filePath);
    if (len(*fs->favorites) > 0) {
        // only hide documents with favorites
        FileHistoryDemote(fs->filePath, true);
    } else {
        HomePageThumbnailChanged(fs);
        FileHistoryRemove(fs);
        DeleteFileState(fs);
    }
    DeleteThumbnailForFile(path);
    ScheduleSaveSettings();
    win->RedrawAll(true);
}

SeqStrings gScrollbarModeNames = "windows\0smart\0overlay\0hidden\0";
SeqStrings gToolbarModeNames = "show\0hide\0overlay\0";
SeqStrings gToolbarPositionNames = "top\0bottom\0";

int ScrollbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gScrollbarModeNames, gSettings->scrollbars);
    if (idx < 0) {
        idx = kScrollbarWindows;
    }
    return idx;
}

bool ScrollbarsAreHidden() {
    return ScrollbarModeFromPrefs() == kScrollbarHidden;
}

bool ScrollbarsUseOverlay() {
    int mode = ScrollbarModeFromPrefs();
    return mode == kScrollbarSmart || mode == kScrollbarOverlay;
}

int ToolbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gToolbarModeNames, gSettings->toolbar);
    if (idx < 0) {
        // not set / invalid: derive from the legacy showToolbar bool
        idx = gSettings->showToolbar ? kToolbarShow : kToolbarHide;
    }
    return idx;
}

bool ToolbarModeIsOverlay() {
    return ToolbarModeFromPrefs() == kToolbarOverlay;
}

bool ToolbarModeIsHidden() {
    return ToolbarModeFromPrefs() == kToolbarHide;
}

void SetToolbarMode(int mode) {
    Str name = SeqStrByIndex(gToolbarModeNames, mode);
    if (len(name) == 0) {
        name = StrL("show");
        mode = kToolbarShow;
    }
    str::ReplaceWithCopy(&gSettings->toolbar, name);
    // keep the legacy bool in sync so old versions stay sane
    gSettings->showToolbar = (mode != kToolbarHide);
}

int FullscreenToolbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gToolbarModeNames, gSettings->fullscreen.toolbar);
    if (idx < 0) {
        idx = gSettings->fullscreen.showToolbar ? kToolbarShow : kToolbarHide;
    }
    return idx;
}

void SetFullscreenToolbarMode(int mode) {
    Str name = SeqStrByIndex(gToolbarModeNames, mode);
    if (len(name) == 0) {
        name = StrL("hide");
        mode = kToolbarHide;
    }
    str::ReplaceWithCopy(&gSettings->fullscreen.toolbar, name);
    gSettings->fullscreen.showToolbar = (mode != kToolbarHide);
}

int ToolbarPositionFromPrefs() {
    int idx = SeqStrIndexIS(gToolbarPositionNames, gSettings->toolbarPosition);
    if (idx < 0) {
        idx = kToolbarTop;
    }
    return idx;
}

bool ToolbarAtBottom() {
    return ToolbarPositionFromPrefs() == kToolbarBottom;
}

// orig's OnMenuViewShowHideToolbar: F8 cycles show -> overlay -> hide, except
// on the home page where it only toggles show / hide
static void OnMenuViewShowHideToolbar(MainWindow* win) {
    if (win->isFullScreen) {
        int mode = FullscreenToolbarModeFromPrefs();
        int next = kToolbarShow;
        if (mode == kToolbarShow) {
            next = kToolbarOverlay;
        } else if (mode == kToolbarOverlay) {
            next = kToolbarHide;
        }
        SetFullscreenToolbarMode(next);
    } else if (win->IsCurrentTabAbout()) {
        int mode = ToolbarModeFromPrefs();
        SetToolbarMode(mode == kToolbarHide ? kToolbarShow : kToolbarHide);
    } else {
        int mode = ToolbarModeFromPrefs();
        int next = kToolbarShow;
        if (mode == kToolbarShow) {
            next = kToolbarOverlay;
        } else if (mode == kToolbarOverlay) {
            next = kToolbarHide;
        }
        SetToolbarMode(next);
    }
    for (MainWindow* w : gWindows) {
        ShowOrHideToolbar(w);
    }
    ScheduleSaveSettings();
}

// --- controller lifetime ----------------------------------------------------

// ng: orig hands the model to a scratch thread that waits for in-flight
// renders. Until the gpui canvas renders pages (step 7) nothing is in flight,
// so the cache is dropped and the controller deleted here.
void DeleteControllerAsync(DocController* ctrl) {
    if (!ctrl) {
        return;
    }
    DisplayModel* dm = ctrl->AsFixed();
    if (dm && gRenderCache) {
        dm->pauseRendering = true;
        gRenderCache->AbortRendering(dm);
        gRenderCache->CancelRenderingBlocking(dm);
        gRenderCache->FreeForDisplayModel(dm);
    }
    delete ctrl;
}

void UpdateTabFileDisplayStateForTab(WindowTab* tab) {
    if (!tab || !tab->ctrl || tab->skipHistory) {
        return;
    }
    RememberDefaultWindowPosition(tab->win);
    FileState* fs = FileHistoryFindByPath(tab->filePath);
    if (!fs) {
        return;
    }
    tab->ctrl->GetDisplayState(fs);
    fs->windowState = gSettings->windowState;
    fs->windowPos = gSettings->windowPos;
    fs->showToc = tab->showToc;
    str::ReplaceWithCopy(&fs->sidebarView, SidebarContentToStr(tab->sidebarContent));
    *fs->tocState = tab->tocState;
}

// ng: gpui reports a window's size but not its position (see "gpui gaps").
// On Windows the frame's own rectangle is read from its HWND, as orig does;
// elsewhere x/y keep whatever the settings file had and only the size is
// refreshed.
void RememberDefaultWindowPosition(MainWindow* win) {
    if (!win) {
        return;
    }
    if (win->InPresentation()) {
        gSettings->windowState = win->windowStateBeforePresentation;
    } else if (win->isFullScreen) {
        gSettings->windowState = WIN_STATE_FULLSCREEN;
    } else if (win->isMaximized) {
        gSettings->windowState = WIN_STATE_MAXIMIZED;
    } else {
        gSettings->windowState = WIN_STATE_NORMAL;
    }
    // orig's SaveCurrentWindowTab writes the sidebar width whatever the window
    // state is
    if (win->sidebarDx > 0) {
        gSettings->sidebarDx = win->sidebarDx;
    }
#if OS_WIN
    // where the frame is, or where it goes back to when it is maximized, so we
    // know which monitor the window is on
    Rect normal;
    if (!win->InPresentation() && AppShellNormalWindowRect(win, &normal) && !normal.IsEmpty()) {
        gSettings->windowPos = normal;
    }
#else
    if (gSettings->windowState != WIN_STATE_NORMAL) {
        return;
    }
    // frameRc is what the shell saw on its last frame, so this works after the
    // window itself is gone
    if (win->frameRc.dx > 0 && win->frameRc.dy > 0) {
        gSettings->windowPos.dx = win->frameRc.dx;
        gSettings->windowPos.dy = win->frameRc.dy;
    }
#endif
}

// --- window title and menu --------------------------------------------------

constexpr const char* kSumatraWindowTitle = "SumatraPDF";

// orig's SetFrameTitleForTab: the path (GetTabTitle() follows FullPathInTitle
// and names an embedded file), then the document's title
static TempStr WindowTitleTemp(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    Str titlePath = tab ? tab->GetTabTitle() : Str{};
    if (len(titlePath) == 0) {
        return str::DupTemp(Str(kSumatraWindowTitle));
    }

    TempStr docTitle = StrL("");
    if (tab->ctrl) {
        // NormalizeWSTemp (not in-place): GetPropertyTemp() may return a string
        // owned by the document, which we must not mutate
        TempStr title = str::NormalizeWSTemp(tab->ctrl->GetPropertyTemp(DocProp::Title));
        if (len(title) > 0) {
            docTitle = fmt("- [%s] ", title);
        }
    }

    if (!IsUIRtl()) {
        return fmt("%s %s- %s", titlePath, docTitle, Str(kSumatraWindowTitle));
    }
    // explicitly revert the title, so that filenames aren't garbled
    return fmt("%s %s- %s", Str(kSumatraWindowTitle), docTitle, titlePath);
}

void UpdateWindowTitle(MainWindow* win) {
    TempStr title = WindowTitleTemp(win);
    WindowTab* tab = win->CurrentTab();
    if (tab) {
        str::ReplaceWithCopy(&tab->frameTitle, title);
    }
    AppShellSetTitle(win, title);
}

void RebuildMenuBar(MainWindow* win) {
    DeleteMenuModel(win->menu);
    win->menu = BuildMenu(win);
    AppShellMenuRebuilt(win);
}

void SelectTabInWindow(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    MainWindow* win = tab->win;
    if (tab == win->CurrentTab()) {
        return;
    }
    TabsSelect(win, win->GetTabIdx(tab));
}

// Find the first window showing a given file.
// note: background tabs are only searched if focusTab is true
// when limitWin is set, only that window's tabs are considered
// ng: orig also consults gMostRecentlyOpenedDoc, which this port doesn't keep
MainWindow* FindMainWindowByFile(Str file, bool focusTab, MainWindow* limitWin) {
    WindowTab* tab = FindTabByFilePath(file, limitWin);
    if (!tab) {
        return nullptr;
    }
    if (focusTab) {
        SelectTabInWindow(tab);
    }
    return tab->win;
}

MainWindow* FindMainWindowBySyncFile(Str path, bool focusTab) {
    for (MainWindow* win : gWindows) {
        Vec<Rect> rects;
        int page;
        auto* dm = win->AsFixed();
        if (dm && dm->pdfSync && dm->pdfSync->SourceToDoc(path, 0, 0, &page, rects) != PDFSYNCERR_UNKNOWN_SOURCEFILE) {
            return win;
        }
        bool bringFore = focusTab && win->TabCount() > 1;
        if (!bringFore) {
            continue;
        }
        // bring a background tab to the foreground
        for (WindowTab* tab : win->Tabs()) {
            if (tab != win->CurrentTab() && tab->AsFixed() && tab->AsFixed()->pdfSync &&
                tab->AsFixed()->pdfSync->SourceToDoc(path, 0, 0, &page, rects) != PDFSYNCERR_UNKNOWN_SOURCEFILE) {
                TabsSelect(win, win->GetTabIdx(tab));
                return win;
            }
        }
    }
    return nullptr;
}

// --- thumbnails (orig's, from SumatraPDF.cpp) --------------------------------

static bool ShouldSaveThumbnail(FileState* ds) {
    // don't create thumbnails if we won't be needing them at all
    if (!HasPermission(Perm::SavePreferences)) {
        return false;
    }

    // don't materialize (hydrate) a cloud-only placeholder file just to make a
    // thumbnail. opening it would force a slow, possibly multi-minute download
    // (e.g. OneDrive "Files On-Demand" dehydrated file). issue #5756
    if (path::IsCloudPlaceholder(ds->filePath)) {
        logf("ShouldSaveThumbnail: skipping cloud placeholder '%s'\n", ds->filePath);
        return false;
    }

    // don't create thumbnails for files that won't need them anytime soon
    Vec<FileState*> list;
    if (gSettings->homePageSortByFrequentlyRead) {
        FileHistoryGetFrequencyOrder(list);
    } else {
        FileHistoryGetRecentlyOpenedOrder(list);
    }
    int idx = VecFind(list, ds);
    if (idx < 0) {
        return false;
    }

    if (HasThumbnail(ds)) {
        return false;
    }
    return true;
}

struct CreateThumbnailFromFileData {
    Str filePath;
    // when set, render from this clone of the open document instead of loading
    // the file again (owned)
    EngineBase* engine = nullptr;
    Pixmap* bmp = nullptr;
    // the thumbnail is rendered off the UI thread, so the per-document ebook
    // settings have to come along as a copy (#4600)
    FileEBookUI* fileEBookUI = nullptr;
    ~CreateThumbnailFromFileData() {
        str::Free(filePath);
        SafeEngineRelease(&engine);
        FreePixmap(bmp);
        DeleteFileEBookUI(fileEBookUI);
    }
};

EBookUI* GetEBookUI() {
    if (!gSettings) {
        return nullptr;
    }
    return &gSettings->eBookUI;
}

// A thumbnail is rendered on a worker thread, where walking the file
// history (only ever touched on the UI thread) would race with it changing.
// Such loads copy the settings before leaving the UI thread and park the copy
// here for the engine to pick up.
static thread_local FileEBookUI* gLoadThreadFileEBookUI;

static void SetLoadThreadFileEBookUI(FileEBookUI* v) {
    gLoadThreadFileEBookUI = v;
}

// per-document overrides of the ebook settings, null unless this document has
// an EBookUI block in FileStates (#4600)
FileEBookUI* GetFileEBookUI(Str filePath) {
    if (!uitask::IsMainUIThread()) {
        return gLoadThreadFileEBookUI;
    }
    if (!gSettings || len(filePath) == 0) {
        return nullptr;
    }
    FileState* fs = FileHistoryFindByPath(filePath);
    return fs ? fs->eBookUI : nullptr;
}

static void CreateThumbnailFromFileFinish(CreateThumbnailFromFileData* d) {
    if (d->bmp) {
        FileState* fs = FileHistoryFindByPath(d->filePath);
        SetThumbnail(fs, d->bmp);
        d->bmp = nullptr;
        MaybeRedrawHomePage();
    }
    delete d;
}

// Prefer a Calibre-style sibling cover image to the document's first page.
static TempStr FindCoverImageTemp(Str docPath) {
    static const char* kCoverExts[] = {".jpg", ".jpeg", ".png"};
    TempStr noExt = path::GetPathNoExtTemp(docPath);
    for (const char* ext : kCoverExts) {
        TempStr cover = str::JoinTemp(noExt, Str(ext));
        if (!str::EqI(cover, docPath) && file::Exists(cover)) {
            return cover;
        }
    }
    return {};
}

static void CreateThumbnailFromFileThread(CreateThumbnailFromFileData* d) {
    EngineBase* engine = d->engine;
    TempStr cover = FindCoverImageTemp(d->filePath);
    if (len(cover) > 0) {
        SafeEngineRelease(&d->engine);
        engine = CreateEngineFromFile(cover, nullptr, true);
    }
    if (!engine && GuessFileTypeFromName(d->filePath, true) == FileType::Epub) {
        Str coverData = EpubCoverImageData(d->filePath);
        if (len(coverData) > 0) {
            engine = CreateEngineImageFromData(coverData);
        }
        str::Free(coverData);
    }
    if (!engine) {
        SetLoadThreadFileEBookUI(d->fileEBookUI);
        engine = CreateEngineFromFile(d->filePath, nullptr, true);
        SetLoadThreadFileEBookUI(nullptr);
    }
    if (!engine) {
        delete d;
        DestroyTempArena();
        return;
    }
    RectF pageRect = engine->PageMediabox(1);
    if (pageRect.IsEmpty()) {
        engine->Release();
        d->engine = nullptr;
        delete d;
        DestroyTempArena();
        return;
    }
    pageRect = engine->Transform(pageRect, 1, 1.0f, 0);
    float zoom = (float)kThumbnailDx / pageRect.dx;
    pageRect.dy = std::min(pageRect.dy, (float)kThumbnailDy / zoom);
    pageRect = engine->Transform(pageRect, 1, 1.0f, 0, true);
    RenderPageArgs args(1, zoom, 0, &pageRect);
    d->bmp = engine->RenderPage(args);
    engine->Release();
    d->engine = nullptr;
    auto fn = MkFunc0<CreateThumbnailFromFileData>(CreateThumbnailFromFileFinish, d);
    uitask::Post(fn, "SetThumbnailFromFile");
    DestroyTempArena();
}

// create a thumbnail by loading the file with a temporary engine; independent
// of the tab lifecycle, so it works even if the tab is closed before the
// render completes
static void CreateThumbnailFromFileAsync(FileState* ds, EngineBase* engine) {
    auto* d = new CreateThumbnailFromFileData();
    d->filePath = str::Dup(ds->filePath);
    d->engine = engine;
    d->fileEBookUI = CopyFileEBookUI(ds->eBookUI);
    auto fn = MkFunc0<CreateThumbnailFromFileData>(CreateThumbnailFromFileThread, d);
    RunAsync(fn, StrL("CreateThumbnailFromFile"));
}

static void CreateThumbnailForFile(MainWindow* win, FileState* ds) {
    if (!ds || !ShouldSaveThumbnail(ds)) {
        return;
    }

    // don't create thumbnails for password protected documents
    // (unless we're also remembering the decryption key anyway)
    if (win->IsDocLoaded()) {
        auto* model = win->AsFixed();
        if (model) {
            auto* engine = model->GetEngine();
            bool withPwd = engine->isPasswordProtected;
            Str decrKey = engine->decryptionKey;
            if (withPwd && len(decrKey) == 0) {
                RemoveThumbnail(ds);
                return;
            }
            // save decryption key to file history so the thumbnail thread can use it
            if (decrKey && !str::Eq(ds->decryptionKey, decrKey)) {
                str::ReplaceWithCopy(&ds->decryptionKey, decrKey);
            }
        }
    }

    // Reopening a converted file can take seconds, so hand the thread a clone
    // of the engine we already have.
    EngineBase* clone = nullptr;
    DisplayModel* dm = win->IsDocLoaded() ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine && (engine->kind == kindEnginePostScript || engine->kind == kindEngineDvi) &&
        str::Eq(engine->FilePath(), ds->filePath)) {
        clone = engine->Clone();
    }
    CreateThumbnailFromFileAsync(ds, clone);
}

// --- loading a document -----------------------------------------------------

// ng: orig's HwndPasswordUI, made asynchronous. orig's blocks in a nested
// message loop until the user answers; a gpui frame cannot, so this one hands
// back the passwords it already knows (the remembered decryption key, -password
// from the command line, DefaultPasswords, and whatever the user typed for this
// file earlier in this session) and, when it runs out, only records that a
// password is wanted. LoadDocument then shows the dialog and loads again.
bool IsStressTesting();

struct SumatraPasswordUI : PasswordUI {
    Str path;
    int pwdIdx = 0;
    bool triedCliPwd = false;
    bool triedSessionPwd = false;
    bool needsPassword = false;
    bool canAsk = true;
    bool rememberPassword = false;

    Str GetPassword(Str path, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) override;
};

// the passwords the user typed this session, so a reload does not ask again
struct SessionPassword {
    Str path;
    Str password;
    bool remember = false;
};
static Vec<SessionPassword*> gSessionPasswords;

static SessionPassword* FindSessionPassword(Str path) {
    for (SessionPassword* sp : gSessionPasswords) {
        if (path::IsSame(sp->path, path)) {
            return sp;
        }
    }
    return nullptr;
}

static void RememberSessionPassword(Str path, Str password, bool remember) {
    SessionPassword* sp = FindSessionPassword(path);
    if (!sp) {
        sp = new SessionPassword();
        sp->path = str::Dup(path);
        VecAppend(gSessionPasswords, sp);
    }
    str::ReplaceWithCopy(&sp->password, password);
    sp->remember = remember;
}

void FreeSessionPasswords() {
    for (SessionPassword* sp : gSessionPasswords) {
        str::Free(sp->path);
        str::Free(sp->password);
        delete sp;
    }
    VecReset(gSessionPasswords);
}

Str SumatraPasswordUI::GetPassword(Str filePath, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) {
    FileState* fileFromHistory = FileHistoryFindByPath(filePath);
    if (fileFromHistory && fileFromHistory->decryptionKey && fileDigest && decryptionKeyOut) {
        TempStr fingerprint = str::MemToHexTemp(Str((const char*)fileDigest, 16));
        Str decryptionKey = fileFromHistory->decryptionKey;
        *saveKey = str::TrimPrefix(decryptionKey, fingerprint);
        if (*saveKey && str::HexToMem(decryptionKey, Str((char*)decryptionKeyOut, 32))) {
            return {};
        }
    }

    *saveKey = false;

    if (!triedSessionPwd) {
        triedSessionPwd = true;
        SessionPassword* sp = FindSessionPassword(filePath);
        if (sp) {
            *saveKey = sp->remember;
            return str::Dup(sp->password);
        }
    }

    if (!triedCliPwd && gFlags && gFlags->password) {
        triedCliPwd = true;
        return str::Dup(gFlags->password);
    }

    // try the list of default passwords before asking the user
    if (pwdIdx < len(*gSettings->defaultPasswords)) {
        Str pwd = (*gSettings->defaultPasswords)[pwdIdx++];
        return str::Dup(pwd);
    }

    if (IsStressTesting() || !canAsk) {
        return {};
    }
    needsPassword = true;
    return {};
}

// Markdown and HTML both render in the WebView2 browser view via MarkdownModel,
// each gated by its own UseFixedPageUI opt-out (fall back to MuPDF/ebook engines).
static bool ShouldUseBrowserView(FileType kind) {
    if (MarkdownModel::IsHtmlFileType(kind)) {
        return !gSettings->htmlUI.useFixedPageUI;
    }
    if (MarkdownModel::IsSupportedFileType(kind)) {
        return !gSettings->markdownUI.useFixedPageUI;
    }
    return false;
}

// ng: orig's CreateControllerForEngineOrFile. A CHM opens in the browser view
// unless ChmUI.UseFixedPageUI is set or no embedded browser is installed, in
// which case the fixed-page ChmEngine renders it, as orig falls back too.
// Markdown / HTML fall back to their fixed-page engines the same way (orig's
// CreateControllerForMarkdown, when SetParentWindow() fails).
static DocController* CreateControllerForFile(MainWindow* win, Str path, PasswordUI* pwdUI) {
    auto timeStart = TimeGet();
    FileType kind = GuessFileTypeFromName(path);
    bool chmInFixedUI = gSettings->chmUI.useFixedPageUI || !BrowserViewAvailable();

    bool haveBrowser = BrowserViewAvailable();
    if (ShouldUseBrowserView(kind) && !haveBrowser) {
        log(StrL("CreateControllerForFile: no browser view, falling back to the fixed-page markdown / html view\n"));
    }
    if (ShouldUseBrowserView(kind) && haveBrowser) {
        MarkdownModel* md = MarkdownModel::Create(path, win->cbHandler);
        if (md) {
            return md;
        }
    }

    EngineBase* engine = CreateEngineFromFile(path, pwdUI, chmInFixedUI);
    if (!engine) {
        // as a last resort, try to open as a chm file. Without a browser
        // view ChmEngine was the fallback and it has just failed (orig's
        // CreateControllerForChm), so the tab shows the load error
        ChmModel* chm = haveBrowser ? ChmModel::Create(path, win->cbHandler) : nullptr;
        if (chm) {
            return chm;
        }
        logf("CreateControllerForFile: failed to load '%s'\n", path);
        return nullptr;
    }
    int nPages = engine->pageCount;
    auto dur = TimeSinceInMs(timeStart);
    logf("CreateControllerForFile: '%s', %d pages, took %.2f ms\n", path, nPages, dur);
    if (nPages <= 0) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return new DisplayModel(engine, win->cbHandler);
}

static bool IsEbookFileType(FileType ft) {
    return ft == FileType::Epub || ft == FileType::Mobi || ft == FileType::Fb2 || ft == FileType::Fb2z ||
           ft == FileType::PalmDoc || ft == FileType::HTML || ft == FileType::Txt || ft == FileType::Lit;
}

static DisplayMode DisplayModeForNewDocument(Str path, EngineBase* engine) {
    DisplayMode dm = gSettings->defaultDisplayModeEnum;
    Str modeStr;
    Kind k = engine ? engine->kind : nullptr;
    if (k == kindEngineComicBooks || k == kindEngineImageDir ||
        (path && IsEngineCbxSupportedFileType(GuessFileTypeFromName(path, true)))) {
        modeStr = gSettings->comicBookUI.defaultDisplayMode;
    } else if (k == kindEngineEpub || k == kindEngineFb2 || k == kindEngineMobi || k == kindEnginePdb ||
               k == kindEngineHtml || (path && IsEbookFileType(GuessFileTypeFromName(path, true)))) {
        modeStr = gSettings->eBookUI.defaultDisplayMode;
    }
    if (modeStr) {
        return DisplayModeFromString(modeStr, dm);
    }
    return dm;
}

// First open only: ComicBookUI.DefaultZoom when set (issue #5946). Empty
// keeps fit page, the historical comic default - not the global DefaultZoom.
static float ZoomForNewDocument(Str path, EngineBase* engine, float fallback) {
    Kind k = engine ? engine->kind : nullptr;
    if (k == kindEngineComicBooks || (path && IsEngineCbxSupportedFileType(GuessFileTypeFromName(path, true)))) {
        float z = gSettings->comicBookUI.defaultZoomFloat;
        if (z != 0) {
            return z;
        }
        return kZoomFitPage;
    }
    return fallback;
}

// First open only: papers use continuous fit-width; slides use single-page
// fit-page (issue #4055).
static bool ShouldUsePageAspect(Str path) {
    if (!IsPageAspectDisplayMode(gSettings->defaultDisplayMode)) {
        return false;
    }
    FileType ft = GuessFileTypeFromName(path, true);
    return ft == FileType::PDF || ft == FileType::Xps || ft == FileType::DjVu || ft == FileType::PS ||
           ft == FileType::Dvi;
}

static void ApplyPageAspect(EngineBase* engine, DisplayMode* modeOut, float* zoomOut) {
    if (!engine || engine->PageCount() < 1) {
        return;
    }
    GetPageAspectView(engine->PageMediabox(1), modeOut, zoomOut);
}

// orig's, from LoadDocumentFinish
static bool showTocByDefault(Str path, EngineBase* engine) {
    if (gSettings->alwaysShowSidebar) {
        return true;
    }
    if (!gSettings->showToc) {
        return false;
    }
    // Comic bookmarks are usually file names; only show ComicInfo.xml ones.
    FileType kind = GuessFileTypeFromName(path);
    if (!IsEngineCbxSupportedFileType(kind)) {
        return true;
    }
    return EngineCbxHasComicInfoToc(engine);
}

// --- reload on file change (orig's file watcher half) -----------------------

// orig re-arms a WM_TIMER on the canvas; the shell's tick counts this down
constexpr int kAutoReloadDelayInMs = 100;
// give up waiting for a writer to go quiet after this long
constexpr u64 kAutoReloadMaxWaitMs = 4000;

static void AutoReloadResetFileState(WindowTab* tab) {
    tab->autoReloadSize = -1;
    tab->autoReloadModTime = {};
    tab->autoReloadStartMs = 0;
}

// orig's AutoReloadFileStillChanging: true while the file keeps changing, i.e.
// whoever writes it isn't done. The caller waits instead of reloading a
// half-written document.
static bool AutoReloadFileStillChanging(WindowTab* tab) {
    if (!tab || len(tab->filePath) == 0) {
        return false;
    }
    u64 now = GetTickCount64();
    if (tab->autoReloadStartMs == 0) {
        tab->autoReloadStartMs = now;
    } else if (now - tab->autoReloadStartMs > kAutoReloadMaxWaitMs) {
        logf("AutoReloadFileStillChanging: '%s' still changing after %d ms, reloading anyway\n", tab->filePath,
             (int)(now - tab->autoReloadStartMs));
        AutoReloadResetFileState(tab);
        return false;
    }

    i64 size = file::GetSize(tab->filePath);
    FILETIME modTime = file::GetModificationTime(tab->filePath);
    bool changed = (size != tab->autoReloadSize) || !FileTimeEq(modTime, tab->autoReloadModTime);
    tab->autoReloadSize = size;
    tab->autoReloadModTime = modTime;
    if (changed) {
        return true;
    }
    AutoReloadResetFileState(tab);
    return false;
}

// orig's ReloadTab: runs on the UI thread, arms the delay, reloads when the
// tab is the current one
static void ReloadTab(WindowTab* tab) {
    MainWindow* win = FindMainWindowByTab(tab);
    if (!win) {
        return;
    }
    logf("ReloadTab: '%s'\n", tab->filePath);
    tab->reloadOnFocus = true;
    if (tab == win->CurrentTab()) {
        tab->autoReloadLeftMs = kAutoReloadDelayInMs;
    }
}

// the watcher calls this on its own thread
static void ScheduleReloadTab(WindowTab* tab) {
    uitask::Post(MkFunc0<WindowTab>(ReloadTab, tab), "ReloadTab");
}

// orig's FileWatcherSubscribe from LoadDocumentFinish
static void WatchTabFile(WindowTab* tab) {
    FileWatcherUnsubscribe(tab->watcher);
    tab->watcher = nullptr;
    if (!gSettings->reloadModifiedDocuments || len(tab->filePath) == 0) {
        return;
    }
    if (IsOpenCachePath(tab->filePath) || path::IsEphemeralHostFile(tab->filePath)) {
        return;
    }
    auto fn = MkFunc0<WindowTab>(ScheduleReloadTab, tab);
    tab->watcher = FileWatcherSubscribe(tab->filePath, fn, true);
}

// ng: orig's kAutoReloadTimerID handler in Canvas.cpp. The shell's 16 ms tick
// drives it instead of a WM_TIMER.
void AutoReloadTick(MainWindow* win, int elapsedMs) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || tab->autoReloadLeftMs <= 0) {
        return;
    }
    tab->autoReloadLeftMs -= elapsedMs;
    if (tab->autoReloadLeftMs > 0) {
        return;
    }
    tab->autoReloadLeftMs = 0;
    if (!tab->reloadOnFocus) {
        return;
    }
    if (tab->ignoreNextAutoReload) {
        // consume the save-triggered watcher event
        tab->ignoreNextAutoReload = false;
        tab->reloadOnFocus = false;
        return;
    }
    // an open menu still holds the page element it was built from. Reload
    // once it is gone, as orig does while TrackPopupMenu is running.
    if (win->gpuiWin && IsTrackedPopupOpenInApp(win->gpuiWin->app)) {
        tab->autoReloadLeftMs = kAutoReloadDelayInMs;
        return;
    }
    if (AutoReloadFileStillChanging(tab)) {
        // a writer (LaTeX etc.) is still producing the file; wait for it to go
        // quiet instead of showing a half-written document
        tab->autoReloadLeftMs = kAutoReloadDelayInMs;
        return;
    }
    ReloadDocument(win, true);
}

// ng: orig's ReloadDocument, minus the password dialog (step 11), the
// annotation bookkeeping (step 13) and the window placement (gpui cannot move
// a window). The remembered state is carried over through FileState, as orig
// does.
void ReloadDocument(MainWindow* win, bool autoRefresh) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || tab->IsNonDocumentTab()) {
        return;
    }
    Str path = tab->filePath;
    if (len(path) == 0) {
        logf("ReloadDocument: tab->filePath is empty, auto refresh: %d\n", (int)autoRefresh);
        return;
    }
    if (!tab->IsDocLoaded()) {
        if (!autoRefresh) {
            LoadDocument(win, path, LoadPrefs::DontSave, LoadReuse::CurrentTab);
        }
        return;
    }
    logf("ReloadDocument: %s, auto refresh: %d\n", path, (int)autoRefresh);
    auto timeStart = TimeGet();
    // the annotation UI holds Annotation* into the engine that is about to go
    CloseAnnotationUiForTab(tab);

    // remember where we are before the controller is replaced. LoadDocument
    // reads it back out of the file history.
    FileState* fs = FileHistoryFindByPath(path);
    if (fs) {
        tab->ctrl->GetDisplayState(fs);
        fs->showToc = tab->showToc;
        *fs->tocState = tab->tocState;
        fs->useDefaultState = false;
    }
    // the page / zoom / scroll position to come back to, whatever
    // RememberStatePerDocument says
    TabState* state = NewTabStateFromTab(tab);
    tab->reloadOnFocus = false;
    AutoReloadResetFileState(tab);
    TempStr pathCopy = str::DupTemp(path);
    LoadDocument(win, pathCopy, LoadPrefs::DontSave, LoadReuse::CurrentTab);
    // the annotation list was emptied above; fill it from the new engine
    RefreshEditAnnotationsAfterEngineChange(win->CurrentTab());
    if (state) {
        SetTabState(win->CurrentTab(), state);
        DeleteTabState(state);
    }
    // reopening reads the file again: slow on a network / cloud drive
    logf("ReloadDocument: reloaded in %.1f ms\n", TimeSinceInMs(timeStart));
}

// ng: the other half of the asynchronous password prompt. The dialog's answer
// goes into the session store, which SumatraPasswordUI hands the engine on the
// next try; Cancel gives orig's "Error loading %s" notification.
struct PasswordRetry {
    MainWindow* win = nullptr;
    Str path;
    LoadPrefs prefs = LoadPrefs::Save;
    LoadReuse reuse = LoadReuse::NewTab;
};

static void OnPasswordEntered(PasswordRetry* retry, PasswordDialogResult* res) {
    MainWindow* win = retry->win;
    TempStr path = str::DupTemp(retry->path);
    LoadPrefs prefs = retry->prefs;
    LoadReuse reuse = retry->reuse;
    bool accepted = res->accepted;
    if (accepted) {
        RememberSessionPassword(path, res->password, res->rememberPassword);
    }
    str::Free(retry->path);
    delete retry;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (!accepted) {
        TempStr msg = fmt(Tr("Error loading %s").s, path);
        ShowWarningNotification(win, msg, kNotif5SecsTimeOut);
        AppShellInvalidate(win);
        return;
    }
    LoadDocument(win, path, prefs, reuse);
}

static void AskForPasswordAndLoad(MainWindow* win, Str path, LoadPrefs prefs, LoadReuse reuse) {
    auto* retry = new PasswordRetry();
    retry->win = win;
    retry->path = str::Dup(path);
    retry->prefs = prefs;
    retry->reuse = reuse;
    // orig: remembering the password requires saving per-document state
    bool canRememberPwd = SettingsRememberOpenedFiles() && gSettings->rememberStatePerDocument;
    TempStr baseName = path::GetBaseNameTemp(path);
    logf("AskForPasswordAndLoad: '%s'\n", path);
    ShowGetPasswordDialog(win, baseName, canRememberPwd, MkFunc1(OnPasswordEntered, retry));
}

#if OS_WIN
// Kind used so only one default-app bar is shown at a time
static Kind kNotifDefaultApp = "defaultApp";
// Cap how many extension links we put in the bar
constexpr int kMaxDefaultAppLinks = 8;

// On the home page: if we registered as Open With for extensions that no longer
// open with us, show a bottom bar with per-extension fix links.
static void MaybeShowDefaultAppNotification(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (!win->IsCurrentTabAbout()) {
        return;
    }
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }
    if (!IsOurExeInstalled()) {
        return;
    }

    StrVec missing;
    CollectNonDefaultRegisteredExtensions(missing);
    logf("MaybeShowDefaultAppNotification: %d extension(s) not ours\n", len(missing));
    ShowDefaultAppNotification(win, missing);
}

// ng: split from MaybeShowDefaultAppNotification so the bar can be shown
// without a real installation (-dbg-control TestDefaultAppNotif)
void ShowDefaultAppNotification(MainWindow* win, const StrVec& missing) {
    if (len(missing) == 0) {
        RemoveNotificationsForGroup(win, kNotifDefaultApp);
        return;
    }

    // "SumatraPDF is no longer the default app for opening [pdf](CmdFixDefaultApp .pdf), ..."
    str::Builder sb;
    sb.Append(StrL("SumatraPDF is no longer the default app for opening "));
    int nShow = std::min(len(missing), kMaxDefaultAppLinks);
    for (int i = 0; i < nShow; i++) {
        if (i > 0) {
            sb.Append(StrL(", "));
        }
        Str ext = missing[i]; // ".pdf"
        // link text without the leading dot: "pdf"
        Str label = (len(ext) > 0 && ext.s[0] == '.') ? Str(ext.s + 1, ext.len - 1) : ext;
        sb.Append(fmt("[%s](CmdFixDefaultApp %s)", label, ext));
    }
    if (len(missing) > nShow) {
        sb.Append(fmt(" and %d more", len(missing) - nShow));
    }
    sb.Append(StrL(". Click a link to fix."));

    NotificationCreateArgs args;
    args.win = win;
    args.msg = ToStrTemp(sb);
    args.timeoutMs = kNotifNoTimeout;
    args.groupId = kNotifDefaultApp;
    args.corner = NotifCorner::BottomBar;
    ShowNotification(args);
}
#endif

static Kind kNotifPersistentWarning = "persistentWarning";
static Kind kNotifDocErrors = "docErrors";

// a warning about the document that just loaded: bottom-right, small margins,
// 16s timeout, shown only while `tab` is current
static void ShowLoadWarning(MainWindow* win, WindowTab* tab, Kind groupId, Str msg, bool plainText) {
    NotificationCreateArgs nargs;
    nargs.win = win;
    nargs.warning = true;
    nargs.timeoutMs = 16 * 1000; // auto-dismiss after 16 seconds
    nargs.groupId = groupId;
    nargs.msg = msg;
    nargs.plainText = plainText;
    // Bind to the tab that just loaded (not whatever is current after a
    // later multi-file finish steals the UI).
    nargs.tab = tab;
    nargs.corner = NotifCorner::BottomRight;
    nargs.xMargin = 2;
    nargs.yMargin = 2;
    ShowNotification(nargs);
}

// orig's LoadDocumentFinish: what the user should know about the document
static void ShowLoadWarnings(MainWindow* win, WindowTab* tab) {
    if (!win->IsDocLoaded()) {
        return;
    }
    TempStr unsupported = win->ctrl->GetPropertyTemp(DocProp::UnsupportedFeatures);
    if (unsupported) {
        Str s = Tr("%s not supported");
        // `unsupported` is a document property
        ShowLoadWarning(win, tab, kNotifPersistentWarning, fmt(s.s, unsupported), true);
    }

    // if the document had parsing errors (the same condition that adds "Show
    // Errors" to the context menu), surface it with a notification whose
    // "Errors" link opens the Show Errors dialog
    DisplayModel* dmErr = win->AsFixed();
    EngineBase* engineErr = dmErr ? dmErr->GetEngine() : nullptr;
    if (engineErr && engineErr->HasErrors()) {
        TempStr msg = fmt("[%s](CmdShowErrors) %s", Tr("Errors"), Tr("in document"));
        ShowLoadWarning(win, tab, kNotifDocErrors, msg, false);
    }

    // EBookUI.FontName names a font we can't load, so the ebook silently
    // rendered in the default font. Say so, otherwise the setting looks like
    // it does nothing (issue #4600)
    Str missingFont = EngineEbookFontUnavailable(engineErr);
    if (missingFont) {
        Str s = Tr("Font \"%s\" not found, using the default font");
        // the font name comes from the settings file
        ShowLoadWarning(win, tab, kNotifPersistentWarning, fmt(s.s, missingFont), true);
    }
}

// orig's OnFrameKeyEsc: Esc dismisses one group of tips at a time
bool DismissNotificationsOnEsc(MainWindow* win) {
    if (RemoveNotificationsForGroup(win, kNotifPersistentWarning)) {
        return true;
    }
    if (RemoveNotificationsForGroup(win, kNotifPageInfo)) {
        win->pageInfoWanted = false;
        return true;
    }
    if (RemoveNotificationsForGroup(win, kNotifCursorPos)) {
        return true;
    }
    return RemoveNotificationsForGroup(win, kNotifZoomOrView);
}

// Keys and wheels the browser page handles itself. Anything else stays with
// the frame (accelerators, find, and so on).
static bool BrowserNavMsg(UINT msg, WPARAM wp) {
#if OS_WIN
    if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
        return true;
    }
    if (msg != WM_KEYDOWN && msg != WM_KEYUP) {
        return false;
    }
    switch ((int)wp) {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_MULTIPLY:
        case VK_DIVIDE:
            return true;
    }
#else
    (void)msg;
    (void)wp;
#endif
    return false;
}

// A wheel or an arrow key over CHM or markdown arrives at the frame. The page
// scrolls from script; the host window does not.
bool ForwardBrowserMsg(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp) {
    if (!win || !BrowserNavMsg(msg, wp)) {
        return false;
    }
    BrowserDocController* doc = win->AsChm();
    if (!doc) {
        doc = win->AsMarkdown();
    }
    if (!doc) {
        return false;
    }
    doc->PassUIMsg(msg, wp, lp);
    return true;
}

// A failed open still gets a tab: the canvas shows the error, and the file
// stays available to "show in folder" (issue #3595). Page number stays 0.
static void ShowLoadErrorTab(MainWindow* win, Str fullPath, LoadReuse reuse) {
    WindowTab* tab = nullptr;
    if (reuse == LoadReuse::CurrentTab) {
        tab = win->CurrentTab();
        if (tab && tab->IsNonDocumentTab()) {
            tab = nullptr;
        }
    }
    if (tab) {
        HideFindBar(win);
        HideSelectionToolbar(win);
        bool hadReading = GetReadAloudSourceTab() == tab || CanContinueReadAloud(tab);
        ResetReadAloudStateForTab(tab);
        if (hadReading) {
            ShowTemporaryNotification(win, Tr("Reading stopped"), 2000);
        }
        ReadingAutoScrollHideBar(win);
        ReadingBarCancelDrag(win);
        DeleteOldSelectionInfo(win, true);
        DocController* prev = tab->ctrl;
        tab->ctrl = nullptr;
        win->ctrl = nullptr;
        win->currentTabTemp = nullptr;
        DeleteControllerAsync(prev);
        tab->SetFilePath(fullPath);
        tab->everPaintedPage = false;
        tab->loadState = WindowTab::LoadState::Error;
        win->currentTabTemp = tab;
    } else {
        SaveCurrentWindowTab(win);
        tab = new WindowTab(win);
        tab->SetFilePath(fullPath);
        tab->loadState = WindowTab::LoadState::Error;
        AddTabToWindow(win, tab);
        win->currentTabTemp = tab;
    }
    win->ctrl = nullptr;
    win->currPageNo = 0;
    ClearTocBox(win);
    InvalidateFindForDocumentChange(win);
    UpdateWindowTitle(win);
    RebuildMenuBar(win);
    TabsUIOnTabsChanged(win);
    AppShellInvalidate(win);
}

// ng: orig's LoadDocumentFinish, the half that puts the remembered FileState
// back on screen. What it drops is the window placement (gpui cannot move a
// window), the UIA notification and the browser-hosted (chm / markdown)
// branch, which has no scroll position to restore yet (step 11).
MainWindow* LoadDocument(MainWindow* win, Str path, LoadPrefs prefs, LoadReuse reuse) {
    if (gCrashOnOpen) {
        log(StrL("LoadDocument: about to call CrashMe()\n"));
        CrashMe();
    }
    if (!win || len(path) == 0) {
        return nullptr;
    }
    TempStr fullPath = path::NormalizeTemp(path);
    SumatraPasswordUI pwdUI;
    DocController* ctrl = CreateControllerForFile(win, fullPath, &pwdUI);
    if (!ctrl) {
        // ng: the engine asked for a password and had none left to try. orig
        // shows the dialog from inside the load; here the dialog answers later
        // and the load is run again with what the user typed
        if (pwdUI.needsPassword) {
            AskForPasswordAndLoad(win, fullPath, prefs, reuse);
            return nullptr;
        }
        TempStr msg = fmt(Tr("Error loading %s").s, fullPath);
        ShowWarningNotification(win, msg, kNotif5SecsTimeOut);
        ShowLoadErrorTab(win, fullPath, reuse);
        return nullptr;
    }

    // ng: orig's ReplaceDocumentInCurrentTab, reduced to what a reload and
    // next / prev file in folder need: the tab keeps its identity, its
    // controller is swapped
    WindowTab* reuseTab = win->CurrentTab();
    if (reuse == LoadReuse::CurrentTab && reuseTab && !reuseTab->IsNonDocumentTab()) {
        HideFindBar(win);
        HideSelectionToolbar(win);
        // orig's CloseDocumentInCurrentTab: the document the session reads is
        // about to be replaced
        bool hadReading = GetReadAloudSourceTab() == reuseTab || CanContinueReadAloud(reuseTab);
        ResetReadAloudStateForTab(reuseTab);
        if (hadReading) {
            ShowTemporaryNotification(win, Tr("Reading stopped"), 2000);
        }
        ReadingAutoScrollHideBar(win);
        ReadingBarCancelDrag(win);
        DeleteOldSelectionInfo(win, true);
        DocController* prev = reuseTab->ctrl;
        reuseTab->ctrl = nullptr;
        win->ctrl = nullptr;
        win->currentTabTemp = nullptr;
        DeleteControllerAsync(prev);
        reuseTab->SetFilePath(fullPath);
        reuseTab->ctrl = ctrl;
        reuseTab->everPaintedPage = false;
        reuseTab->loadState = WindowTab::LoadState::None;
        win->currentTabTemp = reuseTab;
        win->ctrl = ctrl;
        win->lastViewPortSize = Size{};
    } else {
        auto* tab = new WindowTab(win);
        tab->SetFilePath(fullPath);
        tab->ctrl = ctrl;
        SaveCurrentWindowTab(win);
        AddTabToWindow(win, tab);
        win->currentTabTemp = tab;
        win->ctrl = ctrl;
    }
    WindowTab* tab = win->CurrentTab();
    // the previous document's bookmarks and find state; both must run after
    // win->ctrl points at the new one
    ClearTocBox(win);
    InvalidateFindForDocumentChange(win);

    // Never load settings from a preexisting state if the user doesn't wish to
    FileState* fs = nullptr;
    if (gSettings->rememberStatePerDocument) {
        fs = FileHistoryFindByPath(fullPath);
    }
    if (fs && fs->useDefaultState) {
        fs = nullptr;
    }

    DisplayMode displayMode = gSettings->defaultDisplayModeEnum;
    float zoomVirtual = gSettings->defaultZoomFloat;
    ScrollState ss(1, -1, -1);
    int rotation = 0;
    DisplayModel* dmForToc = ctrl ? ctrl->AsFixed() : nullptr;
    bool showToc = showTocByDefault(fullPath, dmForToc ? dmForToc->GetEngine() : nullptr);

    if (fs) {
        // resolved to a real Location once win->ctrl exists, below
        ss.page = ParseStoredPagePos(fs->pageNo).pageNo;
        displayMode = DisplayModeFromString(fs->displayMode, DisplayMode::Automatic);
        showToc = fs->showToc || gSettings->alwaysShowSidebar;
        tab->sidebarContent = SidebarContentFromStr(fs->sidebarView, SidebarContent::Bookmarks);
        ParsedColor* bgParsed = GetPrefsColor(fs->bgCol);
        if (bgParsed->parsedOk) {
            tab->bgColor = bgParsed->col;
            tab->bgColorCheckered = (bgParsed->col == kColorUnset);
        }
        ParsedColor* tabColParsed = GetPrefsColor(fs->tabCol);
        if (tabColParsed->parsedOk) {
            tab->tabColor = tabColParsed->col;
        }
    }

    DisplayModel* dm = ctrl->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine) {
        engine->hideAnnotations = tab->hideAnnotations;
        float imageZoom = gSettings->imageUI.defaultZoomFloat;
        if (engine->kind == kindEngineImage && imageZoom != 0) {
            zoomVirtual = imageZoom;
        }
        // first open: EbookUI / ComicBookUI DefaultDisplayMode override the
        // global default when set. Remembered FileState wins.
        if (!fs) {
            displayMode = DisplayModeForNewDocument(fullPath, engine);
            zoomVirtual = ZoomForNewDocument(fullPath, engine, zoomVirtual);
            if (ShouldUsePageAspect(fullPath)) {
                ApplyPageAspect(engine, &displayMode, &zoomVirtual);
            }
        }
        // First open without per-file remembered state: honor PDF Catalog
        // /OpenAction when it is a safe internal GoTo (issue #1631)
        if (!fs && ss.page == 1) {
            int openPage = engine->GetOpenActionPageNo();
            if (openPage >= 1) {
                ss.page = openPage;
            }
        }
    }

    if (dm) {
        // ng: gpui lays out in 96-dpi dips and scales the frame itself, so the
        // document model works in dips too
        dm->SetUiDpi(96);
        if (fs) {
            dm->SetUniformPageWidth(fs->uniformPageWidth);
            dm->SetTrimEmptyMargins(fs->trimEmptyMargins);
            dm->SetFreePan(fs->freePan);
            // migrate in place only: a SaveSettings() here would rebuild
            // gInitialSessionData and free a TabState a restored tab borrows
            MigrateFileStatePagePos(ctrl, fs);
            StoredPagePos pos = ParseStoredPagePos(fs->pageNo);
            ss.page = PageNoFromStoredPagePos(ctrl, fs->pageNo);
            if (pos.bookmark) {
                ss.loc = ctrl->LocationFromPageNo(ss.page);
            }
        }
        if (engine && engine->HasChapters()) {
            int chapter = ss.loc.IsValid() ? ss.loc.chapter : 1;
            if (!engine->IsChapterLaidOut(chapter)) {
                engine->ChapterPageCount(chapter);
            }
        }
        // orig hands CustomScreenDPI (device pixels per inch) to the model in
        // place of the dpi Windows reports. ng: the model works in dips, which
        // gpui scales by the window's dpi, so the value is converted to them
        int dpi = 96;
        if (gSettings->customScreenDPI > 0) {
            // <= 0 means "not set" (users have been seen setting it to -1)
            int winDpi = std::max(AppShellWindowDpi(win), 1);
            dpi = (gSettings->customScreenDPI * 96 + winDpi / 2) / winDpi;
        }
        dm->SetInitialViewSettings(displayMode, ss.page, win->GetViewPortSize(), dpi);
        // SetInitialViewSettings() has just taken the direction from the
        // engine. A document that states its own keeps it: a remembered
        // `false` is indistinguishable from "never chosen".
        bool declared = engine->preferredLayout.r2lDeclared;
        if (fs && (fs->displayR2L || !declared)) {
            dm->SetDisplayR2L(fs->displayR2L);
        } else if (!fs && !declared) {
            dm->SetDisplayR2L(gSettings->comicBookUI.cbxMangaMode);
        }
    } else {
        ctrl->SetViewPortSize(win->GetViewPortSize());
    }

    if (fs) {
        zoomVirtual = ZoomFromString(fs->zoom, kZoomFitPage);
        if (ctrl->ValidPageNo(ss.page)) {
            if (kZoomFitContent != zoomVirtual && kZoomFitVisible != zoomVirtual) {
                ss.x = fs->scrollPos.x;
                ss.y = fs->scrollPos.y;
            }
            // else let Relayout() scroll to fit the page (again)
        } else if (ctrl->PageCount() > 0) {
            ss.page = limitValue(ss.page, 1, ctrl->PageCount());
        }
        rotation = fs->rotation;
        tab->tocState = *fs->tocState;
    }

    if (dm) {
        dm->Relayout(zoomVirtual, rotation);
        // canvas not sized yet: the first real layout applies this (orig does the same)
        if (dm->pendingRelayout) {
            dm->pendingScroll = ss;
            dm->hasPendingScroll = true;
        } else {
            dm->SetScrollState(ss);
        }
        // the canvas may still be 0x0; AppShell starts the count once it has a size
        win->chapterLayoutStarted = false;
    } else if (ctrl->PageCount() > 0) {
        ctrl->SetZoomVirtual(zoomVirtual, nullptr);
        ChmModel* chm = ctrl->AsChm();
        if (ctrl->AsMarkdown()) {
            // Markdown treats each .md in the directory as a "page". Opening
            // a specific file always starts on that file: FileState page /
            // scroll would jump to whichever .md was last viewed in the
            // folder. CHM still restores.
            int page = ctrl->CurrentPageNo();
            if (page < 1 || page > ctrl->PageCount()) {
                page = 1;
            }
            ctrl->GoToPage(page, false);
        } else if (chm && fs) {
            RectF r(fs->scrollPos.x, fs->scrollPos.y, 0, 0);
            chm->ScrollTo(limitValue(ss.page, 1, ctrl->PageCount()), r, kInvalidZoom);
        } else {
            ctrl->GoToPage(limitValue(ss.page, 1, ctrl->PageCount()), false);
        }
    }

    // a reuse load skipped CloseDocumentInCurrentTab, so the previous
    // document's watcher can still be set; always drop it before re-subscribing
    WatchTabFile(tab);

    if (SettingsRememberOpenedFiles() && !tab->skipHistory) {
        FileState* loaded = FileHistoryMarkFileLoaded(fullPath);
        if (gSettings->showStartPage) {
            CreateThumbnailForFile(win, loaded);
        }
    }

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    if (CanAccessDisk() && !gPluginMode && !IsStressTesting() && !tab->skipHistory) {
#if OS_WIN
        // ng: an automated run must not fill the user's real jump list
        if (gFlags && gFlags->forTesting) {
            logf("AddPathToRecentDocs: skipped for '%s' (-for-testing)\n", fullPath);
        } else {
            AddPathToRecentDocs(fullPath);
        }
#endif

        // Remove Zone.Identifier (Mark of the Web) so that Windows Explorer
        // will show previews/thumbnails for this file without security warnings
        file::DeleteZoneIdentifier(fullPath);
    }
    logf("LoadDocument: '%s', %d pages, page %d\n", fullPath, ctrl->PageCount(), ctrl->CurrentPageNo());

    win->currPageNo = ctrl->CurrentPageNo();
    if (win->InPresentation()) {
        // orig's LoadModelIntoTab: presentation mode has its own sidebar state
        showToc = tab->showTocPresentation;
    }
    SetSidebarVisibility(win, showToc, gSettings->showFavorites);

    UpdateWindowTitle(win);
    RebuildMenuBar(win);
    TabsUIOnTabsChanged(win);
    // keep / restore the page-info tip after a load or reload (issue #4454)
    ShowPageInfoIfWanted(win);
    ShowLoadWarnings(win, tab);
#if OS_WASM
    // opening a file should leave the keyboard on the document, the way the
    // frame HWND has it. A click that opened the file focused a button instead.
    AppShellFocusFrame(win);
#endif
    if (prefs == LoadPrefs::Save) {
        ScheduleSaveSettings();
    }
    StartPendingSearch(win);
    AppShellInvalidate(win);
    return win;
}

// orig's StartLoadDocuments: a file this window already shows is selected,
// not opened again; the rest are added in natural order and the first of
// them ends up the current tab
static void StartLoadDocuments(StrVec& paths, MainWindow* win, bool skipHistory) {
    StrVec pathsToLoad;
    for (Str path : paths) {
        if (SettingsUseTabs() && FindMainWindowByFile(path, true, win)) {
            continue;
        }
        AppendIfNotExists(&pathsToLoad, path);
    }
    SortNatural(&pathsToLoad);
    for (Str path : pathsToLoad) {
        logf("StartLoadDocuments: '%s'\n", path);
        WindowTab* tab = LoadDocument(win, path) ? win->CurrentTab() : nullptr;
        if (tab && skipHistory) {
            tab->skipHistory = true;
        }
    }
    if (len(pathsToLoad) > 1 && SettingsUseTabs() && IsMainWindowValidAndNotClosing(win)) {
        FindMainWindowByFile(pathsToLoad[0], true, win);
    }
}

void OpenDroppedFiles(MainWindow* win, const Str* paths, int n) {
    // orig doesn't register the canvas as a drop target then
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }
    StrVec pathsToLoad;
    for (int i = 0; i < n; i++) {
        pathsToLoad.Append(paths[i]);
    }
    StartLoadDocuments(pathsToLoad, win, false);
}

// --- more than one window ---------------------------------------------------

struct PlaceWindowArgs {
    MainWindow* win = nullptr;
    Rect pos;
    bool maximize = false;
};

static void PlaceWindowDeferred(PlaceWindowArgs* args) {
    AutoDelete delArgs(args);
    if (!IsMainWindowValidAndNotClosing(args->win) || args->win->isQuickLook) {
        return;
    }
    AppShellPlaceWindow(args->win, args->pos, args->maximize);
}

// ng: moving a frame makes gpui render it at once, and a render resets the
// temp arena; a caller that still holds a TempStr (a path it is about to
// load) gets the move after it has returned to the message loop
void PlaceMainWindowLater(MainWindow* win, Rect pos, bool maximize) {
    auto* args = new PlaceWindowArgs{win, pos, maximize};
    uitask::Post(MkFunc0(PlaceWindowDeferred, args), "PlaceWindow");
}

// orig's CreateMainWindow + CreateAndShowMainWindow, the placement half: the
// frame goes where the session (`data`) or the settings remember it, kept on
// a screen, and is maximized if it was.
// ng: gpui opens a window at a size only, so this moves the frame it made;
// that needs the native handle, which only Windows gives us (see "gpui gaps")
void PlaceMainWindow(MainWindow* win, SessionData* data, PlaceWindowWhen when) {
    if (!win || gPluginMode || win->isQuickLook) {
        return;
    }
    int windowState = data ? data->windowState : gSettings->windowState;
    Rect pos = data ? data->windowPos : gSettings->windowPos;
    // -window-pos wins over the remembered position and state, and skips the
    // per-window shift: a test asked for an exact rectangle
    bool fixedPos = !data && gFlags && !gFlags->windowPos.IsEmpty();
    if (fixedPos) {
        pos = gFlags->windowPos;
        windowState = WIN_STATE_NORMAL;
    }
    // a settings file from before the port could read a window's position has
    // x = y = 0 and the client size: such a window stays where gpui put it
    bool hasPos = fixedPos || (!pos.IsEmpty() && (pos.x != 0 || pos.y != 0));
#if OS_WIN
    if (!hasPos) {
        pos = {};
    } else if (data) {
        pos = ShiftRectToWorkArea(pos);
    } else {
        EnsureAreaVisibility(pos);
        if (!fixedPos) {
            // we don't want the windows to overlap so shift each window by a bit
            int nShift = std::max(len(gWindows) - 1, 0);
            pos.x += nShift * MulDiv(15, AppShellWindowDpi(win), 96);
        }
    }
#else
    (void)hasPos;
    pos = {};
#endif
    bool maximize = windowState == WIN_STATE_MAXIMIZED;
    if (when == PlaceWindowWhen::Later) {
        PlaceMainWindowLater(win, pos, maximize);
        return;
    }
    AppShellPlaceWindow(win, pos, maximize);
}

// orig's CreateAndShowMainWindow
MainWindow* CreateAndShowMainWindow(SessionData* data) {
    Rect pos = data ? data->windowPos : gSettings->windowPos;
    int dx = pos.dx > 100 ? pos.dx : 1024;
    int dy = pos.dy > 100 ? pos.dy : 768;
    MainWindow* win = AppShellCreateWindow(AppShellGetApp(), dx, dy);
    if (!win) {
        return nullptr;
    }
    // a session window is made at startup, where nothing holds a temp string
    PlaceMainWindow(win, data, data ? PlaceWindowWhen::Now : PlaceWindowWhen::Later);
    if (data && data->sidebarDx > 0) {
        win->sidebarDx = data->sidebarDx;
    }
    SetSidebarVisibility(win, false, gSettings->showFavorites);
    return win;
}

// orig's DuplicateTabInNewWindow: open the tab's document in a window of its
// own, at the state the tab is in. The tab itself stays where it is.
void DuplicateTabInNewWindow(WindowTab* tab) {
    if (!tab || tab->IsNonDocumentTab()) {
        return;
    }
    Str path = tab->filePath;
    if (len(path) == 0) {
        return;
    }
    TabState* state = NewTabStateFromTab(tab);
    MainWindow* newWin = CreateAndShowMainWindow(nullptr);
    if (!newWin) {
        DeleteTabState(state);
        return;
    }
    logf("DuplicateTabInNewWindow: '%s'\n", path);
    LoadDocument(newWin, path);
    if (state) {
        SetTabState(newWin->CurrentTab(), state);
        DeleteTabState(state);
    }
}

// --- next / prev file in folder ---------------------------------------------

// orig scans the folder on a worker and caches the listing; a second Ctrl +
// Shift + Right is then instant even in a folder with tens of thousands of
// files
static Str gNextPrevDir;
static StrVec gNextPrevDirCache;
static int gNextPrevDirScanGen = 0;
static bool gNextPrevDirReady = false;
static bool gNextPrevDirScanning = false;
static StrVec gFilesFailedToOpen;

static bool IsOpenableNextPrevFile(Str path) {
    FileType kind = GuessFileTypeFromName(path, true);
    return IsSupportedFileType(kind, true);
}

static void RemoveFailedFiles(StrVec& files) {
    for (Str s : gFilesFailedToOpen) {
        int idx = files.Find(s);
        if (idx >= 0) {
            files.RemoveAt(idx);
        }
    }
}

// File history is UI-thread only, so snapshot the paths in this dir before the
// worker runs (unsupported types the user has opened stay navigable)
static void CollectHistoryFilesInDir(Str dir, StrVec& out) {
    Vec<FileState*>* states = FileHistoryStates();
    if (!states) {
        return;
    }
    for (FileState* fs : *states) {
        if (!fs || len(fs->filePath) == 0) {
            continue;
        }
        TempStr d = path::GetDirTemp(fs->filePath);
        if (!str::EqI(d, dir)) {
            continue;
        }
        if (IsOpenableNextPrevFile(fs->filePath)) {
            continue;
        }
        out.Append(fs->filePath);
    }
}

// keep the cached list naturally sorted without a full SortNatural
static void InsertSortedNatural(StrVec* v, Str s) {
    if (v->Contains(s)) {
        return;
    }
    int lo = 0;
    int hi = len(*v);
    while (lo < hi) {
        int mid = lo + ((hi - lo) / 2);
        if (StrLessNatural(v->At(mid), s)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    v->InsertAt(lo, s);
}

// a next / previous file request made while the folder listing was running
struct PendingNextPrevNav {
    MainWindow* win = nullptr;
    bool forward = true;
    Str pathToDelete; // owned
    ~PendingNextPrevNav() { str::Free(pathToDelete); }
};
static PendingNextPrevNav* gPendingNextPrevNav = nullptr;

static void ClearPendingNextPrevNav() {
    delete gPendingNextPrevNav;
    gPendingNextPrevNav = nullptr;
}

struct NextPrevDirScan {
    int gen = 0;
    Str dir; // owned
    StrVec extraFromHistory;
    StrVec files;
    ~NextPrevDirScan() { str::Free(dir); }
};

static void FinishNextPrevDirScan(NextPrevDirScan* r) {
    AutoDelete del(r);
    if (r->gen != gNextPrevDirScanGen) {
        return;
    }
    logf("NextPrevDirScan: %d files in '%s'\n", len(r->files), r->dir);
    gNextPrevDirCache.Reset();
    for (Str s : r->files) {
        gNextPrevDirCache.Append(s);
    }
    RemoveFailedFiles(gNextPrevDirCache);
    gNextPrevDirReady = true;
    gNextPrevDirScanning = false;

    if (!gPendingNextPrevNav) {
        return;
    }
    PendingNextPrevNav* p = gPendingNextPrevNav;
    gPendingNextPrevNav = nullptr;
    if (IsMainWindowValidAndNotClosing(p->win) && !p->win->IsCurrentTabAbout()) {
        OpenNextPrevFileInFolder(p->win, p->forward, p->pathToDelete);
    }
    delete p;
}

static void NextPrevDirScanThread(NextPrevDirScan* req) {
    DirIter di{req->dir};
    for (DirIterEntry* de : di) {
        if (IsOpenableNextPrevFile(de->filePath)) {
            req->files.Append(de->filePath);
        }
    }
    for (int i = 0; i < len(req->extraFromHistory); i++) {
        AppendIfNotExists(&req->files, req->extraFromHistory[i]);
    }
    SortNatural(&req->files);
    uitask::Post(MkFunc0<NextPrevDirScan>(FinishNextPrevDirScan, req), "FinishNextPrevDirScan");
}

static void StartNextPrevDirScan(Str dir) {
    gNextPrevDirScanGen++;
    gNextPrevDirReady = false;
    gNextPrevDirScanning = true;
    gNextPrevDirCache.Reset();
    str::ReplaceWithCopy(&gNextPrevDir, dir);

    auto* req = new NextPrevDirScan;
    req->gen = gNextPrevDirScanGen;
    req->dir = str::Dup(dir);
    CollectHistoryFilesInDir(dir, req->extraFromHistory);
    logf("NextPrevDirScan: start '%s'\n", dir);
    RunAsync(MkFunc0<NextPrevDirScan>(NextPrevDirScanThread, req), StrL("NextPrevDirScan"));
}

static void EnsureNextPrevDirScan(Str filePath) {
    if (len(filePath) == 0 || !CanAccessDisk() || gPluginMode) {
        return;
    }
    TempStr dir = path::GetDirTemp(filePath);
    if (path::IsSame(dir, gNextPrevDir) && (gNextPrevDirReady || gNextPrevDirScanning)) {
        return;
    }
    StartNextPrevDirScan(dir);
}

// null if the background listing is still running. Do not copy the vector.
StrVec* GetNextPrevFilesReady(Str path) {
    EnsureNextPrevDirScan(path);
    if (!gNextPrevDirReady) {
        return nullptr;
    }
    RemoveFailedFiles(gNextPrevDirCache);
    // `path` is the file we are navigating away from and may itself have been
    // removed above; callers locate it to know where to continue from (#5917)
    InsertSortedNatural(&gNextPrevDirCache, path);
    return &gNextPrevDirCache;
}

static void ShowNoFileToOpenNotif(MainWindow* win, bool forward) {
    NotificationCreateArgs nargs;
    nargs.win = win;
    nargs.timeoutMs = kNotifDefaultTimeOut;
    nargs.groupId = kNotifAdHoc;
    Str tip = forward ? Tr("Last file in folder.") : Tr("First file in folder.");
    nargs.msg = fmt("%s [%s](CmdNavigateFilesInFolder)", tip, Tr("Navigate"));
    ShowNotification(nargs);
}

// end-of-document hint for "open next file in folder" discoverability
static Kind kNotifNextFileHint = "nextFileHint";

static bool IsAtDocumentBottom(MainWindow* win) {
    DocController* ctrl = win->ctrl;
    if (!ctrl) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        return dm->IsAtDocumentEnd();
    }
    // CHM / Markdown render in a browser control that scrolls itself, so we
    // can't tell how far down it is
    return false;
}

// next openable file after the current tab's path (no wrap), or empty if none.
// outN/outM are 1-based index of the next file and total count when non-null.
static TempStr PeekNextFileInFolderTemp(MainWindow* win, int* outN, int* outM) {
    if (outN) {
        *outN = 0;
    }
    if (outM) {
        *outM = 0;
    }
    if (!win || win->IsCurrentTabAbout() || !CanAccessDisk() || gPluginMode) {
        return {};
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return {};
    }
    Str path = tab->filePath;
    StrVec* files = GetNextPrevFilesReady(path);
    if (!files) {
        return {}; // listing still running; hint is shown when it finishes
    }
    int nFiles = len(*files);
    if (nFiles < 2) {
        return {};
    }
    int idx = files->Find(path);
    if (idx < 0 || idx + 1 >= nFiles) {
        return {}; // no wrap: already last
    }
    Str next = files->At(idx + 1);
    if (!file::Exists(next)) {
        return {};
    }
    if (outN) {
        *outN = idx + 2; // 1-based index of the next file
    }
    if (outM) {
        *outM = nFiles;
    }
    return str::DupTemp(next);
}

void DismissNextFileScrollHint(MainWindow* win) {
    if (!win) {
        return;
    }
    RemoveNotificationsForGroup(win, kNotifNextFileHint);
}

static void OnNextFileHintClosed(NotificationClosedEvent* ev) {
    RemoveNotification(ev->wnd);
    if (ev->reason != NotifCloseReason::User) {
        return;
    }
    gSettings->showFileNavigateHint = false;
    ScheduleSaveSettings();
}

static void MaybeShowNextFileScrollHint(MainWindow* win) {
    if (!gSettings->showFileNavigateHint) {
        return;
    }
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded() || win->IsCurrentTabAbout()) {
        return;
    }
    if (!IsAtDocumentBottom(win)) {
        return;
    }
    int n = 0, m = 0;
    TempStr nextPath = PeekNextFileInFolderTemp(win, &n, &m);
    if (len(nextPath) == 0) {
        return;
    }
    TempStr name = path::GetBaseNameTemp(nextPath);
    // (Kbd/(Key/...)): key-cap of the bound shortcut; filename and "browse" open
    // the navigate-files dialog. The file name comes from the file system, so it
    // can't go through the markup parser (a "](CmdExec ...)" in it would break
    // out of the link and run a program - GHSA-2wv2-qm2f-vmxh). Build the spans
    // instead: only our markup is parsed and the name is added as a plain link
    auto* rich = new Vec<TipSpan>();
    TipSpansParse(*rich, fmt("(Kbd/(Key/CmdOpenNextFileInFolder)) %s ", Tr("open")));
    TipSpansAddLink(*rich, name, StrL("CmdOpenNextFileInFolder"));
    TipSpansParse(*rich, fmt(" \xc2\xb7 %d/%d \xc2\xb7 [%s](CmdNavigateFilesInFolder)", n, m, Tr("browse")));
    NotificationCreateArgs args;
    args.win = win;
    args.groupId = kNotifNextFileHint;
    args.corner = NotifCorner::BottomRight;
    args.timeoutMs = kNotifNoTimeout;
    args.tab = win->CurrentTab();
    args.richMsg = rich;
    args.onClosed = MkFunc1Void(OnNextFileHintClosed);
    // what NotificationGetMessageTemp reports
    args.msg = fmt("%s %s \xc2\xb7 %d/%d \xc2\xb7 %s", Tr("open"), name, n, m, Tr("browse"));
    ShowNotification(args);
}

// scroll-down at document end: show open-next-file tip; scroll-up: dismiss.
void OnDocumentVerticalScrollIntent(MainWindow* win, bool down) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    // ng: orig calls this after each navigation command; here every one of them
    // already reports its intent through this
    ReadAloudOnUserViewChanged(win);
    if (!down) {
        DismissNextFileScrollHint(win);
        return;
    }
    MaybeShowNextFileScrollHint(win);
}

// --- the page-info tip, CmdTogglePageInfo (orig SumatraPDF.cpp) ------------

// Light separator between page-info values: space + U+00B7 MIDDLE DOT + space.
#define kPageInfoSep " \xC2\xB7 "

static TempStr BuildZoomString(float zoomLevel) {
    TempStr zoomLevelStr = ZoomLevelStr(zoomLevel);
    Str zoomStr = Tr("Zoom");
    return fmt("%s: %s", zoomStr, zoomLevelStr);
}

// Pages shown in the page-info tip: the current page, plus its facing partner
// when that page is also visible (facing / book view with two images).
static int CollectPageInfoPages(DocController* ctrl, int pageNo, int* pagesOut, int maxPages) {
    int n = 0;
    auto add = [&](int p) {
        if (n >= maxPages || !ctrl->ValidPageNo(p)) {
            return;
        }
        for (int i = 0; i < n; i++) {
            if (pagesOut[i] == p) {
                return;
            }
        }
        pagesOut[n++] = p;
    };
    add(pageNo);
    DisplayModel* dm = ctrl->AsFixed();
    if (dm) {
        DisplayMode mode = dm->GetDisplayMode();
        if (IsFacing(mode) || IsBookView(mode)) {
            if (dm->PageVisible(pageNo + 1)) {
                add(pageNo + 1);
            } else if (dm->PageVisible(pageNo - 1)) {
                add(pageNo - 1);
            }
        }
        // stable order for multi-page rows
        if (n == 2 && pagesOut[0] > pagesOut[1]) {
            int t = pagesOut[0];
            pagesOut[0] = pagesOut[1];
            pagesOut[1] = t;
        }
    }
    return n;
}

static void UpdatePageInfoHelper(DocController* ctrl, NotificationWnd* wnd, int pageNo) {
    if (!ctrl->ValidPageNo(pageNo)) {
        pageNo = ctrl->CurrentPageNo();
    }
    int nPages = ctrl->PageCount();
    TempStr pageInfo;
    if (ShowChapterUi(ctrl)) {
        Location loc = ctrl->LocationFromPageNo(pageNo);
        int chapterPages = ctrl->ChapterPageCount(loc.chapter);
        pageInfo = fmt("%s %d / %d, %s %d / %d", Tr("Chapter:"), loc.chapter, ctrl->ChapterCount(), Tr("Page:"),
                       loc.page, chapterPages);
    } else if (ctrl->HasPageLabels()) {
        TempStr label = ctrl->GetPageLabeTemp(pageNo);
        pageInfo = fmt("%s %s (%d / %d)", Tr("Page:"), label, pageNo, nPages);
    } else {
        pageInfo = fmt("%s %d / %d", Tr("Page:"), pageNo, nPages);
    }
    float zoomLevel = ctrl->GetZoomVirtual();
    auto zoomStr = BuildZoomString(zoomLevel);
    pageInfo = str::JoinTemp(pageInfo, StrL(kPageInfoSep), zoomStr);

    // Image extras (issue #4456). Document file name is already on the tab.
    DisplayModel* dm = ctrl->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine && IsEngineImages(engine)) {
        if (engine->kind == kindEngineImage) {
            // Single image file (or multi-frame TIFF/GIF): resolution, size, non-default DPI
            RectF box = engine->PageMediabox(pageNo);
            int w = (int)lroundf(box.dx);
            int h = (int)lroundf(box.dy);
            i64 imgSize = -1;
            EngineImagesGetPageFileInfo(engine, pageNo, nullptr, &imgSize);
            TempStr detail{};
            if (w > 0 && h > 0) {
                detail = fmt("%d x %d", w, h);
            }
            if (imgSize >= 0) {
                TempStr sizeStr = str::FormatSizeShortTemp(imgSize);
                detail = detail ? fmt("%s%s%s", detail, StrL(kPageInfoSep), sizeStr) : sizeStr;
            }
            // fileDPI defaults to 96; only show when the image reports something else
            float dpi = engine->fileDPI;
            if (dpi > 0.5f && fabsf(dpi - 96.0f) > 0.5f) {
                TempStr dpiStr = fmt("%.0f DPI", dpi);
                detail = detail ? fmt("%s%s%s", detail, StrL(kPageInfoSep), dpiStr) : dpiStr;
            }
            if (detail) {
                pageInfo = str::JoinTemp(pageInfo, StrL(kPageInfoSep), detail);
            }
        } else {
            // Comic / image folder: per-page name, dimensions, bytes (both if facing)
            int pages[2] = {};
            int nShow = CollectPageInfoPages(ctrl, pageNo, pages, 2);
            for (int i = 0; i < nShow; i++) {
                int p = pages[i];
                TempStr imgName{};
                i64 imgSize = -1;
                if (!EngineImagesGetPageFileInfo(engine, p, &imgName, &imgSize)) {
                    continue;
                }
                RectF box = engine->PageMediabox(p);
                int w = (int)lroundf(box.dx);
                int h = (int)lroundf(box.dy);
                TempStr detail{};
                auto appendPart = [&](TempStr part) {
                    if (len(part) == 0) {
                        return;
                    }
                    detail = detail ? fmt("%s%s%s", detail, StrL(kPageInfoSep), part) : part;
                };
                appendPart(imgName);
                if (w > 0 && h > 0) {
                    appendPart(fmt("%d x %d", w, h));
                }
                if (imgSize >= 0) {
                    appendPart(str::FormatSizeShortTemp(imgSize));
                }
                if (detail) {
                    pageInfo = str::JoinTemp(pageInfo, StrL(kPageInfoSep), detail);
                }
            }
        }
    }

    NotificationUpdateMessage(wnd, pageInfo);
}

// Show or refresh the page-info tip when the user wants it and a document is
// loaded. CloseDocumentInCurrentTab / About / Favorites remove the
// notification; this restores it.
void ShowPageInfoIfWanted(MainWindow* win) {
    if (!win) {
        return;
    }
    if (!win->pageInfoWanted || !win->IsDocLoaded() || !win->ctrl) {
        // ng: orig drops the tip from CloseDocumentInCurrentTab, About and
        // Favorites; the one place every one of them goes through is enough
        RemoveNotificationsForGroup(win, kNotifPageInfo);
        return;
    }
    NotificationWnd* wnd = GetNotificationForGroup(win, kNotifPageInfo);
    if (wnd) {
        UpdatePageInfoHelper(win->ctrl, wnd, -1);
        return;
    }
    NotificationCreateArgs args;
    args.win = win;
    args.timeoutMs = kNotifNoTimeout;
    args.msg = StrL("");
    args.groupId = kNotifPageInfo;
    // the message carries page labels and image entry names from the document
    args.plainText = true;
    wnd = ShowNotification(args);
    UpdatePageInfoHelper(win->ctrl, wnd, -1);
}

static void TogglePageInfoHelper(MainWindow* win) {
    if (!win) {
        return;
    }
    if (win->pageInfoWanted) {
        win->pageInfoWanted = false;
        RemoveNotificationsForGroup(win, kNotifPageInfo);
        return;
    }
    win->pageInfoWanted = true;
    ShowPageInfoIfWanted(win);
}

// --- the cursor-position tip, CmdToggleCursorPosition ----------------------

enum class MeasurementUnit {
    pt,
    mm,
    in
};

static TempStr FormatCursorPositionTemp(EngineBase* engine, PointF pt, MeasurementUnit unit) {
    pt.x = std::max(pt.x, 0.0f);
    pt.y = std::max(pt.y, 0.0f);
    pt.x /= engine->fileDPI;
    pt.y /= engine->fileDPI;

    // for MeasurementUnit::in
    float factor = 1;
    Str unitName = StrL("in");
    if (unit == MeasurementUnit::pt) {
        factor = 72;
        unitName = StrL("pt");
    } else if (unit == MeasurementUnit::mm) {
        factor = 25.4f;
        unitName = StrL("mm");
    }

    TempStr xPos = str::FormatFloatWithThousandSepTemp((double)pt.x * (double)factor);
    TempStr yPos = str::FormatFloatWithThousandSepTemp((double)pt.y * (double)factor);
    if (unit != MeasurementUnit::in) {
        // use similar precision for all units
        if (xPos.len >= 2 && str::IsDigit(xPos.s[xPos.len - 2])) {
            xPos.len--;
        }
        if (yPos.len >= 2 && str::IsDigit(yPos.s[yPos.len - 2])) {
            yPos.len--;
        }
    }
    return fmt("%s x %s %s", xPos, yPos, unitName);
}

static auto cursorPosUnit = MeasurementUnit::pt;

void UpdateCursorPositionHelper(MainWindow* win, Point pos, NotificationWnd* wnd) {
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm || !wnd) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    PointF pt = dm->CvtFromScreen(pos);
    TempStr posStr = FormatCursorPositionTemp(engine, pt, cursorPosUnit);
    TempStr selStr = {};
    if (!win->selectionMeasure.IsEmpty()) {
        pt = PointF(win->selectionMeasure.dx, win->selectionMeasure.dy);
        selStr = FormatCursorPositionTemp(engine, pt, cursorPosUnit);
    }

    TempStr posInfo = fmt("%s %s", Tr("Cursor position:"), posStr);
    if (selStr) {
        posInfo = fmt("%s - %s %s", posInfo, Tr("Selection:"), selStr);
    }
    NotificationUpdateMessage(wnd, posInfo);
}

// what CmdToggleCursorPosition would switch to. The tip cycles pt -> mm -> in
// and then closes, so the command palette can't say true / false; naming the
// next unit here keeps it in step with ToggleCursorPositionInDoc() below
Str NextCursorPositionUnitName(MainWindow* win) {
    if (!win || !win->AsFixed()) {
        return {};
    }
    if (!GetNotificationForGroup(win, kNotifCursorPos)) {
        return StrL("pt");
    }
    if (cursorPosUnit == MeasurementUnit::pt) {
        return StrL("mm");
    }
    if (cursorPosUnit == MeasurementUnit::mm) {
        return StrL("in");
    }
    return StrL("off");
}

static void ToggleCursorPositionInDoc(MainWindow* win) {
    // "cursor position" tip: make figuring out the current
    // cursor position in cm/in/pt possible (for exact layouting)
    if (!win->AsFixed()) {
        return;
    }
    auto* notif = GetNotificationForGroup(win, kNotifCursorPos);
    if (!notif) {
        NotificationCreateArgs args;
        args.win = win;
        args.groupId = kNotifCursorPos;
        args.timeoutMs = kNotifNoTimeout;
        args.plainText = true;
        notif = ShowNotification(args);
        cursorPosUnit = MeasurementUnit::pt;
    } else {
        if (cursorPosUnit == MeasurementUnit::pt) {
            cursorPosUnit = MeasurementUnit::mm;
        } else if (cursorPosUnit == MeasurementUnit::mm) {
            cursorPosUnit = MeasurementUnit::in;
        } else {
            cursorPosUnit = MeasurementUnit::pt;
            RemoveNotificationsForGroup(win, kNotifCursorPos);
            return;
        }
    }
    UpdateCursorPositionHelper(win, win->dragPrevPos, notif);
}

// A sibling .md / .html is already a page of this model. The browser reports
// the new file only after it finishes loading, so the tab title is set now.
static bool GoToFileInBrowserView(MainWindow* win, Str path) {
    MarkdownModel* md = win->ctrl ? win->ctrl->AsMarkdown() : nullptr;
    if (!md) {
        return false;
    }
    for (int i = 0; i < len(md->pages); i++) {
        if (!path::IsSame(md->pages[i], path)) {
            continue;
        }
        md->GoToPage(i + 1, true);
        WindowTab* tab = win->CurrentTab();
        if (tab && !path::IsSame(tab->filePath, path)) {
            tab->SetFilePath(path);
            tab->SetDisplayName({});
            TabsOnChangedDoc(win);
            UpdateWindowTitle(win);
        }
        return true;
    }
    return false;
}

// pathToDelete is removed from disk once another document has loaded
void OpenNextPrevFileInFolder(MainWindow* win, bool forward, Str pathToDelete) {
    if (!win || win->IsCurrentTabAbout() || !CanAccessDisk() || gPluginMode) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    // dismiss document error notifications from the previous document
    RemoveNotificationsForGroup(win, kNotifDocErrors);
    DismissNextFileScrollHint(win);
    Str path = tab->filePath;
    StrVec* files = GetNextPrevFilesReady(path);
    if (!files) {
        ClearPendingNextPrevNav();
        auto* p = new PendingNextPrevNav;
        p->win = win;
        p->forward = forward;
        p->pathToDelete = str::Dup(pathToDelete);
        gPendingNextPrevNav = p;
        logf("OpenNextPrevFileInFolder: folder scan in progress, deferring\n");
        return;
    }
    int nFiles = len(*files);
    int idx = files->Find(path);
    if (nFiles < 2 || idx < 0) {
        ShowNoFileToOpenNotif(win, forward);
        return;
    }
    // do not wrap around at the ends of the folder; skip files that vanished
    int step = forward ? 1 : -1;
    int next = idx + step;
    Str chosen;
    while (next >= 0 && next < nFiles) {
        Str cand = files->At(next);
        if (file::Exists(cand)) {
            chosen = cand;
            break;
        }
        next += step;
    }
    if (len(chosen) == 0) {
        ShowNoFileToOpenNotif(win, forward);
        return;
    }
    logf("OpenNextPrevFileInFolder: %s -> '%s'\n", Str(forward ? "next" : "prev"), chosen);
    // a sibling .md / .html is already a page of this model. Loading it again
    // would rebuild the outline (#5918).
    if (len(pathToDelete) == 0 && GoToFileInBrowserView(win, chosen)) {
        return;
    }
    UpdateTabFileDisplayStateForTab(tab);
    TempStr chosenCopy = str::DupTemp(chosen);
    // the load resets the temp arena and may free the pending request
    Str toDelete = str::Dup(pathToDelete);
    if (!LoadDocument(win, chosenCopy, LoadPrefs::Save, LoadReuse::CurrentTab)) {
        // remember the failure so the next step skips this file
        gFilesFailedToOpen.Append(chosenCopy);
        str::Free(toDelete);
        return;
    }
    // ng: orig deletes from the asynchronous load's callback
    WindowTab* curr = IsMainWindowValidAndNotClosing(win) ? win->CurrentTab() : nullptr;
    bool moved = curr && len(curr->filePath) > 0 && !path::IsSame(curr->filePath, toDelete);
    if (len(toDelete) > 0 && moved) {
        DeleteFileFromDiskAndHistory(toDelete);
    }
    str::Free(toDelete);
}

// --- annotations (step 13a) -------------------------------------------------

void MainWindowRerender(MainWindow* win) {
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return;
    }
    // restyle/layout can shrink engine pageCount; paint uses dm page numbers
    dm->SyncWithEngineLayout();
    // don't wait for in-flight renders: dm stays alive and a render that lands
    // after this is either still valid or dropped by the darkModeEpoch check
    gRenderCache->AbortRendering(dm);
    gRenderCache->KeepForDisplayModel(dm, dm);
    win->RedrawAll(true);
}

// Re-render one page after an annotation edit, leaving other cached pages alone.
void RerenderTabPage(WindowTab* tab, int pageNo) {
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!dm || pageNo < 1 || pageNo > dm->PageCount()) {
        return;
    }
    gRenderCache->Invalidate(dm, pageNo, dm->GetEngine()->PageMediabox(pageNo));
    MainWindow* win = tab->win;
    if (win->CurrentTab() != tab) {
        return;
    }
    // the following paint queues the render; keep frames coming until it lands
    win->repaintPending = true;
    gRenderCache->RequestRendering(dm, pageNo);
    SidebarRefreshThumbnailPage(win, pageNo);
    AppShellInvalidate(win);
}

void BeginPdfEditOperation(MainWindow* win, const char* name) {
    if (!win || win->pdfEditOperationActive) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine || !EngineSupportsAnnotations(engine)) {
        return;
    }
    EngineMupdfBeginOperation(engine, name);
    win->pdfEditOperationActive = true;
}

void EndPdfEditOperation(MainWindow* win) {
    if (!win || !win->pdfEditOperationActive) {
        return;
    }
    win->pdfEditOperationActive = false;
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine) {
        EngineMupdfEndOperation(engine);
    }
}

static void SetAnnotCreateArgsFromCommand(AnnotCreateArgs& args, CustomCommand* cmd) {
    args.copyToClipboard = GetCommandBoolArg(cmd, kCmdArgCopyToClipboard, false);
    args.setContentToSelection = GetCommandBoolArg(cmd, kCmdArgSetContent, false);

    auto* col = GetCommandArg(cmd, kCmdArgColor);
    if (col && col->colorVal.parsedOk) {
        args.col = col->colorVal;
    }
    auto* bgCol = GetCommandArg(cmd, kCmdArgBgColor);
    if (bgCol && bgCol->colorVal.parsedOk) {
        args.bgCol = bgCol->colorVal;
    }
    auto* interiorCol = GetCommandArg(cmd, kCmdArgInteriorColor);
    if (interiorCol && interiorCol->colorVal.parsedOk) {
        args.interiorCol = interiorCol->colorVal;
    }
    if (GetCommandArg(cmd, kCmdArgOpacity)) {
        args.opacity = GetCommandIntArg(cmd, kCmdArgOpacity, 100);
        setMinMax(args.opacity, 0, 100);
    }
    int textSize = GetCommandIntArg(cmd, kCmdArgTextSize, -1);
    if (textSize >= 0) {
        // set some reasonable limits
        setMinMax(textSize, 5, 128);
        args.textSize = textSize;
    }
    int borderWidth = GetCommandIntArg(cmd, kCmdArgBorderWidth, -1);
    if (borderWidth >= 0) {
        setMinMax(borderWidth, 0, 128);
        args.borderWidth = borderWidth;
    }
    int quadding = QuaddingFromName(GetCommandStringArg(cmd, kCmdArgAlignment, {}));
    if (quadding >= 0) {
        args.quadding = quadding;
    }
}

void SetAnnotCreateArgs(AnnotCreateArgs& args, CustomCommand* cmd) {
    auto& a = gSettings->annotations;
    ParsedColor* col = nullptr;
    ParsedColor* bgCol = nullptr;
    auto typ = args.annotType;
    if (typ == AnnotationType::Text) {
        col = GetParsedColor(a.textIconColor);
    } else if (typ == AnnotationType::Underline) {
        col = GetParsedColor(a.underlineColor);
    } else if (typ == AnnotationType::Highlight) {
        col = GetParsedColor(a.highlightColor);
    } else if (typ == AnnotationType::Squiggly) {
        col = GetParsedColor(a.squigglyColor);
    } else if (typ == AnnotationType::StrikeOut) {
        col = GetParsedColor(a.strikeOutColor);
    } else if (typ == AnnotationType::FreeText) {
        col = GetParsedColor(a.freeTextColor);
        bgCol = GetParsedColor(a.freeTextBackgroundColor);
        if (bgCol && bgCol->parsedOk) {
            args.bgCol = *bgCol;
        }
        args.opacity = a.freeTextOpacity;
        args.textSize = a.freeTextSize;
        args.borderWidth = a.freeTextBorderWidth;
        args.quadding = QuaddingFromName(a.freeTextAlignment);
    } else if (typ == AnnotationType::Line) {
        col = GetParsedColor(a.lineColor);
    } else if (typ == AnnotationType::PolyLine) {
        col = GetParsedColor(a.polyLineColor);
    } else if (typ == AnnotationType::Square) {
        col = GetParsedColor(a.squareColor);
    } else if (typ == AnnotationType::Circle) {
        col = GetParsedColor(a.circleColor);
    } else if (typ == AnnotationType::Polygon) {
        col = GetParsedColor(a.polygonColor);
    } else if (typ == AnnotationType::Ink) {
        col = GetParsedColor(a.inkColor);
        args.borderWidth = a.inkBorderWidth;
    } else if (typ == AnnotationType::Stamp) {
        col = GetParsedColor(a.stampColor);
    } else if (typ == AnnotationType::Caret) {
        col = GetParsedColor(a.caretColor);
    } else if (typ == AnnotationType::FileAttachment) {
        col = GetParsedColor(a.fileAttachmentColor);
    } else if (typ == AnnotationType::Redact) {
        // a redaction mark has no color to pick: it's the black box that
        // replaces the text. MuPDF's default is what we want
    } else {
        logf("SetAnnotCreateArgs: unexpected type %d for default prefs color\n", (int)typ);
    }
    if (col && col->parsedOk) {
        args.col = *col;
    }

    // a command's arguments (e.g. Shift+A's "openedit", or a color) override
    // the settings; ones it doesn't give keep them (#6197)
    if (cmd && cmd->firstArg) {
        SetAnnotCreateArgsFromCommand(args, cmd);
    }
}

// Pick the center of the visible part of the current page when a command has
// no usable canvas point, as happens after clicking an annotation-toolbar button.
static bool SetPointToVisiblePage(DisplayModel* dm, Point& pt, int& pageNo) {
    pageNo = dm->FirstVisiblePageNo();
    if (!dm->ValidPageNo(pageNo)) {
        pageNo = dm->CurrentPageNo();
    }
    if (!dm->ValidPageNo(pageNo)) {
        return false;
    }
    PageInfo* pi = dm->GetPageInfo(pageNo);
    Size viewport = dm->GetViewPort().Size();
    Rect visible = pi->pageOnScreen.Intersect(Rect{0, 0, viewport.dx, viewport.dy});
    if (visible.IsEmpty()) {
        visible = pi->pageOnScreen;
    }
    if (visible.IsEmpty()) {
        return false;
    }
    pt = Point{visible.x + (visible.dx / 2), visible.y + (visible.dy / 2)};
    return true;
}

static void AddUniquePageNo(Vec<int>& pageNos, int pageNo) {
    if (!VecContains(pageNos, pageNo)) {
        VecAppend(pageNos, pageNo);
    }
}

static RectF SelectionRectsUnion(const Vec<RectF>& rects) {
    RectF covered;
    for (const RectF& r : rects) {
        covered = covered.IsEmpty() ? r : covered.Union(r);
    }
    return covered;
}

static Annotation* MakeAnnotationsFromSelection(WindowTab* tab, AnnotCreateArgs* args) {
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return nullptr;
    }
    auto* engine = dm->GetEngine();
    bool supportsAnnots = EngineSupportsAnnotations(engine);
    MainWindow* win = tab->win;
    bool ok = supportsAnnots && win->showSelection && tab->selectionOnPage;
    if (!ok) {
        return nullptr;
    }
    // highlight / underline / squiggly / strike out mark up runs of *text*. A
    // rectangular selection (Ctrl+drag, Select All) isn't one - marking up its
    // bounding boxes produced bars over whitespace - so do nothing.
    bool isTextSelection = dm->textSelection && dm->textSelection->result.len > 0;
    if (!isTextSelection) {
        return nullptr;
    }

    Vec<SelectionOnPage>* s = tab->selectionOnPage;
    Vec<int> pageNos;
    for (auto& sel : *s) {
        int pageNo = sel.pageNo;
        if (!dm->ValidPageNo(pageNo)) {
            continue;
        }
        AddUniquePageNo(pageNos, pageNo);
    }
    if (len(pageNos) == 0) {
        return nullptr;
    }

    if (args->setContentToSelection) {
        bool isTextOnlySelection = false;
        args->content = GetSelectedTextTemp(tab, StrL("\r\n"), isTextOnlySelection);
    }

    Annotation* annot = nullptr;
    Vec<Annotation*> created;
    // Creating annotations and setting their quad points is one gesture.
    EngineMupdfBeginOperation(engine, "Mark up selection");
    defer {
        EngineMupdfEndOperation(engine);
    };
    for (auto pageNo : pageNos) {
        Vec<RectF> rects;
        for (auto& sel : *s) {
            if (pageNo != sel.pageNo) {
                continue;
            }
            VecAppend(rects, sel.rect);
        }
        annot = EngineMupdfCreateAnnotation(engine, pageNo, PointF{}, args);
        if (!annot) {
            // Roll back annots created earlier in this call so we do not leave
            // partial multi-page selections as untracked annotations.
            for (Annotation* a : created) {
                DeleteAnnotation(a);
            }
            return nullptr;
        }
        SetQuadPointsAsRect(annot, rects);
        annot->bounds = GetBounds(annot);
        // Hit testing uses this cache. pdf_bound_annot can miss the quads.
        RectF covered = SelectionRectsUnion(rects);
        if (!covered.IsEmpty() && (annot->bounds.IsEmpty() || annot->bounds.Intersect(covered).IsEmpty())) {
            annot->bounds = covered;
        }
        VecAppend(created, annot);
    }

    // copy selection to clipboard so that user can use Ctrl-V to set contents
    if (args->copyToClipboard) {
        CopySelectionToClipboard(win);
    }
    // callers refresh lists and rerender
    return annot;
}

// A cut removes the original only after its copy has landed, so a failed paste
// can't lose the annotation (issue #5222).
static void DeleteCutAnnotationAfterPaste(WindowTab* tab) {
    Annotation* cut = TakeCutAnnotation();
    if (!cut || !tab || !EngineOwnsAnnotation(tab->GetEngine(), cut)) {
        return;
    }
    DeleteAnnotationAndUpdateUI(tab, cut);
}

static void PasteAnnotationInTab(MainWindow* win, WindowTab* tab) {
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!HasCopiedAnnotation() || !win || !dm) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || !EngineSupportsAnnotations(engine)) {
        return;
    }
    Point pt = win->dragPrevPos;
    int pageNoUnderCursor = dm->GetPageNoByPoint(pt);
    if (pageNoUnderCursor < 0 && !SetPointToVisiblePage(dm, pt, pageNoUnderCursor)) {
        return;
    }
    PointF ptOnPage = dm->CvtFromScreen(pt, pageNoUnderCursor);
    // pasting a cut annotation is a move: the new annotation and the delete of
    // the original make one undo step
    EngineMupdfBeginOperation(engine, "Paste annotation");
    Annotation* pasted = PasteCopiedAnnotation(engine, pageNoUnderCursor, ptOnPage);
    if (pasted) {
        DeleteCutAnnotationAfterPaste(tab);
    }
    EngineMupdfEndOperation(engine);
    if (!pasted) {
        return;
    }
    RefreshAnnotationLists(tab);
    MainWindowRerender(win);
    ToolbarUpdateStateForWindow(win, true);
    EnablePdfAnnotationsToolbar(win);
    SetSelectedAnnotation(tab, pasted);
}

static void ApplyRedactionsInTab(WindowTab* tab) {
    MainWindow* win = tab ? tab->win : nullptr;
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!win || !dm) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || !EngineSupportsAnnotations(engine)) {
        return;
    }
    CancelAnnotationPlacement(win);
    CanvasCancelDrag(win);

    if (!EngineHasRedactMarks(engine)) {
        ShowTemporaryNotification(win, Tr("No redaction marks to apply"));
        return;
    }
    if (gRenderCache) {
        gRenderCache->AbortRendering(dm);
    }
    SetSelectedAnnotation(tab, nullptr);

    Vec<Annotation*> deleted;
    bool ok = EngineMupdfApplyRedactions(engine, deleted);
    for (Annotation* a : deleted) {
        DetachAnnotationFromUI(a);
        DeleteAnnotation(a);
    }
    DeleteOldSelectionInfo(win, true);
    RefreshAnnotationLists(tab);
    ToolbarUpdateStateForWindow(win, true);
    if (!ok) {
        ShowWarningNotification(win, Tr("Failed to apply redactions"), kNotif5SecsTimeOut);
        return;
    }
    MainWindowRerender(win);
    ShowTemporaryNotification(win, Tr("Redactions applied."), kNotif5SecsTimeOut);
}

void DeleteSelectedAnnotation(MainWindow* win) {
    WindowTab* tab = IsMainWindowValidAndNotClosing(win) ? win->CurrentTab() : nullptr;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (annot) {
        DeleteAnnotationAndUpdateUI(tab, annot);
    }
}

// orig's FrameOnCommand annotation half. `isPlacementCommit` is orig's
// kAnnotationPlacementCommandCode, `pt` the canvas point the command applies to
// (empty when it comes from a menu or the toolbar).
void ExecuteAnnotCreateCmd(MainWindow* win, int invokedCmdId, bool isPlacementCommit, Point pt) {
    if (!win || win->isBeingClosed) {
        return;
    }
    CustomCommand* cmd = FindCustomCommand(invokedCmdId);
    int cmdId = cmd ? cmd->origId : invokedCmdId;
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = win->AsFixed();
    AnnotationType annotType = CmdIdToAnnotationType(cmdId);
    Annotation* lastCreatedAnnot = nullptr;
    bool hasPt = !pt.IsEmpty();

    if (cmdId == CmdAnnotationHighlightBrush) {
        // The highlighter is a mode: every text selection finished while it's
        // on is highlighted (the placement commit), until Esc. Text already
        // selected when it's picked is highlighted right away.
        if (!tab) {
            return;
        }
        if (isPlacementCommit || tab->selectionOnPage) {
            AnnotCreateArgs args{AnnotationType::Highlight};
            SetAnnotCreateArgs(args, cmd);
            Annotation* created = MakeAnnotationsFromSelection(tab, &args);
            if (created) {
                if (!KeepCaretAfterMarkup(win)) {
                    StopSelectTextWithKeyboard(win);
                    DeleteOldSelectionInfo(win, true);
                }
                RefreshAnnotationLists(tab);
                MainWindowRerender(win);
                ToolbarUpdateStateForWindow(win, true);
                if (!win->pdfAnnotationsToolbarEnabled) {
                    SetSelectedAnnotation(tab, created);
                }
            }
        }
        if (!isPlacementCommit) {
            StartAnnotationPlacement(win, invokedCmdId);
        }
        return;
    }

    bool isTextMarkup = cmdId == CmdCreateAnnotHighlight || cmdId == CmdCreateAnnotSquiggly ||
                        cmdId == CmdCreateAnnotStrikeOut || cmdId == CmdCreateAnnotUnderline;
    if (isTextMarkup) {
        if (!tab) {
            return;
        }
        AnnotCreateArgs args{annotType};
        SetAnnotCreateArgs(args, cmd);
        lastCreatedAnnot = MakeAnnotationsFromSelection(tab, &args);
    } else {
        if (cmdId == CmdCreateAnnotRedact && !isPlacementCommit && tab) {
            AnnotCreateArgs selArgs{annotType};
            SetAnnotCreateArgs(selArgs, cmd);
            lastCreatedAnnot = MakeAnnotationsFromSelection(tab, &selArgs);
        }
        if (!lastCreatedAnnot) {
            if (CommandUsesPlacementMode(cmdId) && !isPlacementCommit && !hasPt) {
                StartAnnotationPlacement(win, invokedCmdId);
                return;
            }
            if (!tab || !dm) {
                return;
            }
            EngineBase* engine = dm->GetEngine();
            if (!engine || !EngineSupportsAnnotations(engine)) {
                return;
            }
            int pageNoUnderCursor = -1;
            PointF ptOnPage;
            PointF lineEndOnPage;
            AnnotCreateArgs args{annotType};
            SetAnnotCreateArgs(args, cmd);
            if (isPlacementCommit) {
                if (!AnnotationPlacementFillCreate(win, annotType, pt, pageNoUnderCursor, ptOnPage, lineEndOnPage,
                                                   args)) {
                    return;
                }
            } else {
                if (!hasPt) {
                    pt = win->dragPrevPos;
                }
                pageNoUnderCursor = dm->GetPageNoByPoint(pt);
                if (pageNoUnderCursor < 0 && !SetPointToVisiblePage(dm, pt, pageNoUnderCursor)) {
                    return;
                }
                ptOnPage = dm->CvtFromScreen(pt, pageNoUnderCursor);
            }
            lastCreatedAnnot = EngineMupdfCreateAnnotation(engine, pageNoUnderCursor, ptOnPage, &args);
        }
    }

    if (!lastCreatedAnnot) {
        return;
    }
    bool openEdit = GetCommandBoolArg(cmd, kCmdArgOpenEdit, false);
    // CmdCreateAnnot* turns on Edit PDF only when the command has `openedit`
    if (openEdit) {
        EnablePdfAnnotationsToolbar(win);
    }
    // The text selection has done its job: it would sit on top of the markup
    // annotation it just made and keep the selection toolbar open over it.
    // Keyboard caret mode stays on, at the free end of that markup.
    bool keptCaret = AnnotationIsTextMarkup(lastCreatedAnnot->type) && KeepCaretAfterMarkup(win);
    if (!keptCaret) {
        StopSelectTextWithKeyboard(win);
        DeleteOldSelectionInfo(win, true);
    }
    RefreshAnnotationLists(tab);
    MainWindowRerender(win);
    ToolbarUpdateStateForWindow(win, true);

    // Select a new annotation in Edit PDF, and text markup even when that
    // toolbar is off, so Delete has a target.
    if (win->pdfAnnotationsToolbarEnabled || AnnotationIsTextMarkup(lastCreatedAnnot->type)) {
        SetSelectedAnnotation(tab, lastCreatedAnnot);
    }
    // a new free text annotation is a box of placeholder text: put the caret
    // in it rather than make the user find it again
    if (cmdId == CmdCreateAnnotFreeText && lastCreatedAnnot->type == AnnotationType::FreeText) {
        StartFreeTextInPlaceEdit(win, lastCreatedAnnot);
    } else if (openEdit) {
        uitask::Post(MkFunc0(StartSelectedAnnotContentsEdit, win), "StartAnnotContentsEdit");
    }
    logf("ExecuteAnnotCreateCmd: cmd %d -> annotation type %d on page %d\n", cmdId, (int)lastCreatedAnnot->type,
         lastCreatedAnnot->pageNo);
}

// --- saving annotations -----------------------------------------------------

static void ShowSavedAnnotationsNotification(MainWindow* win, Str path) {
    NotificationCreateArgs nargs;
    nargs.win = win;
    nargs.timeoutMs = kNotif5SecsTimeOut;
    nargs.msg = fmt(Tr("Saved annotations to '%s'").s, path);
    ShowNotification(nargs);
}

struct ShowErrorData {
    MainWindow* win;
    Str path;
};

static void ShowSaveAnnotationError(ShowErrorData* d, Str err) {
    ShowWarningNotification(d->win, fmt(Tr("Failed to save '%s': %s").s, d->path, err), kNotifNoTimeout);
}

// Identity of the selected annotation for restore after save/reload
// (Annotation* pointers die with the engine).
struct SavedAnnotSel {
    bool valid = false;
    int pageNo = -1;
    AnnotationType type = AnnotationType::Unknown;
    RectF bounds;
};

static SavedAnnotSel CaptureSelectedAnnotation(WindowTab* tab) {
    SavedAnnotSel key;
    Annotation* a = tab ? tab->selectedAnnotation : nullptr;
    if (!a) {
        return key;
    }
    key.valid = true;
    key.pageNo = a->pageNo;
    key.type = a->type;
    key.bounds = a->bounds;
    return key;
}

static Annotation* FindMatchingAnnotation(WindowTab* tab, const SavedAnnotSel& key) {
    if (!key.valid || !tab) {
        return nullptr;
    }
    EngineBase* engine = tab->GetEngine();
    if (!engine) {
        return nullptr;
    }
    Vec<Annotation*> annots;
    EngineMupdfGetAnnotations(engine, annots);
    for (Annotation* a : annots) {
        if (a->pageNo == key.pageNo && a->type == key.type && a->bounds == key.bounds) {
            return a;
        }
    }
    return nullptr;
}

// Returns the current engine only after proving that tab still belongs to a
// live window. The save dialog runs frames that can close the tab.
static EngineBase* GetLiveTabEngine(WindowTab* tab, MainWindow** winOut) {
    if (winOut) {
        *winOut = nullptr;
    }
    MainWindow* win = FindMainWindowByTab(tab);
    if (!win) {
        return nullptr;
    }
    DisplayModel* dm = tab->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine && winOut) {
        *winOut = win;
    }
    return engine;
}

bool SaveAnnotationsToExistingFile(WindowTab* tab) {
    MainWindow* win = nullptr;
    EngineBase* engine = GetLiveTabEngine(tab, &win);
    if (!engine || win->CurrentTab() != tab) {
        return false;
    }
    Str path = engine->FilePath();
    tab->ignoreNextAutoReload = true;
    ShowErrorData data{win, path};
    auto fn = MkFunc1(ShowSaveAnnotationError, &data);
    bool ok = EngineMupdfSaveUpdated(engine, {}, fn);
    if (!ok) {
        tab->ignoreNextAutoReload = false;
        return false;
    }
    ShowSavedAnnotationsNotification(win, path);

    ReloadDocument(win, false);
    // Re-arm: the save notifies the file watcher, which schedules an
    // auto-reload. We already reloaded above; skip that one watcher event.
    tab->ignoreNextAutoReload = true;
    return true;
}

// what the save-as dialog's answer is applied to
struct SaveAnnotsCtx {
    WindowTab* tab = nullptr;
    Str srcPath; // owned
    Func1<bool> onDone;
};

static void OnSaveAnnotsPathPicked(SaveAnnotsCtx* ctx, SavePathArgs* args) {
    WindowTab* tab = ctx->tab;
    TempStr srcPath = str::DupTemp(ctx->srcPath);
    Func1<bool> onDone = ctx->onDone;
    str::Free(ctx->srcPath);
    delete ctx;

    TempStr dstFilePath = str::DupTemp(args->path);
    if (len(dstFilePath) == 0) {
        onDone.Call(false);
        return;
    }
    MainWindow* win = nullptr;
    EngineBase* engine = GetLiveTabEngine(tab, &win);
    if (!engine || win->CurrentTab() != tab || !str::Eq(engine->FilePath(), srcPath)) {
        onDone.Call(false);
        return;
    }
    if (str::Eq(dstFilePath, srcPath)) {
        onDone.Call(SaveAnnotationsToExistingFile(tab));
        return;
    }

    ShowErrorData data{win, dstFilePath};
    auto fn = MkFunc1(ShowSaveAnnotationError, &data);
    if (!EngineMupdfSaveUpdated(engine, dstFilePath, fn)) {
        onDone.Call(false);
        return;
    }

    // Capture selection before the engine (and Annotation*) is torn down.
    SavedAnnotSel sel = CaptureSelectedAnnotation(tab);
    CloseAnnotationUiForTab(tab);

    UpdateTabFileDisplayStateForTab(tab);
    TempStr newPath = path::NormalizeTemp(dstFilePath);
    LoadDocument(win, newPath, LoadPrefs::DontSave, LoadReuse::CurrentTab);

    ShowSavedAnnotationsNotification(win, newPath);
    if (sel.valid) {
        WindowTab* curr = win->CurrentTab();
        SetSelectedAnnotation(curr, FindMatchingAnnotation(curr, sel));
    }
    onDone.Call(true);
}

// ng: orig blocks in GetSaveFileNameW; here the dialog calls back
void SaveAnnotationsToMaybeNewPdfFile(WindowTab* tab, const Func1<bool>& onDone) {
    MainWindow* win = nullptr;
    EngineBase* engine = GetLiveTabEngine(tab, &win);
    if (!engine || win->CurrentTab() != tab) {
        onDone.Call(false);
        return;
    }
    TempStr srcFileName = str::DupTemp(engine->FilePath());
    // Seed the dialog with "foo Copy.pdf" so Save doesn't overwrite the source
    // unless the user deliberately picks the original name.
    TempStr noExt = path::GetPathNoExtTemp(srcFileName);
    TempStr ext = path::GetExtTemp(srcFileName);
    TempStr suggested = fmt("%s Copy%s", noExt, ext);

    auto* ctx = new SaveAnnotsCtx();
    ctx->tab = tab;
    ctx->srcPath = str::Dup(srcFileName);
    ctx->onDone = onDone;

    auto* args = new SavePathArgs();
    args->win = win;
    args->title = str::Dup(Tr("Save As"));
    args->initialPath = str::Dup(suggested);
    args->defExt = str::Dup(StrL(".pdf"));
    args->filter = str::Dup(fmt("%s\1*.pdf\1\1*.*\1", Tr("PDF documents")));
    args->onDone = MkFunc1(OnSaveAnnotsPathPicked, ctx);
    ShowSavePathDialog(args);
}

bool AnnotationsNeedSavePrompt(WindowTab* tab) {
    if (!tab || tab->askedToSaveAnnotations) {
        return false;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return false;
    }
    // if the file no longer exists (e.g. USB removed, network drive gone),
    // don't touch it: the engine memory-maps it
    if (!file::Exists(dm->GetFilePath())) {
        return false;
    }
    return EngineHasUnsavedAnnotations(dm->GetEngine());
}

struct SavePromptCtx {
    WindowTab* tab = nullptr;
    Func1<bool> onDone;
};

static void OnSaveAnnotsPromptDone(SavePromptCtx* ctx, bool didSave) {
    WindowTab* tab = ctx->tab;
    Func1<bool> onDone = ctx->onDone;
    delete ctx;
    if (!didSave && IsWindowTabValid(tab)) {
        tab->askedToSaveAnnotations = false;
    }
    onDone.Call(didSave);
}

static void OnSaveAnnotsChoice(SavePromptCtx* ctx, int answer) {
    WindowTab* tab = ctx->tab;
    if (!IsWindowTabValid(tab)) {
        Func1<bool> onDone = ctx->onDone;
        delete ctx;
        onDone.Call(true);
        return;
    }
    auto choice = (SaveChoice)answer;
    if (choice == SaveChoice::Discard) {
        Func1<bool> onDone = ctx->onDone;
        delete ctx;
        onDone.Call(true);
        return;
    }
    if (choice == SaveChoice::Cancel) {
        tab->askedToSaveAnnotations = false;
        Func1<bool> onDone = ctx->onDone;
        delete ctx;
        onDone.Call(false);
        return;
    }
    if (choice == SaveChoice::SaveNew) {
        // the save-as dialog answers through the same "did it save?" callback
        SaveAnnotationsToMaybeNewPdfFile(tab, MkFunc1(OnSaveAnnotsPromptDone, ctx));
        return;
    }
    OnSaveAnnotsPromptDone(ctx, SaveAnnotationsToExistingFile(tab));
}

// ng: orig's MaybeSaveAnnotations blocks in a win32 TASKDIALOG; a gpui dialog
// cannot block, so the four choices come back through a callback
void MaybeSaveAnnotations(WindowTab* tab, const Func1<bool>& onDone) {
    if (!AnnotationsNeedSavePrompt(tab) || IsStressTesting()) {
        onDone.Call(true);
        return;
    }
    tab->askedToSaveAnnotations = true;
    auto* ctx = new SavePromptCtx();
    ctx->tab = tab;
    ctx->onDone = onDone;
    ShowUnsavedAnnotationsDialog(tab->win, path::GetBaseNameTemp(tab->filePath), MkFunc1(OnSaveAnnotsChoice, ctx));
}

// the bytes the save-path dialog is about to write out
struct PendingSaveData {
    Str data; // owned
};

static void OnEmbeddedFilePathPicked(PendingSaveData* d, SavePathArgs* args) {
    if (len(args->path) > 0) {
        file::WriteFile(args->path, d->data);
    }
    str::Free(d->data);
    delete d;
}

void SaveEmbeddedFileAs(MainWindow* win, Annotation* annot, Str fileName) {
    if (!win || !CanAccessDisk() || !HasEmbeddedFile(annot)) {
        return;
    }
    Str data = LoadEmbeddedFile(annot);
    if (len(data) == 0) {
        return;
    }
    auto* pending = new PendingSaveData();
    pending->data = data;
    auto* args = new SavePathArgs();
    args->win = win;
    args->title = str::Dup(Tr("Save As"));
    args->initialPath = str::Dup(fileName);
    args->defExt = str::Dup(path::GetExtTemp(fileName));
    args->noDefExt = true;
    args->onDone = MkFunc1(OnEmbeddedFilePathPicked, pending);
    ShowSavePathDialog(args);
}

// Step the document's edit history. MuPDF restores the objects; every wrapper,
// selection and cached rendering that pointed at the old state has to go.
static void UndoRedoInTab(WindowTab* tab, bool redo) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    DisplayModel* dm = tab->AsFixed();
    if (!win || !dm) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || !EngineSupportsAnnotations(engine)) {
        return;
    }
    bool can = redo ? EngineMupdfCanRedo(engine) : EngineMupdfCanUndo(engine);
    if (!can) {
        ShowWarningNotification(win, redo ? Tr("Nothing to redo") : Tr("Nothing to undo"), kNotif5SecsTimeOut);
        return;
    }

    // an in-flight placement or drag would write to what we are about to undo
    CancelAnnotationPlacement(win);
    CanvasCancelDrag(win);
    SetSelectedAnnotation(tab, nullptr);
    if (gRenderCache) {
        gRenderCache->AbortRendering(dm);
    }

    Vec<Annotation*> removed;
    bool ok = redo ? EngineMupdfRedo(engine, removed) : EngineMupdfUndo(engine, removed);
    for (Annotation* a : removed) {
        DetachAnnotationFromUI(a);
        DeleteAnnotation(a);
    }
    // the wrapper deletes above mark the document modified; the journal knows better
    EngineMupdfRefreshModifiedState(engine);
    DeleteOldSelectionInfo(win, true);
    RefreshAnnotationLists(tab);
    NotifyAnnotationsChanged(tab);
    ToolbarUpdateStateForWindow(win, true);
    MainWindowRerender(win);
    if (!ok) {
        ShowWarningNotification(win, redo ? Tr("Nothing to redo") : Tr("Nothing to undo"), kNotif5SecsTimeOut);
    }
}

// returns false if no filter has been appended
static bool AppendFileFilterForDoc(DocController* ctrl, str::Builder& fileFilter) {
    Kind type = nullptr;
    if (ctrl->AsFixed()) {
        type = ctrl->AsFixed()->engineType;
    } else if (ctrl->AsChm()) {
        type = kindEngineChm;
    }
    // markdown has no engine kind; it falls through to the default filter below.
    // Prefer GetDefaultFileExt() where it distinguishes formats (xps, epub, …);
    // fall back to engine kind for the rest.
    auto ext = ctrl->GetDefaultFileExt();
    if (str::EqI(ext, StrL(".xps"))) {
        fileFilter.Append(Tr("XPS documents"));
    } else if (str::EqI(ext, StrL(".docx"))) {
        fileFilter.Append(Tr("Word documents"));
    } else if (str::EqI(ext, StrL(".xlsx"))) {
        fileFilter.Append(Tr("Excel workbooks"));
    } else if (str::EqI(ext, StrL(".pptx"))) {
        fileFilter.Append(Tr("PowerPoint presentations"));
    } else if (str::EqI(ext, StrL(".epub"))) { // NOLINT(bugprone-branch-clone): see kindEngineEpub below
        // .epub can be handled by kindEngineMupdf
        fileFilter.Append(Tr("EPUB ebooks"));
    } else if (type == kindEngineDjVu) {
        fileFilter.Append(Tr("DjVu documents"));
    } else if (type == kindEngineComicBooks) {
        fileFilter.Append(Tr("Comic books"));
    } else if (type == kindEngineImage) {
        Str imgDefExt = ctrl->GetDefaultFileExt();
        if (len(imgDefExt) > 0 && imgDefExt.s[0] == '.') {
            imgDefExt = Str(imgDefExt.s + 1, imgDefExt.len - 1);
        }
        fileFilter.Append(fmt(Tr("Image files (*.%s)").s, imgDefExt));
    } else if (type == kindEngineImageDir) {
        return false; // only show "All files"
    } else if (type == kindEnginePostScript || type == kindEngineDvi) {
        // also offer the PDF the converter produced (SaveFileAs writes it)
        if (type == kindEngineDvi) {
            fileFilter.Append(Tr("DVI documents"));
        } else {
            fileFilter.Append(Tr("PostScript documents"));
        }
        fileFilter.Append(fmt("\1*%s\1", ctrl->GetDefaultFileExt()));
        fileFilter.Append(Tr("PDF documents"));
        fileFilter.Append(StrL("\1*.pdf\1"));
        return false;
    } else if (type == kindEngineChm) {
        fileFilter.Append(Tr("CHM documents"));
    } else if (type == kindEngineEpub) {
        fileFilter.Append(Tr("EPUB ebooks"));
    } else if (type == kindEngineMobi) {
        fileFilter.Append(Tr("Mobi documents"));
    } else if (type == kindEngineFb2) {
        fileFilter.Append(Tr("FictionBook documents"));
    } else if (type == kindEnginePdb) {
        fileFilter.Append(Tr("PalmDoc documents"));
    } else {
        fileFilter.Append(Tr("PDF documents"));
    }
    return true;
}

// what the plain Save As dialog's answer is applied to
struct SaveFileAsCtx {
    MainWindow* win = nullptr;
    Str srcPath; // owned
    Str defExt;  // owned
};

static void OnSaveFileAsPathPicked(SaveFileAsCtx* ctx, SavePathArgs* args) {
    MainWindow* win = ctx->win;
    TempStr srcFileName = str::DupTemp(ctx->srcPath);
    TempStr defExt = str::DupTemp(ctx->defExt);
    str::Free(ctx->srcPath);
    str::Free(ctx->defExt);
    delete ctx;

    TempStr dstFilePath = str::DupTemp(args->path);
    if (len(dstFilePath) == 0 || !IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded()) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    logf("SaveCurrentFileAs: '%s' -> '%s'\n", srcFileName, dstFilePath);

    bool convertedAsPdf = engine && (engine->kind == kindEngineDvi || engine->kind == kindEnginePostScript) &&
                          str::EndsWithI(dstFilePath, StrL(".pdf"));
    // Make sure that the file has a valid extension
    if (!convertedAsPdf && !str::EndsWithI(dstFilePath, defExt)) {
        dstFilePath = str::JoinTemp(dstFilePath, defExt);
    }
    bool ok = true;
    if (convertedAsPdf || (!file::Exists(srcFileName) && engine)) {
        // Recreate nonexistent files from memory
        ok = engine->SaveFileAs(dstFilePath);
    } else if (!path::IsSame(srcFileName, dstFilePath)) {
        ok = file::Copy(dstFilePath, srcFileName, false);
    }
    // Belt-and-suspenders: some failure modes report success while nothing was
    // written, so the user has no way to tell the save silently failed (#1016)
    if (ok && !file::Exists(dstFilePath)) {
        logf("SaveCurrentFileAs(): '%s' doesn't exist after a successful save\n", dstFilePath);
        ok = false;
    }
    if (!ok) {
        MessageBoxWarning(win, Tr("Failed to save a file"));
        return;
    }
}

static void SaveCurrentFileAs(MainWindow* win) {
    if (!CanAccessDisk() || !win->IsDocLoaded()) {
        return;
    }
    auto* ctrl = win->ctrl;
    TempStr srcFileName = str::DupTemp(ctrl->GetFilePath());
    if (len(srcFileName) == 0) {
        ShowTemporaryNotification(win, Tr("File path not available"), kNotif5SecsTimeOut);
        return;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (EngineHasUnsavedAnnotations(engine)) {
        SaveAnnotationsToMaybeNewPdfFile(win->CurrentTab(), {});
        return;
    }
    TempStr defExt = str::DupTemp(ctrl->GetDefaultFileExt());

    auto* ctx = new SaveFileAsCtx();
    ctx->win = win;
    ctx->srcPath = str::Dup(srcFileName);
    ctx->defExt = str::Dup(defExt);

    auto* args = new SavePathArgs();
    args->win = win;
    args->title = str::Dup(Tr("Save As"));
    args->initialPath = str::Dup(srcFileName);
    args->defExt = str::Dup(defExt);

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::Builder fileFilter;
    fileFilter.Reserve(256);
    if (AppendFileFilterForDoc(ctrl, fileFilter)) {
        fileFilter.Append(fmt("\1*%s\1", defExt));
    }
    fileFilter.Append(Tr("All files"));
    fileFilter.Append(StrL("\1*.*\1"));
    args->filter = str::Dup(ToStr(fileFilter));

    // note: no directory, so that the OS picks a reasonable default
    TempStr dstFileName = str::DupTemp(path::GetBaseNameTemp(srcFileName));
    int colonOff = str::IndexOfChar(dstFileName, ':');
    if (colonOff >= 0) {
        // handle embed-marks (for embedded PDF documents):
        // remove the container document's extension and include
        // the embedding reference in the suggested filename
        TempStr mark = str::DupTemp(Str(dstFileName.s + colonOff, len(dstFileName) - colonOff));
        str::TransCharsInPlace(mark, StrL(":"), StrL("_"));
        int extOff = colonOff;
        while (extOff > 0 && dstFileName.s[extOff] != '.') {
            extOff--;
        }
        if (extOff == 0 && dstFileName.s[0] != '.') {
            extOff = colonOff;
        }
        dstFileName = str::JoinTemp(Str(dstFileName.s, extOff), mark);
    } else if (str::EndsWithI(dstFileName, defExt)) {
        // Remove the extension so that it can be re-added depending on the chosen filter
        dstFileName.len -= len(defExt);
    }
    args->nativeFile = str::Dup(dstFileName);
    args->onDone = MkFunc1(OnSaveFileAsPathPicked, ctx);
    ShowSavePathDialog(args);
}

// --- rename / delete / shortcut / cached files (orig SumatraPDF.cpp) -------

// orig keeps its caches (the local cbx copies, the open-cache, the EUTL trust
// list) in <local appdata>/SumatraPDF-data. ng: the app data directory is
// already that - or the exe's directory in portable mode - so they go in a
// "data" subdirectory of it
TempStr GetSumatraDataDirTemp() {
    TempStr dir = GetAppDataDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    return path::JoinTemp(dir, StrL("data"));
}

static void RenameFileInHistory(Str oldPath, Str newPath) {
    logf("RenameFileInHistory: oldPath: '%s', newPath: '%s'\n", oldPath, newPath);
    if (path::IsSame(oldPath, newPath)) {
        return;
    }
    FileState* fs = FileHistoryFindByPath(newPath);
    bool oldIsPinned = false;
    int oldOpenCount = 0;
    if (fs) {
        oldIsPinned = fs->isPinned;
        oldOpenCount = fs->openCount;
        FileHistoryRemove(fs);
        if (len(*fs->favorites) > 0) {
            UpdateFavoritesTreeForAllWindows();
        }
        DeleteFileState(fs);
    }
    fs = FileHistoryFindByPath(oldPath);
    if (!fs) {
        return;
    }
    SetFileStatePath(fs, newPath);
    // merge Frequently Read data, so that a file doesn't accidentally vanish
    fs->isPinned = fs->isPinned || oldIsPinned;
    fs->openCount += oldOpenCount;
    // the thumbnail is recreated by LoadDocument
    FreePixmap(fs->thumbnail);
    fs->thumbnail = nullptr;
}

// ng: orig's CloseDocumentInCurrentTab. The tab keeps its identity and its
// path; the controller goes, so the file is no longer open - which a rename
// on Windows needs
static void CloseDocumentInCurrentTab(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->ctrl) {
        return;
    }
    HideFindBar(win);
    HideSelectionToolbar(win);
    CloseAnnotationUiForTab(tab);
    ResetReadAloudStateForTab(tab);
    ReadingAutoScrollHideBar(win);
    ReadingBarCancelDrag(win);
    DeleteOldSelectionInfo(win, true);
    RemoveNotificationsForGroup(win, kNotifPageInfo);
    RemoveNotificationsForGroup(win, kNotifCursorPos);
    DocController* prev = tab->ctrl;
    tab->ctrl = nullptr;
    win->ctrl = nullptr;
    DeleteControllerAsync(prev);
}

// the file the rename dialog is about to move
struct RenameFileCtx {
    MainWindow* win = nullptr;
    Str srcPath; // owned
};

static void OnRenameFilePathPicked(RenameFileCtx* c, SavePathArgs* args) {
    MainWindow* win = c->win;
    TempStr srcPath = str::DupTemp(c->srcPath);
    str::Free(c->srcPath);
    delete c;
    if (len(args->path) == 0 || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    TempStr dstPath = path::NormalizeTemp(args->path);
    if (path::IsSame(path::NormalizeTemp(srcPath), dstPath)) {
        return;
    }

    UpdateTabFileDisplayStateForTab(win->CurrentTab());
    CloseDocumentInCurrentTab(win);

    bool moveOk = file::RenameReplace(dstPath, srcPath);
    if (!moveOk) {
        // ng: orig passes MOVEFILE_COPY_ALLOWED so the rename may cross
        // volumes; file::RenameReplace cannot, so copy and delete instead
        moveOk = file::Copy(dstPath, srcPath, false) && file::Delete(srcPath);
    }
    if (!moveOk) {
        logf("RenameCurrentFile: '%s' -> '%s' failed\n", srcPath, dstPath);
        LoadDocument(win, srcPath, LoadPrefs::Save, LoadReuse::CurrentTab);
        ShowPlainWarningNotification(win, Tr("Failed to rename the file!"), kNotifNoTimeout);
        return;
    }
    RenameFileInHistory(srcPath, dstPath);
    LoadDocument(win, dstPath, LoadPrefs::Save, LoadReuse::CurrentTab);
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    // the tab and the frame carry the old name until something repaints them
    UpdateWindowTitle(win);
    TabsUIOnTabsChanged(win);
}

static void RenameCurrentFile(MainWindow* win) {
    if (!CanAccessDisk() || !win->IsDocLoaded() || gPluginMode) {
        return;
    }
    auto* ctrl = win->ctrl;
    TempStr srcPath = str::DupTemp(ctrl->GetFilePath());
    // this happens e.g. for embedded documents and directories
    if (!file::Exists(srcPath)) {
        return;
    }
    auto* c = new RenameFileCtx();
    c->win = win;
    c->srcPath = str::Dup(srcPath);

    auto* args = new SavePathArgs();
    args->win = win;
    // note: the other two dialogs are named "Open" and "Save As"
    args->title = str::Dup(Tr("Rename To"));
    args->initialPath = str::Dup(srcPath);
    args->defExt = str::Dup(ctrl->GetDefaultFileExt());
    args->nativeTitle = true;
    str::Builder fileFilter;
    fileFilter.Reserve(256);
    AppendFileFilterForDoc(ctrl, fileFilter);
    fileFilter.Append(fmt("\1*%s\1", args->defExt));
    args->filter = str::Dup(ToStr(fileFilter));
    // Remove the extension so that it can be re-added depending on the chosen filter
    TempStr dstFileName = str::DupTemp(path::GetBaseNameTemp(srcPath));
    if (str::EndsWithI(dstFileName, args->defExt)) {
        dstFileName.len -= len(args->defExt);
    }
    args->nativeFile = str::Dup(dstFileName);
    args->nativeDir = str::Dup(path::GetDirTemp(srcPath));
    args->onDone = MkFunc1(OnRenameFilePathPicked, c);
    ShowSavePathDialog(args);
}

static void DeleteCurrentFile(MainWindow* win) {
    if (!CanAccessDisk() || !win->IsDocLoaded() || gPluginMode) {
        return;
    }
    TempStr path = str::DupTemp(win->ctrl->GetFilePath());
    // this happens e.g. for embedded documents and directories
    if (!file::Exists(path)) {
        return;
    }
    CloseCurrentTab(win, false);
    DeleteFileFromDiskAndHistory(path);
    // CloseCurrentTab may have destroyed the window if it had no more tabs
    if (IsMainWindowValid(win)) {
        win->RedrawAll(true);
    }
}

static void DeleteCurrentFileAndOpenNext(MainWindow* win) {
    if (!CanAccessDisk() || !win->IsDocLoaded() || gPluginMode) {
        return;
    }
    Str path = str::Dup(win->ctrl->GetFilePath());
    // this happens e.g. for embedded documents and directories
    if (len(path) == 0 || !file::Exists(path)) {
        str::Free(path);
        return;
    }
    OpenNextPrevFileInFolder(win, true, path);
    str::Free(path);
}

#if OS_WIN
// the .lnk the shortcut dialog is about to write
struct LnkShortcutCtx {
    MainWindow* win = nullptr;
    Str docPath; // owned
    Str args;    // owned
    Str desc;    // owned
};

static void DeleteLnkShortcutCtx(LnkShortcutCtx* c) {
    str::Free(c->docPath);
    str::Free(c->args);
    str::Free(c->desc);
    delete c;
}

static void OnLnkShortcutPathPicked(LnkShortcutCtx* c, SavePathArgs* args) {
    TempStr fileName = str::DupTemp(args->path);
    if (len(fileName) > 0) {
        if (!str::EndsWithI(fileName, StrL(".lnk"))) {
            fileName = str::JoinTemp(fileName, StrL(".lnk"));
        }
        auto exePath = GetSelfExePathTemp();
        CreateShortcut(fileName, exePath, c->args, c->desc, 1);
    }
    DeleteLnkShortcutCtx(c);
}

static void CreateLnkShortcut(MainWindow* win) {
    if (!CanAccessDisk() || gPluginMode || !win->IsDocLoaded()) {
        return;
    }
    auto* ctrl = win->ctrl;
    Str path = ctrl->GetFilePath();

    // Remove the extension so that it can be replaced with .lnk
    TempStr defExt = str::DupTemp(ctrl->GetDefaultFileExt());
    TempStr dstFileName = str::DupTemp(path::GetBaseNameTemp(path));
    str::TransCharsInPlace(dstFileName, StrL(":"), StrL("_"));
    if (str::EndsWithI(dstFileName, defExt)) {
        dstFileName.len -= len(defExt);
    }

    ScrollState ss(ctrl->CurrentPageNo(), 0, 0);
    if (win->AsFixed()) {
        ss = win->AsFixed()->GetScrollState();
    }
    Str viewMode = DisplayModeToString(ctrl->GetDisplayMode());
    TempStr zoomVirtual = fmt("%.2f", ctrl->GetZoomVirtual());
    if (kZoomFitPage == ctrl->GetZoomVirtual()) {
        zoomVirtual = StrL("fitpage");
    } else if (kZoomFitWidth == ctrl->GetZoomVirtual()) {
        zoomVirtual = StrL("fitwidth");
    } else if (kZoomFitHeight == ctrl->GetZoomVirtual()) {
        zoomVirtual = StrL("fitheight");
    } else if (kZoomFitContent == ctrl->GetZoomVirtual()) {
        zoomVirtual = StrL("fitcontent");
    }
    TempStr label = ctrl->GetPageLabeTemp(ss.page);

    auto* c = new LnkShortcutCtx();
    c->win = win;
    c->docPath = str::Dup(path);
    c->args = str::Dup(fmt("\"%s\" -page %d -view \"%s\" -zoom %s -scroll %d,%d", path, ss.page, viewMode, zoomVirtual,
                           (int)ss.x, (int)ss.y));
    c->desc = str::Dup(fmt(Tr("Bookmark shortcut to page %s of %s").s, label, path));

    auto* args = new SavePathArgs();
    args->win = win;
    args->title = str::Dup(Tr("Bookmark Shortcuts"));
    args->initialPath = str::Dup(path::JoinTemp(path::GetDirTemp(path), str::JoinTemp(dstFileName, StrL(".lnk"))));
    args->defExt = str::Dup(StrL(".lnk"));
    args->filter = str::Dup(fmt("%s\1*.lnk\1", Tr("Bookmark Shortcuts")));
    args->nativeFile = str::Dup(dstFileName);
    args->onDone = MkFunc1(OnLnkShortcutPathPicked, c);
    ShowSavePathDialog(args);
}
#endif

// Unconditionally delete all local copies of comic-book archives that were
// cached under <data>/cbx-cache/ when opening them from a network drive.
// Safe to call with no open document; open documents may still hold a lock
// on a cache file so some deletes can fail (logged).
static void DeleteCachedFiles(MainWindow* win) {
    int nDeleted = 0;
    int nFailed = 0;
    TempStr dataDir = GetSumatraDataDirTemp();
    if (len(dataDir) > 0) {
        TempStr cacheDir = path::JoinTemp(dataDir, StrL("cbx-cache"));
        if (path::GetType(cacheDir) == path::Type::Dir) {
            DirIter di{cacheDir};
            di.includeFiles = true;
            di.includeDirs = false;
            for (DirIterEntry* de : di) {
                TempStr sizeStr = str::FormatSizeShortTemp(de->size);
                if (file::Delete(de->filePath)) {
                    nDeleted++;
                    logf("DeleteCachedFiles: deleted '%s' (%s)\n", de->filePath, sizeStr);
                } else {
                    nFailed++;
                    logf("DeleteCachedFiles: failed to delete '%s' (%s)\n", de->filePath, sizeStr);
                }
            }
            // remove the (now empty, or residual) cache directory itself
            if (nFailed == 0) {
                dir::RemoveAll(cacheDir);
            }
        }
    }
    logf("DeleteCachedFiles: deleted %d, failed %d\n", nDeleted, nFailed);
    if (!win) {
        return;
    }
    TempStr msg;
    if (nDeleted == 0 && nFailed == 0) {
        msg = fmt("%s", Tr("No cached comic book files."));
    } else if (nFailed == 0) {
        msg = fmt(Tr("Deleted %d cached comic book files.").s, nDeleted);
    } else {
        msg = fmt(Tr("Deleted %d cached comic book files, %d failed.").s, nDeleted, nFailed);
    }
    ShowTemporaryNotification(win, msg, kNotif5SecsTimeOut);
}

// CmdShowGeneratedHTML: the HTML the markdown viewer renders, as a file
static void ShowGeneratedMarkdownHtml(MainWindow* win) {
    if (!win || !win->IsDocLoaded() || !CanAccessDisk()) {
        return;
    }
    DocController* ctrl = win->ctrl;
    Str path = ctrl->GetFilePath();
    bool isMarkdown = str::EndsWithI(path, StrL(".md")) || str::EndsWithI(path, StrL(".markdown"));
    if (!isMarkdown) {
        return;
    }

    Str html;
    bool ownsHtml = false;
    MarkdownModel* model = ctrl->AsMarkdown();
    if (model && len(model->currentPageUrl) > 0) {
        html = model->GetDataForUrl(model->currentPageUrl);
    } else {
        Str markdown = file::ReadFile(path);
        if (len(markdown) > 0) {
            html = MarkdownToHtmlPage(markdown);
            ownsHtml = true;
        }
        str::Free(markdown);
    }
    if (len(html) == 0) {
        return;
    }

    TempStr tempPath = GetTempFilePathTemp(StrL("smd"));
    TempStr htmlPath = str::JoinTemp(tempPath, StrL(".html"));
    bool ok = len(tempPath) > 0 && file::Rename(htmlPath, tempPath) && file::WriteFile(htmlPath, html);
    if (ownsHtml) {
        str::Free(html);
    }
    if (!ok) {
        file::Delete(tempPath);
        file::Delete(htmlPath);
        logf("ShowGeneratedMarkdownHtml: failed to create temporary HTML file\n");
        return;
    }
    // ng: orig launches Notepad from the Windows system directory so the user
    // sees the source. There is no portable equivalent, so the file opens in
    // whatever the OS registered for .html
    if (!LaunchFileIfExists(htmlPath)) {
        logf("ShowGeneratedMarkdownHtml: failed to open '%s'\n", htmlPath);
    }
}

// orig's Ctrl+V for an image: save what the clipboard holds as a PNG in our
// data directory and open it as a document. Takes ownership of `image`.
static void PasteImageFromClipboard(MainWindow* win, Pixmap* image) {
    if (!image) {
        ShowWarningNotification(win, Tr("No image in the clipboard"), kNotifDefaultTimeOut);
        return;
    }
    Str png = EncodePngFromPixmap(image);
    FreePixmap(image);
    if (len(png) == 0) {
        return;
    }
    // generate unique path in our data dir: clipboard.png, clipboard.1.png, etc.
    TempStr dataDir = GetAppDataDirTemp();
    dir::CreateAll(dataDir);
    TempStr basePath = path::JoinTemp(dataDir, StrL("clipboard.png"));
    TempStr destPath = MakeUniqueFilePathTemp(basePath);
    bool ok = file::WriteFile(destPath, png);
    str::Free(png);
    if (!ok) {
        logf("PasteImageFromClipboard: failed to write '%s'\n", destPath);
        return;
    }
    OptimizePngFileAsync(destPath);
    LoadDocument(win, destPath);
}

// --- closing ----------------------------------------------------------------

// After a nested close the tab may already be gone. GetTabIdx does not
// dereference `tab`, so a freed pointer just comes back as -1.
static bool TabStillInWindow(MainWindow* win, WindowTab* tab) {
    return IsMainWindowValid(win) && !win->isBeingClosed && win->GetTabIdx(tab) >= 0;
}

// ng: the unsaved-annotations prompt is asynchronous (a gpui dialog cannot
// block), so the close is re-entered once the answer is in
struct PendingCloseTab {
    WindowTab* tab = nullptr;
    bool quitIfLast = false;
};

struct PendingCloseWindow {
    MainWindow* win = nullptr;
    Vec<WindowTab*> tabs;
    int tabIdx = 0;
    bool quitIfLast = false;
};

static void ContinueCloseWindow(PendingCloseWindow* p);

static void OnCloseWindowAfterSavePrompt(PendingCloseWindow* p, bool canClose) {
    MainWindow* win = p->win;
    if (!canClose) {
        if (IsMainWindowValid(win)) {
            win->isClosePending = false;
        }
        delete p;
        return;
    }
    p->tabIdx++;
    ContinueCloseWindow(p);
}

static void ContinueCloseWindow(PendingCloseWindow* p) {
    MainWindow* win = p->win;
    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        delete p;
        return;
    }
    while (p->tabIdx < len(p->tabs)) {
        WindowTab* tab = p->tabs[p->tabIdx];
        if (!TabStillInWindow(win, tab) || !AnnotationsNeedSavePrompt(tab)) {
            p->tabIdx++;
            continue;
        }
        MaybeSaveAnnotations(tab, MkFunc1(OnCloseWindowAfterSavePrompt, p));
        return;
    }

    bool quitIfLast = p->quitIfLast;
    win->isClosePending = false;
    delete p;
    CloseWindow(win, quitIfLast, false);
}

void RequestCloseWindow(MainWindow* win, bool quitIfLast) {
    if (!IsMainWindowValid(win) || win->isBeingClosed || win->isClosePending) {
        return;
    }
    win->isClosePending = true;
    auto* p = new PendingCloseWindow();
    p->win = win;
    p->quitIfLast = quitIfLast;
    for (WindowTab* tab : win->Tabs()) {
        VecAppend(p->tabs, tab);
    }
    ContinueCloseWindow(p);
}

struct PendingCanClose {
    MainWindow* win = nullptr;
    bool quitIfLast = false;
};

static void OnCanCloseAnswer(PendingCanClose* p, int res) {
    MainWindow* win = p->win;
    bool quitIfLast = p->quitIfLast;
    delete p;
    if (res == MbRetYes && IsMainWindowValidAndNotClosing(win)) {
        RequestCloseWindow(win, quitIfLast);
    }
}

// orig's `if (CanCloseWindow(win)) CloseWindow(win, ...)`: CanCloseWindow asks
// before a print job is abandoned. ng: the message box answers through a
// callback, so the close is its continuation
void CloseWindowIfCan(MainWindow* win, bool quitIfLast) {
    if (!win) {
        return;
    }
    if (!win->printThread) {
        RequestCloseWindow(win, quitIfLast);
        return;
    }
    uint flags = MbIconWarning | MbYesNo;
    Str caption = Tr("Printing in progress.");
    Str msg = Tr("Printing is still in progress. Abort and quit?");
    auto* p = new PendingCanClose{win, quitIfLast};
    MsgBox(win, msg, caption, flags, MkFunc1<PendingCanClose, int>(OnCanCloseAnswer, p));
}

static void OnCloseTabAfterSavePrompt(PendingCloseTab* p, bool canClose) {
    WindowTab* tab = p->tab;
    bool quitIfLast = p->quitIfLast;
    delete p;
    if (canClose && IsWindowTabValid(tab)) {
        CloseTab(tab, quitIfLast);
    }
}

void CloseTab(WindowTab* tab, bool quitIfLast) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    if (!TabStillInWindow(win, tab)) {
        return;
    }
    if (AnnotationsNeedSavePrompt(tab)) {
        auto* p = new PendingCloseTab{tab, quitIfLast};
        MaybeSaveAnnotations(tab, MkFunc1(OnCloseTabAfterSavePrompt, p));
        return;
    }
    logf("CloseTab: '%s', quitIfLast: %d\n", tab->filePath, (int)quitIfLast);
    if (tab == win->CurrentTab()) {
        // Cancel while win->ctrl still belongs to the closing tab. The toolbar
        // update in cancellation needs CurrentTab and ctrl to agree.
        CancelAnnotationPlacement(win);
    }
    CloseAnnotationUiForTab(tab);

    AbortFinding(win, true);
    if (!TabStillInWindow(win, tab)) {
        return;
    }
    // the find UI holds the closing tab's document; so does the selection
    // toolbar, which would otherwise stay on screen after the tab is gone
    if (tab == win->CurrentTab()) {
        HideFindBar(win);
        HideSelectionToolbar(win);
    }
    // Stop eventual TTS reading. The full reset (rather than just
    // StopReadAloudIfSourceTab) also drops the pointers to this tab held by the
    // playback bar and the session, which is about to be a dangling one
    ResetReadAloudStateForTab(tab);
    ReadingAutoScrollForgetTab(tab);
    ReadingBarForgetTab(tab);
    RemoveNotificationsForGroup(win, kNotifZoomOrView);
    RemoveNotificationsForTab(tab);
    RememberRecentlyClosedDocument(tab->filePath);

    int tabCount = win->TabCount();
    if (tabCount == 1 || (tabCount == 0 && quitIfLast)) {
        CloseWindow(win, quitIfLast, false);
        return;
    }
    if (win->GetTabIdx(tab) < 0) {
        return;
    }
    RemoveTab(tab);
    delete tab;
    if (!IsMainWindowValid(win)) {
        return;
    }
    // only the home tab left, and another window is open: this one goes away
    WindowTab* lastTab = (win->TabCount() == 1) ? win->GetTab(0) : nullptr;
    if (lastTab && lastTab->IsAboutTab() && len(gWindows) > 1) {
        CloseWindow(win, false, false);
        return;
    }
    UpdateWindowTitle(win);
    RebuildMenuBar(win);
    ScheduleSaveSettings();
    AppShellInvalidate(win);
}

void CloseCurrentTab(MainWindow* win, bool quitIfLast) {
    if (!win) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    bool lastDocTab = !tab || tab->IsAboutTab() || !HasOpenedDocuments(win);
    if (tab && !tab->IsAboutTab()) {
        CloseTab(tab, quitIfLast);
        if (!IsMainWindowValid(win)) {
            return;
        }
        lastDocTab = !HasOpenedDocuments(win);
    }
    if (lastDocTab && quitIfLast) {
        CloseWindow(win, true, false);
        return;
    }
    UpdateWindowTitle(win);
    RebuildMenuBar(win);
    AppShellInvalidate(win);
}

static void ReopenLastClosedFile(MainWindow* win) {
    Str path = PopRecentlyClosedDocument();
    if (len(path) == 0) {
        return;
    }
    logf("ReopenLastClosedFile: '%s'\n", path);
    LoadDocument(win, path);
}

// ng: Menu.cpp and the other non-gpui files reach the clipboard through this
// (gpui's ClipboardSetText needs the window)
void CopyTextToClipboard(MainWindow* win, Str s) {
    if (!win || !win->gpuiWin || len(s) == 0) {
        return;
    }
    gp::ClipboardSetText(win->gpuiWin, ToGpui(s));
}

// takes ownership of bmp, as orig's PixmapFromRenderedBitmap does
void CopyRenderedBitmapToClipboard(MainWindow*, RenderedBitmap* bmp) {
    if (!bmp) {
        return;
    }
#if OS_WIN
    // via the Pixmap, so an image with an alpha channel reaches the clipboard
    // with its transparency intact (#5844, #5598)
    Pixmap* px = PixmapFromRenderedBitmap(bmp);
    ImageEditCopyToClipboard(px);
    FreePixmap(px);
#else
    // ng: nothing creates a RenderedBitmap off Windows (it is a win32 HBITMAP
    // wrapper and is not even a complete type there), so bmp is always null
#endif
}

// orig's WindowTab.cpp SaveDataToFile: a save prompt, then the bytes.
// ng: the prompt is the port's own dialog, so this returns before the write
void SaveDataToFile(MainWindow* win, Str fileName, Str data) {
    if (!CanAccessDisk() || len(data) == 0) {
        return;
    }
    auto* pending = new PendingSaveData();
    pending->data = str::Dup(data);
    auto* args = new SavePathArgs();
    args->win = win;
    args->title = str::Dup(Tr("Save As"));
    args->initialPath = str::Dup(fileName);
    args->defExt = str::Dup(path::GetExtTemp(fileName));
    args->noDefExt = true;
    args->onDone = MkFunc1(OnEmbeddedFilePathPicked, pending);
    ShowSavePathDialog(args);
}

void DeleteFileFromDiskAndHistory(Str path) {
    file::DeleteFileToTrash(path);
    DeleteThumbnailForFile(path);
    FileState* fs = FileHistoryFindByPath(path);
    if (fs) {
        FileHistoryRemove(fs);
        DeleteFileState(fs);
    }
    ScheduleSaveSettings();
}

static void OnHomeDeleteTabClosed(MainWindow* win, Str path, bool ok) {
    if (!ok || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    DeleteFileFromDiskAndHistory(path);
    win->RedrawAll(true);
}

struct HomeDeleteCtx {
    MainWindow* win;
    Str path; // owned
};

static void OnHomeDeleteDone(HomeDeleteCtx* c, bool ok) {
    if (ok) {
        WindowTab* tab = FindTabByFilePath(c->path);
        if (tab) {
            CloseTab(tab, false);
        }
        OnHomeDeleteTabClosed(c->win, c->path, true);
    }
    str::Free(c->path);
    delete c;
}

// orig's CmdDeleteFile in the home page's context menu: a file that is gone
// only leaves the history, one that is there goes to the trash
void DeleteFileFromHomePage(MainWindow* win, Str path) {
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }
    TempStr owned = str::DupTemp(path);
    if (!file::Exists(owned)) {
        ForgetFileFromFrequentlyRead(win, owned);
        return;
    }
    WindowTab* tab = FindTabByFilePath(owned);
    auto* c = new HomeDeleteCtx{win, str::Dup(owned)};
    if (!tab) {
        OnHomeDeleteDone(c, true);
        return;
    }
    MaybeSaveAnnotations(tab, MkFunc1(OnHomeDeleteDone, c));
}

// ng: orig opens an attachment straight out of memory (a tab with no file
// path). Every load here goes through a path, so the bytes land in a temp file
// under the data directory and that is what is opened.
bool OpenDocumentFromMemory(MainWindow* win, Str data, Str nameHint) {
    if (!win || len(data) == 0) {
        return false;
    }
    TempStr name = path::GetBaseNameTemp(nameHint);
    if (len(name) == 0) {
        name = StrL("attachment");
    }
    TempStr dir = path::JoinTemp(GetSumatraDataDirTemp(), StrL("attachments"));
    dir::CreateAll(dir);
    TempStr filePath = MakeUniqueFilePathTemp(path::JoinTemp(dir, name));
    if (!file::WriteFile(filePath, data)) {
        return false;
    }
    logf("OpenDocumentFromMemory: '%s' -> '%s'\n", name, filePath);
    return LoadDocument(win, filePath) != nullptr;
}

void CopyFilePath(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    gp::ClipboardSetText(tab->win->gpuiWin, ToGpui(tab->filePath));
}

static bool FilePickerIsSumatraPDF();

// Show in folder: Explorer (and select the file) unless File / Use SumatraPDF
// file picker is on, in which case open Navigate Files in Folder on that dir
void ShowFileInFolder(MainWindow* win, Str path) {
    if (!win || len(path) == 0 || gPluginMode || !CanAccessDisk()) {
        return;
    }
    if (FilePickerIsSumatraPDF() || OS_WASM) {
        ShowNavFilesInFolder(win, path, false);
        return;
    }
    OpenPathInDefaultFileManager(path);
}

void CloseWindow(MainWindow* win, bool quitIfLast, bool) {
    if (!win || win->isBeingClosed) {
        return;
    }
    win->isClosePending = false;
    win->isBeingClosed = true;
    logf("CloseWindow: 0x%p, %d windows open\n", win, len(gWindows));
    // Stop eventual TTS reading
    StopReadAloudIfSourceWindow(win);
    // the provider processes hold a pipe this window's reader thread reads
    ShutdownAIChatForMainWindow(win);
    DestroyAIChatPanel(win);
    SimpleBrowserWindowCloseFor(win);
    AbortPrinting(win);
    UpdateTabFileDisplayStateForTab(win->CurrentTab());
    RememberDefaultWindowPosition(win);
    // snapshot the last window before it leaves gWindows; an empty list wipes the session
    bool lastWindow = len(gWindows) == 1;
    if (!gDontSaveSettings && lastWindow) {
        ScheduleSaveSettings();
        FlushScheduledSaveSettings();
    }
    RemoveNotificationsForWindow(win);
    TabsOnCloseWindow(win);
    // Last window, and this is not a quit: orig keeps the frame as an empty
    // window and drops the page number and the scrollbars (issue #6062).
    if (lastWindow && !quitIfLast) {
        ClearToolbarLocationEdits(win);
        CanvasHideScrollbars(win);
        UpdateWindowTitle(win);
        RebuildMenuBar(win);
        win->isBeingClosed = false;
        AppShellInvalidate(win);
        return;
    }
    VecRemove(gWindows, win);
    AppShellCloseWindow(win);
    delete win;
    // the closed window is no longer part of the session
    if (!gDontSaveSettings && !lastWindow) {
        ScheduleSaveSettings();
        FlushScheduledSaveSettings();
    }
    if (quitIfLast && len(gWindows) == 0) {
        // the exit flush must not snapshot the empty window list
        gDontSaveSettings = true;
        AppShellQuit();
    }
}

struct PendingExit {
    Vec<WindowTab*> tabs;
    int tabIdx = 0;
};

static bool gExitPending = false;

static void FinishExit() {
    // we want to preserve the session state of all windows, so we save it now:
    // since we are closing the windows one by one, CloseWindow() must not save
    // the session state every time (or we end up with just the last window)
    ScheduleSaveSettings();
    FlushScheduledSaveSettings();
    gDontSaveSettings = true;

    Vec<MainWindow*> toClose = gWindows;
    for (MainWindow* win : toClose) {
        CloseWindow(win, false, false);
    }
    AppShellQuit();
}

static void ContinueExit(PendingExit* p);

static void OnExitSavePromptDone(PendingExit* p, bool canClose) {
    if (!canClose) {
        gExitPending = false;
        delete p;
        return;
    }
    p->tabIdx++;
    ContinueExit(p);
}

static void ContinueExit(PendingExit* p) {
    while (p->tabIdx < len(p->tabs)) {
        WindowTab* tab = p->tabs[p->tabIdx];
        if (!IsWindowTabValid(tab) || !AnnotationsNeedSavePrompt(tab)) {
            p->tabIdx++;
            continue;
        }
        MaybeSaveAnnotations(tab, MkFunc1(OnExitSavePromptDone, p));
        return;
    }
    delete p;
    FinishExit();
}

void OnMenuExit() {
    if (gExitPending) {
        return;
    }
    gExitPending = true;
    auto* p = new PendingExit();
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            VecAppend(p->tabs, tab);
        }
    }
    ContinueExit(p);
}

// --- the sidebar ------------------------------------------------------------

#if OS_WIN
static int SidebarExtraDx(MainWindow* win) {
    int dx = win->sidebarDx;
    if (dx <= 0 && gSettings) {
        dx = gSettings->sidebarDx;
    }
    if (dx < kSidebarMinDx) {
        dx = kSidebarMinDx;
    }
    return dx + kSplitterDx;
}

// SidebarWindowSize = grow opts in; by default the window keeps its size (#6205)
static bool FrameCanResizeForSidebar(MainWindow* win) {
    HWND hwnd = win ? AppShellNativeHwnd(win) : nullptr;
    if (!hwnd || gPluginMode) {
        return false;
    }
    if (!str::EqI(gSettings->sidebarWindowSize, StrL("grow"))) {
        return false;
    }
    if (win->isFullScreen || win->presentation) {
        return false;
    }
    if (IsZoomed(hwnd)) {
        return false;
    }
    if (!HwndIsVisible(hwnd)) {
        return false;
    }
    return true;
}
#endif

// Grow the frame by sidebar minus unused canvas margin (Fit Width has
// none). Skip if already grown (tab switch). Hide undoes the grow.
// ng: needs to move the window, which gpui cannot (see "gpui gaps"): done on
// the HWND on Windows, nothing elsewhere. The sidebar and the canvas margin
// are in dips, the frame in pixels
static void AdjustFrameForSidebarNow(MainWindow* win, bool show) {
#if OS_WIN
    if (!FrameCanResizeForSidebar(win)) {
        if (!show) {
            win->sidebarGrewFrameDx = 0;
        }
        return;
    }

    HWND hwnd = AppShellNativeHwnd(win);
    Rect wr = HwndWindowRect(hwnd);
    Rect work = GetWorkAreaRect(wr, hwnd);
    bool onRight = gSettings->sidebarOnRight;
    int dpi = AppShellWindowDpi(win);

    if (show) {
        if (win->sidebarGrewFrameDx > 0) {
            return;
        }
        int extra = SidebarExtraDx(win);
        int unused = 0;
        if (DisplayModel* dm = win->AsFixed()) {
            unused = dm->UnusedCanvasDx();
        }
        int spare = work.dx - wr.dx;
        int grow = limitValue(MulDiv(extra - unused, dpi, 96), 0, std::max(spare, 0));
        if (grow <= 0) {
            win->sidebarGrewFrameDx = 0;
            return;
        }
        wr.dx += grow;
        if (!onRight) {
            wr.x -= grow;
        }
        wr = ShiftRectToWorkArea(wr, hwnd, true);
        win->sidebarGrewFrameDx = grow;
        if (win->sidebarDx < kSidebarMinDx) {
            win->sidebarDx = extra - kSplitterDx;
        }
        HwndMoveWindow(hwnd, &wr);
        return;
    }

    int extra = win->sidebarGrewFrameDx;
    win->sidebarGrewFrameDx = 0;
    if (extra <= 0 || wr.dx <= extra) {
        return;
    }
    if (!onRight) {
        wr.x += extra;
    }
    wr.dx -= extra;
    wr = ShiftRectToWorkArea(wr, hwnd, true);
    HwndMoveWindow(hwnd, &wr);
#else
    (void)win;
    (void)show;
#endif
}

struct SidebarFrameArgs {
    MainWindow* win = nullptr;
    bool show = false;
};

static void AdjustFrameForSidebarDeferred(SidebarFrameArgs* args) {
    AutoDelete delArgs(args);
    if (!IsMainWindowValidAndNotClosing(args->win)) {
        return;
    }
    // the sidebar may have been toggled again since this was posted
    bool nowSidebar = args->win->uiState.tocVisible || args->win->uiState.favVisible;
    if (nowSidebar == args->show) {
        AdjustFrameForSidebarNow(args->win, args->show);
    }
}

// ng: resizing the frame renders it, which resets the temp arena under the
// callers of SetSidebarVisibility (see PlaceMainWindowLater), so it is posted
static void AdjustFrameForSidebar(MainWindow* win, bool show) {
    auto* args = new SidebarFrameArgs{win, show};
    uitask::Post(MkFunc0(AdjustFrameForSidebarDeferred, args), "AdjustFrameForSidebar");
}

// ng: orig records the wanted visibility in win->uiState and lets a deferred
// RelayoutFrame move the sidebar HWNDs; gpui lays the panes out from the same
// two flags on the next frame, so this only has orig's bookkeeping.
void SetSidebarVisibility(MainWindow* win, bool tocVisible, bool showFavorites) {
    bool requestedFavorites = showFavorites;
    if (!CanAccessDisk()) {
        showFavorites = false;
        requestedFavorites = false;
    }
    // a QuickLook preview shows the page and nothing else; the heading ToC
    // arrives after ApplyExplorerQuickLookChrome() and would re-open the pane
    if (win->isQuickLook) {
        tocVisible = false;
        showFavorites = false;
    }

    bool requestedToc = tocVisible;
    WindowTab* tab = win->CurrentTab();
    SidebarResolveContents(win);
    SidebarContent topContent = tab ? tab->sidebarContent : SidebarContent::Bookmarks;
    SidebarContent bottomContent = win->sidebarBottomContent;
    EngineBase* engine = win->CurrentTab() ? win->CurrentTab()->GetEngine() : nullptr;
    bool headingPending = EngineMupdfHeadingTocPending(engine);

    bool topAvailable =
        (topContent == SidebarContent::Favorites && CanAccessDisk()) ||
        (topContent == SidebarContent::Thumbnails ? win->AsFixed() != nullptr : win->ctrl && win->ctrl->HasToc());
    bool bottomAvailable =
        (bottomContent == SidebarContent::Favorites && CanAccessDisk()) ||
        (bottomContent == SidebarContent::Thumbnails ? win->AsFixed() != nullptr : win->ctrl && win->ctrl->HasToc());
    tocVisible = tocVisible && topAvailable;
    showFavorites = showFavorites && bottomAvailable;

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        tocVisible = false;
        showFavorites = false;
    }

    bool bookmarks = (tocVisible && topContent == SidebarContent::Bookmarks) ||
                     (showFavorites && bottomContent == SidebarContent::Bookmarks);
    if (bookmarks) {
        LoadTocTree(win);
        if (!win->tocLoaded) {
            tocVisible = tocVisible && topContent != SidebarContent::Bookmarks;
            showFavorites = showFavorites && bottomContent != SidebarContent::Bookmarks;
        }
    }

    bool favorites = (tocVisible && topContent == SidebarContent::Favorites) ||
                     (showFavorites && bottomContent == SidebarContent::Favorites);
    if (favorites) {
        PopulateFavTreeIfNeeded(win);
    }

    if (!tab) {
        ReportIf(tocVisible);
    } else if (!win->presentation) {
        bool pending = topContent == SidebarContent::Bookmarks && headingPending;
        if (topAvailable || pending) {
            tab->showToc = requestedToc;
        } else {
            tab->showToc = tocVisible;
        }
    } else if (PM_ENABLED == win->presentation) {
        tab->showTocPresentation = tocVisible;
    }

    // TODO: make this a per-window setting as well?
    gSettings->showFavorites = requestedFavorites;

    bool wasSidebar = win->uiState.tocVisible || win->uiState.favVisible;
    win->uiState.tocVisible = tocVisible;
    win->uiState.favVisible = showFavorites;
    bool nowSidebar = tocVisible || showFavorites;
    if (wasSidebar != nowSidebar) {
        AdjustFrameForSidebar(win, nowSidebar);
    }
    logf("SetSidebarVisibility: toc %d, fav %d\n", (int)tocVisible, (int)showFavorites);
    if (bookmarks) {
        UpdateTocSelection(win, win->currPageNo);
    }
    AppShellInvalidate(win);
}

// --- view and zoom ----------------------------------------------------------

void SwitchToDisplayMode(MainWindow* win, DisplayMode displayMode, bool keepContinuous) {
    if (!win->IsDocLoaded()) {
        return;
    }
    win->ctrl->SetDisplayMode(displayMode, keepContinuous);
}

static void ToggleContinuousView(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    DisplayMode newMode = win->ctrl->GetDisplayMode();
    switch (newMode) {
        case DisplayMode::SinglePage:
        case DisplayMode::Continuous:
            newMode = IsContinuous(newMode) ? DisplayMode::SinglePage : DisplayMode::Continuous;
            break;
        case DisplayMode::Facing:
        case DisplayMode::ContinuousFacing:
            newMode = IsContinuous(newMode) ? DisplayMode::Facing : DisplayMode::ContinuousFacing;
            break;
        case DisplayMode::BookView:
        case DisplayMode::ContinuousBookView:
            newMode = IsContinuous(newMode) ? DisplayMode::BookView : DisplayMode::ContinuousBookView;
            break;
        default:
            break;
    }
    SwitchToDisplayMode(win, newMode);
}

static void ToggleMangaMode(MainWindow* win) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    dm->SetDisplayR2L(!dm->GetDisplayR2L());
    ScrollState state = dm->GetScrollState();
    dm->Relayout(dm->GetZoomVirtual(), dm->GetRotation());
    dm->SetScrollState(state);
}

static void ShowZoomNotification(MainWindow* win, float zoomLevel) {
    // don't show zoom info if showing page info
    NotificationWnd* wnd = GetNotificationForGroup(win, kNotifPageInfo);
    if (wnd) {
        return;
    }
    NotificationCreateArgs args;
    args.groupId = kNotifZoomOrView;
    args.timeoutMs = 2000;
    args.win = win;
    args.msg = BuildZoomString(zoomLevel);
    ShowNotification(args);
}

static void ShowViewModeNotification(MainWindow* win, int cmdId) {
    NotificationWnd* wnd = GetNotificationForGroup(win, kNotifPageInfo);
    if (wnd) {
        return;
    }
    Str viewName;
    if (cmdId == CmdSinglePageView) {
        viewName = Tr("Single Page");
    } else if (cmdId == CmdFacingView) {
        viewName = Tr("Facing");
    } else if (cmdId == CmdBookView) {
        viewName = Tr("Book View");
    } else {
        return;
    }
    TempStr msg = fmt("%s: %s", Tr("View"), viewName);
    NotificationCreateArgs args;
    args.groupId = kNotifZoomOrView;
    args.timeoutMs = 2000;
    args.win = win;
    args.msg = msg;
    ShowNotification(args);
}

// if pt is given, it's a position on the canvas that we try to keep put
void SmartZoom(MainWindow* win, float factor, Point* pt, bool) {
    if (!win->IsDocLoaded()) {
        return;
    }
    if (factor < 0) {
        // if factor is one of the kZoomFit* constants there's no point to zoom around
        pt = nullptr;
    }
    win->ctrl->SetZoomVirtual(factor, pt);
    ShowZoomNotification(win, factor);
}

static void OnMenuZoom(MainWindow* win, int menuId) {
    if (!win->IsDocLoaded()) {
        return;
    }
    float zoom = ZoomMenuItemToZoom(menuId);
    SmartZoom(win, zoom, nullptr, true);
}

static void ChangeZoomLevel(MainWindow* win, float newZoom, bool pagesContinuously) {
    if (!win->IsDocLoaded()) {
        return;
    }
    float zoom = win->ctrl->GetZoomVirtual();
    DisplayMode mode = win->ctrl->GetDisplayMode();
    DisplayMode newMode = pagesContinuously ? DisplayMode::Continuous : DisplayMode::SinglePage;
    WindowTab* tab = win->CurrentTab();

    if (mode != newMode || zoom != newZoom) {
        float prevZoom = tab->prevZoomVirtual;
        DisplayMode prevMode = tab->prevDisplayMode;
        if (mode != newMode) {
            SwitchToDisplayMode(win, newMode);
        }
        OnMenuZoom(win, CmdIdFromVirtualZoom(newZoom));
        // remember the previous values for when the toolbar button is unchecked
        if (kInvalidZoom == prevZoom) {
            tab->prevZoomVirtual = zoom;
            tab->prevDisplayMode = mode;
        } else {
            tab->prevZoomVirtual = prevZoom;
            tab->prevDisplayMode = prevMode;
        }
    } else if (tab->prevZoomVirtual != kInvalidZoom) {
        float prevZoom = tab->prevZoomVirtual;
        SwitchToDisplayMode(win, tab->prevDisplayMode);
        SmartZoom(win, prevZoom, nullptr, true);
    }
}

// --- command dispatch -------------------------------------------------------

static bool FilePickerIsSumatraPDF() {
    return gSettings && str::EqI(gSettings->filePicker, StrL("sumatrapdf"));
}

static void ToggleFilePicker() {
    Str want = FilePickerIsSumatraPDF() ? StrL("os") : StrL("sumatrapdf");
    str::ReplaceWithCopy(&gSettings->filePicker, want);
    ScheduleSaveSettings();
}

#if OS_WIN
struct OpenDocsData {
    MainWindow* win = nullptr;
    bool skipHistory = false;
};

// orig's OpenFileWithOSFilePicker: IFileOpenDialog with multi-select
static void OpenDocsWithOsPicker(OpenDocsData* d) {
    MainWindow* win = d->win;
    bool skipHistory = d->skipHistory;
    delete d;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    StrVec paths;
    if (NativeOpenDocsDlg(win, &paths) && IsMainWindowValidAndNotClosing(win)) {
        StartLoadDocuments(paths, win, skipHistory);
    }
}
#endif

static void OpenFileCmd(MainWindow* win, bool skipHistory, bool forceOsPicker) {
    // don't allow opening different files in plugin mode
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }
    // ng: without an OS picker File / Open did nothing at all
    if ((!forceOsPicker && FilePickerIsSumatraPDF()) || !AppShellHasOsFilePicker()) {
        ShowNavFilesInFolder(win, {}, skipHistory);
        return;
    }
#if OS_WIN
    if (NativeFileDlgEnabled()) {
        // ng: the dialog runs a message loop, so it is shown once gpui has
        // unwound from the key or the menu click
        auto* d = new OpenDocsData{win, skipHistory};
        uitask::Post(MkFunc0(OpenDocsWithOsPicker, d), "OpenFileWithOSFilePicker");
        return;
    }
#endif
    TempStr path = AppShellPromptForFileTemp(win);
    if (len(path) == 0) {
        return;
    }
    WindowTab* tab = LoadDocument(win, path) ? win->CurrentTab() : nullptr;
    if (tab && skipHistory) {
        tab->skipHistory = true;
    }
}

// --- images stamped onto a page (orig's CmdInsertImage / paste) -------------

static Annotation* CreateImageStampAnnotation(MainWindow* win, WindowTab* tab, DisplayModel* dm, Pixmap* image,
                                              Point pt) {
    if (!win || !tab || !dm || !image) {
        return nullptr;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || !EngineSupportsAnnotations(engine)) {
        return nullptr;
    }
    if (pt.IsEmpty()) {
        pt = win->dragPrevPos;
    }
    int pageNoUnderCursor = dm->GetPageNoByPoint(pt);
    if (pageNoUnderCursor < 0) {
        if (!SetPointToVisiblePage(dm, pt, pageNoUnderCursor)) {
            return nullptr;
        }
    }
    PointF ptOnPage = dm->CvtFromScreen(pt, pageNoUnderCursor);
    AnnotCreateArgs args{AnnotationType::Stamp};
    args.stampImage = image;
    return EngineMupdfCreateAnnotation(engine, pageNoUnderCursor, ptOnPage, &args);
}

// what ExecuteAnnotCreateCmd does after a new annotation: select it, refresh
static void FinishImageStamp(MainWindow* win, WindowTab* tab, Annotation* annot) {
    if (!annot) {
        return;
    }
    DeleteOldSelectionInfo(win, true);
    RefreshAnnotationLists(tab);
    MainWindowRerender(win);
    ToolbarUpdateStateForWindow(win, true);
    if (win->pdfAnnotationsToolbarEnabled) {
        SetSelectedAnnotation(tab, annot);
    }
    logf("CreateImageStampAnnotation: page %d\n", annot->pageNo);
}

static void InsertTextSnippet(MainWindow* win, CustomCommand* cmd, Point pt) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!tab || !dm || !cmd) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || !EngineSupportsAnnotations(engine)) {
        return;
    }
    if (pt.IsEmpty()) {
        pt = win->dragPrevPos;
    }
    int pageNo = dm->GetPageNoByPoint(pt);
    if (pageNo < 0 && !SetPointToVisiblePage(dm, pt, pageNo)) {
        return;
    }
    PointF ptOnPage = dm->CvtFromScreen(pt, pageNo);
    AnnotCreateArgs args{AnnotationType::FreeText};
    SetAnnotCreateArgs(args, cmd);
    args.content = GetCommandStringArg(cmd, kCmdArgText, {});
    SizeF size = FreeTextPlacementPageSize(args);
    args.hasRect = true;
    args.rect = {ptOnPage.x, ptOnPage.y, size.dx, size.dy};
    Annotation* annot = EngineMupdfCreateAnnotation(engine, pageNo, ptOnPage, &args);
    if (!annot) {
        return;
    }
    StopSelectTextWithKeyboard(win);
    DeleteOldSelectionInfo(win, true);
    RefreshAnnotationLists(tab);
    MainWindowRerender(win);
    ToolbarUpdateStateForWindow(win, true);
    if (win->pdfAnnotationsToolbarEnabled) {
        SetSelectedAnnotation(tab, annot);
    }
    logf("InsertTextSnippet: page %d\n", annot->pageNo);
}

static void CreateImageStampFromClipboard(MainWindow* win, Point pt) {
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = win->AsFixed();
    Pixmap* image = ImageEditGetClipboard(win);
    if (!image) {
        ShowWarningNotification(win, Tr("No image in the clipboard"), kNotifDefaultTimeOut);
        return;
    }
    Annotation* annot = CreateImageStampAnnotation(win, tab, dm, image, pt);
    FreePixmap(image);
    FinishImageStamp(win, tab, annot);
}

#if OS_WASM
enum class ClipboardImageUse {
    Open,
    Stamp,
};

struct ClipboardImageRead {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    ClipboardImageUse use = ClipboardImageUse::Open;
    Point pt;
};

static ClipboardImageRead gClipboardImageRead;

static void ClipboardImageReady(void*, gp::App*, gp::Window*, const gp::ClipboardItem& item) {
    ClipboardImageRead op = gClipboardImageRead;
    gClipboardImageRead = {};
    if (!IsMainWindowValidAndNotClosing(op.win) || op.win->CurrentTab() != op.tab) {
        return;
    }
    Pixmap* image = nullptr;
    if (item.HasImage()) {
        image = PixmapFromData(Str((char*)item.imageBytes, item.imageBytesLen));
    }
    if (op.use == ClipboardImageUse::Open) {
        if (!image && op.win->pdfAnnotationsToolbarEnabled && HasCopiedAnnotation()) {
            PasteAnnotationInTab(op.win, op.tab);
            return;
        }
        PasteImageFromClipboard(op.win, image);
        return;
    }
    if (!image) {
        ShowWarningNotification(op.win, Tr("No image in the clipboard"), kNotifDefaultTimeOut);
        return;
    }
    Annotation* annot = CreateImageStampAnnotation(op.win, op.tab, op.win->AsFixed(), image, op.pt);
    FreePixmap(image);
    FinishImageStamp(op.win, op.tab, annot);
}

static bool ReadClipboardImage(MainWindow* win, ClipboardImageUse use, Point pt = {}) {
    if (gClipboardImageRead.win) {
        return true;
    }
    gClipboardImageRead = {win, win->CurrentTab(), use, pt};
    if (gp::ClipboardReadAsync(win->gpuiWin, ClipboardImageReady, nullptr)) {
        return true;
    }
    gClipboardImageRead = {};
    return false;
}
#endif

// stamp an image file on the page as a Fill & Sign-style signature (#1744)
enum class ImageStampSource {
    Prompt,
    Signature
};

static void InsertImageAtPath(MainWindow* win, Point pt, Str path) {
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = win->AsFixed();
    if (!tab || !dm || !CanAccessDisk()) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || !EngineSupportsAnnotations(engine)) {
        return;
    }
    logf("InsertImageFromFile: '%s'\n", path);
    Str data = file::ReadFile(path);
    Pixmap* image = PixmapFromData(data);
    str::Free(data);
    if (!image) {
        ShowWarningNotification(win, fmt(Tr("Couldn't load image '%s'").s, path::GetBaseNameTemp(path)),
                                kNotifDefaultTimeOut);
        return;
    }
    Annotation* annot = CreateImageStampAnnotation(win, tab, dm, image, pt);
    FreePixmap(image);
    FinishImageStamp(win, tab, annot);
}

// ng: for the platforms whose picker answers later (AppShellPickFileAsync)
struct ImagePickCtx {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Point pt;
};

static ImagePickCtx gImagePick;

static void OnImagePicked(ImagePickCtx* ctx, Str path) {
    if (!IsMainWindowValidAndNotClosing(ctx->win) || ctx->win->CurrentTab() != ctx->tab) {
        return;
    }
    InsertImageAtPath(ctx->win, ctx->pt, path);
}

static void InsertImageFromFile(MainWindow* win, Point pt, ImageStampSource source) {
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = win->AsFixed();
    if (!tab || !dm || !CanAccessDisk()) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || !EngineSupportsAnnotations(engine)) {
        return;
    }
    TempStr path;
    Str signaturePath = gSettings->annotations.signatureImage;
    if (source == ImageStampSource::Signature && len(signaturePath) > 0 && file::Exists(signaturePath)) {
        path = str::DupTemp(signaturePath);
    }
    if (len(path) == 0) {
        // orig's PickImageFilePathTemp
        TempStr filter =
            fmt("%s\1*.png;*.jpg;*.jpeg;*.jfif;*.bmp;*.gif;*.tif;*.tiff;*.webp;*.heic;*.heif;*.ico\1%s\1*.*\1",
                Tr("Image files"), Tr("All files"));
        gImagePick = {win, tab, pt};
        if (AppShellPickFileAsync(win, Tr("Image files"), filter, MkFunc1(OnImagePicked, &gImagePick))) {
            return;
        }
        path = AppShellPromptForPathTemp(win, Tr("Image files"), filter);
    }
    if (len(path) == 0) {
        return;
    }
    InsertImageAtPath(win, pt, path);
}

static void TocItemToText(str::Builder& s, TocItem* item, int level) {
    while (item) {
        if (item->title) {
            for (int i = 0; i < level; i++) {
                s.AppendChar('\t');
            }
            s.Append(item->title);
            s.AppendChar('\n');
        }
        if (item->child) {
            int nextLevel = item->title ? level + 1 : level;
            TocItemToText(s, item->child, nextLevel);
        }
        item = item->next;
    }
}

// the file of the current tab when it is a standalone image, else empty
static Str CurrentImageTabPathTemp(MainWindow* win) {
    if (!win) {
        return {};
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return {};
    }
    if (tab->GetEngineType() != kindEngineImage) {
        return {};
    }
    return tab->filePath;
}

// Run the print dialog from the ui task queue rather than from whatever
// dispatched the command. The classic dialog (PrintDlgExW) is modal and pumps
// messages, and on Windows 11 it blocks on the out-of-process unified print
// dialog with CoWaitForMultipleHandles, so it must not nest inside a menu's or
// an accelerator's own loop.
static void PrintCurrentFileDeferred(MainWindow* win) {
    // the window can be closed between posting this and running it
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    PrintCurrentFile(win);
}

static void PrintSelectionDeferred(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    PrintCurrentFile(win, false, true);
}

struct ListPrintersResult {
    MainWindow* win = nullptr;
    Str text; // owned; freed in ListPrintersShowResult
};

static void ListPrintersShowResult(ListPrintersResult* d) {
    MainWindow* win = d->win;
    Str text = d->text;
    d->text = {};
    delete d;

    if (IsMainWindowValidAndNotClosing(win)) {
        RemoveNotificationsForGroup(win, kNotifActionResponse);
        ShowTextInWindow(win, StrL("SumatraPDF - Printers"), text);
    }
    str::Free(text);
}

static void ListPrintersThread(MainWindow** winPtr) {
    str::Builder out;
    GetPrintersInfo(out);
    auto* d = new ListPrintersResult;
    d->win = *winPtr;
    d->text = str::Dup(ToStr(out));
    delete winPtr;
    uitask::Post(MkFunc0<ListPrintersResult>(ListPrintersShowResult, d));
}

// https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes
// first entry is value in gLangCodes, second is ISO 639 lang code
static const char* gLangsMap = "am\0hy\0by\0be\0ca-xv\0ca\0cz\0cs\0kr\0ko\0vn\0vi\0cn\0zh-CN\0tw\0zh-TW\0";
static TempStr GetISO639LangCodeFromLangTemp(Str lang) {
    int idx = SeqStrIndex(gLangsMap, lang);
    if (idx < 0 || idx % 2 != 0) {
        return lang;
    }
    return SeqStrByIndex(gLangsMap, idx + 1);
}

// A URL can only carry so much text, and quietly sending less than the user
// selected looks like the service ignored half the request (discussion #5900).
static void NotifyUrlSelectionTruncated(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    NotificationCreateArgs args;
    args.win = tab->win;
    args.tab = tab;
    args.warning = true;
    args.timeoutMs = kNotif5SecsTimeOut;
    args.msg = Tr("Selection was too long for a URL and was shortened.");
    ShowNotification(args);
}

static void LaunchBrowserWithSelection(WindowTab* tab, Str urlPattern) {
    if (!tab || !HasPermission(Perm::InternetAccess) || !HasPermission(Perm::CopySelection)) {
        return;
    }
    bool isTextOnlySelectionOut; // if false, a rectangular selection
    TempStr selText = GetSelectedTextTemp(tab, StrL("\n"), isTextOnlySelectionOut);
    if (len(selText) == 0) {
        return;
    }
    // The budget is for the whole URL, so subtract the pattern around the
    // selection.
    int budget = kMaxUrlEncodedLen - len(urlPattern);
    bool didTruncate = false;
    TempStr encodedSelection = URLEncodeMayTruncateTemp(selText, budget, &didTruncate);
    if (didTruncate) {
        NotifyUrlSelectionTruncated(tab);
    }
    // ${userLang} and ${selection} are typed by the user in the settings file,
    // so a different case is accepted too
    Str lang = trans::GetCurrentLangCode();
    if (str::Eq(lang, StrL("kr"))) {
        lang = StrL("ko");
    }
    TempStr countryCode = GetISO639LangCodeFromLangTemp(lang);
    TempStr uri = str::ReplaceNoCaseTemp(urlPattern, StrL("${userlang}"), countryCode);
    uri = str::ReplaceNoCaseTemp(uri, StrL("${selectionposition}"), FormatSelectionPositionTemp(tab));
    uri = str::ReplaceNoCaseTemp(uri, StrL("${selection}"), encodedSelection);
    logf("LaunchBrowserWithSelection: '%s'\n", uri);
    gp::OpenUrl(ToGpui(uri));
}

// orig's CmdSelectionHandler case: one SelectionHandlers entry from the
// settings file, sent the way its Method says
static void RunSelectionHandler(WindowTab* tab, CustomCommand* cmd) {
    if (!HasPermission(Perm::CopySelection)) {
        return;
    }
    Str exe = GetCommandStringArg(cmd, kCmdArgExe, {});
    if (exe) {
        // ${selectionfile} lets a helper program read arbitrarily long
        // text without going near a command-line length limit
        bool isTextOnly;
        TempStr sel = GetSelectedTextTemp(tab, StrL("\n"), isTextOnly);
        if (len(sel) == 0) {
            return;
        }
        TempStr cmdLine = ExpandSelectionVarsTemp(exe, sel, false, 0, nullptr, tab);
        logf("SelectionHandler: exec '%s'\n", cmdLine);
        RunWithExe(tab, cmdLine, {});
        return;
    }
    TempStr url = str::DupTemp(GetCommandStringArg(cmd, kCmdArgURL, {}));
    if (len(url) == 0) {
        return;
    }
    // try to auto-fix url
    bool isValidURL = str::Contains(url, StrL("://"));
    if (!isValidURL) {
        url = str::JoinTemp(StrL("https://"), url);
    }
    auto method = ParseSelectionSendMethod(GetCommandStringArg(cmd, kCmdArgMethod, {}));
    if (method == SelectionSendMethod::Get) {
        LaunchBrowserWithSelection(tab, url);
        return;
    }
    if (!HasPermission(Perm::InternetAccess)) {
        return;
    }
    bool isTextOnlySelection;
    TempStr selText = GetSelectedTextTemp(tab, StrL("\n"), isTextOnlySelection);
    if (len(selText) == 0) {
        return;
    }
    Str body = GetCommandStringArg(cmd, kCmdArgBody, {});
    if (method == SelectionSendMethod::PostViaBrowser) {
        SelectionHandlerPostViaBrowser(tab, url, body, selText);
        return;
    }
    Str contentType = GetCommandStringArg(cmd, kCmdArgContentType, {});
    Str headers = GetCommandStringArg(cmd, kCmdArgHeaders, {});
    SelectionHandlerPost(tab, url, body, contentType, headers, selText);
}

// ng: orig asks the canvas HWND for the cursor position; here the canvas
// records it on every mouse move (DocCanvas.cpp's OnMouseMove)
static void InvokeInverseSearch(WindowTab* tab) {
    if (!tab) {
        return;
    }
    if (!gSettings->enableTeXEnhancements) {
        return;
    }
    MainWindow* win = tab->win;
    Point pt = win->dragPrevPos;
    OnInverseSearch(win, pt.x, pt.y);
}

static void ZoomToSelection(MainWindow* win) {
    DisplayModel* dm = win->AsFixed();
    WindowTab* tab = win->CurrentTab();
    if (!dm || !tab || !win->showSelection || !tab->selectionOnPage) {
        return;
    }

    // the selection doesn't move in page coordinates while we zoom, so remember
    // it there and map it back to the screen once the new zoom is applied
    int pageNo = 0;
    RectF selPage;
    Rect selScreen;
    bool isFirst = true;
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        Rect rc = sel.GetRect(dm);
        if (rc.IsEmpty()) {
            continue;
        }
        if (isFirst) {
            pageNo = sel.pageNo;
            selPage = sel.rect;
            selScreen = rc;
            isFirst = false;
            continue;
        }
        selScreen = selScreen.Union(rc);
        if (sel.pageNo == pageNo) {
            selPage = selPage.Union(sel.rect);
        }
    }
    Rect viewPort = dm->GetViewPort();
    if (isFirst || selScreen.dx <= 0 || selScreen.dy <= 0 || viewPort.dx <= 0 || viewPort.dy <= 0) {
        return;
    }

    float fx = (float)viewPort.dx / (float)selScreen.dx;
    float fy = (float)viewPort.dy / (float)selScreen.dy;
    float newZoom = dm->GetZoomVirtual(true) * (fx < fy ? fx : fy);
    newZoom = limitValue(newZoom, kZoomMin, kZoomMax);

    // remember the zoom too, so Back undoes the whole "zoom to selection"
    dm->AddNavPoint(true);
    SmartZoom(win, newZoom, nullptr, false);

    // put the middle of the selection in the middle of the window
    Rect rc = dm->CvtToScreen(pageNo, selPage);
    viewPort = dm->GetViewPort();
    // the selection can already be centered on either axis, in which case
    // there's nothing to scroll (ScrollYBy asserts on a 0 delta)
    int dx = rc.x + (rc.dx / 2) - (viewPort.dx / 2);
    int dy = rc.y + (rc.dy / 2) - (viewPort.dy / 2);
    if (0 != dx) {
        dm->ScrollXBy(dx);
    }
    if (0 != dy) {
        dm->ScrollYBy(dy, false);
    }
}

static TempStr ZoomArgTemp(DocController* ctrl) {
    float zoom = ctrl->GetZoomVirtual();
    if (kZoomFitPage == zoom) {
        return StrL("fit page");
    }
    if (kZoomFitWidth == zoom) {
        return StrL("fit width");
    }
    if (kZoomFitHeight == zoom) {
        return StrL("fit height");
    }
    if (kZoomFitContent == zoom) {
        return StrL("fit content");
    }
    if (kZoomFitVisible == zoom) {
        return StrL("fit visible");
    }
    return fmt("%g%%", ctrl->GetZoomVirtual(true));
}

// Copy the current view as the cmd-line args that re-open it:
//   -page 33 -zoom "193%" -scroll 0,0 "C:\dir\file.pdf"
static void CopyLocationToClipboard(MainWindow* win, WindowTab* tab) {
    if (!tab || !tab->ctrl) {
        return;
    }
    DocController* ctrl = tab->ctrl;
    DisplayModel* dm = tab->AsFixed();

    int pageNo = ctrl->CurrentPageNo();
    // -scroll is relative to the page the scroll state reports, not the current one
    TempStr scrollArg = StrL("");
    if (dm) {
        ScrollState ss = dm->GetScrollState();
        pageNo = ss.page;
        // -1 means "unchanged" on either axis, so both -1 is nothing to say
        if (ss.x != -1 || ss.y != -1) {
            scrollArg = fmt(" -scroll %d,%d", (int)ss.x, (int)ss.y);
        }
    }
    TempStr loc = fmt("-page %d -zoom \"%s\"%s \"%s\"", pageNo, ZoomArgTemp(ctrl), scrollArg, tab->filePath);
    CopyTextToClipboard(win, loc);
}

// looks through the file history and removes entries for files that no
// longer exist on disk. Done synchronously on the main thread for simplicity.
static void RemoveDeletedFilesFromHistory(MainWindow* win) {
    Vec<FileState*>* states = FileHistoryStates();
    if (!win || !states) {
        return;
    }
    int nRemoved = 0;
    // iterate from the end because removing changes indices
    for (int i = len(*states) - 1; i >= 0; i--) {
        FileState* fs = (*states)[i];
        Str path = fs->filePath;
        if (len(path) == 0) {
            continue;
        }
        // Skip only when we can't tell deleted from "drive is away": an
        // unplugged USB or offline share (orig, issue #5970)
        if (!path::IsOnAvailableDrive(path)) {
            continue;
        }
        if (DocumentPathExists(path)) {
            continue;
        }
        // don't remove a file that's currently open in some tab
        if (FindTabByFilePath(path)) {
            continue;
        }
        logf("RemoveDeletedFilesFromHistory: removed '%s'\n", path);
        DeleteThumbnailForFile(path);
        // drops the home page layout cache, which points at fs
        FileHistoryRemove(fs);
        DeleteFileState(fs);
        nRemoved++;
    }
    if (nRemoved > 0) {
        ScheduleSaveSettings();
        MaybeRedrawHomePage();
    }
    TempStr msg = fmt(Tr("Deleted files removed from history: %d").s, nRemoved);
    ShowTemporaryNotification(win, msg, kNotif5SecsTimeOut);
}

// --- fullscreen and presentation mode ---------------------------------------

// `[CmdToggleFullscreen on]` sets the state instead of flipping it
static bool ShouldToggle(CustomCommand* cmd, bool curState) {
    if (!GetCommandArg(cmd, kCmdArgState)) {
        return true; // no explicit state: always toggle
    }
    return GetCommandBoolArg(cmd, kCmdArgState, !curState) != curState;
}

// orig's EnterFullScreen. The window frame is AppShellSetFullScreen's job;
// this is the chrome state around it - no tabs, no sidebar in presentation,
// Fullscreen.Toolbar for the toolbar and Fullscreen.ShowMenubar for the menu.
void EnterFullScreen(MainWindow* win, bool presentation) {
    if (!HasPermission(Perm::FullscreenAccess) || gPluginMode) {
        return;
    }
    if (presentation ? win->InPresentation() : win->isFullScreen) {
        return;
    }
    ReportIf(presentation ? win->isFullScreen : win->InPresentation());

    if (presentation) {
        if (!win->IsDocLoaded()) {
            return;
        }
        win->windowStateBeforePresentation = win->isMaximized ? WIN_STATE_MAXIMIZED : WIN_STATE_NORMAL;
        win->presentation = PM_ENABLED;
        // hack: this tells OnMouseMove() to hide the cursor immediately
        win->dragPrevPos = Point(-2, -3);
        win->presCursorHideLeftMs = 1;
    } else {
        win->isFullScreen = true;
    }

    // TODO: make showFavorites a per-window pref
    bool showFavoritesTmp = gSettings->showFavorites;
    if (presentation && (win->uiState.tocVisible || gSettings->showFavorites)) {
        SetSidebarVisibility(win, false, false);
    }
    win->menuBarVisibleBeforeFS = win->isMenuBarVisible;
    win->isMenuBarVisible = !presentation && gSettings->fullscreen.showMenubar;
    ShowOrHideToolbar(win);

    AppShellSetFullScreen(win, true, false);

    if (presentation) {
        win->ctrl->SetInPresentation(true);
    } else if (DisplayModel* dm = win->AsFixed()) {
        dm->ApplyFullscreenDisplayMode(true);
        UpdateToolbarState(win);
    }
    gSettings->showFavorites = showFavoritesTmp;
    RebuildMenuBar(win);
    if (gSettings->preventSleepInFullscreen) {
        AppShellPreventSleep(true);
    }
    logf("EnterFullScreen: presentation %d\n", (int)presentation);
    win->RedrawAll(true);
}

// orig's ExitFullScreen
void ExitFullScreen(MainWindow* win) {
    if (!win->isFullScreen && !win->InPresentation()) {
        return;
    }
    if (gSettings->preventSleepInFullscreen) {
        AppShellPreventSleep(false);
    }
    bool wasPresentation = win->InPresentation();
    if (wasPresentation) {
        win->presentation = PM_DISABLED;
        if (win->IsDocLoaded()) {
            win->ctrl->SetInPresentation(false);
        }
        // re-enable the auto-hidden cursor
        win->presCursorHideLeftMs = -1;
        AppShellShowCursor(win, true);
        // ensure that no ToC is shown when entering presentation mode the next time
        for (WindowTab* tab : win->Tabs()) {
            tab->showTocPresentation = false;
        }
    } else {
        win->isFullScreen = false;
        if (DisplayModel* dm = win->AsFixed()) {
            dm->ApplyFullscreenDisplayMode(false);
            UpdateToolbarState(win);
        }
    }

    bool tocVisible = win->CurrentTab() && win->CurrentTab()->showToc;
    SetSidebarVisibility(win, tocVisible, gSettings->showFavorites);
    win->isMenuBarVisible = win->menuBarVisibleBeforeFS;
    ShowOrHideToolbar(win);

    bool wasMaximized = wasPresentation && win->windowStateBeforePresentation == WIN_STATE_MAXIMIZED;
    AppShellSetFullScreen(win, false, wasMaximized);
    RebuildMenuBar(win);
    logf("ExitFullScreen: was presentation %d\n", (int)wasPresentation);
    win->RedrawAll(true);
}

// --- orig's FrameOnKeydown / FrameOnChar / FrameOnSysChar: the keys that are
// not accelerators. ng: the modifier state comes with the gpui key event
// instead of GetKeyState()

static bool gIsDivideKeyDown = false;

// Map a Windows virtual key to a portable selection extend (unit, delta).
// Returns false if the key is not a selection-extend key.
static bool TextSelectExtendFromVk(int key, TextSelectUnit& unit, int& delta) {
    unit = TextSelectUnit::Glyph;
    delta = 0;
    switch (key) {
        case VK_LEFT:
            delta = -1;
            return true;
        case VK_RIGHT:
            delta = 1;
            return true;
        case VK_UP:
            unit = TextSelectUnit::Line;
            delta = -1;
            return true;
        case VK_DOWN:
            unit = TextSelectUnit::Line;
            delta = 1;
            return true;
        default:
            return false;
    }
}

// Shift+arrows extend an existing text selection instead of scrolling (#5814).
static bool TryExtendTextSelectionFromKey(MainWindow* win, int key, bool isCtrl, bool isShift, bool isAlt) {
    if (!win || !isShift || isCtrl || isAlt) {
        return false;
    }
    if (!CanExtendTextSelection(win)) {
        return false;
    }
    TextSelectUnit unit;
    int delta = 0;
    if (!TextSelectExtendFromVk(key, unit, delta)) {
        return false;
    }
    ExtendTextSelection(win, unit, delta);
    return true;
}

// make sure that idx falls within <0, max-1> inclusive range
// negative numbers wrap from the end
static int wrapIdx(int idx, int max) {
    for (; idx < 0; idx += max) {
        idx += max;
    }
    return idx % max;
}

void AdvanceFocus(MainWindow* win, bool isShift) {
    // Tab order: Frame -> Chapter -> Page -> ToC -> Favorites -> Frame -> ...

    bool hasToolbar = !win->isFullScreen && !win->presentation && gSettings->showToolbar && win->IsDocLoaded();
    int direction = isShift ? -1 : 1;

    // ng: orig's tabOrder is an array of HWNDs
    enum class Stop {
        Frame,
        Chapter,
        Page,
        SidebarTop,
        SidebarBottom
    };
    constexpr int kMaxWindows = 6;
    Stop tabOrder[kMaxWindows] = {Stop::Frame};
    int nWindows = 1;
    if (hasToolbar && ShowChapterUi(win->ctrl) && ToolbarHasChapterBox(win)) {
        tabOrder[nWindows++] = Stop::Chapter;
    }
    if (hasToolbar && win->toolbar) {
        tabOrder[nWindows++] = Stop::Page;
    }
    // note: the find edit is no longer in the toolbar tab order; it lives in the
    // floating findBar and is reached via Ctrl+F / the search toolbar icon
    // the sidebar panels, top then bottom
    if (SidebarPanelVisible(win, true)) {
        tabOrder[nWindows++] = Stop::SidebarTop;
    }
    if (SidebarPanelVisible(win, false)) {
        tabOrder[nWindows++] = Stop::SidebarBottom;
    }
    ReportIf(nWindows > kMaxWindows);

    auto isFocused = [win](Stop s) {
        switch (s) {
            case Stop::Frame:
                return AppShellIsFrameFocused(win);
            case Stop::Chapter:
                return IsToolbarLocationBoxFocused(win, true);
            case Stop::Page:
                return IsToolbarLocationBoxFocused(win, false);
            case Stop::SidebarTop:
                return SidebarPanelHasFocus(win, true);
            case Stop::SidebarBottom:
                return SidebarPanelHasFocus(win, false);
        }
        return false;
    };

    // find the currently focused element
    int i = 0;
    while (i < nWindows) {
        if (isFocused(tabOrder[i])) {
            break;
        }
        i++;
    }
    // if it's not in the tab order, start at the beginning
    if (i == nWindows) {
        i = wrapIdx(-direction, nWindows);
    }
    // focus the next available element
    i = wrapIdx(i + direction, nWindows);
    switch (tabOrder[i]) {
        case Stop::Frame:
            AppShellFocusFrame(win);
            break;
        case Stop::Chapter:
            ToolbarFocusLocationBox(win, true);
            break;
        case Stop::Page:
            ToolbarFocusLocationBox(win, false);
            break;
        case Stop::SidebarTop:
            SidebarFocusPanel(win, true);
            break;
        case Stop::SidebarBottom:
            SidebarFocusPanel(win, false);
            break;
    }
}

bool FrameOnKeydown(MainWindow* win, int key, bool isCtrl, bool isShift, bool isAlt) {
    if (!win->IsDocLoaded()) {
        return false;
    }
    if (TryExtendTextSelectionFromKey(win, key, isCtrl, isShift, isAlt)) {
        return true;
    }
    if (isAlt) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (VK_MULTIPLY == key && dm) {
        dm->RotateBy(90);
    } else if (VK_DIVIDE == key && dm) {
        dm->RotateBy(-90);
        gIsDivideKeyDown = true;
    } else if (VK_DELETE == key && !isCtrl && !isShift) {
        WindowTab* tab = win->CurrentTab();
        if (!tab || !tab->selectedAnnotation) {
            return false;
        }
        DeleteAnnotationAndUpdateUI(tab, tab->selectedAnnotation);
    } else {
        return false;
    }
    return true;
}

static void OnFrameKeyB(MainWindow* win, bool isShift) {
    auto* ctrl = win->ctrl;
    bool isSinglePage = IsSingle(ctrl->GetDisplayMode());

    DisplayModel* dm = win->AsFixed();
    if (dm && !isSinglePage) {
        bool forward = !isShift;
        int currPage = ctrl->CurrentPageNo();
        bool isVisible = dm->FirstBookPageVisible();
        if (forward) {
            isVisible = dm->LastBookPageVisible();
        }
        if (isVisible) {
            return;
        }

        DisplayMode newMode = DisplayMode::BookView;
        if (IsBookView(ctrl->GetDisplayMode())) {
            newMode = DisplayMode::Facing;
        }
        SwitchToDisplayMode(win, newMode, true);

        if (forward && currPage >= ctrl->CurrentPageNo() && (currPage > 1 || newMode == DisplayMode::BookView)) {
            ctrl->GoToNextPage();
        } else if (!forward && currPage <= ctrl->CurrentPageNo()) {
            win->ctrl->GoToPrevPage();
        }
    } else if (win->presentation) {
        win->ChangePresentationMode(PM_BLACK_SCREEN);
    }
}

// the characters orig's FrameOnChar acts on after the link-hint and caret
// modes had their turn; true when `key` was one of them
bool FrameOnChar(MainWindow* win, u32 key, bool isShift) {
    if (!win->IsDocLoaded()) {
        return false;
    }
    if (key >= 'A' && key <= 'Z') {
        key = key - 'A' + 'a';
    }
    switch (key) {
        // per https://en.wikipedia.org/wiki/Keyboard_layout
        // almost all keyboard layouts allow to press either
        // '+' or '=' unshifted (and one of them is also often
        // close to '-'); the other two alternatives are for
        // the major exception: the two Swiss layouts
        case '+':
        case '=':
        case 0xE0:
        case 0xE4:
            ExecuteCmd(win, CmdZoomIn);
            return true;
        case '-':
            ExecuteCmd(win, CmdZoomOut);
            return true;
        case '/':
            if (!gIsDivideKeyDown) {
                FindFirst(win);
            }
            gIsDivideKeyDown = false;
            return true;
        case 'b':
            OnFrameKeyB(win, isShift);
            return true;
    }
    return false;
}

bool FrameOnSysChar(MainWindow* win, int key) {
    // use Alt+1 to Alt+8 for selecting the first 8 tabs and Alt+9 for the last tab
    if (gSettings->useTabs && ('1' <= key && key <= '9')) {
        int idx = key < '9' ? (int)(key - '1') : win->TabCount() - 1;
        if (idx >= 0 && idx < win->TabCount()) {
            TabsSelect(win, idx);
        }
        return true;
    }
    return false;
}

void ToggleFullScreen(MainWindow* win, bool presentation) {
    bool enterFullScreen = presentation ? !win->InPresentation() : !win->isFullScreen;

    if (win->InPresentation() || win->isFullScreen) {
        ExitFullScreen(win);
    } else {
        RememberDefaultWindowPosition(win);
    }

    if (enterFullScreen && (!presentation || win->IsDocLoaded())) {
        EnterFullScreen(win, presentation);
    }
}

// Enter the requested mode even when the window is in the other fullscreen
// mode, as command-line and DDE requests require.
void SwitchToFullScreen(MainWindow* win, bool presentation) {
    if (presentation ? win->isFullScreen : win->InPresentation()) {
        ExitFullScreen(win);
    }
    EnterFullScreen(win, presentation);
}

// only DisplayModel supports an actual presentation mode
static void TogglePresentationMode(MainWindow* win) {
    ToggleFullScreen(win, win->AsFixed() != nullptr);
}

// orig's FlagsEnterFullscreen: -fullscreen / -presentation on the command line
void EnterFullScreenFromFlags(const Flags& flags, MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    if (!flags.enterPresentation && !flags.enterFullScreen) {
        return;
    }
    SwitchToFullScreen(win, flags.enterPresentation);
}

// orig's LoadOnStartup: -view, -zoom, -page, -named-dest and -scroll win over
// the saved view. They apply to the document the command line opened.
static void ApplyStartupViewFlags(const Flags& flags, MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    if (len(flags.namedDest) > 0) {
        win->linkHandler->GotoNamedDest(flags.namedDest);
    } else if (flags.pageNumber > 0 && win->ctrl->ValidPageNo(flags.pageNumber)) {
        win->ctrl->GoToPage(flags.pageNumber, false);
    }
    if (flags.startView != DisplayMode::Automatic) {
        SwitchToDisplayMode(win, flags.startView);
    }
    if (flags.startZoom != kInvalidZoom) {
        SmartZoom(win, flags.startZoom, nullptr, false);
    }
    if ((flags.startScroll.x != -1 || flags.startScroll.y != -1) && win->AsFixed()) {
        DisplayModel* dm = win->AsFixed();
        ScrollState ss = dm->GetScrollState();
        ss.x = flags.startScroll.x;
        ss.y = flags.startScroll.y;
        dm->SetScrollState(ss);
    }
}

bool gLastCmdFellThrough = false;

// CmdDebugShowNotif: one of each notification the shell can draw, so the
// corner, the stacking and the warning colors can be eyeballed at once.
// ng: orig also shows a VirtCtrl tree in a notification; here a notification
// is a message, so that one is dropped
static void ShowDebugNotifications(MainWindow* win) {
    {
        NotificationCreateArgs args;
        args.win = win;
        args.groupId = kNotifAdHoc;
        args.msg = StrL("This is a second notification\nMy friend.");
        args.timeoutMs = kNotifDefaultTimeOut;
        ShowNotification(args);
    }
    {
        NotificationCreateArgs args;
        args.win = win;
        args.groupId = kNotifAdHoc;
        args.msg = StrL("This is a notification");
        args.warning = true;
        args.timeoutMs = kNotifNoTimeout;
        ShowNotification(args);
    }
    {
        NotificationCreateArgs args;
        args.win = win;
        args.groupId = kNotifAdHoc;
        args.corner = NotifCorner::BottomRight;
        args.msg = StrL("Progress");
        args.timeoutMs = kNotifNoTimeout;
        ShowNotification(args);
    }
}

// CmdDebugCorruptMemory: orig's double-free crash test, which is #if 0 there
// (it is meant to be turned on by hand when testing the crash handler)
static void DebugCorruptMemory() {}

static void ReadAloudAtPoint(WindowTab* tab, Point pt) {
    if (!tab) {
        return;
    }
    if (TtsIsSpeaking()) {
        TtsStop();
    }
    ReadAloudFromCursorInTab(tab, pt);
}

void ExecuteCmd(MainWindow* win, int cmdId) {
    gLastCmdFellThrough = false;
    if (!win || win->isBeingClosed) {
        return;
    }
    // the Read Aloud menus carry ids of their own, above CmdLast
    if (HandleReadAloudMenuCommand(win, cmdId)) {
        return;
    }
    logf("ExecuteCmd: %d (%s)\n", cmdId, GetCommandName(cmdId));
    WindowTab* tab = win->CurrentTab();
    DocController* ctrl = win->ctrl;
    DisplayModel* dm = win->AsFixed();

    // a Shortcuts / menu entry is a clone with its own id, so map it back to
    // the command it stands for before anything dispatches on the id (#6184)
    int invokedCmdId = cmdId;
    CustomCommand* cmd = FindCustomCommand(cmdId);
    if (cmd != nullptr) {
        cmdId = cmd->origId;
    }
    // a favorite in the Favorites menu carries its file path and page as args
    if (cmdId == CmdFavorite) {
        GoToFavoriteByCmd(win, cmd);
        return;
    }
    // a recent file in the File menu carries its path as an argument
    if (cmdId == CmdFileHistory && CanAccessDisk()) {
        Str filePath = GetCommandStringArg(cmd, kCmdArgFilePath, {});
        if (len(filePath) > 0) {
            LoadDocument(win, filePath);
        }
        return;
    }
    // one of the viewers we detect ourselves (Explorer, Acrobat, ...)
    if (!win->IsCurrentTabAbout() && IsOpenWithKnownExternalViewerCmd(cmdId)) {
        ViewWithKnownExternalViewer(tab, cmdId);
        return;
    }
    // an ExternalViewers entry from the settings file
    if (cmdId == CmdViewWithExternalViewer) {
        Str cmdLine = GetCommandStringArg(cmd, kCmdArgCommandLine, {});
        if (len(cmdLine) == 0 || !CanAccessDisk() || !tab || !file::Exists(tab->filePath)) {
            return;
        }
        Str filter = GetCommandStringArg(cmd, kCmdArgFilter, {});
        RunWithExe(tab, cmdLine, filter);
        return;
    }
    // a SelectionHandlers entry from the settings file
    if (cmdId == CmdSelectionHandler) {
        RunSelectionHandler(tab, cmd);
        return;
    }

    switch (cmdId) {
        case CmdOpenFile:
        case CmdOpenFileNoHistory:
        case CmdOpenFileWithOSFilePicker:
            OpenFileCmd(win, cmdId == CmdOpenFileNoHistory, cmdId == CmdOpenFileWithOSFilePicker);
            break;

        case CmdOpenFileWithSumatraFilePicker:
            ShowNavFilesInFolder(win, {}, false);
            break;

        case CmdClose:
            CloseCurrentTab(win, false);
            break;

        case CmdCloseCurrentDocument:
            CloseCurrentTab(win, true);
            break;

        case CmdCloseAllTabs:
            CloseAllTabs(win);
            if (!IsMainWindowValidAndNotClosing(win)) {
                return;
            }
            UpdateWindowTitle(win);
            RebuildMenuBar(win);
            break;

        case CmdCloseOtherTabs:
        case CmdCloseTabsToTheRight:
        case CmdCloseTabsToTheLeft: {
            Vec<WindowTab*> toCloseOther;
            Vec<WindowTab*> toCloseRight;
            Vec<WindowTab*> toCloseLeft;
            CollectTabsToClose(win, tab, toCloseOther, toCloseRight, toCloseLeft);
            Vec<WindowTab*>& toClose = toCloseOther;
            if (cmdId == CmdCloseTabsToTheRight) {
                toClose = toCloseRight;
            }
            if (cmdId == CmdCloseTabsToTheLeft) {
                toClose = toCloseLeft;
            }
            CloseCollectedTabs(win, toClose);
            if (!IsMainWindowValidAndNotClosing(win)) {
                return;
            }
            break;
        }

        case CmdReopenLastClosedFile:
            ReopenLastClosedFile(win);
            break;

        case CmdNextTab:
        case CmdPrevTab:
            TabsOnCtrlTab(win, cmdId == CmdPrevTab);
            break;

        case CmdCommandPalette: {
            Str mode = GetCommandStringArg(cmd, kCmdArgMode, {});
            RunCommandPalette(win, mode, 0);
            break;
        }

        case CmdCommandPaletteTOC:
            // alias for `CmdCommandPalette %`: open the palette in TOC mode
            RunCommandPalette(win, Str(kPalettePrefixTOC), 0);
            break;

        case CmdCommandPaletteFavorites:
            // alias for `CmdCommandPalette $`: open the palette in favorites mode
            RunCommandPalette(win, Str(kPalettePrefixFavorites), 0);
            break;

        case CmdNextTabSmart:
        case CmdPrevTabSmart: {
            if (gSettings->ctrlTabSimple) {
                // simple (pre-3.6) behavior: switch tabs immediately, in tab-strip order
                TabsOnCtrlTab(win, cmdId == CmdPrevTabSmart);
                break;
            }
            TabSwitcherStart(win, cmdId == CmdNextTabSmart ? 1 : -1);
            break;
        }

        case CmdMoveTabRight:
            MoveTab(win, 1);
            break;

        case CmdMoveTabLeft:
            MoveTab(win, -1);
            break;

        case CmdShowInFolder:
            if (tab) {
                ShowFileInFolder(win, tab->filePath);
            }
            break;

        case CmdCopyFilePath:
            CopyFilePath(tab);
            break;

        case CmdSendByEmail:
            SendAsEmailAttachment(tab);
            break;

        case CmdPrint:
            // not PrintCurrentFile(win): see PrintCurrentFileDeferred
            uitask::Post(MkFunc0(PrintCurrentFileDeferred, win), "CmdPrint");
            break;

        case CmdPrintSelection:
            uitask::Post(MkFunc0(PrintSelectionDeferred, win), "CmdPrintSelection");
            break;

        case CmdListPrinters: {
            NotificationCreateArgs nargs;
            nargs.win = win;
            nargs.msg = Tr("Collecting list of printers");
            ShowNotification(nargs);
            auto* data = new MainWindow*(win);
            RunAsync(MkFunc0<MainWindow*>(ListPrintersThread, data), StrL("ListPrinters"));
            break;
        }

        case CmdShowGeneratedHTML:
            ShowGeneratedMarkdownHtml(win);
            break;

        case CmdRenameFile:
            RenameCurrentFile(win);
            break;

        case CmdDeleteFile:
            DeleteCurrentFile(win);
            if (!IsMainWindowValidAndNotClosing(win)) {
                return;
            }
            break;

        case CmdDeleteFileAndOpenNext:
            DeleteCurrentFileAndOpenNext(win);
            if (!IsMainWindowValidAndNotClosing(win)) {
                return;
            }
            break;

        case CmdDeleteCachedFiles:
            DeleteCachedFiles(win);
            break;

#if OS_WIN
        case CmdCreateShortcutToFile:
            CreateLnkShortcut(win);
            break;
#endif

        case CmdSaveAs:
            SaveCurrentFileAs(win);
            break;

        case CmdSaveAnnotations:
            if (tab) {
                SaveAnnotationsToExistingFile(tab);
            }
            break;

        case CmdToggleReadAloud: {
            if (!tab) {
                break;
            }
            if (TtsIsSpeaking()) {
                ReadAloudStopRememberPos();
                ToolbarUpdateStateForWindow(win, true);
            } else if (CanContinueReadAloud(tab)) {
                ReadAloudContinueInTab(tab);
            } else {
                ReadAloudInTab(tab);
            }
            break;
        }

        case CmdPauseReadAloud: {
            ReadAloudStopRememberPos();
            ToolbarUpdateStateForWindow(win, true);
            break;
        }

        case CmdContinueReadAloud: {
            if (!TtsIsSpeaking()) {
                ReadAloudContinueInTab(tab);
            }
            break;
        }

        case CmdStopReadAloud:
            ReadAloudPlaybackStop();
            break;

        case CmdReadAloudFromTopPage: {
            if (!tab) {
                break;
            }
            if (TtsIsSpeaking()) {
                TtsStop();
            }
            ReadAloudFromViewportTopInTab(tab);
            break;
        }

        case CmdReadAloudSelection: {
            if (!tab) {
                break;
            }
            if (TtsIsSpeaking()) {
                TtsStop();
            }
            ReadAloudSelectionInTab(tab);
            break;
        }

        case CmdReadAloudFromCursorPosition: {
            ReadAloudAtPoint(tab, win->dragPrevPos);
            break;
        }

        case CmdSaveAnnotationsNewFile:
            if (tab) {
                SaveAnnotationsToMaybeNewPdfFile(tab, {});
            }
            break;

        case CmdDiscardChanges:
            // revert to the on-disk version, discarding unsaved changes
            if (tab && win->IsDocLoaded()) {
                if (tab->selectedAnnotation) {
                    SetSelectedAnnotation(tab, nullptr);
                }
                CloseAnnotationUiForTab(tab);
                ReloadDocument(win, false);
            }
            break;

        case CmdToggleEditPDF:
            TogglePdfAnnotationsToolbar(win);
            break;

        case CmdFindAnnotation:
            EnablePdfAnnotationsToolbar(win);
            ToggleFloatingAnnotList(win);
            break;

        case CmdShowAnnotationText:
            if (tab && win->annotationUnderCursor) {
                ShowAnnotationTextPopup(win, win->annotationUnderCursor);
            } else if (tab && tab->selectedAnnotation) {
                StartSelectedAnnotContentsEdit(win);
            }
            break;

        case CmdCopyAnnotation:
            if (tab && tab->selectedAnnotation && HasPermission(Perm::CopySelection)) {
                CopyAnnotation(tab->selectedAnnotation);
            }
            break;

        case CmdCutAnnotation:
            // The original stays until paste. CutAnnotation only marks it.
            if (tab && tab->selectedAnnotation && HasPermission(Perm::CopySelection)) {
                if (CutAnnotation(tab->selectedAnnotation)) {
                    ShowTemporaryNotification(tab->win, Tr("Annotation cut. Paste to move it."));
                }
            }
            break;

        case CmdPasteAnnotation:
            PasteAnnotationInTab(win, tab);
            break;

        case CmdPasteClipboardImage: {
            // ng: orig's Ctrl+V chain starts by forwarding WM_PASTE to the
            // focused win32 edit; gpui's text fields consume Ctrl+V
            // themselves, so it never gets here. orig then prefers a copied
            // annotation while ours is the most recent copy
            // (GetClipboardSequenceNumber); there is no such counter here, so
            // an image on the clipboard wins
            Pixmap* image = ImageEditGetClipboard(win);
#if OS_WASM
            if (!image && ReadClipboardImage(win, ClipboardImageUse::Open)) {
                break;
            }
#endif
            if (!image && win->pdfAnnotationsToolbarEnabled && HasCopiedAnnotation()) {
                PasteAnnotationInTab(win, tab);
                break;
            }
            PasteImageFromClipboard(win, image);
            break;
        }

        case CmdApplyRedactions:
            ApplyRedactionsInTab(tab);
            break;

        case CmdDeleteAnnotation:
            // A dialog owns the keyboard. Delete removes the selection, or
            // the annotation under the cursor when nothing is selected.
            if (DialogsAccelTable(win) != DialogAccels::All) {
                break;
            }
            if (tab && tab->selectedAnnotation) {
                DeleteAnnotationAndUpdateUI(tab, tab->selectedAnnotation);
            } else if (tab && win->annotationUnderCursor) {
                DeleteAnnotationAndUpdateUI(tab, win->annotationUnderCursor);
            }
            break;

        case CmdCreateAnnotHighlight:
        case CmdCreateAnnotSquiggly:
        case CmdCreateAnnotStrikeOut:
        case CmdCreateAnnotUnderline:
        case CmdCreateAnnotText:
        case CmdCreateAnnotFreeText:
        case CmdCreateAnnotStamp:
        case CmdCreateAnnotCaret:
        case CmdCreateAnnotSquare:
        case CmdCreateAnnotLine:
        case CmdCreateAnnotCircle:
        case CmdCreateAnnotPolygon:
        case CmdCreateAnnotPolyLine:
        case CmdCreateAnnotInk:
        case CmdCreateAnnotRedact:
        case CmdCreateAnnotFileAttachment:
        case CmdAnnotationHighlightBrush:
            // the id the user invoked, not the one it maps to: a custom command
            // carries the color / openedit arguments
            ExecuteAnnotCreateCmd(win, invokedCmdId, false, Point{});
            break;

        case CmdInsertTextSnippet:
            InsertTextSnippet(win, cmd, Point{});
            break;

        case CmdUndo:
        case CmdRedo:
            UndoRedoInTab(tab, cmdId == CmdRedo);
            break;

        case CmdDuplicateInNewTab:
            // the new tab is the same document at the same page. Loading the
            // path alone opens it at page 1 (#6168).
            if (tab && !tab->IsAboutTab() && win->IsDocLoaded() && len(tab->filePath) > 0) {
                TabState* state = NewTabStateFromTab(tab);
                TempStr path = str::DupTemp(tab->filePath);
                if (LoadDocument(win, path) && state) {
                    SetTabState(win->CurrentTab(), state);
                }
                if (state) {
                    DeleteTabState(state);
                }
            }
            break;

        case CmdNewWindow:
            CreateAndShowMainWindow(nullptr);
            return;

        case CmdDuplicateInNewWindow:
            DuplicateTabInNewWindow(tab);
            return;

        case CmdReloadDocument:
            ReloadDocument(win, false);
            break;

#if IS_DEBUG
        case CmdDebugStartStressTest: {
            // TODO: ideally would ask user for the cmd-line args but this will do
            Flags f;
            f.stressTestPath = str::Dup(GetPermArena(), StrL("docs/test"));
            f.stressRandomizeFiles = true;
            f.stressTestMax = 25;
            StartStressTest(&f, win);
            return;
        }
#endif

        case CmdOpenNextFileInFolder:
            OpenNextPrevFileInFolder(win, true);
            return;

        case CmdOpenPrevFileInFolder:
            OpenNextPrevFileInFolder(win, false);
            return;

        case CmdNavigateFilesInFolder:
            ShowNavFilesInFolder(win, {}, false);
            return;

        case CmdToggleFilePicker:
            ToggleFilePicker();
            break;

        case CmdTabGroupSave:
            ShowSaveTabGroupDialog(win);
            break;

        case CmdTabGroupRestore:
            ShowOpenTabGroupDialog(win);
            break;

        case CmdFavoriteShowInTab:
            ToggleFavoritesTab(win);
            break;

        case CmdToggleMenuBar:
            ToggleMenuBar(win, false);
            break;

        case CmdTogglePresentationMode:
            if (ShouldToggle(cmd, win->InPresentation())) {
                TogglePresentationMode(win);
            }
            break;

        case CmdToggleFullscreen:
            if (ShouldToggle(cmd, win->isFullScreen)) {
                ToggleFullScreen(win);
            }
            break;

        case CmdPresentationBlackBackground:
            if (win->InPresentation()) {
                // toggle: pressing the key again restores the slide (so a
                // presenter remote bound to '.' can black out and back)
                bool isBlack = win->presentation == PM_BLACK_SCREEN;
                win->ChangePresentationMode(isBlack ? PM_ENABLED : PM_BLACK_SCREEN);
            }
            break;

        case CmdPresentationWhiteBackground:
            if (win->InPresentation()) {
                bool isWhite = win->presentation == PM_WHITE_SCREEN;
                win->ChangePresentationMode(isWhite ? PM_ENABLED : PM_WHITE_SCREEN);
            }
            break;

        case CmdToggleLaserPointer:
            // the cursor itself is the feedback, so no notification
            ToggleLaserPointer(win);
            break;

        case CmdStartAutoScroll:
            // start middle-click-style auto-scroll without needing a middle button
            StartAutoScrollAtCursor(win);
            break;

        case CmdToggleAutomaticallyScroll:
            ReadingAutoScrollToggle(win);
            break;

        case CmdAutomaticallyScrollFaster:
            ReadingAutoScrollFaster(win);
            break;

        case CmdAutomaticallyScrollSlower:
            ReadingAutoScrollSlower(win);
            break;

        case CmdToggleReadingBar:
            ReadingBarToggle(win);
            break;

        case CmdToggleReadingBarInvert:
            ReadingBarToggleInvert(win);
            break;

        case CmdToggleToolbar:
            if (GetCommandArg(cmd, kCmdArgState)) {
                // explicit state: on -> show (pinned), off -> hide
                bool on = GetCommandBoolArg(cmd, kCmdArgState, true);
                int mode = on ? kToolbarShow : kToolbarHide;
                if (win->isFullScreen) {
                    SetFullscreenToolbarMode(mode);
                } else {
                    SetToolbarMode(mode);
                }
                for (MainWindow* w : gWindows) {
                    ShowOrHideToolbar(w);
                }
                ScheduleSaveSettings();
            } else {
                OnMenuViewShowHideToolbar(win);
            }
            break;

        case CmdToggleToolbarShowReadAloud: {
            bool show = !gSettings->toolbarShowReadAloud;
            if (GetCommandArg(cmd, kCmdArgState)) {
                show = GetCommandBoolArg(cmd, kCmdArgState, true);
            }
            gSettings->toolbarShowReadAloud = show;
            for (MainWindow* w : gWindows) {
                ToolbarUpdateStateForWindow(w, true);
            }
            ScheduleSaveSettings();
            break;
        }

        case CmdGoToNextPage:
        case CmdGoToPrevPage: {
            if (!win->IsDocLoaded()) {
                break;
            }
            if (cmdId == CmdGoToPrevPage) {
                ctrl->GoToPrevPage();
            } else {
                ctrl->GoToNextPage();
            }
            OnDocumentVerticalScrollIntent(win, cmdId == CmdGoToNextPage);
            break;
        }

        case CmdGoToFirstPage:
            if (win->IsDocLoaded()) {
                ctrl->GoToFirstPage();
                OnDocumentVerticalScrollIntent(win, false);
            }
            break;

        case CmdGoToLastPage:
            if (win->IsDocLoaded()) {
                if (!ctrl->GoToLastPage()) {
                    CanvasOnVScroll(win, ScrollMsg::Bottom);
                }
                OnDocumentVerticalScrollIntent(win, true);
            }
            break;

        case CmdGoToPage:
            // orig's OnMenuGoToPage: with the toolbar up the page box takes the
            // focus, otherwise the dialog opens
            if (!ToolbarFocusPageBox(win)) {
                ShowGoToPageDialog(win);
            }
            break;

        case CmdScrollUp:
        case CmdScrollDown: {
            if (!win->IsDocLoaded()) {
                break;
            }
            if (dm && dm->NeedVScroll() && dm->GetZoomVirtual() != kZoomFitContent) {
                CanvasOnVScroll(win, cmdId == CmdScrollUp ? ScrollMsg::LineUp : ScrollMsg::LineDown);
            } else if (cmdId == CmdScrollUp) {
                // in single page view or fit content, scrolls by page
                ctrl->GoToPrevPage(true);
                OnDocumentVerticalScrollIntent(win, false);
            } else {
                ctrl->GoToNextPage();
                OnDocumentVerticalScrollIntent(win, true);
            }
            break;
        }

        case CmdScrollUpHalfPage:
        case CmdScrollDownHalfPage: {
            if (!win->IsDocLoaded()) {
                break;
            }
            bool isCont = IsContinuous(ctrl->GetDisplayMode());
            bool up = cmdId == CmdScrollUpHalfPage;
            int currentPos = CanvasScrollPosV(win);
            CanvasOnVScroll(win, up ? ScrollMsg::HalfPageUp : ScrollMsg::HalfPageDown);
            if (isCont && CanvasScrollPosV(win) == currentPos) {
                if (up) {
                    ctrl->GoToPrevPage(true);
                } else {
                    ctrl->GoToNextPage();
                }
                OnDocumentVerticalScrollIntent(win, !up);
            }
            break;
        }

        case CmdScrollUpPage:
        case CmdScrollDownPage: {
            if (!win->IsDocLoaded()) {
                break;
            }
            bool up = cmdId == CmdScrollUpPage;
            int currentPos = CanvasScrollPosV(win);
            if (ctrl->GetZoomVirtual() != kZoomFitContent) {
                CanvasOnVScroll(win, up ? ScrollMsg::PageUp : ScrollMsg::PageDown);
            }
            if (CanvasScrollPosV(win) == currentPos) {
                if (up) {
                    ctrl->GoToPrevPage(true);
                } else {
                    ctrl->GoToNextPage();
                }
                OnDocumentVerticalScrollIntent(win, !up);
            }
            break;
        }

        case CmdScrollLeft:
        case CmdScrollRight: {
            if (!win->IsDocLoaded()) {
                break;
            }
            bool right = cmdId == CmdScrollRight;
            if (dm && dm->NeedHScroll()) {
                CanvasOnHScroll(win, right ? ScrollMsg::LineRight : ScrollMsg::LineLeft);
            } else if (dm) {
                // manga (R2L): Left advances, Right goes back (issue #3964)
                bool goNext = right != dm->GetDisplayR2L();
                dm->GoToPageHorizontal(right);
                OnDocumentVerticalScrollIntent(win, goNext);
            } else if (right) {
                ctrl->GoToNextPage();
                OnDocumentVerticalScrollIntent(win, true);
            } else {
                ctrl->GoToPrevPage();
                OnDocumentVerticalScrollIntent(win, false);
            }
            break;
        }

        case CmdScrollLeftPage:
            CanvasOnHScroll(win, ScrollMsg::PageLeft);
            break;

        case CmdScrollRightPage:
            CanvasOnHScroll(win, ScrollMsg::PageRight);
            break;

        case CmdZoomFitWidthAndContinuous:
            ChangeZoomLevel(win, kZoomFitWidth, true);
            break;

        case CmdZoomFitPageAndSinglePage:
            ChangeZoomLevel(win, kZoomFitPage, false);
            break;

        case CmdZoomOut:
        case CmdZoomIn: {
            if (!win->IsDocLoaded()) {
                break;
            }
            float towards = (cmdId == CmdZoomIn) ? kZoomMax : kZoomMin;
            float zoom = ctrl->GetNextZoomStep(towards);
            SmartZoom(win, zoom, nullptr, true);
            break;
        }

        case CmdZoom6400:
        case CmdZoom3200:
        case CmdZoom1600:
        case CmdZoom800:
        case CmdZoom400:
        case CmdZoom200:
        case CmdZoom150:
        case CmdZoom125:
        case CmdZoom100:
        case CmdZoom50:
        case CmdZoom25:
        case CmdZoom12_5:
        case CmdZoom8_33:
        case CmdZoomFitPage:
        case CmdZoomFitWidth:
        case CmdZoomFitHeight:
        case CmdZoomFitByOrientation:
        case CmdZoomFitContent:
        case CmdZoomFitVisible:
        case CmdZoomShrinkToFit:
        case CmdZoomActualSize:
            OnMenuZoom(win, cmdId);
            break;

        case CmdSinglePageView:
            SwitchToDisplayMode(win, DisplayMode::SinglePage, true);
            ShowViewModeNotification(win, cmdId);
            break;

        case CmdFacingView:
            SwitchToDisplayMode(win, DisplayMode::Facing, true);
            ShowViewModeNotification(win, cmdId);
            break;

        case CmdBookView:
            SwitchToDisplayMode(win, DisplayMode::BookView, true);
            ShowViewModeNotification(win, cmdId);
            break;

        case CmdToggleContinuousView:
            ToggleContinuousView(win);
            break;

        case CmdToggleMangaMode:
            ToggleMangaMode(win);
            break;

        case CmdRotateLeft:
            if (dm) {
                dm->RotateBy(-90);
            }
            break;

        case CmdRotateRight:
            if (dm) {
                dm->RotateBy(90);
            }
            break;

        case CmdToggleUniformPageWidth:
            if (dm) {
                ScrollState state = dm->GetScrollState();
                dm->SetUniformPageWidth(!dm->GetUniformPageWidth());
                dm->Relayout(dm->GetZoomVirtual(), dm->GetRotation());
                dm->SetScrollState(state);
            }
            break;

        case CmdToggleTrimEmptyMargins:
            if (dm) {
                ScrollState state = dm->GetScrollState();
                dm->SetTrimEmptyMargins(!dm->GetTrimEmptyMargins());
                dm->SetScrollState(state);
            }
            break;

        case CmdToggleFreePan:
            if (dm) {
                ScrollState state = dm->GetScrollState();
                dm->SetFreePan(!dm->GetFreePan());
                dm->SetScrollState(state);
            }
            break;

        case CmdToggleZoom:
            win->ToggleZoom();
            break;

        case CmdZoomToSelection:
            ZoomToSelection(win);
            break;

        case CmdNavigateBack:
            if (ctrl) {
                ctrl->Navigate(-1);
            }
            break;

        case CmdNavigateForward:
            if (ctrl) {
                ctrl->Navigate(1);
            }
            break;

        case CmdNavigateThumbnail:
            RunCommandPalette(win, Str(kPalettePrefixThumbnails), 0);
            break;

        case CmdToggleThumbnails:
            SidebarToggleThumbnails(win);
            break;

        // swaps the page colors for this session, whatever they are and
        // whatever the theme is (orig's comment: issue #5887)
        case CmdInvertColors:
            SetInvertPageColors(!GetInvertPageColors());
            UpdateDocumentColors();
            break;

        case CmdToggleGrayscale:
            gSettings->fixedPageUI.grayscale = !gSettings->fixedPageUI.grayscale;
            UpdateDocumentColors();
            ScheduleSaveSettings();
            break;

        case CmdTogglePreservePdfImages:
            SetPreservePdfImagesInDarkMode(!GetPreservePdfImagesInDarkMode());
            UpdateDocumentColors();
            break;

        case CmdToggleLightDarkTheme:
            ToggleLightDarkTheme();
            ScheduleSaveSettings();
            break;

        case CmdToggleEngineeringDrawingEnhance:
            if (dm) {
                EngineMupdfToggleCadEnhance(dm->GetEngine());
                MainWindowRerender(win);
            }
            break;

        case CmdToggleShowAnnotations:
        case CmdShowAnnotations:
        case CmdHideAnnotations: {
            if (!tab) {
                break;
            }
            bool hide = !tab->hideAnnotations;
            if (cmdId == CmdShowAnnotations) {
                hide = false;
            } else if (cmdId == CmdHideAnnotations) {
                hide = true;
            }
            if (hide == tab->hideAnnotations) {
                break;
            }
            tab->hideAnnotations = hide;
            EngineBase* engine = tab->GetEngine();
            if (engine) {
                engine->hideAnnotations = hide;
            }
            MainWindowRerender(win);
            break;
        }

        case CmdToggleLinks:
            gSettings->showLinks = !gSettings->showLinks;
            for (MainWindow* w : gWindows) {
                w->RedrawAll(true);
            }
            break;

        case CmdToggleImages:
            ToggleShowImageOutlines();
            for (MainWindow* w : gWindows) {
                w->RedrawAll(true);
            }
            break;

        case CmdToggleTransparencyGrid:
            ToggleTransparencyGrid();
            for (MainWindow* w : gWindows) {
                w->RedrawAll(true);
            }
            break;

        case CmdDebugShowFitContentArea:
            ToggleShowFitContentArea();
            for (MainWindow* w : gWindows) {
                w->RedrawAll(true);
            }
            break;

        case CmdTogglePageBoxes:
            win->showPageBoxes = !win->showPageBoxes;
            win->RedrawAll(true);
            break;

        case CmdTogglePageInfo:
            TogglePageInfoHelper(win);
            break;

        case CmdToggleCursorPosition:
            ToggleCursorPositionInDoc(win);
            break;

        case CmdToggleHighlightFormFields:
            gSettings->highlightFormFields = !gSettings->highlightFormFields;
            for (MainWindow* w : gWindows) {
                w->RedrawAll(true);
            }
            break;

        case CmdToggleDisableLinks:
            gSettings->disableLinks = !gSettings->disableLinks;
            ScheduleSaveSettings();
            if (gSettings->disableLinks) {
                for (MainWindow* w : gWindows) {
                    RefHoverHide(w->refHover);
                    StopKeyboardLinkFollowing(w);
                }
            }
            break;

        case CmdToggleHoverPreview:
            if (gSettings->citationHoverDelay < 0) {
                gSettings->citationHoverDelay = kDefaultCitationHoverDelay;
            } else {
                gSettings->citationHoverDelay = -1;
                for (MainWindow* w : gWindows) {
                    RefHoverHide(w->refHover);
                }
            }
            ScheduleSaveSettings();
            break;

        case CmdCopyLocationToClipboard:
            CopyLocationToClipboard(win, tab);
            break;

        // ng: orig's CopySelectionInTabToClipboard, minus the focused edit box.
        // A text selection wins. Otherwise Ctrl+C copies the selected annotation.
        case CmdCopySelection: {
            if (!tab || !HasPermission(Perm::CopySelection)) {
                break;
            }
            if (tab->AsChm()) {
                tab->AsChm()->CopySelection();
            } else if (tab->AsMarkdown()) {
                tab->AsMarkdown()->CopySelection();
                break;
            }
            if (tab->selectionOnPage) {
                CopySelectionToClipboard(win);
                break;
            }
            if (win->pdfAnnotationsToolbarEnabled && tab->selectedAnnotation) {
                CopyAnnotation(tab->selectedAnnotation);
            }
            break;
        }

        case CmdSelectAll:
            OnSelectAll(win);
            break;

        case CmdSelectCurrentPage:
            OnSelectCurrentPage(win);
            break;

        case CmdToggleKeyboardLinkFollowing:
            ToggleKeyboardLinkFollowing(win);
            break;

        case CmdFindFirst:
            // orig: Ctrl+F on the start page filters the file list instead
            if (win->IsCurrentTabAbout()) {
                HomePageFocusSearch(win);
            } else {
                FindFirst(win);
            }
            break;

        case CmdFindNext:
            FindNext(win);
            break;

        case CmdFindPrev:
            FindPrev(win);
            break;

        case CmdFindToggleMatchCase:
            FindToggleMatchCase(win);
            break;

        case CmdFindToggleMatchWholeWord:
            FindToggleMatchWholeWord(win);
            break;

        case CmdFindNextSel:
            FindSelection(win, TextSearch::Direction::Forward);
            break;

        case CmdFindPrevSel:
            FindSelection(win, TextSearch::Direction::Backward);
            break;

        case CmdSelectTextViaKeyboard:
            ToggleSelectTextWithKeyboard(win);
            break;

        // no default shortcut: Ctrl+Shift+Left / Right and friends are taken, so
        // these exist for the user to bind in the Shortcuts settings (#5922)
        case CmdExtendSelectionCharLeft:
            ExtendTextSelection(win, TextSelectUnit::Glyph, -1);
            break;

        case CmdExtendSelectionCharRight:
            ExtendTextSelection(win, TextSelectUnit::Glyph, 1);
            break;

        case CmdExtendSelectionWordLeft:
            ExtendTextSelection(win, TextSelectUnit::Word, -1);
            break;

        case CmdExtendSelectionWordRight:
            ExtendTextSelection(win, TextSelectUnit::Word, 1);
            break;

        case CmdToggleBookmarks:
        case CmdToggleTableOfContents:
            ToggleTocBox(win);
            break;

        case CmdFavoriteToggle:
            ToggleFavorites(win);
            break;

        case CmdGoToHomePage:
            GoToHomeTab(win);
            break;

        case CmdFavoriteAdd:
            AddFavoriteForCurrentPage(win);
            break;

        case CmdFavoriteDel:
            if (ctrl) {
                DelFavorite(ctrl->GetFilePath(), win->currPageNo, ctrl);
            }
            break;

        case CmdToggleFavoritesSort:
            ToggleSortFavoritesByName();
            break;

        case CmdGoToNextFavorite:
            GoToNextFavorite(win, true);
            break;

        case CmdGoToPrevFavorite:
            GoToNextFavorite(win, false);
            break;

        case CmdExpandAll:
            TocExpandAll(win);
            break;

        case CmdCollapseAll:
            TocCollapseAll(win);
            break;

        case CmdTocExpandToLevel1:
            TocExpandToLevel(win, 1);
            break;

        case CmdTocExpandToLevel2:
            TocExpandToLevel(win, 2);
            break;

        case CmdTocExpandToLevel3:
            TocExpandToLevel(win, 3);
            break;

        case CmdTocCollapseSameLevel:
            TocCollapseSameLevel(win, SidebarTocSelection(win));
            break;

        case CmdExpandToCurrentPage:
            ExpandTocToCurrentPage(win);
            break;

        case CmdAutoGenerateTOC: {
            DisplayModel* fixed = win->AsFixed();
            if (!fixed) {
                break;
            }
            win->CurrentTab()->showToc = true;
            fixed->StartHeadingToc(HeadingTocStart::Always);
            if (!EngineMupdfHeadingTocPending(fixed->GetEngine())) {
                SetSidebarVisibility(win, true, gSettings->showFavorites);
            }
            break;
        }

        case CmdZoomCustom: {
            // a zoom-strip level carries the percent; the menu command does not
            if (cmd && cmd->firstArg) {
                SmartZoom(win, cmd->firstArg->floatVal, nullptr, true);
            } else {
                ShowCustomZoomDialog(win);
            }
            break;
        }

        case CmdChangeScrollbar:
            ShowChangeScrollbarDialog(win);
            break;

        case CmdChangeLanguage:
            ShowChangeLanguageDialog(win);
            break;

        case CmdChangeTheme:
            ShowChangeThemeDialog(win);
            break;

        case CmdSetDocumentColorsFollowTheme:
            ShowSetDocumentColorsFollowThemeDialog(win);
            break;

        case CmdChangeBackgroundColor:
            ShowChangeBackgroundColorDialog(win);
            break;

        case CmdSetTabColor:
            ShowSetTabColorDialog(win, tab);
            break;

        case CmdSetInverseSearch:
            ShowInverseSearchDialog(win);
            break;

        case CmdInvokeInverseSearch:
            InvokeInverseSearch(tab);
            break;

        case CmdToggleInverseSearch:
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/5289
            // allow to temporarily disable invoking tex inverse search
            // with left mouse click
            extern bool gDisableInteractiveInverseSearch;
            gDisableInteractiveInverseSearch = !gDisableInteractiveInverseSearch;
            break;

        case CmdChangeEbookSettings:
            ShowEbookSettingsDialog(win);
            break;

        case CmdOptions:
            ShowSettingsDialog(win);
            break;

        case CmdAdvancedSettings:
            ShowAdvancedSettingsDialog(win);
            break;

        case CmdOpenSettingsFile:
            OpenSettingsFile();
            break;

        case CmdTogglePageGrid:
            TogglePageGrid();
            RedrawPageGridWindows();
            break;

        case CmdConfigurePageGrid:
            ShowPageGridDialog(win);
            break;

        case CmdToggleKeyboardHelp:
            ToggleKeyboardHelp(win);
            break;

        case CmdHelpOpenManual:
            ToggleDocumentationWindow();
            break;

        case CmdHelpOpenManualOnWebsite:
            SumatraLaunchBrowser(Str(kManualURL));
            break;

        case CmdHelpOpenKeyboardShortcuts:
            LaunchDocumentation(StrL("Keyboard-shortcuts"));
            break;

        case CmdHelpVisitWebsite:
            SumatraLaunchBrowser(Str(kWebsiteURL));
            break;

        case CmdContributeTranslation:
            SumatraLaunchBrowser(Str(kContributeTranslationsURL));
            break;

        case CmdHelpAbout:
            ShowAboutWindow(win);
            break;

        // e.g. [CmdToggleBoolSetting Fullscreen.ShowMenubar] in Shortcuts
        case CmdToggleBoolSetting: {
            Str settingName = GetCommandStringArg(cmd, kCmdArgName, {});
            if (len(settingName) == 0) {
                MaybeDelayedWarningNotification(StrL(
                    "CmdToggleBoolSetting requires a setting name, e.g. CmdToggleBoolSetting Fullscreen.ShowMenubar"));
                break;
            }
            bool* p = FindSettingsBoolSetting(settingName);
            if (!p) {
                MaybeDelayedWarningNotification(fmt("CmdToggleBoolSetting: unknown boolean setting '%s'", settingName));
                break;
            }
            ToggleSettingsBool(p);
            break;
        }

        case CmdSetTheme: {
            Str theme = GetCommandStringArg(cmd, kCmdArgTheme, {});
            if (len(theme) > 0) {
                SetTheme(theme);
                ScheduleSaveSettings();
            }
            break;
        }

        case CmdExec: {
            Str filter = GetCommandStringArg(cmd, kCmdArgFilter, {});
            Str cmdLine = GetCommandStringArg(cmd, kCmdArgExe, {});
            if (len(cmdLine) > 0) {
                RunWithExe(tab, cmdLine, filter);
            }
            break;
        }

        case CmdRemoveDeletedFilesFromHistory:
            RemoveDeletedFilesFromHistory(win);
            break;

        // ng: orig clears on a worker and reports progress; the history is a
        // vector in gSettings, so clearing it is fast enough to do here
        case CmdClearHistory: {
            Vec<FileState*>* states = FileHistoryStates();
            int nFiles = states ? len(*states) : 0;
            FileHistoryClear(false);
            EmptyThumbnailCacheDirectory();
            ScheduleSaveSettings();
            MaybeRedrawHomePage();
            TempStr msg = fmt(Tr("Cleared history of %d files, deleted thumbnails.").s, nFiles);
            ShowTemporaryNotification(win, msg, kNotif5SecsTimeOut);
            break;
        }

        case CmdShowLog: {
            TempStr path = str::DupTemp(gLogFilePath);
            if (len(path) == 0) {
                path = GetTempFilePathTemp(StrL("slog"));
            }
            WriteCurrentLogToFile(path);
            LaunchFileIfExists(path);
            break;
        }

#if OS_WIN
        // open the OS "Open with" / Default apps UI for this extension
        case CmdFixDefaultApp: {
            Str ext = GetCommandStringArg(cmd, kCmdArgExt, {});
            if (len(ext) > 0) {
                LaunchDefaultAppDialogForExtension(ext);
            }
            break;
        }
#endif

        case CmdDebugTogglePredictiveRender:
            // no notification: the command palette shows the state it will
            // switch to, so announcing the same thing again is redundant
            gPredictiveRender = !gPredictiveRender;
            break;

        case CmdDebugToggleRenderInfo:
            ToggleRenderInfoWindow();
            break;

        case CmdDebugToggleCacheInfo:
            ToggleCacheInfoWindow();
            break;

        // ng: orig flips WS_EX_LAYOUTRTL on every window, which gpui has no
        // equivalent for. What asks IsUIRtl() (text runs, the toolbar and the
        // sidebar order) flips; the rest of the layout does not
        case CmdDebugToggleRtl:
            gForceRtl = !gForceRtl;
            for (MainWindow* w : gWindows) {
                RebuildMenuBar(w);
                w->RedrawAll(true);
            }
            break;

        case CmdDebugShowNotif:
            ShowDebugNotifications(win);
            break;

        case CmdDebugCorruptMemory:
            DebugCorruptMemory();
            break;

        case CmdDebugCrashMe:
            CrashMe();
            break;

        case CmdCheckUpdate:
            StartAsyncUpdateCheck(win, UpdateCheck::UserInitiated);
            break;

        case CmdInstallPrereleaseUpdate:
            DownloadAndInstallPendingUpdate(win);
            break;

        case CmdScreenshot:
#if OS_WIN
            ShowScreenshotPicker(AppShellNativeHwnd(win));
#else
            TakeScreenshots(win);
#endif
            break;

        case CmdSetScreenshotHotkey:
            ShowSetScreenshotHotkeyDialog(win);
            break;

        case CmdCopySelectionAsImage:
            CopySelectionAsImage(win);
            break;

        case CmdTranslateSelection:
            ShowSelectionTranslateDialog(tab, TranslateEngine::Default);
            break;

        case CmdTranslateSelectionWithGoogle:
            ShowSelectionTranslateDialog(tab, TranslateEngine::Google);
            break;

        case CmdTranslateSelectionWithDeepL:
            ShowSelectionTranslateDialog(tab, TranslateEngine::DeepL);
            break;

        case CmdTranslateSelectionWithGrokBuild:
            ShowSelectionTranslateDialog(tab, TranslateEngine::Grok);
            break;

        case CmdTranslateSelectionWithClaudeCode:
            ShowSelectionTranslateDialog(tab, TranslateEngine::Claude);
            break;

        case CmdTranslateSelectionWithOpenAICodex:
            ShowSelectionTranslateDialog(tab, TranslateEngine::Codex);
            break;

        case CmdTranslateSelectionWithAntiGravity:
            ShowSelectionTranslateDialog(tab, TranslateEngine::AntiGravity);
            break;

        case CmdSearchSelectionWithGoogle:
            LaunchBrowserWithSelection(tab, StrL("https://www.google.com/search?q=${selection}"));
            break;

        case CmdSearchSelectionWithBing:
            LaunchBrowserWithSelection(tab, StrL("https://www.bing.com/search?q=${selection}"));
            break;

        case CmdSearchSelectionWithWikipedia:
            LaunchBrowserWithSelection(tab, StrL("https://wikipedia.org/w/index.php?search=${selection}"));
            break;

        case CmdSearchSelectionWithGoogleScholar:
            LaunchBrowserWithSelection(tab, StrL("https://scholar.google.com/scholar?q=${selection}"));
            break;

        case CmdSearchGoogleLens:
            SearchWithGoogleLens(tab);
            break;

        case CmdSearchGoogleLensPage:
            SearchGoogleLensPage(tab, tab && tab->ctrl ? tab->ctrl->CurrentPageNo() : 0);
            break;

        case CmdSearchGoogleLensImage:
            SearchGoogleLensImage(tab, nullptr);
            break;

        case CmdAIChatWithClaudeCode:
            OnAIChatToggle(win, (int)AIChatBackend::Claude);
            break;

        case CmdAIChatWithGrokBuild:
            OnAIChatToggle(win, (int)AIChatBackend::Grok);
            break;

        case CmdAIChatWithOpenAICodex:
            OnAIChatToggle(win, (int)AIChatBackend::Codex);
            break;

        case CmdAIChatWithAntiGravity:
            OnAIChatToggle(win, (int)AIChatBackend::AntiGravity);
            break;

        case CmdSaveImage:
            ShowImageEditWindow(win, ImageEditMode::Save, CurrentImageTabPathTemp(win));
            break;

        case CmdCropImage:
            ShowImageEditWindow(win, ImageEditMode::Crop, CurrentImageTabPathTemp(win));
            break;

        case CmdResizeImage:
            ShowImageEditWindow(win, ImageEditMode::Resize, CurrentImageTabPathTemp(win));
            break;

        case CmdConvertImageToPdf:
            ShowImageEditWindow(win, ImageEditMode::Save, CurrentImageTabPathTemp(win), nullptr,
                                /* selectPdf */ true);
            break;

        case CmdSignDocument:
            ShowSignDocumentDialog(win);
            break;

        case CmdCreateAnnotImageFromClipboard:
#if OS_WASM
            if (ReadClipboardImage(win, ClipboardImageUse::Stamp)) {
                break;
            }
#endif
            CreateImageStampFromClipboard(win, Point{});
            break;

        case CmdInsertImage:
            InsertImageFromFile(win, Point{}, ImageStampSource::Prompt);
            break;

        case CmdSignWithImage:
            InsertImageFromFile(win, Point{}, ImageStampSource::Signature);
            break;

        case CmdPdfBake:
            ShowPdfBakeDialog(win);
            break;

        case CmdConvertToPDF:
            ShowConvertToPdfDialog(win);
            break;

        case CmdConvertPdfToImages:
            ShowConvertPdfToImagesDialog(win);
            break;

        case CmdPdfCompress:
            ShowPdfCompressDialog(win);
            break;

        case CmdPdfDecompress:
            ShowPdfDecompressDialog(win);
            break;

        case CmdPdfDeletePages:
            ShowPdfDeletePageDialog(win);
            break;

        case CmdMergePDF:
            ShowMergePdfDialog(win);
            break;

        case CmdMoveFrameFocus:
            if (!AppShellIsFrameFocused(win)) {
                AppShellFocusFrame(win);
            } else {
                SidebarFocusTop(win);
            }
            break;

        case CmdPdfExtractPages:
            ShowPdfExtractPagesDialog(win);
            break;

        case CmdPdfEncrypt:
            ShowPdfEncryptDialog(win);
            break;

        case CmdPdfDecrypt:
            ShowPdfDecryptDialog(win);
            break;

        case CmdDocumentExtractText:
            ShowPdfExtractTextDialog(win);
            break;

        case CmdSaveSelectionAsImage:
            ShowSaveSelectionAsImageDialog(win);
            break;

        case CmdPdfShowInfo:
            if (tab && len(tab->filePath) > 0 && CouldBePDFDoc(tab)) {
                TempStr info = EngineMupdfGetPdfInfo(tab->filePath);
                if (info) {
                    ShowTextInWindow(win, StrL("PDF Info"), info);
                }
            }
            break;

        case CmdShowErrors: {
            EngineBase* engine = dm ? dm->GetEngine() : nullptr;
            if (engine && engine->HasErrors()) {
                // GetErrorsTextTemp is the engine's internal buffer; ShowTextInWindow
                // copies it before returning, so it must not be kept past this frame.
                TempStr text = engine->GetErrorsTextTemp();
                ShowTextInWindow(win, StrL("Errors"), text);
            }
            break;
        }

        case CmdDocumentShowOutline:
            if (tab && tab->ctrl && tab->ctrl->HasToc()) {
                if (len(tab->filePath) > 0 && CouldBePDFDoc(tab)) {
                    TempStr outline = EngineMupdfGetPdfOutline(tab->filePath);
                    if (outline) {
                        ShowTextInWindow(win, StrL("Document Outline"), outline);
                    }
                } else {
                    TocTree* tocTree = tab->ctrl->GetToc();
                    if (tocTree && tocTree->root) {
                        str::Builder s;
                        TocItemToText(s, tocTree->root, 0);
                        ShowTextInWindow(win, StrL("Document Outline"), ToStr(s));
                    }
                }
            }
            break;

        case CmdProperties:
            if (IsPropertiesDialogVisible()) {
                DeletePropertiesWindow(win);
            } else {
                ShowProperties(win, ctrl);
            }
            break;

        case CmdExit:
            OnMenuExit();
            return;

        default:
            // ng: cmd/port-commands.ts reads this through the automation
            // channel to confirm its static "not handled" list
            gLastCmdFellThrough = true;
            if (tab) {
                logf("ExecuteCmd: %d not handled yet\n", cmdId);
            }
            break;
    }
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    RebuildMenuBar(win);
    AppShellInvalidate(win);
}

// ng: orig forwards a context-menu command to FrameOnCommand with the canvas
// point in the WM_COMMAND's LPARAM. There are no window messages here, so the
// commands that need that point come through this instead.
void ExecuteCmdAtPoint(MainWindow* win, int cmdId, Point pt) {
    if (!win || win->isBeingClosed) {
        return;
    }
    CustomCommand* cmd = FindCustomCommand(cmdId);
    int origId = cmd ? cmd->origId : cmdId;
    WindowTab* tab = win->CurrentTab();
    if (origId == CmdReadAloudFromCursorPosition) {
        gLastCmdFellThrough = false;
        ReadAloudAtPoint(tab, pt);
        RebuildMenuBar(win);
        AppShellInvalidate(win);
        return;
    }
    if (origId == CmdCreateAnnotImageFromClipboard) {
#if OS_WASM
        if (ReadClipboardImage(win, ClipboardImageUse::Stamp, pt)) {
            return;
        }
#endif
        CreateImageStampFromClipboard(win, pt);
        return;
    }
    if (origId == CmdInsertImage) {
        InsertImageFromFile(win, pt, ImageStampSource::Prompt);
        return;
    }
    if (origId == CmdSignWithImage) {
        InsertImageFromFile(win, pt, ImageStampSource::Signature);
        return;
    }
    if (origId == CmdInsertTextSnippet) {
        InsertTextSnippet(win, cmd, pt);
        return;
    }
    if (CmdIdToAnnotationType(origId) != AnnotationType::Unknown) {
        ExecuteAnnotCreateCmd(win, cmdId, false, pt);
        return;
    }
    // Delete / Cut / Copy always work on the annotation the menu was opened
    // on, even when the annotation list has a different selection
    DisplayModel* dm = win->AsFixed();
    if (tab && dm) {
        Annotation* annot = dm->GetAnnotationAtPos(pt, nullptr);
        if (annot) {
            SetSelectedAnnotation(tab, annot);
        } else if (origId == CmdDeleteAnnotation) {
            // A miss must not delete the selection or the hover.
            return;
        }
    }
    // paste reads dragPrevPos; a command sent with a point pastes there
    win->dragPrevPos = pt;
    ExecuteCmd(win, cmdId);
}

// --- startup ----------------------------------------------------------------

static void ParseCommandLine(Flags& flags, int argc, char** argv) {
    // the perm arena, as orig does: the flags outlive every frame and the
    // temp arena is reset on each one
#if OS_WIN
    (void)argc;
    (void)argv;
    ParseFlags(GetPermArena(), WStr(GetCommandLineW()), flags);
#else
    ParseFlagsArgv(GetPermArena(), argc, argv, flags);
#endif
}

#if OS_WIN

// ─── single instance / reuse instance (orig's, Windows only) ──────────────

// Minimal redeclaration of the shell's IVirtualDesktopManager (Windows 10 1607+),
// to tell whether a window is on the user's current virtual desktop. We use a
// distinct name (and don't include <shobjidl.h>) to avoid clashing with the SDK
// declaration; the vtable layout matches so COM calls dispatch correctly. Lets
// reusing an existing instance avoid yanking focus to another desktop (#5630).
struct ISumatraVirtualDesktopManager : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(HWND topLevelWindow, BOOL* onCurrentDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWindowDesktopId(HWND topLevelWindow, GUID* desktopId) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(HWND topLevelWindow, REFGUID desktopId) = 0;
};

// {AA509086-5CA9-4C25-8F95-589D3C07B48A}
static const GUID kClsidVirtualDesktopManager = {0xAA509086,
                                                 0x5CA9,
                                                 0x4C25,
                                                 {0x8F, 0x95, 0x58, 0x9D, 0x3C, 0x07, 0xB4, 0x8A}};
// {A5CD92FF-29BE-454C-8D04-D42879C3B837}
static const GUID kIidVirtualDesktopManager = {0xA5CD92FF,
                                               0x29BE,
                                               0x454C,
                                               {0x8D, 0x04, 0xD4, 0x28, 0x79, 0xC3, 0xB8, 0x37}};

// returns nullptr on Windows without virtual desktops (e.g. Win7) or on failure.
// COM is already initialized (ScopedOle in GpuiMain) by the time we call this.
static ISumatraVirtualDesktopManager* CreateVirtualDesktopManager() {
    ISumatraVirtualDesktopManager* mgr = nullptr;
    CoCreateInstance(kClsidVirtualDesktopManager, nullptr, CLSCTX_ALL, kIidVirtualDesktopManager, (void**)&mgr);
    return mgr;
}

// true if hwnd is on the user's current virtual desktop. Defaults to true when
// we can't tell (no manager on Win7, or the query fails), preserving the old
// "reuse the first instance window" behavior.
static bool IsWindowOnCurrentDesktop(ISumatraVirtualDesktopManager* vdm, HWND hwnd) {
    if (!vdm || !hwnd) {
        return true;
    }
    BOOL onCurrent = FALSE;
    HRESULT hr = vdm->IsWindowOnCurrentVirtualDesktop(hwnd, &onCurrent);
    if (FAILED(hr)) {
        return true;
    }
    return onCurrent != FALSE;
}

// Finds a window of a previously running instance to reuse. Prefers a window on
// the current virtual desktop; if the instance only has windows on other
// desktops, returns one of them and sets *openInNewWindow so the caller opens
// the file in a new window (which Windows places on the current desktop),
// instead of switching to another desktop (#5630). Returns nullptr if we can't
// talk to the previous instance (e.g. it runs elevated and we don't), so the
// caller opens files in this process instead of sending DDE messages that the
// elevated process would never receive.
static HWND FindExistingSumatraProcessHwnd(HANDLE* hMutex, bool* openInNewWindow) {
    *openInNewWindow = false;
    // create a unique identifier for this executable and appdata combination
    // (allows independent side-by-side installations)
    TempStr combinedPath = str::JoinTemp(GetSelfExePathTemp(), StrL("|"), GetAppDataDirTemp());
    str::ToLowerInPlace(combinedPath);
    u32 hash = MurmurHash2(combinedPath);
    TempStr mapId = fmt("SumatraPDF-%08x", hash);

    int retriesLeft = 3;
Retry:
    // use a memory mapping containing a process id as mutex
    HANDLE hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(DWORD), CWStrTemp(mapId));
    if (hMap) {
        bool hasPrevInst = (GetLastError() == ERROR_ALREADY_EXISTS);
        DWORD* procId = (DWORD*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(DWORD));
        if (!procId) {
            CloseHandle(hMap);
        } else if (!hasPrevInst) {
            *procId = GetCurrentProcessId();
            UnmapViewOfFile(procId);
            *hMutex = hMap;
            return nullptr;
        } else {
            // if the mapping already exists, find one window belonging to the original process
            DWORD prevProcId = *procId;
            UnmapViewOfFile(procId);
            CloseHandle(hMap);
            // a non-admin process can't send DDE messages to an admin process, so
            // don't return its window; retrying won't help either
            if (!CanTalkToProcess(prevProcId)) {
                return nullptr;
            }
            Vec<HWND> hwnds;
            AppShellFindOtherInstanceWindows(prevProcId, hwnds);
            // nullptr on Win7 / no virtual desktops -> IsWindowOnCurrentDesktop()
            // returns true for every window, so we reuse the first one as before.
            ISumatraVirtualDesktopManager* vdm = CreateVirtualDesktopManager();
            HWND otherDesktopWnd = nullptr; // a window of the prev instance, on another desktop
            HWND found = nullptr;
            for (HWND hwnd : hwnds) {
                if (IsWindowOnCurrentDesktop(vdm, hwnd)) {
                    found = hwnd;
                    break;
                }
                otherDesktopWnd = hwnd;
            }
            if (vdm) {
                vdm->Release();
            }
            if (found) {
                AllowSetForegroundWindow(prevProcId);
                return found; // reuse the window on the current desktop
            }
            if (otherDesktopWnd) {
                // the previous instance has windows, but none on the current virtual
                // desktop. Reuse it but open the file in a new window (it lands on the
                // current desktop) rather than switching desktops (#5630).
                AllowSetForegroundWindow(prevProcId);
                *openInNewWindow = true;
                return otherDesktopWnd;
            }
        }
    }

    // process is alive but its window isn't ready yet (startup race): retry
    if (--retriesLeft < 0) {
        return nullptr;
    }
    SleepInMs(100);
    goto Retry;
}

static void SendMyselfDDE(Str cmdA, HWND targetHwnd) {
    TempWStr cmd = ToWStrTemp(cmdA);
    if (targetHwnd) {
        // try WM_COPYDATA first, as that allows targetting a specific window
        size_t cbData = (size_t)(len(cmd) + 1) * sizeof(WCHAR);
        COPYDATASTRUCT cds = {kCopyDataDdeW, (DWORD)cbData, (void*)cmd.s};
        LRESULT res = SendMessageW(targetHwnd, WM_COPYDATA, 0, (LPARAM)&cds);
        if (res) {
            return;
        }
        // fall-through to DDEExecute if wasn't handled
    }
    DDEExecute(WStr((WCHAR*)kSumatraDdeServer), WStr((WCHAR*)kSumatraDdeTopic), cmd);
}

// Returns true if the only thing the caller wants is to open a file (no
// goto-page, no view overrides, etc.). In that case we can use the cheaper
// kCopyDataOpen fast path instead of building a DDE grammar string and blocking
// the caller in SendMessageW while the receiver loads the document.
static bool IsSimpleOpenCase(const Flags& i, bool isFirstWin) {
    if (!isFirstWin) {
        return true; // extras only apply to the first window
    }
    if (i.namedDest || i.pageNumber > 0) {
        return false;
    }
    if (i.startView != DisplayMode::Automatic || i.startZoom != kInvalidZoom) {
        return false;
    }
    if (i.startScroll.x != -1 && i.startScroll.y != -1) {
        return false;
    }
    if (i.search) {
        return false;
    }
    if (i.enterPresentation || i.enterFullScreen) {
        return false;
    }
    return true;
}

// Send just the path + newWindow flag to an already-running SumatraPDF via
// WM_COPYDATA. Receiver (OnCopyData) handles it asynchronously so this
// SendMessageW returns fast. Returns true if the message was handled.
static bool SendOpenFileToExistingInstance(HWND targetHwnd, Str fullPath, u32 newWindow) {
    size_t pathLen = (size_t)len(fullPath);
    size_t cbData = sizeof(SumatraOpenCopyData) + pathLen + 1;
    auto* payload = (SumatraOpenCopyData*)malloc(cbData);
    if (!payload) {
        return false;
    }
    payload->newWindow = newWindow;
    memcpy(payload + 1, fullPath.s, pathLen + 1);
    COPYDATASTRUCT cds = {kCopyDataOpen, (DWORD)cbData, payload};
    LRESULT res = SendMessageW(targetHwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    free(payload);
    return res != 0;
}

static bool SendOpenFilesToExistingInstance(HWND targetHwnd, StrVec& paths, u32 newWindow) {
    size_t cbData = sizeof(SumatraOpenManyCopyData);
    for (Str path : paths) {
        cbData += (size_t)path.len + 1;
    }
    if (cbData > MAXDWORD) {
        return false;
    }

    u8* payload = (u8*)malloc(cbData);
    if (!payload) {
        return false;
    }
    auto* data = (SumatraOpenManyCopyData*)payload;
    data->newWindow = newWindow;
    data->pathCount = (u32)len(paths);
    u8* dst = payload + sizeof(*data);
    for (Str path : paths) {
        memcpy(dst, path.s, (size_t)path.len);
        dst += path.len;
        *dst++ = 0;
    }
    COPYDATASTRUCT cds = {kCopyDataOpenMany, (DWORD)cbData, payload};
    LRESULT res = SendMessageW(targetHwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    free(payload);
    return res != 0;
}

// delegate file opening to a previously running instance by sending a DDE message
static void OpenUsingDDE(HWND targetHwnd, Str path, Flags& i, bool isFirstWin) {
    TempStr fullPath = path::NormalizeTemp(path);

    // 2 forces opening a new window
    u32 newWindow = i.inNewWindow ? 2 : 0;

    // Common case: Explorer double-clicks a file while SumatraPDF is already
    // running (reuseInstance). Use the simpler kCopyDataOpen format; the
    // receiver loads async so Explorer's child SumatraPDF process can exit
    // instantly instead of blocking on the file load.
    if (targetHwnd && !i.reuseDdeInstance && IsSimpleOpenCase(i, isFirstWin)) {
        if (SendOpenFileToExistingInstance(targetHwnd, fullPath, newWindow)) {
            return;
        }
        // fall through to the DDE grammar path if WM_COPYDATA wasn't handled
    }

    str::Builder cmd;
    cmd.Append(fmt("[Open(\"%s\", %d, 1, 0)]", fullPath, newWindow));
    if (i.namedDest && isFirstWin) {
        cmd.Append(fmt("[GotoNamedDest(\"%s\", \"%s\")]", fullPath, i.namedDest));
    } else if (i.pageNumber > 0 && isFirstWin) {
        cmd.Append(fmt("[GotoPage(\"%s\", %d)]", fullPath, i.pageNumber));
    }
    if ((i.startView != DisplayMode::Automatic || i.startZoom != kInvalidZoom ||
         (i.startScroll.x != -1 && i.startScroll.y != -1)) &&
        isFirstWin) {
        Str viewModeStr = DisplayModeToString(i.startView);
        cmd.Append(fmt("[SetView(\"%s\", \"%s\", %.2f, %d, %d)]", fullPath, viewModeStr, i.startZoom, i.startScroll.x,
                       i.startScroll.y));
    }
    if (i.search) {
        cmd.Append(fmt("[Search(\"%s\",\"%s\")]", fullPath, i.search));
    }
    if ((i.enterPresentation || i.enterFullScreen) && isFirstWin) {
        Str name = i.enterPresentation ? StrL("Presentation") : StrL("FullScreen");
        cmd.Append(fmt("[%s(\"%s\")]", name, fullPath));
    }

    if (i.reuseDdeInstance) {
        targetHwnd = nullptr; // force DDEExecute
    }
    SendMyselfDDE(ToStr(cmd), targetHwnd);
}

// orig's WinMain: hand the command line to an already running instance when
// -reuse-instance / -dde / the ReuseInstance setting asks for it. Returns true
// when this process is done and should exit.
static bool ForwardToExistingInstance(Flags& flags, HANDLE* hMutex) {
    bool openInNewWindow = false;
    HWND existingInstanceHwnd = FindExistingSumatraProcessHwnd(hMutex, &openInNewWindow);

    HWND existingHwnd = nullptr;
    if (flags.printDialog || gForTesting) {
        // the print dialog and the test runs always use this process
    } else if (flags.reuseDdeInstance || flags.dde) {
        Vec<HWND> hwnds;
        AppShellFindOtherInstanceWindows(0, hwnds);
        existingHwnd = len(hwnds) > 0 ? hwnds[0] : nullptr;
    } else if (gSettings->reuseInstance) {
        existingHwnd = existingInstanceHwnd;
    }

    if (flags.dde) {
        logf("sending flags.dde '%s', hwnd: 0x%p\n", flags.dde, existingHwnd);
        SendMyselfDDE(flags.dde, existingHwnd);
        return true;
    }
    if (!existingHwnd) {
        return false;
    }

    int nFiles = len(flags.fileNames);
    // reusing an instance whose windows are all on other virtual desktops:
    // open the first file in a new window so it lands on the current desktop
    // (#5630). Subsequent files then open into that (now current) window.
    bool reuseInNewWindow = openInNewWindow && (existingHwnd == existingInstanceHwnd);
    // -new-window: each file in its own window. -new-window-tabs: one new
    // window, remaining files as tabs in that window (issue #5044).
    bool userNewWindowEach = flags.inNewWindow && !flags.inNewWindowTabs;
    bool userNewWindowTabs = flags.inNewWindowTabs;
    bool canBatchOpen = nFiles > 1 && !flags.reuseDdeInstance && IsSimpleOpenCase(flags, true) && !userNewWindowEach;
    if (canBatchOpen) {
        StrVec paths;
        for (Str path : flags.fileNames) {
            paths.Append(path::NormalizeTemp(path));
        }
        u32 newWindow = (reuseInNewWindow || userNewWindowTabs) ? 2 : 0;
        if (SendOpenFilesToExistingInstance(existingHwnd, paths, newWindow)) {
            return true;
        }
    }
    for (int n = 0; n < nFiles; n++) {
        Str path = flags.fileNames[n];
        bool isFirstWindow = (0 == n);
        bool savedInNewWindow = flags.inNewWindow;
        if (reuseInNewWindow && n == 0) {
            flags.inNewWindow = true;
        } else if (userNewWindowTabs) {
            flags.inNewWindow = (n == 0);
        } else if (userNewWindowEach) {
            flags.inNewWindow = true;
        }
        OpenUsingDDE(existingHwnd, path, flags, isFirstWindow);
        flags.inNewWindow = savedInNewWindow;
    }
    if (0 == nFiles) {
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/2306
        // if -new-window cmd-line flag given, create a new window
        // even if there are no files to open
        if (flags.inNewWindow || flags.inNewWindowTabs) {
            return false;
        }
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/3386
        // e.g. when shift-click in taskbar, open a new window
        SendMyselfDDE(StrL("[NewWindow]"), existingHwnd);
    }
    return true;
}

#endif

#if OS_WIN
// orig's SetupPluginMode(): -plugin <parent hwnd> makes us a frameless viewer
// inside a host window (a browser plugin). Windows only, as in orig.
static bool SetupPluginMode(Flags& i) {
    if (!IsWindow(i.hwndPluginParent) || len(i.fileNames) == 0) {
        return false;
    }

    gPluginURL = Str(i.pluginURL);
    if (len(gPluginURL) == 0) {
        gPluginURL = Str(i.fileNames[0]);
    }

    // don't save preferences for plugin windows (and don't allow fullscreen mode)
    RestrictPolicies(Perm::SavePreferences | Perm::FullscreenAccess);

    i.reuseDdeInstance = i.exitWhenDone = false;
    gSettings->reuseInstance = false;
    // don't allow tabbed navigation
    gSettings->useTabs = false;
    // always display the toolbar when embedded (as there's no menubar in that case)
    gSettings->showToolbar = true;
    // never allow esc as a shortcut to quit
    gSettings->escToExit = false;
    // never show the sidebar by default
    gSettings->showToc = false;
    if (DisplayMode::Automatic == gSettings->defaultDisplayModeEnum) {
        // if the user hasn't changed the default display mode,
        // display documents as single page/continuous/fit width
        // (similar to Adobe Reader, Google Chrome and how browsers display HTML)
        gSettings->defaultDisplayModeEnum = DisplayMode::Continuous;
        gSettings->defaultZoomFloat = kZoomFitWidth;
    }
    // use fixed page UI for all document types (so that the context menu always
    // contains all plugin specific entries and the main window is never closed)
    gSettings->chmUI.useFixedPageUI = true;

    // extract some command line arguments from the URL's hash fragment where available
    // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#nameddest=G4.1501531
    int hashIdx = i.pluginURL ? str::IndexOfChar(i.pluginURL, '#') : -1;
    if (hashIdx >= 0) {
        TempStr args = str::DupTemp(Str(i.pluginURL.s + hashIdx + 1));
        str::TransCharsInPlace(args, StrL("#"), StrL("&"));
        StrVec parts;
        Split(&parts, args, StrL("&"), true);
        for (int k = 0; k < len(parts); k++) {
            Str part = parts[k];
            Str pageArg = part;
            int pageNo;
            if (str::TrimPrefixI(pageArg, StrL("page=")) && !str::IsNull(str::Parse(pageArg, "%d%$", &pageNo))) {
                i.pageNumber = pageNo;
            } else if (str::TrimPrefixI(part, StrL("nameddest=")) && part) {
                i.namedDest = str::Dup(GetPermArena(), part);
            } else if (!str::ContainsChar(part, '=') && part) {
                i.namedDest = str::Dup(GetPermArena(), part);
            }
        }
    }
    return true;
}

// orig's MaybeMakePluginWindow(). gpui has no parent-window option in WinOpts
// (see "gpui gaps"), but it does hand out the native handle, so the reparenting
// is orig's win32 code on that handle.
// return false if failed in a way that should abort the app
static bool MaybeMakePluginWindow(MainWindow* win, HWND hwndParent) {
    if (!hwndParent) {
        return true;
    }
    HWND hwndFrame = AppShellNativeHwnd(win);
    logf("MakePluginWindow: win: 0x%p, hwndFrame: 0x%p, hwndParent: 0x%p (isWindow: %d), gPluginURL: %s\n", win,
         (void*)hwndFrame, (void*)hwndParent, (int)IsWindow(hwndParent),
         len(gPluginURL) == 0 ? StrL("<null>") : gPluginURL);
    ReportIf(!gPluginMode);

    if (!IsWindow(hwndParent) || !hwndFrame) {
        // we validated hwndParent for validity at startup but I'm seeing cases
        // in crash reports were it's not valid here
        // I assume the window went away so we just abort
        return false;
    }

    // first SetParent as top-level window (may fail but primes the window manager)
    SetParent(hwndFrame, hwndParent);

    // strip styles and set WS_CHILD
    LONG ws = GetWindowLongW(hwndFrame, GWL_STYLE);
    ws &= ~(WS_POPUP | WS_BORDER | WS_CAPTION | WS_THICKFRAME);
    ws |= WS_CHILD;
    SetWindowLongW(hwndFrame, GWL_STYLE, ws);

    // second SetParent after WS_CHILD is set
    SetParent(hwndFrame, hwndParent);
    Rect r = HwndClientRect(hwndParent);
    HwndMoveWindow(hwndFrame, &r);
    ShowWindow(hwndFrame, SW_SHOW);
    UpdateWindow(hwndFrame);

    // from here on, we depend on the plugin's host to resize us
    HwndSetFocus(hwndFrame);
    return true;
}
#endif

// when the process started, so the canvas can say how long the first page
// took to appear (the -dbg-control performance snapshot)
static TimeStamp gAppStartTime;

double AppElapsedMs() {
    return TimeSinceInMs(gAppStartTime);
}

static void ReplaceColor(ParsedColor& col, Str maybeColor) {
    ParsedColor c;
    ParseColor(c, maybeColor);
    if (c.parsedOk) {
        SetColorText(col, SerializeColorTemp(c.col));
    }
}

// orig's UpdateSettings: the command line flags that override a setting for
// this run. ng: -window-pos is applied where the frame is placed
// (CreateAndShowMainWindow), so it is not copied into gSettings here.
static void UpdateSettings(const Flags& i) {
    if (len(i.inverseSearchCmdLine) > 0) {
        str::ReplaceWithCopy(&gSettings->inverseSearchCmdLine, i.inverseSearchCmdLine);
        gSettings->enableTeXEnhancements = true;
    }
    if (i.invertColors) {
        SetDocumentColorsFollowTheme(DocumentColorsFollowTheme::Smart);
    }

    Str arg;
    Str param;
    for (int n = 0; n < len(i.globalPrefArgs); n++) {
        arg = i.globalPrefArgs[n];
        if (str::EqI(arg, StrL("-esc-to-exit"))) {
            gSettings->escToExit = true;
        } else if (str::EqI(arg, StrL("-bgcolor")) || str::EqI(arg, StrL("-bg-color"))) {
            // -bgcolor is for backwards compat (was used pre-1.3)
            // -bg-color is for consistency
            param = i.globalPrefArgs[++n];
            ReplaceColor(gSettings->mainWindowBackground, param);
        } else if (str::EqI(arg, StrL("-set-color-range"))) {
            param = i.globalPrefArgs[++n];
            ReplaceColor(gSettings->fixedPageUI.textColor, param);
            param = i.globalPrefArgs[++n];
            ReplaceColor(gSettings->fixedPageUI.backgroundColor, param);
        } else if (str::EqI(arg, StrL("-fwdsearch-offset"))) {
            param = i.globalPrefArgs[++n];
            gSettings->forwardSearch.highlightOffset = ParseInt(param);
            gSettings->enableTeXEnhancements = true;
        } else if (str::EqI(arg, StrL("-fwdsearch-width"))) {
            param = i.globalPrefArgs[++n];
            gSettings->forwardSearch.highlightWidth = ParseInt(param);
            gSettings->enableTeXEnhancements = true;
        } else if (str::EqI(arg, StrL("-fwdsearch-color"))) {
            param = i.globalPrefArgs[++n];
            ReplaceColor(gSettings->forwardSearch.highlightColor, param);
            gSettings->enableTeXEnhancements = true;
        } else if (str::EqI(arg, StrL("-fwdsearch-permanent"))) {
            param = i.globalPrefArgs[++n];
            gSettings->forwardSearch.highlightPermanent = ParseInt(param);
            gSettings->enableTeXEnhancements = true;
        } else if (str::EqI(arg, StrL("-manga-mode"))) {
            param = i.globalPrefArgs[++n];
            gSettings->comicBookUI.cbxMangaMode = str::EqI(StrL("true"), param) || str::Eq(StrL("1"), param);
        }
    }
}

extern "C" {
int muconvert_main(int argc, char** argv);
int mudraw_main(int argc, char** argv);
int mutrace_main(int argc, char** argv);
int murun_main(int argc, char** argv);
int pdfclean_main(int argc, char** argv);
int pdfextract_main(int argc, char** argv);
int pdfinfo_main(int argc, char** argv);
int pdfposter_main(int argc, char** argv);
int pdfshow_main(int argc, char** argv);
int pdfpages_main(int argc, char** argv);
int pdfcreate_main(int argc, char** argv);
int pdfmerge_main(int argc, char** argv);
int pdfsign_main(int argc, char** argv);
int pdfrecolor_main(int argc, char** argv);
int pdftrim_main(int argc, char** argv);
int pdfbake_main(int argc, char** argv);
int mugrep_main(int argc, char** argv);
int pdfaudit_main(int argc, char** argv);
int fz_redirect_io_to_existing_console();
}

// won't collide with a tool's own exit code
constexpr int kNoCliTool = -1234321;

struct CliTool {
    const char* name;
    int (*fn)(int argc, char** argv);
};

// same names as orig's `SumatraPDF <tool>` (src/sumatrapdf-tool.cpp)
static CliTool gCliTools[] = {
    {"run", murun_main},          {"draw", mudraw_main},    {"convert", muconvert_main}, {"audit", pdfaudit_main},
    {"bake", pdfbake_main},       {"clean", pdfclean_main}, {"create", pdfcreate_main},  {"extract", pdfextract_main},
    {"info", pdfinfo_main},       {"merge", pdfmerge_main}, {"pages", pdfpages_main},    {"poster", pdfposter_main},
    {"recolor", pdfrecolor_main}, {"show", pdfshow_main},   {"sign", pdfsign_main},      {"trim", pdftrim_main},
    {"grep", mugrep_main},        {"trace", mutrace_main},
};

// `SumatraPDF draw file.pdf` is a mupdf tool. Run it before any window exists,
// or the GUI stays up and a caller such as jpeg-xl-pdf times out.
static int MaybeRunCliTool(int argc, char** argv) {
    if (argc < 2) {
        return kNoCliTool;
    }
    Str name(argv[1]);
    int (*fn)(int, char**) = nullptr;
    for (const CliTool& t : gCliTools) {
        if (str::EqI(name, Str(t.name))) {
            fn = t.fn;
            break;
        }
    }
    if (!fn) {
        return kNoCliTool;
    }
    fz_redirect_io_to_existing_console();
    InstallEmbeddedFontLoader();
    return fn(argc - 1, argv + 1);
}

#if IS_DEBUG
// -extract-text: print one page as hex and exit. Orig does this before any
// window; leaving it to the GUI never returns.
static void ExtractPageTextToStdout(EngineBase* engine, int pageNo) {
    PageText pageText = engine->ExtractPageText(pageNo);
    if (len(pageText.text) == 0) {
        FreePageText(&pageText);
        return;
    }
    TempStr s = str::ReplaceTemp(pageText.text, StrL("\n"), StrL("_"));
    printf("text on page %d: '", pageNo);
    for (int i = 0; i < len(s); i++) {
        printf("%02x ", (u8)s.s[i]);
    }
    printf("'\n");
    FreePageText(&pageText);
}

static void TestExtractPages(const Flags& ci) {
#if OS_WIN
    RedirectIOToExistingConsole();
#endif
    gLogToConsole = false;
    for (Str fileName : ci.fileNames) {
        EngineBase* engine = CreateEngineFromFile(fileName, nullptr, true);
        if (!engine) {
            printf("failed to create engine for file '%s'\n", CStrTemp(fileName));
            continue;
        }
        if (ci.pageNumber < 0) {
            int nPages = engine->PageCount();
            for (int i = 1; i <= nPages; i++) {
                ExtractPageTextToStdout(engine, i);
            }
        } else {
            ExtractPageTextToStdout(engine, ci.pageNumber);
        }
        SafeEngineRelease(&engine);
    }
    fflush(stdout);
}
#endif

int GpuiMain(int argc, char** argv) {
    int toolRes = MaybeRunCliTool(argc, argv);
    if (toolRes != kNoCliTool) {
        return toolRes;
    }
    gAppStartTime = TimeGet();
#if OS_DARWIN
    AppShellDisableAutoTermination();
#endif
#if OS_WIN
    // ng: orig's WinMain does this; without it WIC and GDI+ decode nothing, so
    // e.g. reading a cached thumbnail back fails
    ScopedOle ole;
    ScopedGdiPlus gdiPlus(true);
    // the dynamically loaded win32 entry points (dbghelp for the crash handler
    // and the hang detector, the per-monitor DPI calls); orig's WinMain too
    InitDynCalls();
    NoDllHijacking();
#endif
    InitPerfLog();
    uitask::Initialize(uitask::Dispatch::Queue);

    gFlags = new Flags();
    ParseCommandLine(*gFlags, argc, argv);
#if OS_WASM
    // a page has no command line; `?file=` names a document in MEMFS
    if (TempStr path = WasmQueryFileTemp(); len(path) > 0) {
        gFlags->fileNames.Append(path);
    }
#endif
    if (gFlags->forTesting) {
        gForTesting = true;
    }
    if (gFlags->perfLogFile) {
        SetPerfLogPath(gFlags->perfLogFile);
    }
    // ng: the log and the crash info go under the application data directory
    // (orig has a build-specific one for them), so -appdata is applied before
    // either is set up, not with the settings
    if (len(gFlags->appdataDir) > 0) {
        SetAppDataDir(gFlags->appdataDir);
    }
    if (gFlags->startPerfLog) {
        StartPerfLog();
    }
    if (len(gFlags->logFile) > 0) {
        StartLogToFile(gFlags->logFile, true);
    } else if (gFlags->log) {
        // orig's GetLogFilePathTemp(). ng: the application data directory
        // stands in for orig's build-specific one, as for the crash info
        TempStr logPath = GetPathInAppDataDirTemp(StrL("sumatra-log.txt"));
        if (len(logPath) > 0) {
            StartLogToFile(logPath, true);
        }
    }
    if (len(gFlags->updateSelfTo) > 0) {
        UpdateSelfTo(gFlags->updateSelfTo, gFlags->sleepMs);
    }

    InitializePolicies(gFlags->restrictedUse);
    // must follow InitializePolicies(): it asks whether we may use the network
    InstallSumatraCrashHandler(gFlags->forTesting);
    gCrashOnOpen = gFlags->crashOnOpen;
#if OS_WIN
    if (len(gFlags->installRegRoot) > 0) {
        SetInstallRegistryTestRoot(gFlags->installRegRoot);
    }
#endif
    // the installer / uninstaller modes of the exe: their own window, no
    // settings, no document model (orig runs them from WinMain the same way)
    if (gFlags->uninstall) {
        int exitCode = RunUninstaller(gFlags);
        uitask::Destroy();
        return exitCode;
    }
    if (gFlags->install || gFlags->fastInstall || gFlags->runInstallNow || gFlags->justExtractFiles) {
        int exitCode = RunInstaller(gFlags);
        ScheduleDeleteTempInstaller();
        uitask::Destroy();
        return exitCode;
    }
    if (gFlags->quickLookAgent) {
        // the hidden Explorer Space-bar hook; no UI, no settings
        RunExplorerQuickLookAgentLoop();
        delete gFlags;
        gFlags = nullptr;
        uitask::Destroy();
        return 0;
    }
    // -appdata <dir>: settings, history and thumbnails live there instead of
    // the per-user location (orig does this in WinMain)
    if (len(gFlags->upgradeFrom) > 0) {
        logf(" flags.upgradeFrom: '%s'\n", gFlags->upgradeFrom);
        StartInstallerAutoUpgrade(gFlags->upgradeFrom);
        delete gFlags;
        gFlags = nullptr;
        uitask::Destroy();
        return 0;
    }
    if (len(gFlags->deleteFile) > 0) {
        logf(" flags.deleteFile: '%s'\n", gFlags->deleteFile);
#if OS_WIN
        RedirectIOToExistingConsole();
#endif
        // sleeping for a bit to make sure that the program that launched us
        // had time to exit so that we can overwrite it
        if (gFlags->sleepMs > 0) {
            SleepInMs(gFlags->sleepMs);
        }
        // TODO: retry if file busy?
        bool ok = file::Delete(gFlags->deleteFile);
        if (ok) {
            logf("Deleted '%s'\n", gFlags->deleteFile);
        } else {
            logf("Failed to delete '%s'\n", gFlags->deleteFile);
        }
        if (gFlags->exitWhenDone) {
#if OS_WIN
            HandleRedirectedConsoleOnShutdown();
#endif
            delete gFlags;
            gFlags = nullptr;
            uitask::Destroy();
            return 0;
        }
    }
    LoadSettings();
#if IS_DEBUG
    if (gFlags->testExtractPage) {
        TestExtractPages(*gFlags);
        uitask::Destroy();
        return 0;
    }
#endif
    if (len(gFlags->lang) > 0) {
        SetCurrentLang(gFlags->lang);
    }
    UpdateSettings(*gFlags);
    // the console-only dump modes; orig runs them from WinMain the same way
    if (gFlags->dumpExif) {
        gLogToConsole = false;
        DumpExif(*gFlags);
        uitask::Destroy();
        return 0;
    }
    if (gFlags->dumpChm) {
        gLogToConsole = false;
        int exitCode = DumpChm(*gFlags);
        uitask::Destroy();
        return exitCode;
    }
    if (gFlags->showPrintersDialog) {
        // -console / -silent: list to stdout only, no dialog window (#5810)
        ShowPrintersDialog(gFlags->silent || gFlags->showConsole);
        delete gFlags;
        gFlags = nullptr;
        uitask::Destroy();
        return 0;
    }
#if OS_WIN
    if (gFlags->showConsole) {
        RedirectIOToConsole();
    }
#endif
    if (len(gFlags->pathsToBenchmark) > 0) {
        BenchFileOrDir(gFlags->pathsToBenchmark);
    }
    if (gFlags->exitImmediately) {
        CleanUpSettings();
        trans::Destroy();
        FreeAcceleratorTables();
        delete gFlags;
        gFlags = nullptr;
        uitask::Destroy();
        return 0;
    }
    // -print-dialog takes precedence: if the user explicitly asked for the
    // print dialog, show it (handled after the file loads, below) instead of
    // printing silently, even when -print-to/-print-to-default is also given
    // (fixes #3975)
    if (len(gFlags->printerName) > 0 && !gFlags->printDialog) {
        // note: this prints all files. exit code is 0 on success, otherwise the
        // category of the first failure (see PrintResult), so an automated
        // caller knows why (#3478)
        PrintResult printRes = PrintResult::Ok;
        for (Str path : gFlags->fileNames) {
            PrintResult r = PrintFile(path, gFlags->printerName, !gFlags->silent, gFlags->printSettings);
            if (r != PrintResult::Ok && printRes == PrintResult::Ok) {
                printRes = r;
            }
        }
        int printExitCode = (int)printRes;
        logf("Finished printing, exitCode: %d\n", printExitCode);
        CleanUpSettings();
        trans::Destroy();
        FreeAcceleratorTables();
        delete gFlags;
        gFlags = nullptr;
        uitask::Destroy();
        return printExitCode;
    }
#if OS_WIN
    // hand the command line to an already running instance when asked to
    // (-reuse-instance, -dde, or the ReuseInstance setting). The mapping this
    // opens is what makes us the "first" instance; it lives until we exit
    HANDLE hInstanceMutex = nullptr;
    if (ForwardToExistingInstance(*gFlags, &hInstanceMutex)) {
        CleanUpSettings();
        trans::Destroy();
        FreeAcceleratorTables();
        delete gFlags;
        gFlags = nullptr;
        uitask::Destroy();
        return 0;
    }
#endif

#if OS_WIN
    if (gFlags->hwndPluginParent) {
        // check early to avoid a crash in MakePluginWindow()
        if (!IsWindow(gFlags->hwndPluginParent)) {
            logf("-plugin argument is not a valid window handle (hwnd)\n");
            uitask::Destroy();
            return 1;
        }
        if (!SetupPluginMode(*gFlags)) {
            uitask::Destroy();
            return 1;
        }
    }
#endif

    // LoadSettings() already built the accelerator table (orig's WinMain does
    // not build it either); building it twice trips ReportIf(gAccels)
    DetectExternalViewers();
    FileWatcherInit();
    RunAsync(MkFunc0Void(DeleteStaleDviCache), StrL("DeleteStaleDviCache"));
    gRenderCache = new RenderCache();

    gp::App* app = gp::AppNew();
    gpc::Init(app);
    // the current theme's colors become gpui's before the first element is built
    AppShellSetApp(app);
    SumatraUpdateTheme();

    int dx = gSettings->windowPos.dx > 100 ? gSettings->windowPos.dx : 1024;
    int dy = gSettings->windowPos.dy > 100 ? gSettings->windowPos.dy : 768;
    MainWindow* win = AppShellCreateWindow(app, dx, dy);
    if (!gFlags->quickLook) {
        PlaceMainWindow(win, nullptr, PlaceWindowWhen::Now);
    }
#if !OS_WIN
    ReRegisterGlobalHotkeys();
#endif
    if (gFlags->quickLook) {
        win->isQuickLook = true;
        ApplyExplorerQuickLookChrome(win);
    }
    SetSidebarVisibility(win, false, gSettings->showFavorites);
    ShowMaybeDelayedNotifications(win);

    // keep the session we started with alive (a lazily restored tab still
    // reads its TabState) and let SaveSettings() snapshot the live one
    TakeInitialSessionData();
    InstallSessionStateHook();
    InstallLayoutNotifHooks();
#if OS_WIN
    // a DDE / WM_COPYDATA open that arrives while the command line is still
    // being opened is queued instead of racing it
    gIsStartup = true;
#endif
    bool restoredSession = false;
    if (len(gFlags->fileNames) == 0 || SettingsUseTabs()) {
        restoredSession = RestoreSession(win);
        // the session may have opened more windows; the command line goes into
        // the first one, as orig does
        win = len(gWindows) > 0 ? gWindows[0] : win;
    }
    SortNatural(&gFlags->fileNames);
    for (Str path : gFlags->fileNames) {
        // a file the restored session already opened is selected, not re-opened
        WindowTab* open = restoredSession ? FindTabByFilePath(path::NormalizeTemp(path)) : nullptr;
        if (open) {
            TabsSelect(win, win->GetTabIdx(open));
            continue;
        }
        LoadDocument(win, path);
        if (gFlags->printDialog) {
            PrintCurrentFile(win, gFlags->exitWhenDone);
        }
    }
#if OS_WIN
    LoadDdeOpenOnStartup(win);
    gIsStartup = false;
#endif
    // orig's LoadOnStartup: WindowState = 3 in the settings, then the command
    // line flags, which win over it
    if (!restoredSession && gSettings->windowState == WIN_STATE_FULLSCREEN) {
        EnterFullScreen(win);
    }
    EnterFullScreenFromFlags(*gFlags, win);
    ApplyStartupViewFlags(*gFlags, win);
#if OS_WIN
    if (gFlags->hwndPluginParent && !MaybeMakePluginWindow(win, gFlags->hwndPluginParent)) {
        logf("MaybeMakePluginWindow() failed, quitting\n");
        AppShellQuit();
    }
#endif
    if (len(gFlags->stressTestPath) > 0) {
        // don't save file history and preference changes
        RestrictPolicies(Perm::SavePreferences);
        RebuildMenuBar(win);
        StartStressTest(gFlags, win);
    }
    // call this once it's clear whether Perm::SavePreferences has been granted
    RegisterSettingsForFileChanges();
    // -forward-search <source file> <line>: jump to the matching place in the
    // PDF and flash the mark there (orig does this in LoadOnStartup)
    if (gFlags->forwardSearchOrigin && gFlags->forwardSearchLine && win->AsFixed() && win->AsFixed()->pdfSync) {
        int page;
        Vec<Rect> rects;
        TempStr srcPath = path::NormalizeTemp(gFlags->forwardSearchOrigin);
        int ret = win->AsFixed()->pdfSync->SourceToDoc(srcPath, gFlags->forwardSearchLine, 0, &page, rects);
        ShowForwardSearchResult(win, srcPath, gFlags->forwardSearchLine, 0, ret, page, rects);
    }
    if (len(gFlags->search) > 0) {
        StartSearchFromCommandLine(win, gFlags->search);
    }
    StartAsyncUpdateCheck(win, UpdateCheck::Automatic);
#if OS_WIN
    // home-page bottom bar if we lost default-app status for registered extensions
    if (win->IsCurrentTabAbout()) {
        uitask::Post(MkFunc0(MaybeShowDefaultAppNotification, win), "MaybeShowDefaultAppNotification");
    }
#endif
    StartSumatraControl(gFlags->controlPipeName);

    // on by default in debug builds; release builds can opt in by calling
    // StartUiHangDetector() themselves
    if (gIsDebugBuild) {
        StartUiHangDetector();
    }

    int res = AppShellRun(app);
    StopUiHangDetector();
    SavePerfLog();
    UnregisterSettingsForFileChanges();
    ShutdownWin11Printing();
    TtsRelease();
    ClearPendingNextPrevNav();
#if OS_WIN
    DisconnectLastDragDataObject();
    NativeCursorsDelete();
#endif

    // the window is off the screen but its model is still here, and the shell
    // cached its geometry on every frame, so the session snapshot SaveSettings
    // takes below still sees the tabs and the window size
    if (!gDontSaveSettings) {
        ScheduleSaveSettings();
        FlushScheduledSaveSettings();
    }
    // a window leaves the list before it is deleted: the destructor of the
    // next one walks gWindows (ReadAloudForgetTab for each of its tabs)
    while (len(gWindows) > 0) {
        MainWindow* w = gWindows[0];
        VecRemoveAt(gWindows, 0);
        delete w;
    }
    DeleteSyncTempFiles();
    delete gRenderCache;
    gRenderCache = nullptr;
    FileWatcherWaitForShutdown();
    FreeExternalViewers();
    FreeAcceleratorTables();
    CleanUpSettings();
    trans::Destroy();
    delete gFlags;
    gFlags = nullptr;
    uitask::Destroy();
    gp::AppFree(app);
#if OS_WIN
    if (hInstanceMutex) {
        CloseHandle(hInstanceMutex);
    }
#endif
    DestroyPerfLog();
    return res;
}
