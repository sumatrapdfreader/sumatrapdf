/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/WinDynCalls.h"
#include "base/DirScan.h"
#include <dwmapi.h>
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/FileWatcher.h"
#include "base/GuessFileType.h"
#include "base/SquareTreeParser.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/Http.h"
#include "base/Crypto.h"
#include "base/ScopedWin.h"
#include "base/GdiPlusUtil.h"
#include "base/Archive.h"
#include "base/Timer.h"
#include "base/LzmaSimpleArchive.h"
#include "base/CmdLineArgsIter.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/win/WebView.h"

#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/win/TabsCtrl.h"

#include "SimpleBrowserWindow.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "PdfDarkMode.h"
#include "Annotation.h"
#include "FormFields.h"
#include "PdfTools.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "MarkdownToc.h"
#include "EmbeddedResources.h"
#include "PalmDbReader.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "MobiDoc.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "PdfSync.h"
#include "RenderCache.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "UpdateCheck.h"
#include "resource.h"
#include "Commands.h"
#include "Flags.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "Canvas.h"
#include "CaptionGlyphs.h"
#include "RefHover.h"
#include "CrashHandler.h"
#include "ExternalViewers.h"
#include "Favorites.h"
#include "FileThumbnails.h"
#include "Menu.h"
#include "PngOptimizer.h"
#include "Print.h"
#include "SearchAndDDE.h"
#include "Selection.h"
#include "LinkFollow.h"
#include "SelectTextKeyboard.h"
#include "KeyboardHelp.h"
#include "SelectionToolbar.h"
#include "ScreenshotCapture.h"
#include "Screenshot.h"
#include "ImageSaveCropResize.h"
#include "StressTesting.h"
#include "HomePage.h"
#include "SumatraProperties.h"
#include "TabGroupsManage.h"
#include "TableOfContents.h"
#include "Tabs.h"
#include "Toolbar.h"
#include "SvgIcons.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "Translations.h"
#include "uia/Provider.h"
#include "SumatraConfig.h"
#include "EditAnnotations.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "SelectionTranslate.h"
#include "SelectionHandlers.h"
#include "CommandPalette.h"
#include "AdvancedSettingsDialog.h"
#include "ChangeLanguageDialog.h"
#include "ChangeScrollbarDialog.h"
#include "ChangeThemeDialog.h"
#include "ChangeColorDialog.h"
#include "CustomZoomDialog.h"
#include "GetPasswordDialog.h"
#include "GoToPageDialog.h"
#include "InverseSearchDialog.h"
#include "SettingsDialog.h"
#include "NavFilesInFolder.h"
#include "Installer.h"
#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"
#include "Theme.h"
#include "DarkMode_win.h"
#include "TextToSpeech.h"
#include "ReadAloudHighlight.h"
#include "ReadAloudPlaybackBar.h"
#include "SumatraLog.h"

using Gdiplus::Graphics;
using Gdiplus::Pen;
using Gdiplus::SolidBrush;

constexpr const char* kRestrictionsFileName = "sumatrapdfrestrict.ini";

constexpr const char* kSumatraWindowTitle = "SumatraPDF";
constexpr const WCHAR* kSumatraWindowTitleW = L"SumatraPDF";

// Text-to-speech/read-aloud helpers are implemented together near the end of this file.
static void ReadAloudClearSourceTab();
static void ReadAloudContinueInTab(WindowTab* tab);
static void ReadAloudFromViewportTopInTab(WindowTab* tab);
static void ReadAloudInTab(WindowTab* tab);
static void ReadAloudSelectionInTab(WindowTab* tab);
static void ReadAloudStopRememberPos();
static void ResetReadAloudStateForTab(WindowTab* tab);
static void StopReadAloudIfSourceTab(WindowTab* tab);
static void StopReadAloudIfSourceWindow(MainWindow* win);

// used to show it in debug, but is not very useful,
// so always disable

// in plugin mode, the window's frame isn't drawn and closing and
// fullscreen are disabled, so that SumatraPDF can be displayed
// embedded (e.g. in a web browser)
Str gPluginURL; // owned by Flags in WinMain
bool gMyWindowWasEmbedded = false;

bool NeedsWindowEmbeddingHacks() {
    return gMyWindowWasEmbedded || gPluginMode;
}

bool SettingsUseTabs() {
    return gGlobalPrefs->useTabs && !gMyWindowWasEmbedded;
}

bool SettingsRestoreSession() {
    return gGlobalPrefs->restoreSession && !gMyWindowWasEmbedded && !gForTesting;
}

bool SettingsRememberOpenedFiles() {
    return gGlobalPrefs->rememberOpenedFiles && !gMyWindowWasEmbedded;
}

static Kind kNotifPersistentWarning = "persistentWarning";
static Kind kNotifDocErrors = "docErrors";
static Kind kNotifZoomOrView = "zoomOrView";

HBITMAP gBitmapReloadingCue;
RenderCache* gRenderCache;
HCURSOR gCursorDrag;

// set after mouse shortcuts involving the Alt key (so that the menu bar isn't activated)
bool gSupressNextAltMenuTrigger = false;

bool gCrashOnOpen = false;
bool gRedrawLog = false;

// returns false when the relayout was skipped (nothing layout-affecting changed)
static bool RelayoutFrame(MainWindow* win, bool updateToolbars = true, int sidebarDx = -1);
static void UpdateOverlayScrollbarPositions(MainWindow* win);

static Str HwndName(HWND hwnd) {
    WCHAR cls[64]{};
    GetClassNameW(hwnd, cls, dimof(cls));
    if (wstr::Eq(cls, FRAME_CLASS_NAME)) {
        return "frame";
    }
    if (wstr::Eq(cls, CANVAS_CLASS_NAME)) {
        return "canvas";
    }
    // TODO: could identify more windows (rebar, toc, etc.)
    return "other";
}

static void LogRedraw(Str what, HWND hwnd, const RECT* rc = nullptr) {
    if (!gRedrawLog) {
        return;
    }
    if (rc) {
        logf("redraw: %s hwnd=0x%p (%s) rc=(%d,%d,%d,%d)\n", what, hwnd, HwndName(hwnd), rc->left, rc->top, rc->right,
             rc->bottom);
    } else {
        logf("redraw: %s hwnd=0x%p (%s)\n", what, hwnd, HwndName(hwnd));
    }
}

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

static Str gNextPrevDir = {};
static StrVec gNextPrevDirCache; // cached files in gNextPrevDir

static void CloseDocumentInCurrentTab(MainWindow* /*win*/, bool keepUIEnabled, bool deleteModel);
static void SetFrameTitleForTab(WindowTab* tab, bool needRefresh);
static void OnSidebarSplitterMove(VirtSplitter::MoveEvent* /*ev*/);
static void OnFavSplitterMove(VirtSplitter::MoveEvent* /*ev*/);

EBookUI* GetEBookUI() {
    if (!gGlobalPrefs) return nullptr;
    return &gGlobalPrefs->eBookUI;
}

LoadArgs::LoadArgs(Str origPath, MainWindow* win) {
    this->fileArgs = ParseFileArgs(origPath);
    Str cleanPath = origPath;
    if (fileArgs) {
        cleanPath = fileArgs->cleanPath;
        logf("LoadArgs: origPath='%s', cleanPath='%s'\n", origPath, cleanPath);
    }
    TempStr path = path::NormalizeTemp(cleanPath);
    if (!str::EqI(path, cleanPath)) {
        logf("LoadArgs: cleanPath='%s', path='%s'\n", cleanPath, path);
    }
    this->fileName = str::Dup(path);
    this->win = win;
}

LoadArgs::~LoadArgs() {
    // async load may leave an engine if the finish path never ran (e.g. tab
    // destroyed with pendingLoadArgs); never leave a leaked EngineBase
    SafeEngineRelease(&engine);
    delete fileArgs;
    str::Free(fileName);
    str::Free(displayName);
}

Str LoadArgs::FilePath() const {
    return fileName;
}

void LoadArgs::SetFilePath(Str path) {
    str::ReplaceWithCopy(&fileName, path);
}

Str LoadArgs::DisplayName() const {
    return displayName;
}

void LoadArgs::SetDisplayName(Str name) {
    str::ReplaceWithCopy(&displayName, name);
}

LoadArgs* LoadArgs::Clone() {
    LoadArgs* res = new LoadArgs(fileName, win);
    res->SetDisplayName(displayName);
    res->tabState = this->tabState;
    res->targetTab = this->targetTab;
    res->forceReuse = this->forceReuse;
    res->noSavePrefs = this->noSavePrefs;
    res->onFinished = this->onFinished;
    res->hwndPwdParent = this->hwndPwdParent;
    res->engine = this->engine;
    return res;
}

void SetCurrentLang(Str langCode) {
    if (!langCode) {
        return;
    }
    str::ReplaceWithCopy(&gGlobalPrefs->uiLanguage, langCode);
    trans::SetCurrentLangByCode(gGlobalPrefs->uiLanguage);
}

#define DEFAULT_FILE_PERCEIVED_TYPES "audio,video,webpage"
#define DEFAULT_LINK_PROTOCOLS "http,https,mailto,file"

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
    TempStr restrictPath = GetPathInExeDirTemp(kRestrictionsFileName);
    if (!file::Exists(restrictPath)) {
        Split(&gAllowedLinkProtocols, DEFAULT_LINK_PROTOCOLS, ",");
        Split(&gAllowedFileTypes, DEFAULT_FILE_PERCEIVED_TYPES, ",");
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
            Split(&gAllowedLinkProtocols, protocols, ",", true);
        }
        value = polsec->GetValue(StrL("SafeFileTypes"));
        if (value) {
            TempStr protocols = str::DupTemp(value);
            str::ToLowerInPlace(protocols);
            str::TransCharsInPlace(protocols, StrL(" :;"), StrL(",,,"));
            Split(&gAllowedFileTypes, protocols, ",", true);
        }
    }
}

void RestrictPolicies(Perm revokePermission) {
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
    if (gPluginMode) {
        // pass the URI back to the browser
        ReportIf(len(gWindows) == 0);
        if (len(gWindows) == 0) {
            return false;
        }
        HWND plugin = gWindows[0]->hwndFrame;
        HWND parent = GetAncestor(plugin, GA_PARENT);
        int urlLen = len(url);
        if (!parent || !url || (urlLen > 4096)) {
            return false;
        }
        TempStr urlZ = str::DupTemp(url);
        COPYDATASTRUCT cds = {0x4C5255 /* URL */, (DWORD)urlZ.len + 1, urlZ.s};
        return SendMessageW(parent, WM_COPYDATA, (WPARAM)plugin, (LPARAM)&cds);
    }

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
        return false;
    }

    return LaunchFileShell(url, nullptr, "open");
}

bool DocIsSupportedFileType(FileType kind) {
    if (EpubDoc::IsSupportedFileType(kind)) {
        return true;
    }
    if (Fb2Doc::IsSupportedFileType(kind)) {
        return true;
    }
    if (MobiDoc::IsSupportedFileType(kind)) {
        return true;
    }
    if (PalmDoc::IsSupportedFileType(kind)) {
        return true;
    }
    return false;
}

// lets the shell open a file of any supported perceived type
// in the default application for opening such files
bool OpenFileExternally(Str path) {
    if (!CanAccessDisk() || gPluginMode) {
        return false;
    }

    // check if this file's perceived type is allowed
    TempStr ext = path::GetExtTemp(path);
    TempStr perceivedType = ReadRegStrTemp(HKEY_CLASSES_ROOT, ext, "PerceivedType");
    // since we allow following hyperlinks, also allow opening local webpages
    if (str::EndsWithI(path, StrL(".htm")) || str::EndsWithI(path, StrL(".html")) ||
        str::EndsWithI(path, StrL(".xhtml"))) {
        perceivedType = str::DupTemp("webpage");
    }
    str::ToLowerInPlace(perceivedType);
    if (gAllowedFileTypes.Contains(StrL("*"))) {
        /* allow all file types (not recommended) */;
    } else if (!perceivedType || !gAllowedFileTypes.Contains(perceivedType)) {
        return false;
    }

    // TODO: only do this for trusted files (cf. IsUntrustedFile)?
    return LaunchFileShell(path);
}

void SwitchToDisplayMode(MainWindow* win, DisplayMode displayMode, bool keepContinuous) {
    if (!win->IsDocLoaded()) {
        return;
    }

    win->ctrl->SetDisplayMode(displayMode, keepContinuous);
    UpdateToolbarState(win);
}

WindowTab* FindTabByController(DocController* ctrl) {
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (tab->ctrl == ctrl) {
                return tab;
            }
        }
    }
    return nullptr;
}

static WindowTab* FindTabByFileInWindow(Str file, MainWindow* win) {
    TempStr normFile = path::NormalizeTemp(file);
    for (WindowTab* tab : win->Tabs()) {
        if (tab->type != WindowTab::Type::Document) {
            continue;
        }
        Str fp = tab->filePath;
        if (len(fp) == 0) {
            continue;
        }
        if (path::IsSame(fp, normFile)) {
            return tab;
        }
    }
    return nullptr;
}

WindowTab* FindTabByFile(Str file, MainWindow* limitWin) {
    if (limitWin) {
        return FindTabByFileInWindow(file, limitWin);
    }
    for (MainWindow* win : gWindows) {
        if (WindowTab* tab = FindTabByFileInWindow(file, win)) {
            return tab;
        }
    }
    return nullptr;
}

// ok for tab to be null
void SelectTabInWindow(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    auto* win = tab->win;
    if (tab == win->CurrentTab()) {
        return;
    }
    TabsSelect(win, win->GetTabIdx(tab));
}

// Find the first window showing a given PDF file
// note: background tabs are only searched if focusTab is true
// when limitWin is set, only that window's tabs are considered
MainWindow* FindMainWindowByFile(Str file, bool focusTab, MainWindow* limitWin) {
    WindowTab* tab = nullptr;
    if (!file) {
        return nullptr;
    }
    if (!limitWin && gMostRecentlyOpenedDoc != nullptr) {
        auto lastPath = gMostRecentlyOpenedDoc->GetFilePath();
        if (path::IsSame(lastPath, file)) {
            tab = FindTabByController(gMostRecentlyOpenedDoc);
        }
    }
    if (!tab) {
        tab = FindTabByFile(file, limitWin);
    }
    if (!tab) {
        return nullptr;
    }
    if (focusTab) {
        SelectTabInWindow(tab);
    }
    return tab->win;
}

// Paths currently being loaded. Tabs are only created in LoadDocumentFinish, so
// without this set a second open for the same path (common when Explorer multi-
// opens password PDFs: cmdline load + reuseInstance DDE/COPYDATA during the
// password dialog message pump) would create a duplicate tab. UI thread only.
static StrVec gFilesLoading;

static int IndexOfLoadingFile(Str path) {
    if (!path) {
        return -1;
    }
    int n = len(gFilesLoading);
    // Fast path: entries are stored normalized, but the caller often already has
    // the same string (or only differs by case). Avoid NormalizeTemp / IsSame —
    // both can hit the network on UNC paths and stall the UI.
    for (int i = 0; i < n; i++) {
        if (str::EqI(gFilesLoading[i], path)) {
            return i;
        }
    }
    TempStr norm = path::NormalizeTemp(path);
    if (!norm || str::EqI(norm, path)) {
        return -1; // already compared as-is
    }
    for (int i = 0; i < n; i++) {
        if (str::EqI(gFilesLoading[i], norm)) {
            return i;
        }
    }
    return -1;
}

// True if a tab already shows this file, or a load for it is already in progress
// (tab is only created when load finishes, so mid-password / async loads need this).
bool IsDocumentOpenOrLoading(Str file) {
    if (!file) {
        return false;
    }
    if (FindTabByFile(file)) {
        return true;
    }
    return IndexOfLoadingFile(file) >= 0;
}

// Mark/unmark a path as currently loading. Call from the UI thread only.
void BeginDocumentLoad(Str file) {
    if (!file) {
        return;
    }
    if (IndexOfLoadingFile(file) >= 0) {
        return;
    }
    gFilesLoading.Append(path::NormalizeTemp(file));
}

void EndDocumentLoad(Str file) {
    int idx = IndexOfLoadingFile(file);
    if (idx >= 0) {
        gFilesLoading.RemoveAt(idx);
    }
}

// Find the first window that has been produced from <file>
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

static bool gShowPassword = false;

class HwndPasswordUI : public PasswordUI {
    HWND hwnd;
    int pwdIdx;
    bool triedCliPwd = false;

  public:
    explicit HwndPasswordUI(HWND hwnd) : hwnd(hwnd), pwdIdx(0) {}

    Str GetPassword(Str path, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) override;
};

/* Get password for a given path, can be nullptr if user cancelled the
   dialog box or if the encryption key has been filled in instead.
   Caller needs to free() the result. */
Str HwndPasswordUI::GetPassword(Str path, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) {
    FileState* fileFromHistory = FileHistoryFindByPath(path);
    if (fileFromHistory && fileFromHistory->decryptionKey && fileDigest && decryptionKeyOut) {
        TempStr fingerprint = str::MemToHexTemp(Str((const char*)fileDigest, 16));
        Str decryptionKey = fileFromHistory->decryptionKey;
        *saveKey = str::TrimPrefix(decryptionKey, fingerprint);
        if (*saveKey && str::HexToMem(decryptionKey, Str((char*)decryptionKeyOut, 32))) {
            return {};
        }
    }

    *saveKey = false;

    if (!triedCliPwd && gCli && gCli->password) {
        triedCliPwd = true;
        return str::Dup(gCli->password);
    }

    // try the list of default passwords before asking the user
    if (pwdIdx < len(*gGlobalPrefs->defaultPasswords)) {
        Str pwd = (*gGlobalPrefs->defaultPasswords)[pwdIdx++];
        return str::Dup(pwd);
    }

    if (IsStressTesting()) {
        return {};
    }

    // can't show a dialog (e.g. thumbnail generation thread)
    if (!hwnd) {
        return {};
    }

    // extract the filename from the URL in plugin mode instead
    // of using the more confusing temporary filename
    if (gPluginMode) {
        TempStr urlName = url::GetFileNameTemp(gPluginURL);
        if (urlName) {
            path = urlName;
        }
    }
    path = path::GetBaseNameTemp(Str(path));

    // check if the window is still valid as it might have been closed by now
    if (!IsWindow(hwnd)) {
        ReportIf(true);
        hwnd = GetForegroundWindow();
    }
    // make sure that the password dialog is visible
    HwndToForeground(hwnd);

    // remembering the password requires saving per-document state
    bool canRememberPwd = SettingsRememberOpenedFiles() && gGlobalPrefs->rememberStatePerDocument;
    bool* rememberPwd = canRememberPwd ? saveKey : nullptr;
    return ShowGetPasswordDialog(hwnd, path, rememberPwd, &gShowPassword);
}

// True while a tab is mid-load (async open). Used so we don't treat a plain
// home/empty window as "still loading" for WindowState bookkeeping.
static bool WindowHasDocumentLoading(MainWindow* win) {
    if (!win) {
        return false;
    }
    for (WindowTab* tab : win->Tabs()) {
        if (tab->loadState == WindowTab::LoadState::Loading || tab->loadState == WindowTab::LoadState::LoadedPending) {
            return true;
        }
    }
    return false;
}

// update global windowState for next default launch when either
// no pdf is opened or a document without window dimension information
void RememberDefaultWindowPosition(MainWindow* win) {
    // ignore spurious WM_SIZE and WM_MOVE messages happening during initialization
    if (!HwndIsVisible(win->hwndFrame)) {
        return;
    }

    // While a document is still loading, the frame may be briefly shown at the
    // restored (non-maximized) WindowPos size before SW_MAXIMIZE/fullscreen is
    // applied. Do not persist that transient size over a maximized/fullscreen
    // WindowState preference (fixes #5529). Only apply this while a tab is
    // actually loading — a home/empty window must still record the user's
    // normal (non-maximized) size when they resize or close it.
    if (!win->IsDocLoaded() && WindowHasDocumentLoading(win)) {
        int intended = gGlobalPrefs->windowState;
        if (intended == WIN_STATE_MAXIMIZED && !IsZoomed(win->hwndFrame)) {
            return;
        }
        if (intended == WIN_STATE_FULLSCREEN && !win->isFullScreen) {
            return;
        }
    }

    if (win->presentation) {
        gGlobalPrefs->windowState = win->windowStateBeforePresentation;
    } else if (win->isFullScreen) {
        gGlobalPrefs->windowState = WIN_STATE_FULLSCREEN;
    } else if (IsZoomed(win->hwndFrame)) {
        gGlobalPrefs->windowState = WIN_STATE_MAXIMIZED;
    } else if (!IsIconic(win->hwndFrame)) {
        gGlobalPrefs->windowState = WIN_STATE_NORMAL;
    }

    // win->sidebarDx is the layout's source of truth; the toc box rect is
    // stale when the sidebar is hidden or only favorites are showing
    gGlobalPrefs->sidebarDx = win->sidebarDx > 0 ? win->sidebarDx : HwndWindowRect(win->hwndTocBox).dx;

    if (IsIconic(win->hwndFrame) || win->presentation) {
        return;
    }

    if (WIN_STATE_NORMAL == gGlobalPrefs->windowState) {
        gGlobalPrefs->windowPos = HwndWindowRect(win->hwndFrame);
    } else if (WIN_STATE_MAXIMIZED == gGlobalPrefs->windowState) {
        // use GetWindowPlacement to get the non-maximized position
        // so we know which monitor the window is on (for #5277)
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        if (GetWindowPlacement(win->hwndFrame, &wp)) {
            gGlobalPrefs->windowPos = ToRect(wp.rcNormalPosition);
        }
    }
}

static void UpdateDisplayStateWindowRect(MainWindow* win, FileState* fs, bool updateGlobal = true) {
    if (updateGlobal) {
        RememberDefaultWindowPosition(win);
    }

    fs->windowState = gGlobalPrefs->windowState;
    fs->windowPos = gGlobalPrefs->windowPos;
    fs->sidebarDx = gGlobalPrefs->sidebarDx;
}

static void UpdateSidebarDisplayState(WindowTab* tab, FileState* fs) {
    ReportIf(!tab);
    MainWindow* win = tab->win;
    fs->showToc = tab->showToc;
    if (win->tocLoaded && tab == win->CurrentTab()) {
        TocTree* tocTree = tab->ctrl->GetToc();
        UpdateTocExpansionState(tab->tocState, win->tocTreeView, tocTree);
    }
    *fs->tocState = tab->tocState;
}

void UpdateTabFileDisplayStateForTab(WindowTab* tab) {
    if (!tab || !tab->ctrl) {
        return;
    }
    MainWindow* win = tab->win;
    // TODO: this is called multiple times for each tab
    RememberDefaultWindowPosition(win);
    Str fp = tab->filePath;
    FileState* fs = FileHistoryFindByPath(fp);
    if (!fs) {
        return;
    }
    tab->ctrl->GetDisplayState(fs);
    UpdateDisplayStateWindowRect(win, fs, false);
    UpdateSidebarDisplayState(tab, fs);
}

static bool gForceRtl = false;

bool IsUIRtl() {
    if (gForceRtl) {
        return true;
    }
    return trans::IsCurrLangRtl();
}

uint MbRtlReadingMaybe() {
    if (IsUIRtl()) {
        return MB_RTLREADING;
    }
    return 0;
}

void MessageBoxWarning(HWND hwnd, Str msg, Str title) {
    uint type = MB_OK | MB_ICONEXCLAMATION | MbRtlReadingMaybe();
    if (!title) {
        title = _TRA("Warning");
    }
    MsgBox(hwnd, msg, title, type);
}

static BOOL CALLBACK SetRtlCallback(HWND hwnd, LPARAM lParam) {
    HwndSetRtl(hwnd, (bool)lParam);
    return TRUE;
}

// updates the layout for a window to either left-to-right or right-to-left
// depending on the currently used language (see IsUIRtl)
static void UpdateWindowRtlLayout(MainWindow* win) {
    bool wasRTL = HwndIsRtl(win->hwndFrame);
    bool isRTL = IsUIRtl();
    if (wasRTL == isRTL) {
        return;
    }

    // https://www.microsoft.com/middleeast/msdn/mirror.aspx
    HwndSetRtl(win->hwndFrame, isRTL);
    EnumChildWindows(win->hwndFrame, SetRtlCallback, (LPARAM)isRTL);

    // https://github.com/sumatrapdfreader/sumatrapdf/issues/5326
    // Rtl reverses mouse positions on x-axis which messes up
    // identification of elements on page
    // I could 1. UnmirrorRtl() or 2. make canvas always non-rtl
    // for now chose 2
    HwndSetRtl(win->hwndCanvas, false);
    // tabs use LTR hwnd coords for painting/hit-testing; RTL tab order follows parent
    if (win->tabsCtrl) {
        HwndSetRtl(win->tabsCtrl->hwnd, false);
    }

    bool tocVisible = win->uiState.tocVisible;
    bool favVisible = gGlobalPrefs->showFavorites;
    if (tocVisible || favVisible) {
        SetSidebarVisibility(win, false, false);
    }

    if (win->tabsCtrl) win->tabsCtrl->LayoutTabs();

    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE);
    RelayoutCaption(win);

    RelayoutNotifications(win->hwndCanvas);

    // TODO: also update the canvas scrollbars (?)

    // ensure that the ToC sidebar is on the correct side and that its
    // title and close button are also correctly laid out
    if (tocVisible || favVisible) {
        SetSidebarVisibility(win, tocVisible, favVisible);
        if (tocVisible) {
            SendMessageW(win->hwndTocBox, WM_SIZE, 0, 0);
        }
        if (favVisible) {
            SendMessageW(win->hwndFavBox, WM_SIZE, 0, 0);
        }
    }
    ReCreateToolbar(win);
    // RTL is not part of the layout snapshot; force a full relayout and repaint
    win->uiState.layout = {};
    ScheduleUiUpdate(win);
    uint redrawFlags = RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW;
    RedrawWindow(win->hwndFrame, nullptr, nullptr, redrawFlags);
}

static bool IsMenubarVisible() {
    if (SettingsUseTabs()) {
        return gGlobalPrefs->showMenubarWithTabs;
    }
    return gGlobalPrefs->showMenubar;
}

static bool MenuBarButtonsNeedRebuild(HMENU oldMenu, HMENU newMenu) {
    int oldCount = oldMenu ? GetMenuItemCount(oldMenu) : 0;
    int newCount = newMenu ? GetMenuItemCount(newMenu) : 0;
    if (oldCount != newCount) {
        return true;
    }
    MENUITEMINFOW oldMii{};
    oldMii.cbSize = sizeof(MENUITEMINFOW);
    oldMii.fMask = MIIM_SUBMENU | MIIM_STRING;
    MENUITEMINFOW newMii = oldMii;
    for (int i = 0; i < newCount; i++) {
        oldMii.dwTypeData = nullptr;
        oldMii.cch = 0;
        newMii.dwTypeData = nullptr;
        newMii.cch = 0;
        GetMenuItemInfoW(oldMenu, i, TRUE, &oldMii);
        GetMenuItemInfoW(newMenu, i, TRUE, &newMii);
        if (!!oldMii.hSubMenu != !!newMii.hSubMenu || oldMii.cch != newMii.cch) {
            return true;
        }
        if (oldMii.cch == 0) {
            continue;
        }

        oldMii.cch++;
        newMii.cch++;
        WCHAR* oldName = AllocArrayTemp<WCHAR>((int)oldMii.cch);
        WCHAR* newName = AllocArrayTemp<WCHAR>((int)newMii.cch);
        oldMii.dwTypeData = oldName;
        newMii.dwTypeData = newName;
        GetMenuItemInfoW(oldMenu, i, TRUE, &oldMii);
        GetMenuItemInfoW(newMenu, i, TRUE, &newMii);
        if (!wstr::Eq(WStr(oldName), WStr(newName))) {
            return true;
        }
    }
    return false;
}

// After the menu changes (e.g. home page -> document), repaint the menu bar so
// stale caption/rebar pixels are not left behind (issue #5763).
static void RedrawMenuBarForWindow(MainWindow* win) {
    if (!win || win->presentation || win->isFullScreen || !IsMenubarVisible()) {
        return;
    }
    if (win->tabsInTitlebar) {
        if (!IsShowingMenuBarRebar(win)) {
            return;
        }
        RelayoutCaption(win);
        RECT r = ToRECT(win->captionRect);
        HwndInvalidateRect(win->hwndFrame, win->captionRect, true);
        RedrawWindow(win->hwndFrame, &r, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
        if (win->hwndMenuReBar) {
            RedrawWindow(win->hwndMenuReBar, nullptr, nullptr,
                         RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        }
        if (win->tabsCtrl && win->tabsCtrl->IsVisible()) {
            RedrawWindow(win->tabsCtrl->hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
        }
    } else if (GetMenu(win->hwndFrame)) {
        DrawMenuBar(win->hwndFrame);
    }
}

void RebuildMenuBarForWindow(MainWindow* win) {
    HMENU oldMenu = win->menu;
    win->menu = BuildMenu(win);
    bool redrawMenuBar = false;
    if (!win->presentation && !win->isFullScreen && IsMenubarVisible()) {
        if (win->tabsInTitlebar) {
            // use rebar menu bar instead of native menu when tabs are in titlebar
            if (IsShowingMenuBarRebar(win)) {
                if (MenuBarButtonsNeedRebuild(oldMenu, win->menu)) {
                    RebuildMenuBarButtons(win);
                    redrawMenuBar = true;
                }
            }
        } else {
            SetMenu(win->hwndFrame, win->menu);
            redrawMenuBar = true;
        }
    }
    FreeMenuOwnerDrawInfoData(oldMenu);
    DestroyMenu(oldMenu);
    if (redrawMenuBar) {
        RedrawMenuBarForWindow(win);
    }
}

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
    if (gGlobalPrefs->homePageSortByFrequentlyRead) {
        FileHistoryGetFrequencyOrder(list);
    } else {
        FileHistoryGetRecentlyOpenedOrder(list);
    }
    int idx = list.Find(ds);
    if (idx < 0) {
        return false;
    }

    if (HasThumbnail(ds)) {
        return false;
    }
    return true;
}

struct ControllerCallbackHandler : DocControllerCallback {
    MainWindow* win{nullptr};

  public:
    explicit ControllerCallbackHandler(MainWindow* win) : win(win) {}
    ~ControllerCallbackHandler() override = default;

    void Repaint() override { ScheduleRepaint(win, 0); }
    void PageNoChanged(DocController* ctrl, int pageNo) override;
    void ZoomChanged(DocController* ctrl, float zoomVirtual) override;
    void UpdateScrollbars(Size canvas) override;
    void RequestRendering(DisplayModel* dm, int pageNo) override;
    void RequestPredictiveRendering(DisplayModel* dm, int originPageNo, const int* pages, int nPages) override;
    void CleanUp(DisplayModel* dm) override;
    void RenderThumbnail(DisplayModel* dm, Size size, const OnBitmapRendered* /*saveThumbnail*/) override;
    void GotoLink(IPageDestination* dest) override { win->linkHandler->GotoLink(dest); }
    void FocusFrame(bool always) override;
    void SaveDownload(Str url, Str /*data*/) override;
    void FindResultReceived(int gen, int current, int total) override {
        BrowserFindResultReceived(win, gen, current, total);
    }
    void FindAllResultReceived(Str payload) override { BrowserFindAllResultReceived(win, payload); }
    void TocChanged(DocController* ctrl) override;
};

void ControllerCallbackHandler::TocChanged(DocController* ctrl) {
    WindowTab* tab = FindTabByController(ctrl);
    if (!tab) {
        return;
    }
    ReloadTocTree(tab);
}

DocControllerCallback* CreateControllerCallbackHandler(MainWindow* win) {
    return new ControllerCallbackHandler(win);
}

struct ThumbnailRenderData {
    const OnBitmapRendered* saveThumbnail = nullptr;
};

static void ThumbnailRenderFinished(ThumbnailRenderData* d, PageRenderRequest* req) {
    // extract bitmap from request and pass to original callback
    // the callback takes ownership of the bitmap (the present-layer handle)
    RenderedBitmap* bmp = RenderedBitmapFromPixmap(req->bmp);
    req->bmp = nullptr; // prevent double-free
    d->saveThumbnail->Call(bmp);
    delete d->saveThumbnail;
    delete d;
}

void ControllerCallbackHandler::RenderThumbnail(DisplayModel* dm, Size size, const OnBitmapRendered* saveThumbnail) {
    auto* engine = dm->GetEngine();
    RectF pageRect = engine->PageMediabox(1);
    if (pageRect.IsEmpty()) {
        // saveThumbnail must always be called for clean-up code
        saveThumbnail->Call(nullptr);
        return;
    }

    pageRect = engine->Transform(pageRect, 1, 1.0f, 0);
    float zoom = (float)size.dx / pageRect.dx;
    pageRect.dy = std::min(pageRect.dy, (float)size.dy / zoom);
    pageRect = engine->Transform(pageRect, 1, 1.0f, 0, true);

    // always render thumbnails with anti-aliasing for quality
    bool savedAntiAlias = engine->disableAntiAlias;
    engine->disableAntiAlias = false;

    auto* td = new ThumbnailRenderData();
    td->saveThumbnail = saveThumbnail;
    auto cb = MkFunc1(ThumbnailRenderFinished, td);
    gRenderCache->Render(dm, 1, 0, zoom, pageRect, cb);
    engine->disableAntiAlias = savedAntiAlias;
}

struct CreateThumbnailFromFileData {
    Str filePath;
    Pixmap* bmp = nullptr;
    ~CreateThumbnailFromFileData() {
        str::Free(filePath);
        FreePixmap(bmp);
    }
};

static void CreateThumbnailFromFileFinish(CreateThumbnailFromFileData* d) {
    if (d->bmp) {
        FileState* fs = FileHistoryFindByPath(d->filePath);
        SetThumbnail(fs, d->bmp);
        d->bmp = nullptr;
    }
    delete d;
}

static void CreateThumbnailFromFileThread(CreateThumbnailFromFileData* d) {
    HwndPasswordUI pwdUI(nullptr);
    EngineBase* engine = CreateEngineFromFile(d->filePath, &pwdUI, true);
    if (!engine) {
        delete d;
        return;
    }
    RectF pageRect = engine->PageMediabox(1);
    if (pageRect.IsEmpty()) {
        engine->Release();
        delete d;
        return;
    }
    pageRect = engine->Transform(pageRect, 1, 1.0f, 0);
    float zoom = (float)kThumbnailDx / pageRect.dx;
    pageRect.dy = std::min(pageRect.dy, (float)kThumbnailDy / zoom);
    pageRect = engine->Transform(pageRect, 1, 1.0f, 0, true);
    RenderPageArgs args(1, zoom, 0, &pageRect);
    d->bmp = engine->RenderPage(args);
    engine->Release();
    auto fn = MkFunc0<CreateThumbnailFromFileData>(CreateThumbnailFromFileFinish, d);
    uitask::Post(fn, "SetThumbnailFromFile");
}

// create a thumbnail by loading the file with a temporary engine
// used for lazy-loaded files that don't have a loaded controller
static void CreateThumbnailFromFileAsync(FileState* ds) {
    auto* d = new CreateThumbnailFromFileData();
    d->filePath = str::Dup(ds->filePath);
    auto fn = MkFunc0<CreateThumbnailFromFileData>(CreateThumbnailFromFileThread, d);
    RunAsync(fn, "CreateThumbnailFromFile");
}

static void CreateThumbnailForFile(MainWindow* win, FileState* ds) {
    if (!ShouldSaveThumbnail(ds)) {
        return;
    }

    // don't create thumbnails for password protected documents
    // (unless we're also remembering the decryption key anyway)
    if (win->IsDocLoaded()) {
        auto* model = win->AsFixed();
        if (model) {
            auto* engine = model->GetEngine();
            bool withPwd = engine->IsPasswordProtected();
            Str decrKey = engine->decryptionKey;
            if (withPwd && !decrKey) {
                RemoveThumbnail(ds);
                return;
            }
            // save decryption key to file history so the thumbnail thread can use it
            if (decrKey && !str::Eq(ds->decryptionKey, decrKey)) {
                str::ReplaceWithCopy(&ds->decryptionKey, decrKey);
            }
        }
    }

    // always use file-based async thumbnail creation; it's independent
    // of the tab lifecycle so it works even if the tab is closed before
    // the render completes
    CreateThumbnailFromFileAsync(ds);
}

/* Send the request to render a given page to a rendering thread */
void ControllerCallbackHandler::RequestRendering(DisplayModel* dm, int pageNo) {
    ReportIf(!dm);
    if (!dm) {
        return;
    }
    // don't render any plain images on the rendering thread,
    // they'll be rendered directly in DrawDocument during
    // WM_PAINT on the UI thread
    if (dm->ShouldCacheRendering(pageNo)) {
        gRenderCache->RequestRendering(dm, pageNo);
    }
}

void ControllerCallbackHandler::RequestPredictiveRendering(DisplayModel* dm, int originPageNo, const int* pages,
                                                           int nPages) {
    ReportIf(!dm);
    if (!dm) {
        return;
    }
    gRenderCache->RequestPredictiveRendering(dm, originPageNo, pages, nPages);
}

void ControllerCallbackHandler::CleanUp(DisplayModel* dm) {
    gRenderCache->CancelRenderingBlocking(dm);
    gRenderCache->FreeForDisplayModel(dm);
}

// ~DisplayModel waits for in-flight renders (CleanUp -> CancelRenderingBlocking)
// because a render thread keeps using the model and its engine until the current
// page is done, and mupdf only checks the abort cookie between display-list ops
// -- one image decode isn't interruptible. Doing that wait on the UI thread froze
// the app for the length of the decode on every tab close.
//
// So hand the model to a scratch thread that does the waiting, and delete it back
// on the UI thread once no render is using it: the delete itself keeps running on
// the UI thread (it touches gRenderCache and the engine), only the waiting moves
// off it. The one thing that can't survive the delay is the model's callback
// handler -- see DeleteControllerFinish().
static AtomicInt gPendingControllerDeletes;

static void DeleteControllerFinish(DocController* ctrl) {
    DisplayModel* dm = ctrl->AsFixed();
    if (dm) {
        // ctrl->cb is the MainWindow's ControllerCallbackHandler and the window
        // can be closed while we're waiting, so don't let ~DisplayModel call
        // into it (cf. DeleteOrphanedController). Do CleanUp()'s work here
        // instead: the renders are already done, this just drops the cache.
        gRenderCache->FreeForDisplayModel(dm);
        ctrl->cb = nullptr;
    }
    delete ctrl;
    AtomicIntDec(&gPendingControllerDeletes);
}

static void WaitForRendersThenDelete(DocController* ctrl) {
    // blocking wait, but we're not the UI thread
    gRenderCache->CancelRenderingBlocking(ctrl->AsFixed());
    auto fn = MkFunc0<DocController>(DeleteControllerFinish, ctrl);
    uitask::Post(fn, "DeleteControllerFinish");
}

void DeleteControllerAsync(DocController* ctrl) {
    if (!ctrl) {
        return;
    }
    DisplayModel* dm = ctrl->AsFixed();
    if (!dm) {
        // only DisplayModel is rendered by the render threads
        delete ctrl;
        return;
    }
    // no new requests for this model from here on
    dm->pauseRendering = true;
    gRenderCache->AbortRendering(dm);
    if (!gRenderCache->IsRenderingFor(dm)) {
        // nothing to wait for, the common case
        delete ctrl;
        return;
    }
    AtomicIntInc(&gPendingControllerDeletes);
    auto fn = MkFunc0<DocController>(WaitForRendersThenDelete, ctrl);
    if (!StartThread(fn, "DeleteController")) {
        // couldn't spawn: fall back to deleting (and waiting) right here
        AtomicIntDec(&gPendingControllerDeletes);
        delete ctrl;
    }
}

// Called during shutdown, before the render cache goes away: the scratch threads
// use gRenderCache and their deletes are queued as ui tasks.
void WaitForPendingControllerDeletes() {
    while (AtomicIntGet(&gPendingControllerDeletes) > 0) {
        uitask::DrainQueue();
        Sleep(10);
    }
    uitask::DrainQueue();
}

void ControllerCallbackHandler::FocusFrame(bool always) {
    if (always || !FindMainWindowByHwnd(GetFocus())) {
        HwndSetFocus(win->hwndFrame);
    }
}

void ControllerCallbackHandler::SaveDownload(Str url, Str data) {
    TempStr path = url::GetFileNameTemp(url);
    // LinkSaver linkSaver(win->CurrentTab(), win->hwndFrame, fileName);
    SaveDataToFile(win->hwndFrame, path, data);
}

static void makeFullScrollbar(SCROLLINFO& si) {
    si.nPos = 0;
    si.nMin = 0;
    si.nMax = 99;
    si.nPage = 100;
}

SeqStrings gScrollbarModeNames = "windows\0smart\0overlay\0hidden\0";

int ScrollbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gScrollbarModeNames, gGlobalPrefs->scrollbars);
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

OverlayScrollbar::Mode ScrollbarsOverlayMode() {
    if (ScrollbarModeFromPrefs() == kScrollbarOverlay) {
        return OverlayScrollbar::Mode::Thick;
    }
    return OverlayScrollbar::Mode::Smart;
}

SeqStrings gToolbarModeNames = "show\0hide\0overlay\0";

int ToolbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gToolbarModeNames, gGlobalPrefs->toolbar);
    if (idx < 0) {
        // not set / invalid: derive from the legacy showToolbar bool
        idx = gGlobalPrefs->showToolbar ? kToolbarShow : kToolbarHide;
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
    if (!name) {
        name = "show";
        mode = kToolbarShow;
    }
    str::ReplaceWithCopy(&gGlobalPrefs->toolbar, name);
    // keep the legacy bool in sync so old versions stay sane
    gGlobalPrefs->showToolbar = (mode != kToolbarHide);
}

int FullscreenToolbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gToolbarModeNames, gGlobalPrefs->fullscreen.toolbar);
    if (idx < 0) {
        // not set / invalid: derive from the legacy Fullscreen.ShowToolbar bool
        idx = gGlobalPrefs->fullscreen.showToolbar ? kToolbarShow : kToolbarHide;
    }
    return idx;
}

void SetFullscreenToolbarMode(int mode) {
    Str name = SeqStrByIndex(gToolbarModeNames, mode);
    if (!name) {
        name = "hide";
        mode = kToolbarHide;
    }
    str::ReplaceWithCopy(&gGlobalPrefs->fullscreen.toolbar, name);
    gGlobalPrefs->fullscreen.showToolbar = (mode != kToolbarHide);
}

SeqStrings gToolbarPositionNames = "top\0bottom\0";

int ToolbarPositionFromPrefs() {
    int idx = SeqStrIndexIS(gToolbarPositionNames, gGlobalPrefs->toolbarPosition);
    if (idx < 0) {
        idx = kToolbarTop;
    }
    return idx;
}

bool ToolbarAtBottom() {
    return ToolbarPositionFromPrefs() == kToolbarBottom;
}

// Reverse the content-row HBox so the sidebar is on the right. RTL frames
// already mirror via WS_EX_LAYOUTRTL, so don't also reverse the HBox.
static bool SidebarOnRightLayout() {
    return gGlobalPrefs && gGlobalPrefs->sidebarOnRight && !IsUIRtl();
}

void ControllerCallbackHandler::UpdateScrollbars(Size canvas) {
    ReportIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();

    // called on every viewport change, so this is where scrolling is noticed;
    // the recompute itself is debounced
    KeyboardLinkFollowingViewportChanged(win);

    bool hideScrollbar = ScrollbarsAreHidden();
    bool useOverlay = ScrollbarsUseOverlay();
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;

    Size viewPort = dm->GetViewPort().Size();

    if (viewPort.dx >= canvas.dx) {
        makeFullScrollbar(si);
    } else {
        si.nPos = dm->GetViewPort().x;
        si.nMin = 0;
        si.nMax = canvas.dx - 1;
        si.nPage = viewPort.dx;
    }

    bool showHScroll = (viewPort.dx < canvas.dx) && !hideScrollbar;
    if (useOverlay) {
        SetScrollInfo(win->hwndCanvas, SB_HORZ, &si, FALSE);
        // SetScrollInfo's last arg is redraw, not visibility, and a non-empty
        // range re-shows the window scrollbar -- hide it explicitly so overlay
        // mode doesn't show both the window scrollbar and the overlay one
        ShowScrollBar(win->hwndCanvas, SB_HORZ, FALSE);
        if (!win->overlayScrollH) {
            win->overlayScrollH =
                OverlayScrollbarCreate(win->hwndCanvas, OverlayScrollbar::Type::Horz, ScrollbarsOverlayMode());
        }
        if (showHScroll) {
            OverlayScrollbarShow(win->overlayScrollH, true);
            OverlayScrollbarSetInfo(win->overlayScrollH, &si, TRUE);
        } else {
            OverlayScrollbarShow(win->overlayScrollH, false);
        }
    } else {
        // Set range/page before showing so first paint has a thumb (issue #5850)
        SetScrollInfo(win->hwndCanvas, SB_HORZ, &si, TRUE);
        ShowScrollBar(win->hwndCanvas, SB_HORZ, showHScroll);
    }

    bool isSinglePageMode = gGlobalPrefs->scrollbarInSinglePage && (dm->GetDisplayMode() == DisplayMode::SinglePage);
    bool showVScroll = true;
    if (isSinglePageMode) {
        int pageCount = dm->PageCount();
        int currentPage = dm->CurrentPageNo();
        si.nPos = currentPage - 1; // 0-based position
        si.nMin = 0;
        si.nMax = pageCount - 1; // 0-based max
        si.nPage = 1;            // One page visible at a time
    } else {
        if (viewPort.dy >= canvas.dy) {
            makeFullScrollbar(si);
        } else {
            si.nPos = dm->GetViewPort().y;
            si.nMin = 0;
            si.nMax = canvas.dy - 1;
            si.nPage = viewPort.dy;

            if (kZoomFitPage != dm->GetZoomVirtual()) {
                // keep the top/bottom 5% of the previous page visible after paging down/up
                si.nPage = (uint)(si.nPage * 0.95);
                si.nMax -= viewPort.dy - (int)si.nPage;
            }
        }
        showVScroll = (viewPort.dy < canvas.dy);
    }
    bool showScrollbar = !hideScrollbar;
    BOOL showWinScrollbar = showScrollbar && !useOverlay;

    if (useOverlay || hideScrollbar) {
        SetScrollInfo(win->hwndCanvas, SB_VERT, &si, FALSE);
        // hide the window scrollbar explicitly (see SB_HORZ note above)
        ShowScrollBar(win->hwndCanvas, SB_VERT, FALSE);
    } else {
        // SetScrollInfo first so the NC area is not a blank strip on first show
        // (ShowScrollBar alone can paint an empty white track — issue #5850).
        DWORD styleBefore = (DWORD)GetWindowLongW(win->hwndCanvas, GWL_STYLE);
        bool wantV = showWinScrollbar && showVScroll;
        SetScrollInfo(win->hwndCanvas, SB_VERT, &si, TRUE);
        ShowScrollBar(win->hwndCanvas, SB_VERT, wantV);
        // Dark scrollbar theme is applied at canvas create / theme change —
        // not on every UpdateScrollbars. SetWindowTheme mid-update re-enters
        // uxtheme/comctl32 and can AV (crash 8c1831c15000001).
        // Only force a frame repaint when the bar newly appears; SetScrollInfo
        // already redraws the thumb on position changes. This is a no-op while
        // the frame is hidden (including the WM_SETREDRAW FALSE window inside
        // RelayoutFrame, which clears WS_VISIBLE) — RelayoutFrame repeats it
        // with RDW_FRAME once redrawing is back on (issue #5850).
        if (wantV) {
            DWORD styleAfter = (DWORD)GetWindowLongW(win->hwndCanvas, GWL_STYLE);
            bool newlyShown = !(styleBefore & WS_VSCROLL) && (styleAfter & WS_VSCROLL);
            if (newlyShown) {
                RedrawWindow(win->hwndCanvas, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
            }
        }
    }

    if (useOverlay) {
        if (!win->overlayScrollV) {
            win->overlayScrollV =
                OverlayScrollbarCreate(win->hwndCanvas, OverlayScrollbar::Type::Vert, ScrollbarsOverlayMode());
        }
        if (showVScroll && showScrollbar) {
            OverlayScrollbarShow(win->overlayScrollV, true);
            OverlayScrollbarSetInfo(win->overlayScrollV, &si, TRUE);
        } else {
            OverlayScrollbarShow(win->overlayScrollV, false);
        }
    }
}

static TempStr BuildZoomString(float zoomLevel) {
    TempStr zoomLevelStr = ZoomLevelStr(zoomLevel);
    Str zoomStr = _TRA("Zoom");
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

// Light separator between page-info values: space + U+00B7 MIDDLE DOT + space.
#define PAGE_INFO_SEP " \xC2\xB7 "

static void UpdatePageInfoHelper(DocController* ctrl, NotificationWnd* wnd, int pageNo) {
    if (!ctrl->ValidPageNo(pageNo)) {
        pageNo = ctrl->CurrentPageNo();
    }
    int nPages = ctrl->PageCount();
    TempStr pageInfo = fmt("%s %d / %d", _TRA("Page:"), pageNo, nPages);
    if (ctrl->HasPageLabels()) {
        TempStr label = ctrl->GetPageLabeTemp(pageNo);
        pageInfo = fmt("%s %s (%d / %d)", _TRA("Page:"), label, pageNo, nPages);
    }
    float zoomLevel = ctrl->GetZoomVirtual();
    auto zoomStr = BuildZoomString(zoomLevel);
    pageInfo = str::JoinTemp(pageInfo, StrL(PAGE_INFO_SEP), zoomStr);

    // Image extras (issue #4456). Document file name is already on the tab.
    DisplayModel* dm = ctrl->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine && IsEngineImages(engine)) {
        if (engine->kind == kindEngineImage) {
            // Single image file (or multi-frame TIFF/GIF): resolution · size · non-default DPI
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
                detail = detail ? fmt("%s%s%s", detail, StrL(PAGE_INFO_SEP), sizeStr) : sizeStr;
            }
            // fileDPI defaults to 96; only show when the image reports something else
            float dpi = engine->GetFileDPI();
            if (dpi > 0.5f && fabsf(dpi - 96.0f) > 0.5f) {
                TempStr dpiStr = fmt("%.0f DPI", dpi);
                detail = detail ? fmt("%s%s%s", detail, StrL(PAGE_INFO_SEP), dpiStr) : dpiStr;
            }
            if (detail) {
                pageInfo = str::JoinTemp(pageInfo, StrL(PAGE_INFO_SEP), detail);
            }
        } else {
            // Comic / image folder: per-page name · dimensions · bytes (both if facing)
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
                    if (!part) {
                        return;
                    }
                    detail = detail ? fmt("%s%s%s", detail, StrL(PAGE_INFO_SEP), part) : part;
                };
                appendPart(imgName);
                if (w > 0 && h > 0) {
                    appendPart(fmt("%d x %d", w, h));
                }
                if (imgSize >= 0) {
                    appendPart(str::FormatSizeShortTemp(imgSize));
                }
                if (detail) {
                    pageInfo = str::JoinTemp(pageInfo, StrL(PAGE_INFO_SEP), detail);
                }
            }
        }
    }

    NotificationUpdateMessage(wnd, pageInfo);
}

// Show or refresh the page-info tip when the user wants it and a document is loaded.
// CloseDocumentInCurrentTab / About / Favorites remove the notification; this restores it.
static void ShowPageInfoIfWanted(MainWindow* win) {
    if (!win || !win->pageInfoWanted || !win->IsDocLoaded() || !win->ctrl) {
        return;
    }
    NotificationWnd* wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (wnd) {
        UpdatePageInfoHelper(win->ctrl, wnd, -1);
        return;
    }
    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.timeoutMs = 0;
    args.msg = "";
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
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifPageInfo);
        return;
    }
    win->pageInfoWanted = true;
    ShowPageInfoIfWanted(win);
}

void ControllerCallbackHandler::ZoomChanged(DocController* ctrl, float /*zoomVirtual*/) {
    // discard change requests from documents
    // loaded asynchronously in a background tab
    if (win->ctrl != ctrl) {
        return;
    }
    NotificationWnd* wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (!wnd) {
        return;
    }
    UpdatePageInfoHelper(win->ctrl, wnd, win->currPageNo);
}

// The current page edit box is updated with the current page number
void ControllerCallbackHandler::PageNoChanged(DocController* ctrl, int pageNo) {
    // discard page number change requests from documents
    // loaded asynchronously in a background tab
    if (win->ctrl != ctrl) {
        return;
    }

    ReportIf(!win->ctrl || win->ctrl->PageCount() <= 0);
    if (!win->ctrl || win->ctrl->PageCount() == 0) {
        return;
    }

    // GoToPage often fires PageNoChanged for the same page (e.g. GoToNextPage
    // fully revealing the current page while the previous is still visible).
    // Toolbar enable/state and the page-total label only need a real page change;
    // refreshing them otherwise over-invalidates the toolbar (comics/scroll).
    bool pageChanged = pageNo != win->currPageNo;

    if (pageChanged && kInvalidPageNo != pageNo) {
        TempStr label = win->ctrl->GetPageLabeTemp(pageNo);
        // HwndSetText is a no-op when the text is unchanged
        if (win->pageEdit) {
            win->pageEdit->SetText(label);
        }
        ToolbarUpdateStateForWindow(win, false);
        if (win->ctrl->HasPageLabels()) {
            UpdateToolbarPageText(win, win->ctrl->PageCount(), true);
        }
    }

    // Markdown multi-file: each .md is a "page". Keep tab path/title/tooltip and
    // ctrl path in sync so Show in folder / Copy path / Properties match the
    // file currently displayed (not only the one first opened).
    MarkdownModel* md = ctrl->AsMarkdown();
    if (md && kInvalidPageNo != pageNo && md->ValidPageNo(pageNo)) {
        WindowTab* tab = win->CurrentTab();
        Str pagePath = md->GetFilePath();
        if (tab && pagePath && !path::IsSame(tab->filePath, pagePath)) {
            tab->SetFilePath(pagePath);
            // Prefer filePath for titles so FullPathInTitle applies.
            tab->SetDisplayName({});
            TabsOnChangedDoc(win);
            SetFrameTitleForTab(tab, false);
            HwndSetText(win->hwndFrame, tab->frameTitle);
        }
    }

    NotificationWnd* wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (!pageChanged) {
        if (wnd) {
            UpdatePageInfoHelper(win->ctrl, wnd, pageNo);
        }
        return;
    }

    UpdateTocSelection(win, pageNo);
    win->currPageNo = pageNo;

    if (!wnd) {
        return;
    }
    UpdatePageInfoHelper(win->ctrl, wnd, pageNo);
}

// Debug check: ctrl->GetFilePath() should match path; logs and ReportIf on mismatch.
static NO_INLINE void VerifyController(DocController* ctrl, Str path) {
    if (!ctrl) {
        return;
    }
    Str ctrlFilePath = ctrl->GetFilePath();
    if (str::Eq(ctrlFilePath, path)) {
        return;
    }
    Str s1 = ctrlFilePath ? ctrlFilePath : StrL("<null>");
    Str s2 = path ? path : StrL("<null>");
    logf("VerifyController: ctrl->FilePath: '%s', filePath: '%s'\n", s1, s2);
    ReportIf(true);
}

// Markdown and HTML both render in the WebView2 browser view via MarkdownModel,
// each gated by its own UseFixedPageUI opt-out (fall back to MuPDF/ebook engines).
static bool ShouldUseBrowserView(FileType kind) {
    if (MarkdownModel::IsHtmlFileType(kind)) {
        return !gGlobalPrefs->htmlUI.useFixedPageUI;
    }
    if (MarkdownModel::IsSupportedFileType(kind)) {
        return !gGlobalPrefs->markdownUI.useFixedPageUI;
    }
    return false;
}

static DocController* CreateControllerForMarkdown(Str path, MainWindow* win) {
    FileType kind = GuessFileType(path, true);
    if (!MarkdownModel::IsSupportedFileType(kind)) {
        return nullptr;
    }
    MarkdownModel* mdModel = MarkdownModel::Create(path, win->cbHandler);
    if (!mdModel) {
        return nullptr;
    }
    DocController* ctrl = nullptr;
    if (!mdModel->SetParentHwnd(win->hwndCanvas)) {
        log("CreateControllerForMarkdown: WebView2 unavailable, falling back to MuPDF markdown view\n");
        delete mdModel;
        return nullptr;
    }
    mdModel->RemoveParentHwnd();
    ctrl = mdModel;
    ReportIf(!ctrl || !ctrl->AsMarkdown() || ctrl->AsFixed());
    VerifyController(ctrl, path);
    return ctrl;
}

static DocController* CreateControllerForChm(Str path, PasswordUI* pwdUI, MainWindow* win) {
    FileType kind = GuessFileType(path, true);

    bool isChm = ChmModel::IsSupportedFileType(kind);
    if (!isChm) {
        return nullptr;
    }
    ChmModel* chmModel = ChmModel::Create(path, win->cbHandler);
    if (!chmModel) {
        return nullptr;
    }
    // make sure that MSHTML can't be used as a potential exploit
    // vector through another browser and our plugin (which doesn't
    // advertise itself for Chm documents but could be tricked into
    // loading one nonetheless); note: this crash should never happen,
    // since gGlobalPrefs->chmUI.useFixedPageUI is set in SetupPluginMode
    ReportIf(gPluginMode);
    // if the interactive backend (WebView2 / IE CLSID_WebBrowser) isn't
    // available, fall back on ChmEngine's fixed-page rendering
    DocController* ctrl = nullptr;
    if (!chmModel->SetParentHwnd(win->hwndCanvas)) {
        log("CreateControllerForChm: interactive CHM backend unavailable, falling back to ChmEngine fixed-page view\n");
        delete chmModel;
        EngineBase* engine = CreateEngineFromFile(path, pwdUI, true);
        if (!engine) {
            log("CreateControllerForChm: ChmEngine fallback also failed, can't display CHM\n");
            return nullptr;
        }
        ReportIf(engine->kind != kindEngineChm);
        ctrl = new DisplayModel(engine, win->cbHandler);
        ReportIf(!ctrl || !ctrl->AsFixed() || ctrl->AsChm());
    } else {
        // another ChmModel might still be active
        chmModel->RemoveParentHwnd();
        ctrl = chmModel;
        ReportIf(!ctrl->AsChm() || ctrl->AsFixed());
    }
    ReportIf(!ctrl);
    VerifyController(ctrl, path);
    return ctrl;
}

// this allows us to target the right file when processing
// a sequence of DDE commands. Without this commands target
// the tab by path and if there's more than one with the same
// path, we pick the first one
// https://github.com/sumatrapdfreader/sumatrapdf/issues/3903
DocController* gMostRecentlyOpenedDoc = nullptr;

DocController* CreateControllerForEngineOrFile(EngineBase* engine, Str path, PasswordUI* pwdUI, MainWindow* win) {
    auto timeStart = TimeGet();
    bool chmInFixedUI = gGlobalPrefs->chmUI.useFixedPageUI;
    if (!engine) {
        FileType kind = GuessFileTypeFromName(path);
        if (ShouldUseBrowserView(kind)) {
            auto* mdCtrl = CreateControllerForMarkdown(path, win);
            if (mdCtrl) {
                gMostRecentlyOpenedDoc = mdCtrl;
                return mdCtrl;
            }
        }
    }
    // TODO: sniff file content only once
    if (!engine) {
        engine = CreateEngineFromFile(path, pwdUI, chmInFixedUI);
    }
    if (!engine) {
        // as a last resort, try to open as chm file
        auto* ctrl = CreateControllerForChm(path, pwdUI, win);
        gMostRecentlyOpenedDoc = ctrl;
        return ctrl;
    }
    int nPages = engine ? engine->pageCount : 0;
    auto dur = TimeSinceInMs(timeStart);
    logf("CreateControllerForEngineOrFile: '%s', %d pages, took %2.f ms\n", path, nPages, dur);
    if (nPages <= 0) {
        // seen nPages < 0 in a crash in epub file
        SafeEngineRelease(&engine);
        return nullptr;
    }
    DocController* ctrl = new DisplayModel(engine, win->cbHandler);
    ReportIf(!ctrl || !ctrl->AsFixed() || ctrl->AsChm());
    VerifyController(ctrl, path);
    gMostRecentlyOpenedDoc = ctrl;
    return ctrl;
}

static void SetFrameTitleForTab(WindowTab* tab, bool needRefresh) {
    Str titlePath = tab->displayName ? Str(tab->displayName) : tab->filePath;
    TempStr embeddedFileName = ParseEmbeddedPdfName(titlePath).fileName;
    if (embeddedFileName) {
        titlePath = embeddedFileName;
    }
    if (!gGlobalPrefs->fullPathInTitle) {
        titlePath = path::GetBaseNameTemp(titlePath);
    }

    TempStr docTitle = "";
    if (tab->ctrl) {
        // NormalizeWSTemp (not in-place): GetPropertyTemp() may return a string
        // owned by the document, which we must not mutate
        TempStr title = str::NormalizeWSTemp(tab->ctrl->GetPropertyTemp(DocProp::Title));
        if (len(title) > 0) {
            docTitle = fmt("- [%s] ", title);
        }
    }

    TempStr s = nullptr;
    if (!IsUIRtl()) {
        s = fmt("%s %s- %s", titlePath, docTitle, Str(kSumatraWindowTitle));
    } else {
        // explicitly revert the title, so that filenames aren't garbled
        s = fmt("%s %s- %s", Str(kSumatraWindowTitle), docTitle, titlePath);
    }
    if (needRefresh && tab->ctrl) {
        // TODO: this isn't visible when tabs are used
        // base the prefix on the freshly-built title 's', not tab->frameTitle:
        // the latter may already carry the prefix from a previous refresh, so
        // reusing it stacks "[..] [..] [..] file.pdf" on repeated changes (#5690)
        s = fmt(_TRA("[Changes detected; refreshing] %s").s, s);
    }
    str::ReplaceWithCopy(&tab->frameTitle, s);
}

static void UpdateUiForCurrentTab(MainWindow* win) {
    // hide the scrollbars before any other relayouting (for assertion in MainWindow::GetViewPortSize)
    if (!win->AsFixed()) {
        if (!ScrollbarsAreHidden() && !ScrollbarsUseOverlay()) {
            ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);
        }
        OverlayScrollbarShow(win->overlayScrollV, false);
        OverlayScrollbarShow(win->overlayScrollH, false);
    }

    // menu for chm and ebook docs is different, so we have to re-create it
    RebuildMenuBarForWindow(win);
    // the toolbar isn't supported for ebook docs (yet)
    ShowOrHideToolbar(win);
    // TODO: unify?
    ToolbarUpdateStateForWindow(win, true);
    UpdateToolbarState(win);

    int pageCount = win->ctrl ? win->ctrl->PageCount() : 0;
    UpdateToolbarPageText(win, pageCount);
    UpdateToolbarFindText(win);

    // Keep / restore page-info tip after reload or tab switch (issue #4454)
    ShowPageInfoIfWanted(win);

    UpdateFindbox(win);

    HwndSetText(win->hwndFrame, win->CurrentTab()->frameTitle);

    bool onlyNumbers = !win->ctrl || !win->ctrl->HasPageLabels();
    if (win->pageEdit) {
        win->pageEdit->SetNumbersOnly(onlyNumbers);
    }
}

static bool showTocByDefault(Str path) {
    if (!gGlobalPrefs->showToc) {
        return false;
    }
    // we don't want to show toc by default for comic book files
    FileType kind = GuessFileTypeFromName(path);
    bool showByDefault = !IsEngineCbxSupportedFileType(kind);
    return showByDefault;
}

static bool IsEbookFileType(FileType ft) {
    return ft == FileType::Epub || ft == FileType::Mobi || ft == FileType::Fb2 || ft == FileType::Fb2z ||
           ft == FileType::PalmDoc || ft == FileType::HTML || ft == FileType::Txt;
}

// Per-type DefaultDisplayMode (empty = inherit the global DefaultDisplayMode).
// Used only on first open when there is no remembered FileState (issue #2588).
static DisplayMode DisplayModeForNewDocument(Str path, EngineBase* engine) {
    DisplayMode dm = gGlobalPrefs->defaultDisplayModeEnum;
    Str modeStr;
    Kind k = engine ? engine->kind : nullptr;
    if (k == kindEngineComicBooks || k == kindEngineImageDir ||
        (path && IsEngineCbxSupportedFileType(GuessFileTypeFromName(path, true)))) {
        modeStr = gGlobalPrefs->comicBookUI.defaultDisplayMode;
    } else if (k == kindEngineEpub || k == kindEngineFb2 || k == kindEngineMobi || k == kindEnginePdb ||
               k == kindEngineHtml || k == kindEngineTxt ||
               (path && IsEbookFileType(GuessFileTypeFromName(path, true)))) {
        modeStr = gGlobalPrefs->eBookUI.defaultDisplayMode;
    }
    if (modeStr) {
        return DisplayModeFromString(modeStr, dm);
    }
    return dm;
}

// Research articles vs slides (issue #4055): portrait -> continuous + fit
// width, landscape -> single page + fit page. First open only.
static bool ShouldUsePageAspectForView(Str path) {
    if (!IsPageAspectDisplayMode(gGlobalPrefs->defaultDisplayMode)) {
        return false;
    }
    FileType ft = GuessFileTypeFromName(path, true);
    return ft == FileType::PDF || ft == FileType::Xps || ft == FileType::DjVu || ft == FileType::PS;
}

static void ApplyPageAspectView(EngineBase* engine, DisplayMode* modeOut, float* zoomOut) {
    if (!engine || engine->PageCount() < 1) {
        return;
    }
    RectF box = engine->PageMediabox(1);
    if (box.dx <= 0 || box.dy <= 0) {
        return;
    }
    if (box.dx > box.dy) {
        *modeOut = DisplayMode::SinglePage;
        *zoomOut = kZoomFitPage;
    } else {
        *modeOut = DisplayMode::Continuous;
        *zoomOut = kZoomFitWidth;
    }
}

// Document is represented as DocController. Replace current DocController (if any) with ctrl
// in current tab.
// meaning of the internal values of LoadArgs:
// isNewWindow : if true then 'win' refers to a newly created window that needs
//   to be resized and placed
static void SetTabLoadError(WindowTab* tab, Str path);

// placeWindow : if true then the Window will be moved/sized according
//   to the 'state' information even if the window was already placed
//   before (isNewWindow=false)
static void ReplaceDocumentInCurrentTab(LoadArgs* args, DocController* ctrl, FileState* fs) {
    MainWindow* win = args->win;
    ReportIf(!win);
    if (!win) {
        return;
    }
    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        ReportIf(true);
        return;
    }
    if (ctrl) {
        // a previous failed load may have left a reason behind
        str::Free(tab->loadErrorReason);
        tab->loadErrorReason = {};
    } else {
        // no controller means the load failed (e.g. a reload of a file that has
        // since been deleted or overwritten); record why for the error screen
        SetTabLoadError(tab, args->FilePath());
    }

    // Never load settings from a preexisting state if the user doesn't wish to
    // (unless we're just refreshing the document, i.e. only if state && !state->useDefaultState)
    if (!fs && gGlobalPrefs->rememberStatePerDocument) {
        Str fn = args->FilePath();
        fs = FileHistoryFindByPath(fn);
        if (fs) {
            if (fs->windowPos.IsEmpty()) {
                fs->windowPos = gGlobalPrefs->windowPos;
            }
            EnsureAreaVisibility(fs->windowPos);
        }
    }
    if (fs && fs->useDefaultState) {
        fs = nullptr;
    }

    DisplayMode displayMode = gGlobalPrefs->defaultDisplayModeEnum;
    float zoomVirtual = gGlobalPrefs->defaultZoomFloat;
    ScrollState ss(1, -1, -1);
    int rotation = 0;
    Str path = args->FilePath();
    bool showToc = showTocByDefault(path);
    bool showAsFullScreen = WIN_STATE_FULLSCREEN == gGlobalPrefs->windowState;
    int showType = SW_NORMAL;
    if (gGlobalPrefs->windowState == WIN_STATE_MAXIMIZED || showAsFullScreen) {
        showType = SW_MAXIMIZE;
    }

    if (fs) {
        ss.page = fs->pageNo;
        displayMode = DisplayModeFromString(fs->displayMode, DisplayMode::Automatic);
        showAsFullScreen = WIN_STATE_FULLSCREEN == fs->windowState;
        if (fs->windowState == WIN_STATE_NORMAL) {
            showType = SW_NORMAL;
        } else if (fs->windowState == WIN_STATE_MAXIMIZED || showAsFullScreen) {
            showType = SW_MAXIMIZE;
        } else if (fs->windowState == WIN_STATE_MINIMIZED) {
            showType = SW_MINIMIZE;
        }
        showToc = fs->showToc;
        if (win->ctrl && win->presentation) {
            showToc = tab->showTocPresentation;
        }
        ParsedColor* bgParsed = GetPrefsColor(fs->bgCol);
        if (bgParsed->parsedOk) {
            tab->bgColor = bgParsed->col;
            tab->bgColorCheckered = (bgParsed->col == kColorUnset);
        }
        ParsedColor* tabColParsed = GetPrefsColor(fs->tabCol);
        if (tabColParsed->parsedOk) {
            tab->tabColor = tabColParsed->col;
            // AddTabToWindow() copied tab->tabColor into the TabInfo before the
            // document loaded, when it was still unset, so push it again now -
            // the tab control paints from the TabInfo (issue #5884)
            SetTabInfoColor(tab);
        }
    }

    AbortFinding(args->win, true);

    DocController* prevCtrl = win->ctrl;
    tab->ctrl = ctrl;
    win->ctrl = tab->ctrl;

    // Reload/replace swaps the document; clear any tip for the previous page.
    win->DeleteToolTip();

    // Drop find-match coords / match-count cache for the previous engine (reload
    // or replace). Find UI text is kept; count restarts against the new engine
    // if find is still open. Must run after win->ctrl points at the new document.
    InvalidateFindForDocumentChange(win);

    EngineBase* engine = tab->GetEngine();
    if (engine) {
        engine->hideAnnotations = tab->hideAnnotations;
        float imageZoom = gGlobalPrefs->imageUI.defaultZoomFloat;
        if (engine->kind == kindEngineImage && imageZoom != 0) {
            zoomVirtual = imageZoom;
        }
        // first open: EbookUI / ComicBookUI DefaultDisplayMode override the
        // global default when set (issue #2588). Remembered FileState wins.
        if (!fs) {
            displayMode = DisplayModeForNewDocument(path, engine);
            if (ShouldUsePageAspectForView(path)) {
                ApplyPageAspectView(engine, &displayMode, &zoomVirtual);
            }
        }
        // First open without per-file remembered state: honor PDF Catalog
        // /OpenAction when it is a safe internal GoTo (issue #1631). Does not
        // override history, -page / -named-dest (applied later), or reloads.
        if (!fs && ss.page == 1) {
            int openPage = engine->GetOpenActionPageNo();
            if (openPage >= 1) {
                ss.page = openPage;
            }
        }
    }

    // ToC items might hold a reference to an Engine, so make sure to
    // delete them before destroying the whole DisplayModel
    // (same for linkOnLastButtonDown)
    ClearTocBox(win);
    ClearMouseState(win);

    if (win->ctrl) {
        DisplayModel* dm = win->AsFixed();
        if (dm) {
            int dpi = gGlobalPrefs->customScreenDPI;
            // <= 0 means "not set" (users have been seen setting it to -1)
            if (dpi <= 0) {
                dpi = DpiGetForHwnd(win->hwndFrame);
            }
            // WindowMargin / PageSpacing follow the window's dpi, not the
            // (physical-size) CustomScreenDPI used for zoom
            dm->SetUiDpi(win->frameDpi > 0 ? win->frameDpi : DpiGetForHwnd(win->hwndFrame));
            dm->SetInitialViewSettings(displayMode, ss.page, win->GetViewPortSize(), dpi);
            if (fs) {
                dm->SetDisplayR2L(fs->displayR2L);
            } else if (tab->GetEngineType() == kindEngineComicBooks || tab->GetEngineType() == kindEngineImageDir) {
                dm->SetDisplayR2L(gGlobalPrefs->comicBookUI.cbxMangaMode);
            }
            if (prevCtrl && prevCtrl->AsFixed() && str::Eq(win->ctrl->GetFilePath(), prevCtrl->GetFilePath())) {
                gRenderCache->KeepForDisplayModel(prevCtrl->AsFixed(), dm);
                dm->CopyNavHistory(*prevCtrl->AsFixed());
            }
            // tell UI Automation about content change
            if (win->uiaProvider) {
                win->uiaProvider->OnDocumentUnload();
                win->uiaProvider->OnDocumentLoad(dm);
            }
        } else if (IsBrowserDocController(win->ctrl)) {
            if (win->AsChm()) {
                win->AsChm()->SetParentHwnd(win->hwndCanvas);
            } else {
                win->AsMarkdown()->SetParentHwnd(win->hwndCanvas);
            }
            FillCanvasThemeBackground(win->hwndCanvas);
            win->ctrl->SetDisplayMode(displayMode);
            // Markdown treats each .md in the directory as a "page". When the
            // user opens a specific file (File/Open, drag&drop, home thumbnail),
            // always start on that file — FileState page/scroll would jump to
            // whichever .md was last viewed in the folder. CHM still restores.
            if (win->AsMarkdown()) {
                int page = win->ctrl->CurrentPageNo();
                if (page < 1 || page > win->ctrl->PageCount()) {
                    page = 1;
                }
                ss.page = page;
                win->ctrl->GoToPage(page, false);
            } else {
                ss.page = limitValue(ss.page, 1, win->ctrl->PageCount());
                if (fs) {
                    RectF r(fs->scrollPos.x, fs->scrollPos.y, 0, 0);
                    win->AsChm()->ScrollTo(ss.page, r, kInvalidZoom);
                } else {
                    win->ctrl->GoToPage(ss.page, false);
                }
            }
        } else {
            ReportIf(true);
        }
    } else {
        fs = nullptr;
    }

    if (fs) {
        ReportIf(!win->IsDocLoaded());
        zoomVirtual = ZoomFromString(fs->zoom, kZoomFitPage);
        if (win->ctrl->ValidPageNo(ss.page)) {
            if (kZoomFitContent != zoomVirtual) {
                ss.x = fs->scrollPos.x;
                ss.y = fs->scrollPos.y;
            }
            // else let win->AsFixed()->Relayout() scroll to fit the page (again)
        } else if (win->ctrl->PageCount() > 0) {
            ss.page = limitValue(ss.page, 1, win->ctrl->PageCount());
        }
        // else let win->ctrl->GoToPage(ss.page, false) verify the page number
        rotation = fs->rotation;
        tab->tocState = *fs->tocState;
    }

    // DisplayModel needs a valid zoom value before any relayout
    // caused by showing/hiding UI elements happends.
    // Relayout before tearing down prevCtrl so a WM_PAINT pumped during
    // the teardown never calls SetViewPortSize with an invalid zoom.
    // Pause rendering until the remembered page is restored so Relayout,
    // ShowWindow, and sidebar setup don't request page 1 first (issue #4973).
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        dm->pauseRendering = true;
        dm->Relayout(zoomVirtual, rotation);
    } else if (win && win->ctrl && win->IsDocLoaded()) {
        win->ctrl->SetZoomVirtual(zoomVirtual, nullptr);
    }

    // A folder-navigation load replaces the document without closing the tab
    // first. Stop watching the old path before LoadDocumentFinish subscribes
    // the tab to the new one.
    if (prevCtrl && ctrl && !path::IsSame(prevCtrl->GetFilePath(), ctrl->GetFilePath())) {
        FileWatcherUnsubscribe(tab->watcher);
        tab->watcher = nullptr;
    }
    delete prevCtrl;

#if defined(ENABLE_REDRAW_ON_RELOAD)
    // TODO: why is this needed?
    if (!args->isNewWindow && win->IsDocLoaded()) {
        win->RedrawAll();
    }
#endif

    SetFrameTitleForTab(tab, false);
    UpdateUiForCurrentTab(win);

    if (CanAccessDisk() && tab->GetEngineType() == kindEngineMupdf) {
        ReportIf(!win->AsFixed() || win->AsFixed()->pdfSync);
        path = args->FilePath();
        // note: we used to set gGlobalPrefs->enableTeXEnhancements to true on
        // success to expose SyncTeX in the UI but that made an explicit
        // EnableTeXEnhancements = false impossible as it was persisted on exit;
        // the setting is now only changed by the user or -inverse-search et al.
        // (issue #1289)
        Synchronizer::Create(path, win->AsFixed()->GetEngine(), &win->AsFixed()->pdfSync);
    }

    bool shouldPlace = args->isNewWindow || args->placeWindow && fs;
    if (args->noPlaceWindow) {
        shouldPlace = false;
    }
    if (shouldPlace) {
        if (args->isNewWindow && fs && !fs->windowPos.IsEmpty() && showType == SW_NORMAL) {
            // Make sure it doesn't have a position like outside of the screen etc.
            Rect rect = ShiftRectToWorkArea(fs->windowPos);
            // This shouldn't happen until !win.IsAboutWindow(), so that we don't
            // accidentally update gGlobalState with this window's dimensions
            HwndMoveWindow(win->hwndFrame, &rect);
        }
        if (args->showWin) {
            ShowWindow(win->hwndFrame, showType);
            if (IsRunningOnWine()) {
                Rect wr = HwndWindowRect(win->hwndFrame);
                Rect cr = HwndClientRect(win->hwndFrame);
                logf(
                    "LoadDocument: showWin windowRect=(%d,%d,%d,%d) clientRect=(%d,%d,%d,%d) "
                    "captionRect=(%d,%d,%d,%d)\n",
                    wr.x, wr.y, wr.dx, wr.dy, cr.x, cr.y, cr.dx, cr.dy, win->captionRect.x, win->captionRect.y,
                    win->captionRect.dx, win->captionRect.dy);
            }
        }

#if 0
        // fix https://github.com/sumatrapdfreader/sumatrapdf/issues/5456
        // bad initial layout with RememberOpenedFiles = false
        // it's redundant with LayoutAndFocusOnStartup()

        // Fire deferred SWP_FRAMECHANGED for custom caption so the
        // non-client area is recalculated and the client rect is correct.
        // ShowMainWindow normally does this, but this code path bypasses it.
        if (win->tabsInTitlebar) {
            uint swpFlags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
            SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, swpFlags);
        }
#endif

        if (win) {
            UpdateWindow(win->hwndFrame);
        }
        if (args->isNewWindow && win) {
            HwndEnsureOnScreen(win->hwndFrame);
        }
    }

    // if the window isn't shown and win.canvasRc is still empty, zoom
    // has not been determined yet
    // cf. https://code.google.com/archive/p/sumatrapdf/issues/2541
    // ReportIf(win->IsDocLoaded() && args->showWin && win->canvasRc.IsEmpty() && !win->AsChm());

    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        return;
    }
    SetSidebarVisibility(win, showToc, gGlobalPrefs->showFavorites);
    if (dm) {
        dm->pauseRendering = false;
    }
    // restore scroll state after the canvas size has been restored
    if ((args->showWin || ss.page != 1) && dm) {
        dm->SetScrollState(ss);
    }

    tab->canvasRc = win->canvasRc;
    TabsOnChangedDoc(win);

    if (!win->IsDocLoaded()) {
        win->RedrawAll(true);
        return;
    }

    // Use `tab` (the document just loaded into this tab), not win->CurrentTab():
    // multi-file open/session restore can finish loads out of order so another
    // tab may already be selected by the time we get here.
    TempStr unsupported = win->ctrl->GetPropertyTemp(DocProp::UnsupportedFeatures);
    if (unsupported) {
        Str s = _TRA("%s not supported");
        TempStr msg = fmt(s.s, unsupported);
        NotificationCreateArgs nargs;
        nargs.hwndParent = win->hwndCanvas;
        nargs.warning = true;
        nargs.timeoutMs = 16 * 1000; // auto-dismiss after 16 seconds
        nargs.groupId = kNotifPersistentWarning;
        nargs.msg = msg;
        nargs.plainText = true; // `unsupported` is a document property
        nargs.tab = tab;        // only show while this tab is active
        nargs.corner = NotifCorner::BottomRight;
        nargs.xMargin = 2;
        nargs.yMargin = 2;
        ShowNotification(nargs);
    }

    // if the document had parsing errors (the same condition that adds "Show
    // Errors" to the context menu), surface it with a notification whose
    // "Errors" link opens the Show Errors dialog (matching the unsupported-
    // features notification: bottom-right, small margins, 16s timeout)
    DisplayModel* dmErr = win->AsFixed();
    EngineBase* engineErr = dmErr ? dmErr->GetEngine() : nullptr;
    if (engineErr && engineErr->HasErrors()) {
        TempStr msg = fmt("[%s](CmdShowErrors) %s", _TRA("Errors"), _TRA("in document"));
        NotificationCreateArgs nargs;
        nargs.hwndParent = win->hwndCanvas;
        nargs.warning = true;
        nargs.timeoutMs = 16 * 1000; // auto-dismiss after 16 seconds
        nargs.groupId = kNotifDocErrors;
        nargs.msg = msg;
        // Bind to the tab that just loaded (not whatever is current after a
        // later multi-file finish steals the UI).
        nargs.tab = tab;
        nargs.corner = NotifCorner::BottomRight;
        nargs.xMargin = 2;
        nargs.yMargin = 2;
        ShowNotification(nargs);
    }

    // Multi-file open / session restore: each finished load can leave tab-tied
    // notifs from earlier loads visible on the canvas. Re-sync visibility to
    // the active tab so "Show Errors" is not shown on the wrong document.
    ShowNotificationsForActiveTab(win->hwndCanvas, win->CurrentTab());

    // This should only happen after everything else is ready
    if ((args->isNewWindow || args->placeWindow) && args->showWin && showAsFullScreen) {
        EnterFullScreen(win);
    } else {
        win->RedrawAll(false);
    }
    if (!args->isNewWindow && win->presentation && win->ctrl) {
        win->ctrl->SetInPresentation(true);
    }
}

void ReloadDocument(MainWindow* win, bool autoRefresh, bool canAskForPassword) {
    WindowTab* tab = win->CurrentTab();

    if (!tab) {
        return;
    }
    // TODO: maybe should ensure it never is called for IsAboutTab() ?
    // This only happens if gLazyLoading is true
    if (tab->IsNonDocumentTab()) {
        return;
    }

    // tear down any in-place form-field edit before the engine is deleted below,
    // so the overlay's widget pointer can't dangle into freed memory
    CommitFormFieldEdit(false);

    tab->selectedAnnotation = nullptr;
    win->annotationBeingDragged = nullptr;
    win->annotationBeingResized = false;
    win->annotationUnderCursor = nullptr;
    // EditAnnotationsWindow keeps non-owning Annotation* from the engine; clear
    // them before ReplaceDocumentInCurrentTab deletes the old engine.
    InvalidateEditAnnotationsOnEngineChange(tab);
    // Do not clear ignoreNextAutoReload here: SaveAnnotationsToExistingFile sets it
    // so the file-watcher auto-reload from that write is skipped. Clearing it on every
    // ReloadDocument caused a second full open right after the intentional post-save
    // reload (race with render/RefHover on a just-rewritten PDF).

    if (!tab->IsDocLoaded()) {
        if (!autoRefresh) {
            if (len(tab->filePath) == 0) {
                logf("ReloadDocument: tab->filePath is empty, can't reload\n");
                return;
            }
            LoadArgs args(tab->filePath, win);
            args.forceReuse = true;
            args.noSavePrefs = true;
            args.tabState = tab->tabState;
            LoadDocument(&args);
        }
        return;
    }

    // A reload nobody asked for must not pop a password dialog. If the file was
    // replaced by a different, encrypted document - Outlook rewriting an
    // attachment's temp path is the reported case - every window watching that
    // path would put a modal dialog on screen out of nowhere (#3493). With a
    // null hwnd, HwndPasswordUI still tries the remembered decryption key and
    // DefaultPasswords, then gives up quietly: the tab keeps the document it
    // has, and the user is asked if they reload it themselves.
    HwndPasswordUI pwdUI(canAskForPassword ? win->hwndFrame : nullptr);
    Str path = tab->filePath;
    if (len(path) == 0) {
        logf("ReloadDocument: tab->filePath is empty, auto refresh: %d\n", (int)autoRefresh);
        return;
    }
    logfa("ReloadDocument: %s, auto refresh: %d\n", path, (int)autoRefresh);

    // Save display state before potentially destroying the old controller
    FileState* fs = NewFileState(path);
    tab->ctrl->GetDisplayState(fs);
    UpdateDisplayStateWindowRect(win, fs);
    UpdateSidebarDisplayState(tab, fs);

    DocController* ctrl = CreateControllerForEngineOrFile(nullptr, path, &pwdUI, win);
    // We don't allow PDF-repair if it is an autorefresh because
    // a refresh event can occur before the file is finished being written,
    // in which case the repair could fail. Instead, if the file is broken,
    // we postpone the reload until the next autorefresh event
    if (!ctrl && autoRefresh) {
        SetFrameTitleForTab(tab, true);
        HwndSetText(win->hwndFrame, tab->frameTitle);
        DeleteFileState(fs);
        return;
    }
    // Set the windows state based on the actual window's placement
    int wstate = WIN_STATE_NORMAL;
    if (win->isFullScreen) {
        wstate = WIN_STATE_FULLSCREEN;
    } else {
        if (IsZoomed(win->hwndFrame)) {
            wstate = WIN_STATE_MAXIMIZED;
        } else if (IsIconic(win->hwndFrame)) {
            wstate = WIN_STATE_MINIMIZED;
        }
    }
    fs->windowState = wstate;
    fs->useDefaultState = false;

    LoadArgs args(tab->filePath, win);
    args.showWin = true;
    args.placeWindow = false;
    ReplaceDocumentInCurrentTab(&args, ctrl, fs);

    if (!ctrl) {
        DeleteFileState(fs);
        return;
    }

    // after reload, refresh the annotations list in the edit window
    // so that it stays in sync with the new engine
    UpdateAnnotationsList(tab->editAnnotsWindow);

    tab->reloadOnFocus = false;

    if (gGlobalPrefs->showStartPage) {
        // refresh the thumbnail for this file
        FileState* state = FileHistoryFindByPath(fs->filePath);
        if (state) {
            CreateThumbnailForFile(win, state);
        }
    }

    if (tab->AsFixed()) {
        // save a newly remembered password into file history so that
        // we don't ask again at the next refresh
        Str decryptionKey = tab->AsFixed()->GetEngine()->decryptionKey;
        if (decryptionKey) {
            FileState* fs2 = FileHistoryFindByPath(fs->filePath);
            if (fs2 && !str::Eq(fs2->decryptionKey, decryptionKey)) {
                str::ReplaceWithCopy(&fs2->decryptionKey, decryptionKey);
            }
        }
    }

    DeleteFileState(fs);
}

constexpr int kSplitterDx = 5;
constexpr int kSplitterDy = 4;

// show / collapse a node of the frame's content row; a collapsed one takes no
// space and, for the virtual controls, isn't painted or hit-tested either
static void SetVis(ILayout* l, bool visible) {
    l->SetVisibility(visible ? Visibility::Visible : Visibility::Collapse);
}

// A splitter is a virtual control in the frame's tree: the frame paints it and
// hands it the mouse. `isLive` false means the panes only move on release
static VirtSplitter* NewFrameSplitter(SplitterType type, bool isLive) {
    auto* s = new VirtSplitter();
    s->type = type;
    s->isLive = isLive;
    s->bgColor = ThemeControlBackgroundColor();
    s->SetIsVisible(false); // shown by the relayout when the pane is up
    return s;
}

// (re)tells the frame's root which splitters exist; they are created as their
// panes are (the AI chat one last)
void FrameSyncSplitters(MainWindow* win) {
    if (!win->frameRoot) {
        win->frameRoot = new VirtRoot(win->hwndFrame);
        win->frameRoot->SetBounds(HwndClientRect(win->hwndFrame));
    }
    Vec<VirtCtrl*> tops;
    if (win->captionLayout) {
        CollectVirtCtrls(win->captionLayout, tops);
    }
    VirtSplitter* all[] = {win->sidebarSplitter, win->favSplitter, win->aiChatSplitter};
    for (VirtSplitter* s : all) {
        if (s) {
            tops.Append(s);
        }
    }
    win->frameRoot->SetTops(tops);
}

//--- tabs-in-titlebar caption

static Kind kindCaptionBtn = "captionBtn";

struct VirtCaptionButton : VirtCtrl {
    MainWindow* win = nullptr;
    int id = 0;
    Size idealSize;

    VirtCaptionButton(MainWindow* w, int idIn);
    Size GetIdealSize() override;
    void SetBounds(Rect) override;
};

VirtCaptionButton::VirtCaptionButton(MainWindow* w, int idIn) {
    win = w;
    id = idIn;
    kind = kindCaptionBtn;
    flags |= vwfNoHitTest;
}

Size VirtCaptionButton::GetIdealSize() {
    return idealSize;
}

void VirtCaptionButton::SetBounds(Rect r) {
    VirtCtrl::SetBounds(r);
    if (!win) {
        return;
    }
    win->captionBtn[id].rect = r;
    win->captionBtn[id].id = id;
    win->captionBtn[id].visible = GetVisibility() == Visibility::Visible;
}

constexpr int kTabsButtonGapX = 32;

static void CreateCaptionLayout(MainWindow* win) {
    for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
        win->capBtn[i] = new VirtCaptionButton(win, i);
        win->captionBtn[i].id = i;
    }
    win->capMenuSlot = new HwndSlot();
    win->capMenuSlot->mapRtlX = true;
    win->capTabsRow1 = new HwndSlot();
    win->capTabsRow1->mapRtlX = true;
    win->capTabsRow2 = new HwndSlot();
    win->capTabsRow2->mapRtlX = true;
    win->capGap = new Spacer(0, 0);
    win->capDrag1 = new Spacer(0, 0);
    win->capRow2Lead = new Spacer(0, 0);
    win->capRow2Trail = new Spacer(0, 0);

    // single row: sys | menu | tabs | gap | min | max/restore | close
    // two row:     sys | menu hwnd | drag | min | max/restore | close
    win->captionRow1 = new HBox();
    win->captionRow1->alignCross = CrossAxisAlign::CrossEnd;
    win->captionRow1->AddChild(win->capBtn[CB_SYSTEM_MENU]);
    win->captionRow1->AddChild(win->capBtn[CB_MENU]);
    win->captionRow1->AddChild(win->capMenuSlot);
    win->captionRow1->AddChild(win->capTabsRow1, 1);
    win->captionRow1->AddChild(win->capDrag1, 1);
    win->captionRow1->AddChild(win->capGap);
    win->captionRow1->AddChild(win->capBtn[CB_MINIMIZE]);
    win->captionRow1->AddChild(win->capBtn[CB_MAXIMIZE]);
    win->captionRow1->AddChild(win->capBtn[CB_RESTORE]);
    win->captionRow1->AddChild(win->capBtn[CB_CLOSE]);

    // two-row tabs sit under the menu, stopping short of the window buttons
    win->captionRow2 = new HBox();
    win->captionRow2->alignCross = CrossAxisAlign::Stretch;
    win->captionRow2->AddChild(win->capRow2Lead);
    win->captionRow2->AddChild(win->capTabsRow2, 1);
    win->captionRow2->AddChild(win->capRow2Trail);

    win->captionLayout = new VBox();
    win->captionLayout->alignCross = CrossAxisAlign::Stretch;
    win->captionLayout->AddChild(win->captionRow1);
    win->captionLayout->AddChild(win->captionRow2);
}

// The frame's chrome + content: a VBox of caption / tabs / menu / toolbar
// over the content row [ToC / Favorites column] | splitter | (canvas stacked
// with the full-window Favorites tab) | splitter | AI chat. Each HWND is an
// HwndSlot; RelayoutFrame sets winPos so SetBounds batches the moves.
// The AI chat parts stay in the row even while that panel doesn't exist —
// they are simply collapsed.
static void CreateFrameLayout(MainWindow* win) {
    win->tocSlot = new HwndSlot();
    win->favSlot = new HwndSlot();
    win->fullFavSlot = new HwndSlot();
    win->canvasSlot = new HwndSlot();
    win->aiChatSlot = new HwndSlot();
    win->tabsSlot = new HwndSlot();
    win->tabsSlot->mapRtlX = true;
    win->menuSlot = new HwndSlot();
    win->menuSlot->mapRtlX = true;
    win->toolbarTopSlot = new HwndSlot();
    win->toolbarBottomSlot = new HwndSlot();
    CreateCaptionLayout(win);

    // the webview is expensive to resize, so this one only moves the panes
    // when the drag ends
    win->aiChatSplitter = NewFrameSplitter(SplitterType::Vert, false);
    win->aiChatSplitter->thickness = kSplitterDx;

    auto* sidebar = new VBox();
    sidebar->alignCross = CrossAxisAlign::Stretch;
    sidebar->AddChild(win->tocSlot);
    sidebar->AddChild(win->favSplitter);
    sidebar->AddChild(win->favSlot, 1);

    // canvas and the Favorites tab share this box; only one HWND is shown
    auto* content = new Overlay();
    content->AddChild(win->canvasSlot);
    content->AddChild(win->fullFavSlot);

    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::Stretch;
    row->AddChild(sidebar);
    row->AddChild(win->sidebarSplitter);
    row->AddChild(content, 1);
    row->AddChild(win->aiChatSplitter);
    row->AddChild(win->aiChatSlot);
    win->frameLayout = row;

    auto* chrome = new VBox();
    chrome->alignCross = CrossAxisAlign::Stretch;
    chrome->AddChild(win->captionLayout);
    chrome->AddChild(win->tabsSlot);
    chrome->AddChild(win->menuSlot);
    chrome->AddChild(win->toolbarTopSlot);
    chrome->AddChild(win->frameLayout, 1);
    chrome->AddChild(win->toolbarBottomSlot);
    win->chromeLayout = chrome;
    FrameSyncSplitters(win);
}

static void CreateSidebar(MainWindow* win) {
    win->sidebarSplitter = NewFrameSplitter(SplitterType::Vert, true);
    win->sidebarSplitter->thickness = kSplitterDx;
    win->sidebarSplitter->onMove = MkFunc1Void(OnSidebarSplitterMove);
    FrameSyncSplitters(win);

    CreateToc(win);

    win->favSplitter = NewFrameSplitter(SplitterType::Horiz, true);
    win->favSplitter->thickness = kSplitterDy;
    win->favSplitter->onMove = MkFunc1Void(OnFavSplitterMove);
    FrameSyncSplitters(win);

    CreateFrameLayout(win);

    CreateFavorites(win);

    CreateAIChatPanel(win);

    if (win->uiState.tocVisible) {
        HwndRepaintNow(win->hwndTocBox);
    }

    if (gGlobalPrefs->showFavorites) {
        HwndRepaintNow(win->hwndFavBox);
    }
}

static void UpdateToolbarSidebarText(MainWindow* win) {
    UpdateToolbarPageText(win, -1);
    UpdateToolbarFindText(win);
    UpdateToolbarButtonsToolTipsForWindow(win);

    win->tocLabel->SetText(_TRA("Bookmarks"));
    win->tocLabel->Invalidate();
    win->favLabel->SetText(_TRA("Favorites"));
    win->favLabel->Invalidate();
}

static Color DwmFrameBorderColorForCurrentTheme() {
    return IsCurrentThemeDefault() ? (Color)DWMWA_COLOR_DEFAULT : ThemeControlBackgroundColor();
}

// Win11 DWM attributes; ignored (HRESULT failure) on older Windows.
static void SetWindowBorderColor(HWND hwnd, Color color) {
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &color, sizeof(color));
}

static void SetWindowRoundedCorners(HWND hwnd, bool rounded) {
    auto cornerPref = rounded ? DWMWCP_ROUND : DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
    Color borderColor = rounded ? DWMWA_COLOR_DEFAULT : DWMWA_COLOR_NONE;
    SetWindowBorderColor(hwnd, borderColor);
}

static void UpdateWindowFrameBorderColor(MainWindow* win) {
    if (!win || !win->hwndFrame) {
        return;
    }
    // Maximized / fullscreen can't edge-resize; hide the DWM border so it does
    // not show as a bright 1px seam against the taskbar (issue #5851).
    if (IsZoomed(win->hwndFrame) || win->isFullScreen || win->presentation) {
        SetWindowBorderColor(win->hwndFrame, (Color)DWMWA_COLOR_NONE);
        return;
    }
    SetWindowBorderColor(win->hwndFrame, DwmFrameBorderColorForCurrentTheme());
}

static MainWindow* CreateMainWindow() {
    Rect windowPos = gGlobalPrefs->windowPos;
    if (!windowPos.IsEmpty()) {
        EnsureAreaVisibility(windowPos);
    } else {
        windowPos = GetDefaultWindowPos();
    }
    // we don't want the windows to overlap so shift each window by a bit
    int nShift = len(gWindows);
    windowPos.x += nShift * DpiScale(15);

    WStr clsName = WStrL(FRAME_CLASS_NAME);
    WStr title = WStr(kSumatraWindowTitleW);
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    int x = windowPos.x;
    int y = windowPos.y;
    int dx = windowPos.dx;
    int dy = windowPos.dy;
    HINSTANCE h = GetModuleHandle(nullptr);
    HWND hwndFrame =
        CreateWindowExW(WS_EX_APPWINDOW, clsName.s, title.s, style, x, y, dx, dy, nullptr, nullptr, h, nullptr);
    if (!hwndFrame) {
        return nullptr;
    }
    DpiSetFromHwnd(hwndFrame);

    // WM_NCCALCSIZE returning 0 disables DWM rounded corners; re-enable them.
    if (!IsRunningOnWine()) {
        SetWindowRoundedCorners(hwndFrame, true);
    }

    ReportIf(nullptr != FindMainWindowByHwnd(hwndFrame));
    MainWindow* win = new MainWindow(hwndFrame);
    win->frameDpi = RoundUp(DpiGetForHwnd(hwndFrame), 4);
    if (win->frameDpi <= 0) {
        win->frameDpi = 96;
    }
    UpdateWindowFrameBorderColor(win);

    // don't add a WS_EX_STATICEDGE so that the scrollbars touch the
    // screen's edge when maximized (cf. Fitts' law) and there are
    // no additional adjustments needed when (un)maximizing
    clsName = CANVAS_CLASS_NAME;
    // WS_CLIPSIBLINGS so the canvas doesn't paint over the floating overlay
    // toolbar (a higher-Z sibling) in overlay mode
    style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    if (!ScrollbarsAreHidden() && !ScrollbarsUseOverlay()) {
        style |= WS_HSCROLL | WS_VSCROLL;
    }
    /* position and size determined in OnSize */
    Rect rcFrame = HwndClientRect(hwndFrame);
    win->hwndCanvas =
        CreateWindowExW(0, clsName.s, nullptr, style, 0, 0, rcFrame.dx, rcFrame.dy, hwndFrame, nullptr, h, nullptr);
    if (!win->hwndCanvas) {
        delete win;
        return nullptr;
    }

    // hide scrollbars to avoid showing/hiding on empty window
    if (!ScrollbarsAreHidden() && !ScrollbarsUseOverlay()) {
        ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);
    }

    ReportIf(win->menu);
    win->menu = BuildMenu(win);
    // menu bar is shown later, after SetTabsInTitlebar decides the mode:
    // if tabsInTitlebar, we use a rebar menu bar; otherwise native SetMenu
    win->brControlBgColor = CreateSolidBrush(ThemeControlBackgroundColor());

    // Note: don't send WM_SETREDRAW to hwndFrame here. The frame is hidden
    // (shown later by ShowMainWindow / LoadDocument) so nothing paints anyway,
    // and DefWindowProc's WM_SETREDRAW TRUE handling *shows* the window, which
    // would flash a normal-size standard-caption window before the custom
    // caption / maximized / fullscreen state is applied (the old fix for the
    // dark-theme startup flash, #5421, predates creating the frame hidden).
    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    Tooltip::CreateArgs args;
    args.parent = win->hwndCanvas;
    args.font = GetAppFont();
    args.isRtl = IsUIRtl();

    win->infotip = new Tooltip();
    win->infotip->Create(args);

    CreateTabbar(win);
    CreateToolbar(win);
    // create the floating find bar hidden; it owns win->findEdit
    win->findBar = CreateFindBar(win);
    CreateSidebar(win);
    UpdateFindbox(win);
    if (CanAccessDisk() && !gPluginMode) {
        RegisterCanvasDropTarget(win->hwndCanvas);
    }

    if (len(gWindows) == 0) {
        InitScreenshotHost();
        InitImageEditHost();
        if (!NeedsWindowEmbeddingHacks()) {
            RegisterScreenshotHotkey(win->hwndFrame);
        }
    }
    gWindows.Append(win);
    ShowMaybeDelayedNotifications(win->hwndCanvas);
    // needed for RTL languages
    UpdateWindowRtlLayout(win);
    UpdateToolbarSidebarText(win);

    {
        GESTURECONFIG gc = {0, GC_ALLGESTURES, 0};
        SetGestureConfig(win->hwndCanvas, 0, 1, &gc, sizeof(GESTURECONFIG));
    }

    // Set tabsInTitlebar state without SWP_FRAMECHANGED; the frame change
    // is deferred to ShowMainWindow so the shell sees a normal frame during
    // the first ShowWindow and creates the taskbar button.
    {
        bool inTitleBar = SettingsUseTabs();
        win->tabsInTitlebar = inTitleBar;
        win->tabsCtrl->inTitleBar = inTitleBar;
        if (inTitleBar) {
            RelayoutCaption(win);
        }
    }

    // now show the menu bar in the appropriate style
    if (IsMenubarVisible() && !NeedsWindowEmbeddingHacks()) {
        if (win->tabsInTitlebar) {
            CreateMenuBarRebar(win);
        } else {
            SetMenu(win->hwndFrame, win->menu);
        }
    }

    // TODO: this is hackish. in general we should divorce
    // layout re-calculations from MainWindow and creation of windows
    win->UpdateCanvasSize();
    DarkModeApplyToNewFrame(win);

    // show menu bar rebar now that layout is done
    ShowMenuBarRebar(win);

    return win;
}

void ShowMainWindow(MainWindow* win, int windowState) {
    if (WIN_STATE_FULLSCREEN == windowState || WIN_STATE_MAXIMIZED == windowState) {
        ShowWindow(win->hwndFrame, SW_MAXIMIZE);
    } else {
        ShowWindow(win->hwndFrame, SW_SHOW);
    }

    // Fire the deferred SWP_FRAMECHANGED for custom caption (tabsInTitlebar).
    // Must happen after ShowWindow so the shell sees a visible window and
    // creates the taskbar button before we remove the standard frame.
    if (win->tabsInTitlebar) {
        uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
        SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);
    }

    // go fullscreen before the first paint so the user doesn't see the
    // intermediate maximized window (EnterFullScreen requires a visible
    // window, so it can't happen before ShowWindow above)
    if (WIN_STATE_FULLSCREEN == windowState) {
        EnterFullScreen(win);
    }

    // Hidden startup windows can miss the final titlebar/menu-bar geometry
    // until they become visible. Force one relayout before the first paint.
    RelayoutFrame(win);
    UpdateWindow(win->hwndFrame);
    UpdateToolbarFindText(win);
    HwndEnsureOnScreen(win->hwndFrame);

    if (IsRunningOnWine()) {
        Rect wr = HwndWindowRect(win->hwndFrame);
        Rect cr = HwndClientRect(win->hwndFrame);
        logf("ShowMainWindow: windowRect=(%d,%d,%d,%d) clientRect=(%d,%d,%d,%d) captionRect=(%d,%d,%d,%d)\n", wr.x,
             wr.y, wr.dx, wr.dy, cr.x, cr.y, cr.dx, cr.dy, win->captionRect.x, win->captionRect.y, win->captionRect.dx,
             win->captionRect.dy);
    }

    // the `true ||` is deliberate (always foreground); silence /analyze C6286/C6240
#pragma warning(suppress : 6286 6240)
    if (len(gWindows) == 1 && (true || IsDebuggerPresent())) {
        HwndToForeground(win->hwndFrame);
    }

    if (win->tabsInTitlebar && !win->isFullScreen) {
        RECT r = ToRECT(win->captionRect);
        HwndInvalidateRect(win->hwndFrame, win->captionRect, true);
        RedrawWindow(win->hwndFrame, &r, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
        if (win->hwndMenuReBar && HwndIsVisible(win->hwndMenuReBar)) {
            RedrawWindow(win->hwndMenuReBar, nullptr, nullptr,
                         RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        }
        if (win->tabsCtrl && win->tabsCtrl->IsVisible()) {
            RedrawWindow(win->tabsCtrl->hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
        }
    }
}

// Kind used so only one default-app bar is shown at a time
static Kind kNotifDefaultApp = "defaultApp";
// Cap how many extension links we put in the bar
constexpr int kMaxDefaultAppLinks = 8;

// On the home page: if we registered as Open With for extensions that no longer
// open with us, show a bottom bar with per-extension fix links.
void MaybeShowDefaultAppNotification(MainWindow* win) {
    if (!win || !win->hwndCanvas || win->isBeingClosed) {
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
    if (len(missing) == 0) {
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifDefaultApp);
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
    args.hwndParent = win->hwndCanvas;
    args.msg = ToStrTemp(sb);
    args.timeoutMs = kNotifNoTimeout;
    args.groupId = kNotifDefaultApp;
    args.corner = NotifCorner::BottomBar;
    ShowNotification(args);
}

MainWindow* CreateAndShowMainWindow(SessionData* data, bool showWin) {
    int windowState = gGlobalPrefs->windowState;
    MainWindow* win = CreateMainWindow();
    if (!win) {
        return nullptr;
    }
    // CreateMainWindow can inadvertently change windowState (e.g. via layout); restore it
    gGlobalPrefs->windowState = windowState;

    if (data) {
        windowState = data->windowState;
        Rect rect = ShiftRectToWorkArea(data->windowPos);
        HwndMoveWindow(win->hwndFrame, &rect);
        // TODO: also restore data->sidebarDx
    }

    // always set up toolbar and sidebar, even if we defer showing
    ShowOrHideToolbar(win);
    SetSidebarVisibility(win, false, gGlobalPrefs->showFavorites);
    ToolbarUpdateStateForWindow(win, true);

    if (showWin) {
        ShowMainWindow(win, windowState);
    }
    return win;
}

void DeleteMainWindow(MainWindow* win) {
    int winIdx = gWindows.Remove(win);

    int nWindowsLeft = len(gWindows);
    logf("DeleteMainWindow: win: 0x%p, hwndFrame: 0x%p, hwndCanvas: 0x%p, winIdx : %d, nWindowsLeft: %d\n", win,
         win->hwndFrame, win->hwndCanvas, winIdx, nWindowsLeft);
    if (winIdx < 0) {
        logf("  not deleting because not in gWindows, probably already deleted\n");
        return;
    }

    DeletePropertiesWindow(win->hwndFrame);
    DestroyToolbar(win);
    RevokeCanvasDropTarget(win->hwndCanvas);

    ReportIf(win->findThread && WaitForSingleObject(win->findThread, 0) == WAIT_TIMEOUT);
    ReportIf(win->printThread && WaitForSingleObject(win->printThread, 0) == WAIT_TIMEOUT);

    // UIA disconnect/release is in ~MainWindow

    delete win;
}

void UpdateAfterThemeChange() {
    // the icon pixmaps are rendered in the theme's colors
    DestroySvgPixmapIconsCache();
    for (auto* win : gWindows) {
        DeleteObject(win->brControlBgColor);
        win->brControlBgColor = CreateSolidBrush(ThemeControlBackgroundColor());

        UpdateControlsColors(win);
        RebuildMenuBarForWindow(win);
        UpdateToolbarAfterThemeChange(win);
        RecreateFindBar(win);
        UpdateFindWindowTheme(win);
        RefreshSelectionToolbarIcons(win);
        UpdateAIChatTheme(win);
        DarkModeApplyToFrameAfterThemeChange(win);
        UpdateWindowFrameBorderColor(win);
        // TODO: this only rerenders canvas, not frame, even with
        // includingNonClientArea == true.
        MainWindowRerender(win, true);
        uint flags = RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN;
        RedrawWindow(win->hwndFrame, nullptr, nullptr, flags);
    }
    UpdateDocumentColors();
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
        // TODO: merge favorites as well?
        if (len(*fs->favorites) > 0) {
            UpdateFavoritesTreeForAllWindows();
        }
        DeleteFileState(fs);
    }
    fs = FileHistoryFindByPath(oldPath);
    if (fs) {
        SetFileStatePath(fs, newPath);
        // merge Frequently Read data, so that a file
        // doesn't accidentally vanish from there
        fs->isPinned = fs->isPinned || oldIsPinned;
        fs->openCount += oldOpenCount;
        // the thumbnail is recreated by LoadDocument
        FreePixmap(fs->thumbnail);
        fs->thumbnail = nullptr;
    }
}

static void ReloadTab(WindowTab* tab) {
    // tab might have been closed, so first ensure it's still valid
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/1958
    MainWindow* win = FindMainWindowByTab(tab);
    if (win == nullptr) {
        return;
    }
    tab->reloadOnFocus = true;
    if (tab == win->CurrentTab()) {
        // delay the reload slightly, in case we get another request immediately after this one
        SetTimer(win->hwndCanvas, AUTO_RELOAD_TIMER_ID, AUTO_RELOAD_DELAY_IN_MS, nullptr);
    }
}

static void ScheduleReloadTab(WindowTab* tab) {
    auto fn = MkFunc0<WindowTab>(ReloadTab, tab);
    uitask::Post(fn, "ReloadTab");
}

static void AutoReloadResetFileState(WindowTab* tab) {
    tab->autoReloadSize = -1;
    tab->autoReloadModTime = {};
    tab->autoReloadStartMs = 0;
}

// Called from the AUTO_RELOAD_TIMER tick. Returns true if the file changed
// since the previous tick, i.e. whoever is writing it isn't done: the caller
// then re-arms the timer instead of reloading a half-written document.
//
// The first tick after a notification always reports "changing" (there's no
// previous state to compare against), so a reload happens one interval later
// than it used to. That's deliberate: a LaTeX run used to produce two reloads,
// one of a truncated file ("document has no pages") and one of the finished
// file.
//
// Gives up after kAutoReloadMaxWaitMs so a file that is appended to
// continuously (a log being tailed) still reloads.
//
// Note: file::GetSize()/GetModificationTime() go through the 1-hour network
// attribute cache, so on a network drive both values look stable right away
// and we reload immediately, as before.
bool AutoReloadFileStillChanging(WindowTab* tab) {
    if (!tab || !tab->filePath) {
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

// return true if adjustd path
static bool AdjustPathForMaybeMovedFile(LoadArgs* args) {
    Str path = args->FilePath();
    if (DocumentPathExists(path)) {
        return false;
    }
    bool failEarly = args->win && !args->forceReuse && !args->engine;
    bool fileInHistory = FileHistoryFindByPath(path) != nullptr;
    if (!failEarly || !fileInHistory) {
        return failEarly;
    }
    // try to find non-existent files with history data
    // on a different removable drive before failing
    Str adjPath = str::DupTemp(path);
    if (AdjustVariableDriveLetter(adjPath)) {
        RenameFileInHistory(path, adjPath);
        args->SetFilePath(adjPath);
    }
    return false;
}

static void LoadDocumentMarkNotExist(MainWindow* win, Str path, bool noSavePrefs, bool showWin) {
    // don't show a deliberately hidden window (session restore at startup:
    // ShowMainWindow shows it later with the remembered maximized/fullscreen
    // state; showing here would flash a normal-size window first)
    if (showWin) {
        // Use ShowMainWindow so SW_SHOW does not drop a pending maximize (#5529)
        ShowMainWindow(win, gGlobalPrefs->windowState);
    }

    // display the notification ASAP (SaveSettings() can introduce a notable delay)
    win->RedrawAll(true);

    if (!FileHistoryMarkFileInexistent(path)) {
        return;
    }
    // TODO: handle this better. see https://github.com/sumatrapdfreader/sumatrapdf/issues/1674
    if (!noSavePrefs) {
        SaveSettings();
    }
    // update the Frequently Read list
    if (1 == len(gWindows) && gWindows[0]->IsCurrentTabAbout()) {
        gWindows[0]->RedrawAll(true);
    }
}

// files that failed to open, so that a broken file doesn't block next/prev
// file in folder. Not persisted: a file that fails now might open in the next
// session (it could be locked by another program right now).
// The nodes come from the perm arena: the list is small, lives for the whole
// session and is never freed
static StrNode* gFilesFailedToOpen = nullptr;

static void MarkFileFailedToOpen(Str path) {
    if (!path || FindStrNode(gFilesFailedToOpen, path)) {
        return;
    }
    StrNode* node = AllocStrNode(GetPermArena(), path);
    if (node) {
        ListInsertFront(&gFilesFailedToOpen, node);
    }
}

// a file that loads again is no longer broken (e.g. the program holding a lock
// on it exited), so stop skipping it when navigating the folder
static void MarkFileOpenedOk(Str path) {
    if (!path) {
        return;
    }
    StrNode* node = FindStrNode(gFilesFailedToOpen, path);
    if (node) {
        // only unlinked; the node's bytes stay in the perm arena
        ListRemove(&gFilesFailedToOpen, node);
    }
}

// Why a document failed to load. "Error loading foo.pdf" on its own leaves the
// user guessing, and the three cases they can actually do something about -
// the file is gone, they may not read it, another program has it locked - are
// cheap to tell apart by trying to open it the way the engines do. Anything
// that opens but doesn't load is a format we don't handle or a damaged file.
static TempStr FileLoadErrorReasonTemp(Str path) {
    if (!file::Exists(path)) {
        return str::DupTemp(_TRA("The file does not exist"));
    }
    WCHAR* pathW = CWStrTemp(path);
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE h = CreateFileW(pathW, GENERIC_READ, share, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        return str::DupTemp(_TRA("The file format is not supported or the file is damaged"));
    }
    DWORD err = GetLastError();
    switch (err) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return str::DupTemp(_TRA("The file does not exist"));
        case ERROR_ACCESS_DENIED:
            return str::DupTemp(_TRA("Access denied"));
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            return str::DupTemp(_TRA("The file is in use by another program"));
    }
    return GetLastErrorStrTemp(err);
}

// remember why, so the canvas can show it next to "Error loading <file>"
static void SetTabLoadError(WindowTab* tab, Str path) {
    // also remember the file itself: next/prev in folder skips files that
    // failed, so it doesn't stop on the same broken file over and over (#5917).
    // Loads that never got a tab are marked in LoadDocument()
    MarkFileFailedToOpen(path);
    if (!tab) {
        return;
    }
    tab->loadState = WindowTab::LoadState::Error;
    str::Free(tab->loadErrorReason);
    tab->loadErrorReason = str::Dup(FileLoadErrorReasonTemp(path));
}

static void ShowFileNotFound(MainWindow* win, Str path, bool noSavePrefs, bool showWin) {
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.warning = true;
    nargs.msg = fmt(_TRA("File %s not found").s, path);
    nargs.plainText = true; // `path` is not ours, don't parse it as tip markup
    ShowNotification(nargs);
    LoadDocumentMarkNotExist(win, path, noSavePrefs, showWin);
}

// A failed load that had no tab of its own used to leave only a notification.
// Without a tab there is no document context, so the file could not be handed to
// an external viewer or shown in its folder, and opening several bad files at
// once replaced one notification with the next (issue #3595). Give the failure a
// tab instead: the canvas paints the error screen for a tab that has a path and
// no controller, and the file keeps a place in the UI to act on.
static void ShowLoadErrorInTab(MainWindow* win, LoadArgs* args, Str path) {
    WindowTab* tab = args->targetTab;
    if (!tab && args->forceReuse) {
        // reload / replace of the current document: the error belongs in its tab
        tab = win->CurrentTab();
        if (tab && tab->IsAboutTab()) {
            tab = nullptr;
        }
    }
    bool isNewTab = false;
    if (!tab && win->tabsCtrl && !gPluginMode) {
        tab = new WindowTab(win);
        tab->SetFilePath(path);
        tab->SetDisplayName(args->DisplayName());
        // must be before AddTabToWindow(): selecting the tab runs
        // LoadModelIntoTab(), which would see LoadState::None and try to load
        // the file all over again
        SetTabLoadError(tab, path);
        AddTabToWindow(win, tab);
        // like the successful path does: the new tab is the current one
        win->currentTabTemp = tab;
        isNewTab = true;
    }
    if (!tab) {
        // nowhere to put it (plugin mode has no tabs)
        ShowErrorLoadingNotification(win, path, args->noSavePrefs, args->showWin);
        return;
    }
    if (!isNewTab) {
        SetTabLoadError(tab, path);
    }
    win->ctrl = nullptr;
    if (tab == win->CurrentTab()) {
        // the title bar names the file that failed, like it did before the tab
        // went away
        SetFrameTitleForTab(tab, false);
        HwndInvalidate(win->hwndCanvas);
    }
    LoadDocumentMarkNotExist(win, path, args->noSavePrefs, args->showWin);
}

void ShowErrorLoadingNotification(MainWindow* win, Str path, bool noSavePrefs, bool showWin) {
    // Same translation as Canvas OnPaintError ("Error loading %s").
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.msg = fmt("%s: %s", fmt(_TRA("Error loading %s").s, path), FileLoadErrorReasonTemp(path));
    // `path` is attacker-controlled, so the message must not be parsed as tip
    // markup: a "[x](CmdExec ...)" in it would become a clickable command link
    nargs.plainText = true;
    nargs.warning = true;
    nargs.timeoutMs = 1000 * 5;
    ShowNotification(nargs);
    LoadDocumentMarkNotExist(win, path, noSavePrefs, showWin);
}

extern void SetTabState(WindowTab* tab, TabState* state);

// we call this via uitask::Post so that SaveSettings() doesn't run
// synchronously in the middle of LoadDocumentFinish while other
// documents may still be loading or tabs are being closed
// (fixes crashes with dangling tab->ctrl under rapid DDE opens + hooks)
static void SaveSettingsVoid() {
    SaveSettings();
}

// delete a loaded-but-not-yet-attached controller when its target window went
// away mid-load. If the window was already destroyed, its cbHandler is gone
// too, so null cb to keep ~DisplayModel from calling into freed memory
// (nothing was rendered for the orphan, so skipping cb->CleanUp() is fine).
static void DeleteOrphanedController(MainWindow* win, DocController*& ctrl) {
    if (!ctrl) {
        return;
    }
    if (!IsMainWindowValid(win)) {
        ctrl->cb = nullptr;
    }
    delete ctrl;
    ctrl = nullptr;
}

MainWindow* LoadDocumentFinish(LoadArgs* args) {
    MainWindow* win = args->win;
    Str fullPath = args->FilePath();
    WindowTab* targetTab = args->targetTab;

    // it loaded, so it's no longer one of the files next/prev skips
    MarkFileOpenedOk(fullPath);

    bool openNewTab = SettingsUseTabs() && !args->forceReuse;
    ReportIf(openNewTab && args->forceReuse);

    // Clear any tip from the previous document/tab (LoadModelIntoTab also does
    // this on tab switch; cover the first open / about-page case here too).
    win->DeleteToolTip();

    if (targetTab) {
        ReportIf(targetTab != win->CurrentTab());
        if (targetTab != win->CurrentTab()) {
            DeleteOrphanedController(win, args->ctrl);
            return nullptr;
        }
        targetTab->loadState = WindowTab::LoadState::None;
        targetTab->loadCopyBytesCopied = -1;
        targetTab->loadCopyBytesTotal = 0;
    } else if (win->IsCurrentTabAbout()) {
        // drop the About / home page's virtual controls; rebuilt on next paint
        HomePageDestroyChrome(win);
        // there's no tab to reuse at this point
        args->forceReuse = false;
    } else {
        if (openNewTab) {
            SaveCurrentWindowTab(args->win);
        }
        CloseDocumentInCurrentTab(win, true, args->forceReuse);
    }
    if (targetTab) {
        targetTab->SetFilePath(fullPath);
        targetTab->SetDisplayName(args->DisplayName());
    } else if (!args->forceReuse) {
        // insert a new tab for the loaded document
        WindowTab* tab = new WindowTab(win);
        tab->SetFilePath(fullPath);
        tab->SetDisplayName(args->DisplayName());
        win->currentTabTemp = AddTabToWindow(win, tab);

        if (!IsMainWindowValid(win) || win->isBeingClosed) {
            // the ctrl was not attached to the tab yet, don't leak it
            DeleteOrphanedController(win, args->ctrl);
            return nullptr;
        }

        // logf("LoadDocument: !forceReuse, created win->CurrentTab() at 0x%p\n", win->CurrentTab());
    } else {
        win->CurrentTab()->SetFilePath(fullPath);
        win->CurrentTab()->SetDisplayName(args->DisplayName());
#if 0
        auto path = ToUtf8Temp(fullPath);
        logf("LoadDocument: forceReuse, set win->CurrentTab() (0x%p) filePath to '%s'\n", win->CurrentTab(), path.Get());
#endif
    }

    // TODO: stop remembering/restoring window positions when using tabs?
    args->placeWindow = !SettingsUseTabs();
    bool lazyLoad = args->lazyLoad;
    if (!lazyLoad) {
        if (!IsMainWindowValid(win) || win->isBeingClosed) {
            // the ctrl was not attached to the tab yet, don't leak it
            DeleteOrphanedController(win, args->ctrl);
            return nullptr;
        }
        ReplaceDocumentInCurrentTab(args, args->ctrl, nullptr);
    }

    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        return nullptr;
    }

    if (gPluginMode) {
        // hide the menu for embedded documents opened from the plugin
        SetMenu(win->hwndFrame, nullptr);
        return win;
    }

    auto* currTab = win->CurrentTab();
    currTab->loadState = WindowTab::LoadState::None;
    currTab->loadCopyBytesCopied = -1;
    currTab->loadCopyBytesTotal = 0;
    Str path = currTab->filePath;
#if 0
    int nPages = 0;
    if (currTab->ctrl) {
        nPages = currTab->ctrl->PageCount();
    }
    logf("LoadDocument: after ReplaceDocumentInCurrentTab win->CurrentTab() is 0x%p, path: '%s', %d pages\n", currTab,
         path.Get(), nPages);
#endif
    // when lazy loading: first time remember tab state, second time is
    // real loading so restore tab state
    if (!currTab->ctrl && !currTab->tabState) {
        currTab->tabState = args->tabState;
    } else if (currTab->tabState) {
        SetTabState(currTab, currTab->tabState);
        currTab->tabState = nullptr;
    }
    // forceReuse / targetTab loads skip CloseDocumentInCurrentTab, so the
    // previous document's watcher can still be set (e.g. open next file in
    // folder, multi-file open). Always drop it before re-subscribing.
    FileWatcherUnsubscribe(currTab->watcher);
    currTab->watcher = nullptr;

    if (gGlobalPrefs->reloadModifiedDocuments) {
        auto fn = MkFunc0(ScheduleReloadTab, currTab);
        // was gGlobalPrefs->enableTeXEnhancements because people complained
        // about network traffic. but then people complained it stopped working
        // we'll now recommend ReloadModifiedDocuments = false for those
        // who complain
        bool enableManualCheck = true;
        currTab->watcher = FileWatcherSubscribe(path, fn, enableManualCheck);
    }

    if (SettingsRememberOpenedFiles()) {
        ReportIf(!str::Eq(fullPath, path));
        FileState* ds = FileHistoryMarkFileLoaded(fullPath);
        if (gGlobalPrefs->showStartPage) {
            CreateThumbnailForFile(win, ds);
        }
        // TODO: this seems to save the state of file that we just opened
        // add a way to skip saving currTab?
        if (!args->noSavePrefs) {
            auto fn = MkFunc0Void(SaveSettingsVoid);
            uitask::Post(fn, "SaveSettingsAfterDocLoad");
        }
    }

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    if (CanAccessDisk() && !gPluginMode && !IsStressTesting()) {
        AddPathToRecentDocs(fullPath);

        // Remove Zone.Identifier (Mark of the Web) so that Windows Explorer
        // will show previews/thumbnails for this file without security warnings
        file::DeleteZoneIdentifier(fullPath);
    }

    return win;
}

static MainWindow* MaybeCreateWindowForFileLoad(LoadArgs* args) {
    MainWindow* win = args->win;
    bool openNewTab = SettingsUseTabs() && !args->forceReuse;
    if (openNewTab && !args->win) {
        // modify the args so that we always reuse the same window
        // TODO: enable the tab bar if tabs haven't been initialized
        if (len(gWindows) > 0) {
            win = gWindows.Last();
            args->win = win;
            args->isNewWindow = false;
        }
    }

    if (!win && 1 == len(gWindows) && gWindows[0]->IsCurrentTabAbout()) {
        win = gWindows[0];
        args->win = win;
        args->isNewWindow = false;
    } else if (!win || !openNewTab && !args->forceReuse && win->IsDocLoaded()) {
        MainWindow* currWin = win;
        // during startup, create window hidden to avoid flashing the about page;
        // it will be shown by ReplaceDocumentInCurrentTab or ShowMainWindow later
        win = CreateAndShowMainWindow(nullptr, !gIsStartup);
        if (!win) {
            return nullptr;
        }
        args->win = win;
        args->isNewWindow = true;
        if (currWin) {
            RememberFavTreeExpansionState(currWin);
            win->expandedFavorites = currWin->expandedFavorites;
        }
    }
    return win;
}

struct LoadDocumentAsyncData {
    LoadArgs* args = nullptr;
    LoadDocumentAsyncData() = default;
    ~LoadDocumentAsyncData() { delete args; }
};

static void LoadDocumentAsync(LoadDocumentAsyncData* d);

// When loading many documents at once (e.g. multiple files on the cmd-line or
// dropped together) we don't want to spawn an unbounded number of loading
// threads. Cap the number of concurrent background loads and queue the rest;
// each finished load starts the next queued one. All of this state is only
// touched on the UI thread (StartLoadDocument and LoadDocumentAsyncFinish),
// so it needs no locking.
static int gLoadThreadsActive = 0;
static int gMaxLoadThreads = 0;
static Vec<LoadDocumentAsyncData*> gLoadQueue;
static bool gLoadQueueDispatchPosted = false;
static UINT_PTR gLoadingMessageTimer = 0;

static void CALLBACK LoadingMessageTimerProc(HWND /*hwnd*/, UINT /*msg*/, UINT_PTR timerId, DWORD /*time*/) {
    bool hasLoadingTabs = false;
    u64 now = GetTickCount64();
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (tab->loadState != WindowTab::LoadState::Loading) {
                continue;
            }
            hasLoadingTabs = true;
            if (tab == win->CurrentTab() && tab->loadStartedAt != 0 && now - tab->loadStartedAt >= 1000) {
                HwndInvalidate(win->hwndCanvas);
            }
        }
    }
    if (!hasLoadingTabs) {
        KillTimer(nullptr, timerId);
        gLoadingMessageTimer = 0;
    }
}

static void StartLoadingMessageTimer(WindowTab* tab) {
    if (!tab) {
        return;
    }
    tab->loadStartedAt = GetTickCount64();
    tab->loadCopyBytesCopied = -1;
    tab->loadCopyBytesTotal = 0;
    if (gLoadingMessageTimer == 0) {
        gLoadingMessageTimer = SetTimer(nullptr, 0, 1000, LoadingMessageTimerProc);
    }
}

static bool IsLoadTargetValid(LoadArgs* args) {
    if (!IsMainWindowValid(args->win) || args->win->isBeingClosed) {
        return false;
    }
    return !args->targetTab || FindMainWindowByTab(args->targetTab) == args->win;
}

static void DispatchQueuedDocumentLoads();

static void StartLoadDocumentThread(LoadDocumentAsyncData* data) {
    // loading status is painted on the tab canvas (StartLoadingMessageTimer);
    // start the timer only now that we're actually loading, not while the
    // file was sitting in the queue
    LoadArgs* args = data->args;
    // Snapshot HWND for the password dialog before leaving the UI thread.
    if (args->win && args->win->hwndFrame) {
        args->hwndPwdParent = args->win->hwndFrame;
    }
    StartLoadingMessageTimer(args->targetTab);
    gLoadThreadsActive++;
    auto fn = MkFunc0<LoadDocumentAsyncData>(LoadDocumentAsync, data);
    RunAsync(fn, "LoadDocumentThread");
}

// start a background load now if a thread slot is free, otherwise queue it
static void StartOrQueueLoadDocument(LoadDocumentAsyncData* data) {
    gLoadQueue.Append(data);
    if (gLoadQueueDispatchPosted) {
        return;
    }
    gLoadQueueDispatchPosted = true;
    if (gMaxLoadThreads == 0) {
        // at most min(4, CpuCoreCount()) concurrent loads
        int n = CpuCoreCount();
        gMaxLoadThreads = n < 4 ? n : 4;
    }
    auto fn = MkFunc0Void(DispatchQueuedDocumentLoads);
    uitask::Post(fn, "DispatchDocumentLoads");
}

static void DispatchQueuedDocumentLoads() {
    gLoadQueueDispatchPosted = false;
    if (gMaxLoadThreads == 0) {
        // at most min(4, CpuCoreCount()) concurrent loads
        int n = CpuCoreCount();
        gMaxLoadThreads = n < 4 ? n : 4;
    }
    while (gLoadThreadsActive < gMaxLoadThreads && len(gLoadQueue) > 0) {
        int idx = 0;
        for (int i = 0; i < len(gLoadQueue); i++) {
            LoadArgs* args = gLoadQueue[i]->args;
            if (IsLoadTargetValid(args) && args->targetTab && args->targetTab == args->win->CurrentTab()) {
                idx = i;
                break;
            }
        }
        LoadDocumentAsyncData* next = gLoadQueue.PopAt(idx);
        if (!IsLoadTargetValid(next->args)) {
            EndDocumentLoad(next->args->FilePath());
            next->args->onFinished.Call(false);
            delete next;
            continue;
        }
        StartLoadDocumentThread(next);
    }
}

// called on the UI thread when a background load finishes; frees its slot
// and starts the next queued load, if any
static void OnLoadDocumentThreadFinished() {
    gLoadThreadsActive--;
    ReportIf(gLoadThreadsActive < 0);
    DispatchQueuedDocumentLoads();
}

// true if targetTab still wants this load (path not replaced by a newer open)
static bool IsLoadStillWanted(LoadArgs* args) {
    if (!args || !args->targetTab) {
        return true;
    }
    Str want = args->targetTab->filePath;
    Str got = args->FilePath();
    if (str::EqI(want, got)) {
        return true;
    }
    if (len(want) > 0 && len(got) > 0 && path::IsSame(want, got)) {
        return true;
    }
    return false;
}

// Drop engine/controller produced by a load whose window/tab went away.
static void DiscardFailedAsyncLoad(LoadArgs* args) {
    if (!args) {
        return;
    }
    SafeEngineRelease(&args->engine);
    DeleteOrphanedController(args->win, args->ctrl);
}

static void LoadDocumentAsyncFinish(LoadDocumentAsyncData* d) {
    AutoDelete delData(d);
    OnLoadDocumentThreadFinished();

    auto* args = d->args;
    Str path = args->FilePath();
    EndDocumentLoad(path);
    MainWindow* win = args->win;
    if (!IsLoadTargetValid(args)) {
        DiscardFailedAsyncLoad(args);
        args->onFinished.Call(false);
        return;
    }
    // forceReuse next/prev can start a newer load into the same tab while this
    // one is still finishing; drop the stale result instead of swapping docs
    if (!IsLoadStillWanted(args)) {
        DiscardFailedAsyncLoad(args);
        args->onFinished.Call(false);
        return;
    }

    // DisplayModel needs win->cbHandler: create it only on the UI thread after
    // the window is known to still exist (load thread must not touch MainWindow).
    if (!args->ctrl && args->engine) {
        EngineBase* eng = args->engine;
        args->engine = nullptr;
        args->ctrl = CreateControllerForEngineOrFile(eng, path, nullptr, win);
        // CreateControllerForEngineOrFile takes ownership of eng (or releases it)
    }

    if (args->targetTab) {
        args->targetTab->loadStartedAt = 0;
        args->targetTab->loadCopyBytesCopied = -1;
        args->targetTab->loadCopyBytesTotal = 0;
    }
    if (!args->ctrl) {
        ShowLoadErrorInTab(win, args, path);
        // re-sync win->ctrl with current tab after ShowErrorLoadingNotification
        // which can pump messages and change tab selection
        WindowTab* currTab = win->CurrentTab();
        win->ctrl = currTab ? currTab->ctrl : nullptr;
        args->onFinished.Call(false);
        return;
    }
    if (args->targetTab && args->targetTab != win->CurrentTab()) {
        WindowTab* tab = args->targetTab;
        tab->loadState = WindowTab::LoadState::LoadedPending;
        tab->pendingLoadArgs = args;
        d->args = nullptr;
        args->onFinished.Call(true);
        return;
    }
    args->activateExisting = false;
    LoadDocumentFinish(args);
    args->onFinished.Call(true);
}

// Network-drive cbx cache copy progress. Stored on the loading tab and
// painted on the canvas when that tab is selected; the elapsed-time timer
// only invalidates and re-reads these fields so it does not replace the
// copy message.
struct CopyProgressUITask {
    WindowTab* targetTab = nullptr;
    i64 bytesCopied = 0;
    i64 bytesTotal = 0;
};

static void UpdateCopyProgressUI(CopyProgressUITask* task) {
    AutoDelete delTask(task);
    WindowTab* tab = task->targetTab;
    MainWindow* win = FindMainWindowByTab(tab);
    if (!win) {
        return;
    }
    ReportIf(tab->win != win);
    tab->loadCopyBytesCopied = task->bytesCopied;
    tab->loadCopyBytesTotal = task->bytesTotal;
    if (tab == win->CurrentTab()) {
        HwndInvalidate(tab->win->hwndCanvas);
    }
}

struct CopyProgressState {
    WindowTab* targetTab = nullptr;
};

static void OnFileCopyProgress(CopyProgressState* s, file::CopyProgress* p) {
    if (!s->targetTab) {
        return;
    }
    auto* task = new CopyProgressUITask;
    task->targetTab = s->targetTab;
    task->bytesCopied = p->bytesCopied;
    task->bytesTotal = p->bytesTotal;
    auto fn = MkFunc0<CopyProgressUITask>(UpdateCopyProgressUI, task);
    uitask::Post(fn, "CopyProgress");
}

// Background load thread: create the engine only. Never touch MainWindow* /
// WindowTab* here — the UI thread may CloseWindow/DeleteMainWindow while we
// are in CreateEngineFromFile (ASan heap-use-after-free on win->cbHandler).
// DisplayModel is built on the UI thread in LoadDocumentAsyncFinish.
static void LoadDocumentAsync(LoadDocumentAsyncData* d) {
    auto* args = d->args;
    AtomicIntInc(&gDangerousThreadCount);
    Str path = args->FilePath();
    EngineBase* engine = args->engine;

    // Network-drive cbx cache copy reports bytes onto the loading tab canvas.
    // targetTab is only used to post UI tasks; FindMainWindowByTab rejects dead tabs.
    CopyProgressState copyState;
    copyState.targetTab = args->targetTab;
    if (args->targetTab) {
        file::gFileCopyProgressCb = MkFunc1<CopyProgressState, file::CopyProgress*>(OnFileCopyProgress, &copyState);
    }

    HwndPasswordUI pwdUI(args->hwndPwdParent);
    bool chmInFixedUI = gGlobalPrefs->chmUI.useFixedPageUI;
    if (!engine) {
        engine = CreateEngineFromFile(path, &pwdUI, chmInFixedUI);
    }
    if (engine && engine->pageCount <= 0) {
        // same guard as CreateControllerForEngineOrFile
        SafeEngineRelease(&engine);
    }
    args->engine = engine;
    // args->ctrl stays null until LoadDocumentAsyncFinish

    file::gFileCopyProgressCb = {};

    auto fn = MkFunc0<LoadDocumentAsyncData>(LoadDocumentAsyncFinish, d);
    uitask::Post(fn, "TaskLoadDocumentAsyncFinish");
    AtomicIntDec(&gDangerousThreadCount);
}

// height / width of the area a document is displayed in, 0 if not known yet
static float EbookLayoutAspectForWindow(MainWindow* win) {
    if (!win) {
        return 0;
    }
    // A hidden frame is the startup window, which is shown (and maximized) only
    // after the document loads -- its canvas still has the creation size.
    bool canvasIsReal = win->hwndCanvas && HwndIsVisible(win->hwndFrame);
    Rect rc = canvasIsReal ? HwndClientRect(win->hwndCanvas) : Rect();
    if (rc.dx > 0 && rc.dy > 0) {
        return (float)rc.dy / (float)rc.dx;
    }
    // So use the size the window is about to be restored to. That is the frame,
    // not the canvas, so take off the toolbar and tab bar: guessing too tall
    // would leave the page overflowing, which is the whole bug.
    if (gGlobalPrefs->windowState == WIN_STATE_MAXIMIZED || gGlobalPrefs->windowState == WIN_STATE_FULLSCREEN) {
        HMONITOR mon = MonitorFromWindow(win->hwndFrame, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof(mi)};
        if (GetMonitorInfoW(mon, &mi)) {
            rc = ToRect(mi.rcWork);
        }
    } else {
        rc = gGlobalPrefs->windowPos;
    }
    if (rc.dx < 1 || rc.dy < 1) {
        // nothing remembered (first run): the hidden frame already has the
        // size it will be shown at
        rc = win->hwndFrame ? HwndClientRect(win->hwndFrame) : Rect();
    }
    if (rc.dx < 1 || rc.dy < 1) {
        return 0;
    }
    int chromeDy = 0;
    if (gGlobalPrefs->showToolbar) {
        chromeDy += DpiScale(40);
    }
    if (SettingsUseTabs()) {
        chromeDy += DpiScale(34);
    }
    rc.dy = std::max(rc.dy - chromeDy, 100);
    return (float)rc.dy / (float)rc.dx;
}

void StartLoadDocument(LoadArgs* argsIn) {
    if (gCrashOnOpen) {
        log("LoadDocumentAsync: about to call CrashMe()\n");
        CrashMe();
    }

    MainWindow* win = argsIn->win;
    bool failEarly = AdjustPathForMaybeMovedFile(argsIn);
    Str path = argsIn->FilePath();
    if (failEarly) {
        ShowFileNotFound(win, path, argsIn->noSavePrefs, argsIn->showWin);
        argsIn->onFinished.Call(false);
        return;
    }

    if (argsIn->activateExisting) {
        MainWindow* limitWin = argsIn->activateExistingInWindow ? win : nullptr;
        MainWindow* existing = FindMainWindowByFile(path, true, limitWin);
        if (existing) {
            existing->Focus();
            argsIn->onFinished.Call(true);
            return;
        }
        // Tab is created only after load finishes; without this, a second open of
        // the same path while a password dialog is up creates a duplicate tab.
        if (IndexOfLoadingFile(path) >= 0) {
            logf("StartLoadDocument: skipping in-flight load of '%s'\n", path);
            argsIn->onFinished.Call(true);
            return;
        }
    }

    win = MaybeCreateWindowForFileLoad(argsIn);
    if (!win) {
        argsIn->onFinished.Call(false);
        return;
    }

    if (argsIn->targetTab) {
        argsIn->targetTab->loadState = WindowTab::LoadState::Loading;
        if (argsIn->targetTab == win->CurrentTab()) {
            HwndInvalidate(win->hwndCanvas);
        }
    } else if (SettingsUseTabs() && !argsIn->forceReuse) {
        if (!win->IsCurrentTabAbout()) {
            SaveCurrentWindowTab(win);
            CloseDocumentInCurrentTab(win, true, false);
        }
        WindowTab* tab = new WindowTab(win);
        tab->SetFilePath(path);
        tab->SetDisplayName(argsIn->DisplayName());
        tab->loadState = WindowTab::LoadState::Loading;
        argsIn->targetTab = AddTabToWindow(win, tab);
        win->currentTabTemp = argsIn->targetTab;
        LoadModelIntoTab(argsIn->targetTab);
    } else if (argsIn->forceReuse && win->CurrentTab() && !win->IsCurrentTabAbout()) {
        // Reuse current tab (next/prev in folder, navigate dialog, etc.): drop the
        // old document immediately so the canvas paints the standard "Loading ..."
        // message while the engine loads on a background thread.
        WindowTab* tab = win->CurrentTab();
        DeleteOldSelectionInfo(win, true);
        if (tab->ctrl || win->ctrl) {
            CloseDocumentInCurrentTab(win, true, true);
        }
        tab->SetFilePath(path);
        tab->SetDisplayName(argsIn->DisplayName());
        tab->loadState = WindowTab::LoadState::Loading;
        tab->loadCopyBytesCopied = -1;
        tab->loadCopyBytesTotal = 0;
        argsIn->targetTab = tab;
        win->currentTabTemp = tab;
        win->ctrl = nullptr;
        SetFrameTitleForTab(tab, false);
        UpdateUiForCurrentTab(win);
        TabsOnChangedDoc(win);
        // show loading UI right away (don't wait for the background thread to start)
        StartLoadingMessageTimer(tab);
        HwndInvalidate(win->hwndCanvas);
        UpdateWindow(win->hwndCanvas);
    } else if (SettingsUseTabs() && win->CurrentTab()) {
        argsIn->targetTab = win->CurrentTab();
        argsIn->targetTab->loadState = WindowTab::LoadState::Loading;
        HwndInvalidate(win->hwndCanvas);
    }

    // Lay reflowable ebooks out for this window's shape, so Fit Width shows a
    // whole page instead of one that overflows the window (issue #3472). Done
    // here because the load thread must not touch MainWindow.
    EngineMupdfSetEbookLayoutAspect(EbookLayoutAspectForWindow(win));

    LoadArgs* args = argsIn->Clone();
    BeginDocumentLoad(path);

    // when using mshtml to display CHM files, we can't load in a thread
    // TODO: that's because we create web control on a thread which
    // violates threading rules and that happens as part of CreateControllerForEngineOrFile()
    // we could probably delay creating web control but that's more complicated
    {
        FileType kind = GuessFileTypeFromName(path);
        bool isChm = !gGlobalPrefs->chmUI.useFixedPageUI && ChmModel::IsSupportedFileType(kind);
        bool isMd = ShouldUseBrowserView(kind);
        if (isChm || isMd) {
            // TODO: repeating the code below
            HwndPasswordUI pwdUI(win->hwndFrame ? win->hwndFrame : nullptr);
            EngineBase* engine = args->engine;
            StartLoadingMessageTimer(args->targetTab);
            args->ctrl = CreateControllerForEngineOrFile(engine, path, &pwdUI, win);
            if (args->targetTab) {
                args->targetTab->loadStartedAt = 0;
                args->targetTab->loadCopyBytesCopied = -1;
                args->targetTab->loadCopyBytesTotal = 0;
            }
            EndDocumentLoad(path);
            // CreateController can pump messages (WebView2 / COM / password UI)
            if (!IsLoadTargetValid(args)) {
                DiscardFailedAsyncLoad(args);
                args->onFinished.Call(false);
                delete args;
                return;
            }
            if (!args->ctrl) {
                ShowLoadErrorInTab(win, args, path);
                // re-sync win->ctrl with current tab after ShowErrorLoadingNotification
                // which can pump messages and change tab selection
                WindowTab* currTab = win->CurrentTab();
                win->ctrl = currTab ? currTab->ctrl : nullptr;
                args->onFinished.Call(false);
                delete args;
                return;
            }
            // Multi-file Open (StartLoadDocuments) selects only the first tab as
            // current. Browser-view loads finish synchronously here; if this tab
            // is not current, defer attach the same way LoadDocumentAsyncFinish
            // does — LoadModelIntoTab will call LoadDocumentFinish later.
            // Without this we ReportIf and drop the controller (crash 8c4d3ae8).
            if (args->targetTab && args->targetTab != win->CurrentTab()) {
                WindowTab* tab = args->targetTab;
                tab->loadState = WindowTab::LoadState::LoadedPending;
                tab->pendingLoadArgs = args;
                args->onFinished.Call(true);
                return;
            }
            args->activateExisting = false;
            LoadDocumentFinish(args);
            args->onFinished.Call(true);
            delete args;
            return;
        }
    }

    auto* data = new LoadDocumentAsyncData;
    data->args = args;
    StartOrQueueLoadDocument(data);
}

void StartLoadDocuments(StrVec& paths, MainWindow* win) {
    if (!SettingsUseTabs() || !win) {
        for (Str path : paths) {
            LoadArgs args(path, win);
            args.activateExisting = true;
            args.activateExistingInWindow = win != nullptr;
            StartLoadDocument(&args);
        }
        return;
    }

    StrVec pathsToLoad;
    for (Str path : paths) {
        if (!DocumentPathExists(path)) {
            LoadArgs args(path, win);
            StartLoadDocument(&args);
            continue;
        }
        if (FindMainWindowByFile(path, true, win)) {
            continue;
        }
        AppendIfNotExists(&pathsToLoad, path);
    }
    if (pathsToLoad.IsEmpty()) {
        return;
    }

    if (!win->IsCurrentTabAbout()) {
        SaveCurrentWindowTab(win);
    }

    Vec<WindowTab*> tabs;
    for (int i = 0; i < len(pathsToLoad); i++) {
        WindowTab* tab = new WindowTab(win);
        tab->SetFilePath(pathsToLoad[i]);
        tab->loadState = WindowTab::LoadState::Loading;
        bool deferUpdate = i != len(pathsToLoad) - 1;
        tabs.Append(AddTabToWindow(win, tab, deferUpdate));
    }
    WindowTab* visibleTab = tabs[0];
    win->tabsCtrl->SetSelected(win->GetTabIdx(visibleTab));
    LoadModelIntoTab(visibleTab);

    for (int i = 0; i < len(pathsToLoad); i++) {
        LoadArgs args(pathsToLoad[i], win);
        args.targetTab = tabs[i];
        StartLoadDocument(&args);
    }
}

// reads page count and creates a child element for each page
MainWindow* LoadDocument(LoadArgs* args) {
    if (gCrashOnOpen) {
        log("LoadDocument: about to call CrashMe()\n");
        CrashMe();
    }

    Str path = args->FilePath();
    if (args->activateExisting) {
        MainWindow* limitWin = args->activateExistingInWindow ? args->win : nullptr;
        MainWindow* existing = FindMainWindowByFile(path, true, limitWin);
        if (existing) {
            existing->Focus();
            return existing;
        }
        // Tab is created only after load finishes; without this, a second open of
        // the same path while a password dialog is up creates a duplicate tab.
        if (IndexOfLoadingFile(path) >= 0) {
            logf("LoadDocument: skipping in-flight load of '%s'\n", path);
            return args->win;
        }
    }

    MainWindow* win = args->win;
    bool failEarly = AdjustPathForMaybeMovedFile(args);

    // fail fast if the file doesn't exist and there is a window the user
    // has just been interacting with
    if (failEarly) {
        ShowFileNotFound(win, path, args->noSavePrefs, args->showWin);
        return nullptr;
    }

    win = MaybeCreateWindowForFileLoad(args);
    if (!win) {
        return nullptr;
    }

    // see the same call in StartLoadDocument (issue #3472)
    EngineMupdfSetEbookLayoutAspect(EbookLayoutAspectForWindow(win));

    BeginDocumentLoad(path);
    auto timeStart = TimeGet();
    HwndPasswordUI pwdUI(win->hwndFrame);
    DocController* ctrl = nullptr;
    if (!args->lazyLoad) {
        ctrl = CreateControllerForEngineOrFile(args->engine, path, &pwdUI, win);
        {
            auto durMs = TimeSinceInMs(timeStart);
            if (ctrl) {
                int nPages = ctrl->PageCount();
                logf("LoadDocument: %.2f ms, %d pages for '%s'\n", (float)durMs, nPages, path);
            } else {
                logf("LoadDocument: failed to load '%s' in %.2f ms\n", path, (float)durMs);
                MarkFileFailedToOpen(path);
            }
        }

        if (!ctrl) {
            EndDocumentLoad(path);
            // ensure window is visible even if loading failed
            // (it may have been created hidden during startup)
            if (!HwndIsVisible(win->hwndFrame)) {
                ShowMainWindow(win, gGlobalPrefs->windowState);
            }
            ShowLoadErrorInTab(win, args, path);
            // re-sync win->ctrl with current tab: the above can pump messages
            // and change tab selection
            WindowTab* currTab = win->CurrentTab();
            win->ctrl = currTab ? currTab->ctrl : nullptr;
            return win;
        }
    }
    args->ctrl = ctrl;
    MainWindow* result = LoadDocumentFinish(args);
    EndDocumentLoad(path);
    return result;
}

// Loads document data into the MainWindow.
void LoadModelIntoTab(WindowTab* tab) {
    if (!tab) {
        return;
    }

    MainWindow* win = tab->win;
    // Document content is about to change; drop any page-element / about-page tip
    // so it cannot linger over the new document.
    win->DeleteToolTip();
    // the numbered link overlay and the selection caret belong to the outgoing
    // document
    StopKeyboardLinkFollowing(win);
    StopSelectTextWithKeyboard(win);
    if (gGlobalPrefs->lazyLoading && win->ctrl && !tab->ctrl && !tab->IsNonDocumentTab() &&
        tab->loadState == WindowTab::LoadState::None) {
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.msg = fmt(_TRA("Please wait - loading...").s);
        args.warning = true;
        ShowNotification(args);
        // Use ShowMainWindow so SW_SHOW does not drop a pending maximize (#5529)
        ShowMainWindow(win, gGlobalPrefs->windowState);
        // display the notification ASAP
        win->RedrawAll(true);
    }
    // ShowWindow / RedrawAll can pump messages, potentially destroying win
    if (!IsMainWindowValid(win)) {
        return;
    }
    DisplayModel* prevDm = win->AsFixed();
    if (prevDm && win->isFullScreen && !win->InPresentation()) {
        prevDm->ApplyFullscreenDisplayMode(false);
    }
    CloseDocumentInCurrentTab(win, true, false);

    win->currentTabTemp = tab;
    win->ctrl = tab->ctrl;

    if (tab->loadState == WindowTab::LoadState::LoadedPending) {
        LoadArgs* args = tab->pendingLoadArgs;
        tab->pendingLoadArgs = nullptr;
        win->ctrl = nullptr;
        LoadDocumentFinish(args);
        delete args;
        return;
    }

    // Favorites tab: no document, no viewport — full-area tree via RelayoutFrame
    if (tab->IsFavoritesTab()) {
        InvalidateFindForDocumentChange(win);
        UpdateUiForCurrentTab(win);
        PopulateFavTreeIfNeeded(win);
        // force layout: sidebar vs full-tab favorites share showFavorites, and a
        // stale UILayout snapshot would skip RelayoutFrame (blank until re-select)
        win->uiState.layout = {};
        RelayoutFrame(win, true, -1);
        LayoutFavoritesContainer(win);
        // expand all file groups; start typing in the search box
        if (win->favTreeView) {
            win->favTreeView->ExpandAll();
            RedrawWindow(win->favTreeView->hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
        if (win->favFilterEdit) {
            HwndSetFocus(win->favFilterEdit->hwnd);
            win->favFilterEdit->SetCursorPositionAtEnd();
        }
        return;
    }

    // Find matches / count cache are for the previous tab's document. Drop them
    // before paint (UpdateWindow below). Keeps find box text (#5308); rebuilds
    // all-match highlights if find UI is still open.
    InvalidateFindForDocumentChange(win);

    if (win->AsChm()) {
        win->AsChm()->SetParentHwnd(win->hwndCanvas);
        FillCanvasThemeBackground(win->hwndCanvas);
    } else if (win->AsMarkdown()) {
        win->AsMarkdown()->SetParentHwnd(win->hwndCanvas);
        FillCanvasThemeBackground(win->hwndCanvas);
    } else if (win->AsFixed() && win->uiaProvider) {
        // tell UI Automation about content change
        win->uiaProvider->OnDocumentLoad(win->AsFixed());
    }

    UpdateUiForCurrentTab(win);
    PickAnotherRandomPromotion();

    if (win->InPresentation()) {
        SetSidebarVisibility(win, tab->showTocPresentation, gGlobalPrefs->showFavorites);
    } else {
        SetSidebarVisibility(win, tab->showToc, gGlobalPrefs->showFavorites);
    }

    // Leaving Favorites tab: restore canvas size/visibility before SetViewPortSize
    // (deferred ScheduleUiUpdate would leave canvas hidden / wrong size).
    win->uiState.layout = {};
    RelayoutFrame(win, true, -1);

    DisplayModel* dm = win->AsFixed();
    if (dm) {
        Size viewPort = win->GetViewPortSize();
        if (viewPort.IsEmpty()) {
            // still no canvas (e.g. minimized); skip Relayout that asserts in CalcZoomReal
            logf("LoadModelIntoTab: empty viewport, skipping SetViewPortSize\n");
        } else if (tab->canvasRc != win->canvasRc) {
            win->ctrl->SetViewPortSize(viewPort);
        } else {
            // avoid double setting of scroll state -> it gets triggered by SetViewPortSize();
            dm->SetScrollState(dm->GetScrollState());
        }
        if (dm->InPresentation() != win->InPresentation()) {
            dm->SetInPresentation(win->InPresentation());
        } else if (win->isFullScreen && !win->InPresentation()) {
            dm->ApplyFullscreenDisplayMode(true);
        }
    } else if (IsBrowserDocController(win->ctrl)) {
        win->ctrl->GoToPage(win->ctrl->CurrentPageNo(), false);
    }
    tab->canvasRc = win->canvasRc;

    win->showSelection = tab->selectionOnPage != nullptr;
    if (win->showSelection) {
        ShowSelectionToolbar(win);
    }
    if (win->uiaProvider) {
        win->uiaProvider->OnSelectionChanged();
    }

    HwndSetFocus(win->hwndFrame);
    if (tab->type == WindowTab::Type::None) {
        logf("LoadModelIntoTab: tab 0x%p has Type::None, skipping reload\n", tab);
    } else if (!tab->IsAboutTab()) {
        if (gGlobalPrefs->lazyLoading && !tab->ctrl && tab->loadState == WindowTab::LoadState::None) {
            ReloadDocument(win, false);
        } else {
            if (tab->reloadOnFocus) {
                tab->reloadOnFocus = false;
                ReloadDocument(win, true);
            }
        }
    }
    HwndInvalidate(win->hwndCanvas);
    UpdateWindow(win->hwndCanvas);

    // show/hide notifications that are tied to a specific tab
    ShowNotificationsForActiveTab(win->hwndCanvas, tab);
    // CloseDocumentInCurrentTab cleared page-info; restore if still wanted
    ShowPageInfoIfWanted(win);

    if (IsMainWindowValid(win)) {
        bool aiChatWas = win->uiState.aiChatVisible;
        AIChatSyncPanelsToCurrentTab(win);
        if (aiChatWas != win->uiState.aiChatVisible) {
            ScheduleUiUpdate(win);
        }
        OnAIChatTabChanged(win);
    }
}

enum class MeasurementUnit {
    pt,
    mm,
    in
};

static TempStr FormatCursorPositionTemp(EngineBase* engine, PointF pt, MeasurementUnit unit) {
    pt.x = std::max(pt.x, 0.0f);
    pt.y = std::max(pt.y, 0.0f);
    pt.x /= engine->GetFileDPI();
    pt.y /= engine->GetFileDPI();

    // for MeasurementUnit::in
    float factor = 1;
    Str unitName = "in";
    if (unit == MeasurementUnit::pt) {
        factor = 72;
        unitName = "pt";
    } else if (unit == MeasurementUnit::mm) {
        factor = 25.4f;
        unitName = "mm";
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
    ReportIf(!win->AsFixed());
    EngineBase* engine = win->AsFixed()->GetEngine();
    PointF pt = win->AsFixed()->CvtFromScreen(pos);
    TempStr posStr = FormatCursorPositionTemp(engine, pt, cursorPosUnit);
    TempStr selStr = {};
    if (!win->selectionMeasure.IsEmpty()) {
        pt = PointF(win->selectionMeasure.dx, win->selectionMeasure.dy);
        selStr = FormatCursorPositionTemp(engine, pt, cursorPosUnit);
    }

    TempStr posInfo = fmt("%s %s", _TRA("Cursor position:"), posStr);
    if (selStr) {
        posInfo = fmt("%s - %s %s", posInfo, _TRA("Selection:"), selStr);
    }
    NotificationUpdateMessage(wnd, posInfo);
}

// re-render the document currently displayed in this window
void MainWindowRerender(MainWindow* win, bool includeNonClientArea) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    // don't wait for in-flight renders: dm stays alive and a render that lands
    // after this is either still valid or dropped by the darkModeEpoch check
    gRenderCache->AbortRendering(dm);
    gRenderCache->KeepForDisplayModel(dm, dm);
    if (includeNonClientArea) {
        win->RedrawAllIncludingNonClient();
    } else {
        win->RedrawAll(true);
    }
}

static void RerenderEverything() {
    for (auto* win : gWindows) {
        // rerender the currently displayed tab right away
        MainWindowRerender(win);
        // drop cached renders of the other (non-current) tabs so they
        // get re-rendered with the new colors when switched to (issue #5646)
        DisplayModel* currentDm = win->AsFixed();
        for (WindowTab* tab : win->Tabs()) {
            DisplayModel* dm = tab->AsFixed();
            if (dm && dm != currentDm) {
                // no wait: only reached after darkModeEpoch was bumped, so a
                // render still in flight is discarded when it finishes
                gRenderCache->AbortRendering(dm);
                gRenderCache->FreeForDisplayModel(dm);
            }
        }
    }
}

static void RerenderFixedPage() {
    for (auto* win : gWindows) {
        if (win->AsFixed()) {
            MainWindowRerender(win, true);
        }
    }
}

void UpdateDocumentColors() {
    Color bg;
    Color text = ThemePageRenderColors(bg);
    bool pagesDark = !IsLightColor(bg);
    Color link = pagesDark ? ThemeWindowLinkColor() : 0;

    // dark-mode options that also affect rendered pages but not the two
    // cache colors; a change must invalidate cached renders the same way
    static bool s_lastPreservePdfImages = false;
    static int s_lastDocumentColorsFollowTheme = -1;
    bool preservePdfImages = pagesDark && GetPreservePdfImagesInDarkMode();
    int documentColorsFollowTheme = (int)GetDocumentColorsFollowTheme();

    if ((text == gRenderCache->textColor) && (bg == gRenderCache->backgroundColor) &&
        (link == gRenderCache->linkColor) && preservePdfImages == s_lastPreservePdfImages &&
        documentColorsFollowTheme == s_lastDocumentColorsFollowTheme) {
        return; // colors didn't change
    }
    s_lastPreservePdfImages = preservePdfImages;
    s_lastDocumentColorsFollowTheme = documentColorsFollowTheme;

    gRenderCache->textColor = text;
    gRenderCache->backgroundColor = bg;
    gRenderCache->linkColor = link;
    gRenderCache->darkModeEpoch++;

    // also drop the engines' cached dark-mode analyses / processed images
    // and regenerate markdown previews (their colors are baked into the html)
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            DisplayModel* dm = tab->AsFixed();
            if (dm) {
                EngineMupdfInvalidateDarkMode(dm->GetEngine());
                continue;
            }
            MarkdownModel* mm = tab->AsMarkdown();
            if (mm) {
                mm->UpdateTheme();
            }
        }
    }

    RerenderEverything();
}

void UpdateFixedPageScrollbarsVisibility() {
    bool showOverlayScrollbar = ScrollbarsUseOverlay();
    auto mode = ScrollbarsOverlayMode();
    for (MainWindow* w : gWindows) {
        OverlayScrollbarSetMode(w->overlayScrollV, mode);
        OverlayScrollbarSetMode(w->overlayScrollH, mode);
        OverlayScrollbarShow(w->overlayScrollV, showOverlayScrollbar);
        OverlayScrollbarShow(w->overlayScrollH, showOverlayScrollbar);
        // changing the scrollbar mode changes whether window scrollbars reserve
        // canvas space, so the usable viewport changes. Relayout the document
        // (recomputes page layout and window-scrollbar visibility for the new
        // mode), the same way a resize does; RerenderFixedPage() then redraws.
        if (DisplayModel* dm = w->AsFixed()) {
            dm->SetViewPortSize(w->GetViewPortSize());
        }
    }
    RerenderFixedPage();
}

static void OnMenuExit() {
    if (gPluginMode) {
        return;
    }

    for (MainWindow* win : gWindows) {
        if (!CanCloseWindow(win)) {
            return;
        }
    }

    // we want to preserve the session state of all windows,
    // so we save it now
    // since we are closing the windows one by one,
    // CloseWindow() must not save the session state every time
    // (or we will end up with just the last window)
    SaveSettings();
    gDontSaveSettings = true;

    // CloseWindow removes the MainWindow from gWindows,
    // so use a stable copy for iteration
    Vec<MainWindow*> toClose = gWindows;
    for (MainWindow* win : toClose) {
        CloseWindow(win, true, false);
    }
}

// quit by going through the same path as CmdExit so that windows close
// their tabs before ~MainWindow (a raw PostQuitMessage leaves tabs open
// and trips TabCount() > 0 in the WinMain cleanup loop)
void PostAppExit() {
    if (len(gWindows) == 0) {
        PostQuitMessage(0);
        return;
    }
    PostMessageW(gWindows[0]->hwndFrame, WM_COMMAND, CmdExit, 0);
}

// closes a document inside a MainWindow and optionally turns it into
// about window (set keepUIEnabled if a new document will be loaded
// into the tab right afterwards and ReplaceDocumentInCurrentTab would revert
// the UI disabling afterwards anyway)
static void CloseDocumentInCurrentTab(MainWindow* win, bool keepUIEnabled, bool deleteModel) {
    // tear down any in-place form-field edit before the model/engine goes away,
    // so the overlay's widget pointer can't dangle (cancel: don't write/re-render
    // a document that's being closed or reloaded)
    CommitFormFieldEdit(false);
    bool wasntFixed = !win->AsFixed();
    // the canvas HWND is shared across tabs; wipe leftover page pixels so a
    // following markdown/CHM tab cannot flash this document on resize
    if (!wasntFixed) {
        FillCanvasThemeBackground(win->hwndCanvas);
    }
    if (win->AsChm()) {
        win->AsChm()->RemoveParentHwnd();
    } else if (win->AsMarkdown()) {
        win->AsMarkdown()->RemoveParentHwnd();
    }
    ClearTocBox(win);
    // stop render threads before waiting on find: they hold pagesLock/renderLock
    // that the find thread needs for text extraction (issue: stress-test hang in
    // AbortFinding while RenderCacheThread holds engine locks).
    //
    // Only actually wait for them when there is a find to wait on. With no find
    // running this call has nothing to protect -- the model outlives us here
    // (deleteModel callers free it later, and ~DisplayModel does its own
    // CancelRenderingBlocking()) -- and blocking on an in-flight image decode froze the
    // UI for the length of the decode on every tab switch.
    if (DisplayModel* dm = win->AsFixed()) {
        bool findRunning = win->findThread || win->findCountThread;
        if (findRunning) {
            gRenderCache->CancelRenderingBlocking(dm);
        } else {
            gRenderCache->AbortRendering(dm);
        }
    }
    AbortFinding(win, true);

    ClearMouseState(win);
    win->annotationUnderCursor = nullptr;
    win->annotationBeingDragged = nullptr;
    win->annotationBeingResized = false;

    win->fwdSearchMark.show = false;
    // hide the citation-hover popup and cancel a pending hover: it
    // belongs to the document being closed / replaced
    RefHoverHide(win->refHover, win->hwndCanvas);
    if (win->uiaProvider) {
        win->uiaProvider->OnDocumentUnload();
    }
    win->ctrl = nullptr;
    WindowTab* currentTab = win->CurrentTab();
    if (currentTab) {
        currentTab->selectedAnnotation = nullptr;
        ResetReadAloudStateForTab(currentTab);
        // Edit panel holds non-owning Annotation* into the engine about to die.
        if (deleteModel) {
            CloseAndDeleteEditAnnotationsWindow(currentTab);
        }
    }
    if (deleteModel) {
        if (currentTab) {
            DeleteControllerAsync(currentTab->ctrl);
            currentTab->ctrl = nullptr;
            FileWatcherUnsubscribe(currentTab->watcher);
            currentTab->watcher = nullptr;
        }
    } else {
        win->currentTabTemp = nullptr;
    }
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifActionResponse);
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifPageInfo);
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifCursorPos);
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifZoomOrView);

    // Tab/document change aborts any in-progress drag/select. Without releasing
    // capture, LoadModelIntoTab left the canvas capturing the mouse after the
    // previous tab's button-down (cf. OnSelectionStop).
    win->mouseAction = MouseAction::None;
    if (win->hwndCanvas && GetCapture() == win->hwndCanvas) {
        ReleaseCapture();
    }

    DeletePropertiesWindow(win->hwndFrame);

    {
        // on 3.4.6 we would call DeleteOldSelectionInfo()
        // but it wouldn't delete tab->selectionOnPage because
        // win->currentTab was null. In 3.5 we changed
        // to win->GetCurrentTab() which always returns something
        // other calls to DeleteOldSelectionInfo() might
        // incorrectly clear tab->selectionOnPage
        win->showSelection = false;
        win->selectionMeasure = SizeF();
    }

    if (!keepUIEnabled) {
        SetSidebarVisibility(win, false, gGlobalPrefs->showFavorites);
        ToolbarUpdateStateForWindow(win, true);
        UpdateToolbarPageText(win, 0);
        UpdateToolbarFindText(win);
        UpdateFindbox(win);
        UpdateTabWidth(win);
        if (wasntFixed) {
            // restore the full menu and toolbar
            RebuildMenuBarForWindow(win);
            ShowOrHideToolbar(win);
        }
        if (!ScrollbarsAreHidden() && !ScrollbarsUseOverlay()) {
            ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);
        }
        win->RedrawAll();
        HwndSetText(win->hwndFrame, kSumatraWindowTitle);
        ReportIf(win->TabCount() != 0 || win->CurrentTab());
    }

    // Note: this causes https://code.google.com/archive/p/sumatrapdf/issues/2702. For whatever reason
    // edit ctrl doesn't receive WM_KILLFOCUS if we do SetFocus() here, even if we call SetFocus() later on
    // HwndSetFocus(win->hwndFrame);
}

static void ShowSavedAnnotationsNotification(HWND hwndParent, Str path) {
    str::Builder msg;
    msg.Append(fmt(_TRA("Saved annotations to '%s'").s, path));
    NotificationCreateArgs nargs;
    nargs.hwndParent = hwndParent;
    nargs.font = GetDefaultGuiFont();
    nargs.timeoutMs = 5000;
    nargs.msg = ToStr(msg);
    nargs.plainText = true; // `path` is not ours, don't parse it as tip markup
    ShowNotification(nargs);
}

static void ShowSavedAnnotationsFailedNotification(HWND hwndParent, Str path, Str mupdfErr) {
    str::Builder msg;
    msg.Append(fmt(_TRA("Failed to save '%s': %s").s, path, mupdfErr));
    // both `path` and the mupdf error come from the document, so no markup
    ShowPlainWarningNotification(hwndParent, ToStr(msg), 0);
}

struct ShowErrorData {
    WindowTab* tab;
    Str path;
};

static void ShowSaveAnnotationError(ShowErrorData* d, Str err) {
    auto* tab = d->tab;
    auto path = d->path;
    ShowSavedAnnotationsFailedNotification(tab->win->hwndCanvas, path, err);
}

// Identity of the selected annotation for restore after save/reload (Annotation*
// pointers die with the engine).
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

bool SaveAnnotationsToExistingFile(WindowTab* tab) {
    if (!tab) {
        return false;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return false;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return false;
    }
    Str path = engine->FilePath();
    tab->ignoreNextAutoReload = true;
    ShowErrorData data{tab, path};
    auto fn = MkFunc1(ShowSaveAnnotationError, &data);
    bool ok = EngineMupdfSaveUpdated(engine, nullptr, fn);
    if (!ok) {
        tab->ignoreNextAutoReload = false;
        return false;
    }
    ShowSavedAnnotationsNotification(tab->win->hwndCanvas, path);

    // Capture selection before the engine (and Annotation*) is torn down.
    SavedAnnotSel sel = CaptureSelectedAnnotation(tab);
    // have to re-open edit annotations window because the current has
    // a reference to deleted Engine
    bool hadEditAnnotations = CloseAndDeleteEditAnnotationsWindow(tab);
    ReloadDocument(tab->win, false);
    // Re-arm: the save notifies the file watcher, which schedules an auto-reload.
    // We already reloaded above; skip that one watcher event so we do not open
    // the PDF twice (and race background work against a just-rewritten file).
    tab->ignoreNextAutoReload = true;
    if (hadEditAnnotations) {
        Annotation* match = FindMatchingAnnotation(tab, sel);
        ShowEditAnnotationsWindow(tab, match);
    }

    return true;
}

static void InvokeInverseSearch(WindowTab* tab) {
    if (!tab) {
        return;
    }
    if (!gGlobalPrefs->enableTeXEnhancements) {
        return;
    }
    MainWindow* win = tab->win;
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    OnInverseSearch(win, pt.x, pt.y);
}

// returns true if saved successully
bool SaveAnnotationsToMaybeNewPdfFile(WindowTab* tab) {
    if (!tab) {
        return false;
    }
    WCHAR dstFileName[MAX_PATH + 1]{};

    OPENFILENAME ofn{};
    str::Builder fileFilter(256);
    fileFilter.Append(_TRA("PDF documents"));
    fileFilter.Append("\1*.pdf\1");
    fileFilter.Append("\1*.*\1");
    Str fileFilterStr = ToStr(fileFilter);
    str::TransCharsInPlace(fileFilterStr, StrL("\1"), StrL("\0"));
    WCHAR* fileFilterW = CWStrTemp(fileFilterStr);

    EngineBase* engine = tab->AsFixed()->GetEngine();
    TempStr srcFileName = str::Dup(engine->FilePath());
    // Seed the dialog with "foo Copy.pdf" so Save doesn't overwrite the source
    // unless the user deliberately picks the original name.
    TempStr noExt = path::GetPathNoExtTemp(srcFileName);
    TempStr ext = path::GetExtTemp(srcFileName);
    TempStr suggested = fmt("%s Copy%s", noExt, ext);
    TempWStr suggestedW = ToWStrTemp(suggested);
    wstr::BufSet(WStr(dstFileName, dimof(dstFileName)), suggestedW);

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = tab->win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilterW;
    ofn.nFilterIndex = 1;
    // ofn.lpstrTitle = _TRA("Rename To");
    // ofn.lpstrInitialDir = initDir;
    ofn.lpstrDefExt = L".pdf";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    bool ok = GetSaveFileNameW(&ofn);
    if (!ok) {
        str::Free(srcFileName);
        return false;
    }
    TempStr dstFilePath = ToUtf8Temp(dstFileName);
    bool savingToExisting = str::Eq(dstFilePath, srcFileName);
    if (savingToExisting) {
        str::Free(srcFileName);
        return SaveAnnotationsToExistingFile(tab);
    }

    ShowErrorData data{tab, dstFilePath};
    auto fn = MkFunc1(ShowSaveAnnotationError, &data);
    ok = EngineMupdfSaveUpdated(engine, dstFilePath, fn);
    if (!ok) {
        str::Free(srcFileName);
        return false;
    }

    // Capture selection before the engine (and Annotation*) is torn down.
    SavedAnnotSel sel = CaptureSelectedAnnotation(tab);
    // have to re-open edit annotations window because the current has
    // a reference to deleted Engine
    bool hadEditAnnotations = CloseAndDeleteEditAnnotationsWindow(tab);

    auto* win = tab->win;
    UpdateTabFileDisplayStateForTab(tab);
    CloseDocumentInCurrentTab(win, true, true);
    HwndSetFocus(win->hwndFrame);

    TempStr newPath = path::NormalizeTemp(dstFilePath);
    // TODO: this should be 'duplicate FileInHistory"
    RenameFileInHistory(srcFileName, newPath);
    str::Free(srcFileName);

    LoadArgs args(newPath, win);
    args.forceReuse = true;
    LoadDocument(&args);

    ShowSavedAnnotationsNotification(win->hwndCanvas, newPath);
    if (hadEditAnnotations) {
        Annotation* match = FindMatchingAnnotation(tab, sel);
        ShowEditAnnotationsWindow(tab, match);
    }
    return true;
}

enum class SaveChoice {
    Discard,
    SaveNew,
    SaveExisting,
    Cancel,
};

static SaveChoice ShouldSaveAnnotationsDialog(HWND hwndParent, Str filePath) {
    TempStr fileName = path::GetBaseNameTemp(filePath);
    TempStr mainInstrA = fmt(_TRA("Unsaved changes in '%s'").s, fileName);
    WCHAR* mainInstr = CWStrTemp(mainInstrA);
    auto content = _TRA("Save changes?");

    constexpr int kBtnIdDiscard = 100;
    constexpr int kBtnIdSaveToExisting = 101;
    constexpr int kBtnIdSaveToNew = 102;
    // constexpr int kBtnIdCancel = 103;
    TASKDIALOGCONFIG dialogConfig{};
    TASKDIALOG_BUTTON buttons[4];

    buttons[0].nButtonID = kBtnIdSaveToExisting;
    auto s = _TRA("&Save to existing PDF");
    buttons[0].pszButtonText = CWStrTemp(s);
    buttons[1].nButtonID = kBtnIdSaveToNew;
    s = _TRA("Save to &new PDF");
    buttons[1].pszButtonText = CWStrTemp(s);
    buttons[2].nButtonID = kBtnIdDiscard;
    s = _TRA("&Discard changes");
    buttons[2].pszButtonText = CWStrTemp(s);
    buttons[3].nButtonID = IDCANCEL;
    s = _TRA("&Cancel");
    buttons[3].pszButtonText = CWStrTemp(s);

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    s = _TRA("Unsaved changes");
    dialogConfig.pszWindowTitle = CWStrTemp(s);
    dialogConfig.pszMainInstruction = mainInstr;
    dialogConfig.pszContent = CWStrTemp(content);
    dialogConfig.nDefaultButton = IDCANCEL;
    dialogConfig.dwFlags = (TASKDIALOG_FLAGS)flags;
    dialogConfig.cxWidth = 0;
    dialogConfig.pfCallback = nullptr;
    dialogConfig.dwCommonButtons = 0;
    dialogConfig.cButtons = dimof(buttons);
    dialogConfig.pButtons = &buttons[0];
    dialogConfig.pszMainIcon = TD_INFORMATION_ICON;
    dialogConfig.hwndParent = hwndParent;

    int buttonPressedId = 0;

    auto hr = TaskDialogIndirect(&dialogConfig, &buttonPressedId, nullptr, nullptr);
    ReportIf(hr == E_INVALIDARG);
    bool discard = (hr != S_OK) || (buttonPressedId == kBtnIdDiscard);
    if (discard) {
        return SaveChoice::Discard;
    }
    switch (buttonPressedId) {
        case kBtnIdSaveToExisting:
            return SaveChoice::SaveExisting;
        case kBtnIdSaveToNew:
            return SaveChoice::SaveNew;
        case kBtnIdDiscard:
            return SaveChoice::Discard;
        case IDCANCEL:
            return SaveChoice::Cancel;
    }
    ReportIf(true);
    return SaveChoice::Cancel;
}

// if returns true, can proceed with closing
// if returns false, should cancel closing
// false if the user canceled (don't proceed with closing/replacing the doc)
bool MaybeSaveAnnotations(WindowTab* tab) {
    if (!tab) {
        return true;
    }
    // TODO: hacky because CloseCurrentTab() can call CloseWindow() and
    // they both ask to save annotations
    // Could determine in CloseCurrentTab() if will CloseWindow() and
    // not ask
    if (tab->askedToSaveAnnotations) {
        return true;
    }

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return true;
    }
    EngineBase* engine = dm->GetEngine();
    // shouldn't really happen but did happen.
    // don't block stress testing if opening a document flags it hasving unsaved annotations
    if (IsStressTesting()) {
        return true;
    }
    // if the file no longer exists (e.g. USB removed, network drive disconnected),
    // don't try to access it - engine uses memory-mapped I/O and accessing
    // pages of a gone file causes EXCEPTION_IN_PAGE_ERROR
    auto filePath = dm->GetFilePath();
    if (!file::Exists(filePath)) {
        logf("MaybeSaveAnnotations: file '%s' no longer exists, skipping\n", filePath);
        return true;
    }
    bool shouldConfirm = EngineHasUnsavedAnnotations(engine);
    if (!shouldConfirm) {
        return true;
    }
    tab->askedToSaveAnnotations = true;
    MainWindow* win = tab->win;
    auto path = dm->GetFilePath();
    auto choice = ShouldSaveAnnotationsDialog(win->hwndFrame, path);
    // the dialog pumps messages; during that, the window can be destroyed
    // (e.g. WM_CLOSE from plugin host) which frees win and tab
    if (!IsMainWindowValid(win)) {
        return true;
    }
    switch (choice) {
        case SaveChoice::Discard:
            return true;
        case SaveChoice::SaveNew: {
            bool didSave = SaveAnnotationsToMaybeNewPdfFile(tab);
            if (!didSave) {
                tab->askedToSaveAnnotations = false;
            }
            return didSave;
        }
        case SaveChoice::SaveExisting: {
            // const char* path = engine->FileName();
            ShowErrorData data{tab, path};
            auto fn = MkFunc1(ShowSaveAnnotationError, &data);
            bool didSave = EngineMupdfSaveUpdated(engine, nullptr, fn);
            if (!didSave) {
                // fn reported the error. Don't close: that would discard the
                // annotations. Re-arm the prompt so the user can retry or pick
                // Discard, same as the SaveNew case above.
                tab->askedToSaveAnnotations = false;
                return false;
            }
        } break;
        case SaveChoice::Cancel:
            tab->askedToSaveAnnotations = false;
            return false;
        default:
            ReportIf(true);
    }
    return true;
}

void CloseTab(WindowTab* tab, bool quitIfLast) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    logf("CloseTab: tab: 0x%p win: 0x%p, hwndFrame: 0x%x, quitIfLast: %d, dm: 0x%p\n", tab, win, win->hwndFrame,
         (int)quitIfLast, tab->AsFixed());

    AbortFinding(win, true);
    // Dismiss find UI when closing the active tab so floating results from that
    // document cannot be clicked after a different tab is shown (issue #5807).
    // Tab switches already call HideFindBar via SaveCurrentWindowTab.
    // Same for the floating selection/highlight toolbar: it holds a tab*
    // and would otherwise stay on screen after the tab is gone.
    if (tab == win->CurrentTab()) {
        HideFindBar(win);
        HideSelectionToolbar(win);
    }
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifPageInfo);
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifAnnotation);
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifZoomOrView);
    RemoveNotificationsForTab(tab);

    RememberRecentlyClosedDocument(tab->filePath);

    // TODO: maybe should have a way to over-ride this for unconditional close?
    bool canClose = MaybeSaveAnnotations(tab);
    if (!canClose) {
        return;
    }
    // MaybeSaveAnnotations() can show a dialog that pumps messages.
    // During message pumping, the window might be destroyed
    // (e.g., WM_DESTROY from a plugin host). If so, everything
    // is already cleaned up by the reentrant CloseWindow().
    if (!IsMainWindowValid(win)) {
        return;
    }

    // Stop eventual TTS reading. The full reset (rather than just
    // StopReadAloudIfSourceTab) also drops the pointers to this tab held by the
    // playback bar and the session, which is about to be a dangling one
    ResetReadAloudStateForTab(tab);

    int tabCount = win->TabCount();
    if (tabCount == 1 || (tabCount == 0 && quitIfLast)) {
        if (CanCloseWindow(win)) {
            CloseWindow(win, quitIfLast, false);
            return;
        }
    } else {
        ReportIf(gPluginMode && !gWindows.Contains(win));
        RemoveTab(tab);
        // RemoveTab -> LoadModelIntoTab can pump messages, potentially destroying win
        // and its cbHandler. Since tab was already removed from win's tab list,
        // ~MainWindow won't delete it, so we must delete it here.
        // Null out cb to prevent dangling pointer access in ~DisplayModel.
        if (!IsMainWindowValid(win)) {
            if (tab->ctrl) {
                tab->ctrl->cb = nullptr;
            }
            delete tab;
            return;
        }
        delete tab;
    }

    if (!IsMainWindowValid(win)) {
        return;
    }

    tabCount = win->TabCount();
    WindowTab* lastTab = (tabCount == 1) ? win->GetTab(0) : nullptr;
    if (lastTab && lastTab->type == WindowTab::Type::About) {
        // showing only home page tab so remove it
        // if there are other windows, close this one
        if (len(gWindows) > 1) {
            CloseWindow(win, false, false);
        } else {
            tab = win->GetTab(0);
            // re-use quitIfLast logic
            CloseTab(tab, quitIfLast);
            return;
        }
    }

    SaveSettings();
}

// closes the current tab, selecting the next one
// if there's only a single tab left, the window is closed if there
// are other windows, else the Frequently Read page is displayed
void CloseCurrentTab(MainWindow* win, bool quitIfLast) {
    WindowTab* tab = win->CurrentTab();
    logf("CloseCurrentTab: tab: 0x%p win: 0x%p, hwndFrame: 0x%x, quitIfLast: %d\n", tab, win, win->hwndFrame,
         (int)quitIfLast);
    if (tab) {
        CloseTab(tab, quitIfLast);
    } else {
        // Close tabless Frequently Read/About page
        CloseWindow(win, true, false);
    }
}

bool CanCloseWindow(MainWindow* win) {
    if (!win) {
        return false;
    }
    // a plugin window should only be closed when its parent is destroyed
    if (gPluginMode && !gWindows.Contains(win)) {
        return false;
    }

    if (win->printThread && !win->printCanceled && WaitForSingleObject(win->printThread, 0) == WAIT_TIMEOUT) {
        UINT flags = MB_ICONEXCLAMATION | MB_YESNO | MbRtlReadingMaybe();
        auto caption = _TRA("Printing in progress.");
        auto msg = _TRA("Printing is still in progress. Abort and quit?");
        int res = MsgBox(win->hwndFrame, msg, caption, flags);
        if (IDNO == res) {
            return false;
        }
    }

    return true;
}

/* Close the documents associated with window 'hwnd'.
   Closes the window unless this is the last window in which
   case it switches to empty window and disables the "File\Close"
   menu item. */
void CloseWindow(MainWindow* win, bool quitIfLast, bool forceClose) {
    if (!win) {
        return;
    }
    // guard against reentrant CloseWindow calls triggered by Windows theme
    // system message pumping (uxtheme.dll). The forceClose=true path from
    // WM_DESTROY is the expected reentrant cleanup and must still proceed.
    if (win->isBeingClosed && !forceClose) {
        return;
    }
    logf("CloseWindow: win: 0x%p, hwndFrame: 0x%x, quitIfLast: %d, forceClose: %d\n", win, win->hwndFrame,
         (int)quitIfLast, (int)forceClose);
    win->isBeingClosed = true;
    ReportIf(forceClose && !quitIfLast);
    if (forceClose) {
        quitIfLast = true;
    }

    // when used as an embedded plugin, closing should happen automatically
    // when the parent window is destroyed (cf. WM_DESTROY)
    if (gPluginMode && !gWindows.Contains(win) && !forceClose) {
        win->isBeingClosed = false;
        return;
    }

    AbortFinding(win, true);
    AbortPrinting(win);

    for (auto& tab : win->Tabs()) {
        if (tab->AsFixed()) {
            tab->AsFixed()->pauseRendering = true;
        }
    }

    if (win->presentation) {
        ExitFullScreen(win);
    }

    bool canCloseWindow = true;
    for (auto& tab : win->Tabs()) {
        bool canCloseTab = MaybeSaveAnnotations(tab);
        if (!canCloseTab) {
            canCloseWindow = false;
        }
        // MaybeSaveAnnotations() can show a dialog that pumps messages.
        // During message pumping, the window might be destroyed by a
        // reentrant CloseWindow() call (e.g., from WM_DESTROY).
        if (!IsMainWindowValid(win)) {
            return;
        }
    }

    // TODO: should be more intelligent i.e. close the tabs we can and only
    // leave those where user cancelled closing
    // would have to remember a list of tabs to not close above
    // if list not empty, only close the tabs not on the list
    if (!canCloseWindow) {
        win->isBeingClosed = false;
        for (auto& tab : win->Tabs()) {
            if (tab->AsFixed()) {
                tab->AsFixed()->pauseRendering = false;
            }
        }
        return;
    }

    // Stop eventual TTS reading
    StopReadAloudIfSourceWindow(win);
    bool lastWindow = (1 == len(gWindows));
    // if not the last window, save after the window is removed from gWindows
    // (so its now-closed state isn't re-saved); via defer so it also runs on the
    // reentrant early-return path below (#5418, #5668)
    bool saveAfterClose = !lastWindow;
    defer {
        if (saveAfterClose) {
            SaveSettings();
        }
    };
    // RememberDefaultWindowPosition becomes a no-op once the window is hidden
    RememberDefaultWindowPosition(win);
    // hide the window before saving prefs (closing seems slightly faster that way)
    if (!lastWindow || quitIfLast) {
        ShowWindow(win->hwndFrame, SW_HIDE);
        // ShowWindow can pump messages. If the window is embedded (e.g. in Total Commander),
        // the host may react by sending WM_DESTROY, which triggers a reentrant CloseWindow()
        // that frees win. Check if win is still valid before continuing.
        if (!IsMainWindowValid(win)) {
            return;
        }
    }

    // if this is a last window, save state before closing window
    // if not last, save after closing window (#5418)
    if (lastWindow) {
        SaveSettings();
    }
    TabsOnCloseWindow(win);

    if (forceClose) {
        // WM_DESTROY has already been sent, so don't destroy win->hwndFrame again
        DeleteMainWindow(win);
    } else if (lastWindow && !quitIfLast) {
        /* last window - don't delete it */
        CloseDocumentInCurrentTab(win, false, false);
        win->isBeingClosed = false;
        HwndSetFocus(win->hwndFrame);
        ReportIf(!gWindows.Contains(win));
    } else {
        HWND hwnd = win->hwndFrame;
        DeleteMainWindow(win);
        DestroyWindow(hwnd);
    }

    if (lastWindow && quitIfLast) {
        int nWindows = len(gWindows);
        logf("Calling PostQuitMessage() in CloseWindow() because closing lastWindow, nWindows: %d\n", nWindows);
        ReportDebugIf(nWindows != 0);
        PostQuitMessage(0);
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
        fileFilter.Append(_TRA("XPS documents"));
    } else if (str::EqI(ext, StrL(".epub"))) { // NOLINT(bugprone-branch-clone): see kindEngineEpub below
        // .epub can be handled by kindEngineMupdf
        fileFilter.Append(_TRA("EPUB ebooks"));
    } else if (type == kindEngineDjVu) {
        fileFilter.Append(_TRA("DjVu documents"));
    } else if (type == kindEngineComicBooks) {
        fileFilter.Append(_TRA("Comic books"));
    } else if (type == kindEngineImage) {
        Str imgDefExt = ctrl->GetDefaultFileExt();
        if (len(imgDefExt) > 0 && imgDefExt.s[0] == '.') {
            imgDefExt = Str(imgDefExt.s + 1, imgDefExt.len - 1);
        }
        fileFilter.Append(fmt(_TRA("Image files (*.%s)").s, imgDefExt));
    } else if (type == kindEngineImageDir) {
        return false; // only show "All files"
    } else if (type == kindEnginePostScript) {
        fileFilter.Append(_TRA("PostScript documents"));
    } else if (type == kindEngineChm) {
        fileFilter.Append(_TRA("CHM documents"));
    } else if (type == kindEngineEpub) {
        fileFilter.Append(_TRA("EPUB ebooks"));
    } else if (type == kindEngineMobi) {
        fileFilter.Append(_TRA("Mobi documents"));
    } else if (type == kindEngineFb2) {
        fileFilter.Append(_TRA("FictionBook documents"));
    } else if (type == kindEnginePdb) {
        fileFilter.Append(_TRA("PalmDoc documents"));
    } else if (type == kindEngineTxt) {
        fileFilter.Append(_TRA("Text documents"));
    } else {
        fileFilter.Append(_TRA("PDF documents"));
    }
    return true;
}

static void SaveCurrentFileAs(MainWindow* win) {
    if (!CanAccessDisk()) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }

    auto* ctrl = win->ctrl;
    TempStr srcFileName = ctrl->GetFilePath();
    if (gPluginMode) {
        // fall back to a generic "filename" instead of the more confusing temporary filename
        srcFileName = "filename";
        TempStr urlName = url::GetFileNameTemp(gPluginURL);
        if (urlName) {
            srcFileName = urlName;
        }
    }

    if (!srcFileName) {
        ShowTemporaryNotification(win->hwndCanvas, _TRA("File path not available"), kNotif5SecsTimeOut);
        return;
    }

    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (EngineHasUnsavedAnnotations(engine)) {
        SaveAnnotationsToMaybeNewPdfFile(win->CurrentTab());
        return;
    }

    auto defExt = ctrl->GetDefaultFileExt();
    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::Builder fileFilter(256);
    if (AppendFileFilterForDoc(ctrl, fileFilter)) {
        fileFilter.Append(fmt("\1*%s\1", defExt));
    }
    fileFilter.Append(_TRA("All files"));
    fileFilter.Append("\1*.*\1");
    Str fileFilterStr = ToStr(fileFilter);
    str::TransCharsInPlace(fileFilterStr, StrL("\1"), StrL("\0"));

    WCHAR dstFileName[MAX_PATH];
    TempStr baseName = path::GetBaseNameTemp(srcFileName);
    str::BufSet(dstFileName, dimof(dstFileName), baseName);
    if (wstr::ContainsChar(WStr(dstFileName), L':')) {
        // handle embed-marks (for embedded PDF documents):
        // remove the container document's extension and include
        // the embedding reference in the suggested filename
        WStr colon = wstr::SliceFromChar(WStr(dstFileName), L':');
        wstr::TransCharsInPlace(colon, WStrL(L":"), WStrL(L"_"));
        int colonOff = (int)(colon.s - dstFileName);
        int extOff = colonOff;
        while (extOff > 0 && dstFileName[extOff] != L'.') {
            extOff--;
        }
        if (extOff == 0 && dstFileName[0] != L'.') {
            extOff = colonOff;
        }
        memmove(dstFileName + extOff, colon.s, (len(colon) + 1) * sizeof(WCHAR));
    } else if (wstr::EndsWithI(dstFileName, ToWStrTemp(defExt))) {
        // Remove the extension so that it can be re-added depending on the chosen filter
        int idx = len(dstFileName) - len(defExt);
        dstFileName[idx] = '\0';
    }

    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = CWStrTemp(fileFilterStr);
    ofn.nFilterIndex = 1;
    // defExt can be null, we want to skip '.'
    if (len(defExt) > 0 && defExt.s[0] == '.') {
        defExt = Str(defExt.s + 1, defExt.len - 1);
    }
    ofn.lpstrDefExt = CWStrTemp(defExt);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    // note: explicitly not setting lpstrInitialDir so that the OS
    // picks a reasonable default (in particular, we don't want this
    // in plugin mode, which is likely the main reason for saving as...)

    bool ok = GetSaveFileNameW(&ofn);
    if (!ok) {
        // GetSaveFileNameW() returns FALSE both on user cancellation (extended
        // error == 0) and on an actual failure such as a path that is too long
        // (FNERR_BUFFERTOOSMALL). Only the latter deserves a warning (issue #1016).
        DWORD cdErr = CommDlgExtendedError();
        if (cdErr != 0) {
            logf("GetSaveFileNameW() failed, CommDlgExtendedError() = 0x%x\n", (uint)cdErr);
            MessageBoxWarning(win->hwndFrame, _TRA("Failed to save a file"));
        }
        return;
    }

    // GetSaveFileNameW() runs a modal dialog that pumps messages.
    // During the dialog, a file watcher notification can trigger ReloadDocument(),
    // destroying the old engine and invalidating srcFileName, defExt, engine pointers.
    // Re-acquire everything from the (potentially new) controller.
    if (!win->IsDocLoaded()) {
        return;
    }
    ctrl = win->ctrl;
    srcFileName = ctrl->GetFilePath();
    if (gPluginMode) {
        srcFileName = "filename";
        TempStr urlName = url::GetFileNameTemp(gPluginURL);
        if (urlName) {
            srcFileName = urlName;
        }
    }
    if (!srcFileName) {
        ShowTemporaryNotification(win->hwndCanvas, _TRA("File path not available"), kNotif5SecsTimeOut);
        return;
    }
    defExt = ctrl->GetDefaultFileExt();
    if (len(defExt) > 0 && defExt.s[0] == '.') {
        defExt = Str(defExt.s + 1, defExt.len - 1);
    }
    dm = win->AsFixed();
    engine = dm ? dm->GetEngine() : nullptr;

    TempStr realDstFileName = ToUtf8Temp(dstFileName);

    // Make sure that the file has a valid extension
    if (!str::EndsWithI(realDstFileName, defExt)) {
        realDstFileName = str::JoinTemp(realDstFileName, defExt);
    }

    logf("Saving '%s' to '%s'\n", srcFileName, realDstFileName);

    // TODO: engine->SaveFileA() is stupid
    // Replace with EngineGetDocumentData() and save that if not empty
    TempStr errorMsg = nullptr;
    if (!file::Exists(srcFileName) && engine) {
        // Recreate nonexistent files from memory...
        logf("calling engine->SaveFileAs(%s)\n", realDstFileName);
        ok = engine->SaveFileAs(realDstFileName);
    } else if (!path::IsSame(srcFileName, realDstFileName)) {
        ok = file::Copy(realDstFileName, srcFileName, false);
        if (ok) {
            // Make sure that the copy isn't write-locked or hidden
            const DWORD attributesToDrop = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
            DWORD attributes = file::GetAttributes(realDstFileName);
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & attributesToDrop)) {
                file::SetAttributes(realDstFileName, attributes & ~attributesToDrop);
            }
        } else {
            DWORD err = 0;
            TempStr s = GetLastErrorStrTemp(err);
            if (len(s) > 0) {
                errorMsg = fmt("%s\n\n%s", _TRA("Failed to save a file"), s);
            }
        }
    }
    // Belt-and-suspenders: some failure modes (e.g. a destination path longer
    // than MAX_PATH) can report success while nothing was actually written, so
    // the user has no way to tell the save silently failed (issue #1016).
    if (ok && !file::Exists(realDstFileName)) {
        logf("SaveCurrentFileAs(): '%s' doesn't exist after a reportedly successful save\n", realDstFileName);
        ok = false;
    }
    if (!ok) {
        TempStr msg = errorMsg ? errorMsg : Str(_TRA("Failed to save a file"));
        logf("SaveCurrentFileAs() failed with '%s'\n", msg);
        MessageBoxWarning(win->hwndFrame, msg);
    }

    auto path = ctrl->GetFilePath();
    if (ok && IsUntrustedFile(path, gPluginURL)) {
        file::SetZoneIdentifier(realDstFileName);
    }
}

void SumatraOpenPathInDefaultFileManager(Str path) {
    if (gPluginMode || !CanAccessDisk()) {
        return;
    }
    OpenPathInDefaultFileManager(path);
}

static void ShowCurrentFileInFolder(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    auto* ctrl = win->ctrl;
    SumatraOpenPathInDefaultFileManager(ctrl->GetFilePath());
}

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
    if (model && model->currentPageUrl) {
        html = model->GetDataForUrl(model->currentPageUrl);
    } else {
        Str markdown = file::ReadFile(path);
        if (markdown) {
            html = MarkdownToHtmlPage(markdown);
            ownsHtml = true;
        }
        str::Free(markdown);
    }
    if (!html) {
        return;
    }

    TempStr tempPath = GetTempFilePathTemp(StrL("smd"));
    TempStr htmlPath = str::JoinTemp(tempPath, StrL(".html"));
    bool ok = tempPath && file::Rename(htmlPath, tempPath) && file::WriteFile(htmlPath, html);
    if (ownsHtml) {
        str::Free(html);
    }
    if (!ok) {
        file::Delete(tempPath);
        file::Delete(htmlPath);
        logf("ShowGeneratedMarkdownHtml: failed to create temporary HTML file\n");
        return;
    }

    WCHAR systemDir[MAX_PATH]{};
    UINT systemDirLen = GetSystemDirectoryW(systemDir, dimof(systemDir));
    if (systemDirLen == 0 || systemDirLen >= dimof(systemDir)) {
        logf("ShowGeneratedMarkdownHtml: failed to find the Windows system directory\n");
        return;
    }

    TempStr notepadPath = path::JoinTemp(ToUtf8Temp(systemDir), StrL("notepad.exe"));
    TempStr args = QuoteCmdLineArgTemp(htmlPath);
    AutoCloseHandle process(LaunchProcessWithCmdLine(notepadPath, args));
    if (!process) {
        logf("ShowGeneratedMarkdownHtml: failed to launch Notepad for '%s'\n", htmlPath);
    }
}

// tab showing path in any main window, or nullptr
WindowTab* FindTabByFilePath(Str path) {
    if (len(path) == 0) {
        return nullptr;
    }
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (path::IsSame(tab->filePath, path)) {
                return tab;
            }
        }
    }
    return nullptr;
}

// move to the recycle bin and forget it in the file history / thumbnail cache
void DeleteFileFromDiskAndHistory(Str path) {
    file::DeleteFileToTrash(path);
    DeleteThumbnailForFile(path);
    FileState* fs = FileHistoryFindByPath(path);
    if (fs) {
        FileHistoryRemove(fs);
        DeleteFileState(fs);
    }
    SaveSettings();
}

static void DeleteCurrentFile(MainWindow* win) {
    if (!CanAccessDisk()) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }
    if (gPluginMode) {
        return;
    }
    auto* ctrl = win->ctrl;
    TempStr path = str::DupTemp(ctrl->GetFilePath());
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

static void RenameCurrentFile(MainWindow* win) {
    if (!CanAccessDisk()) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }
    if (gPluginMode) {
        return;
    }

    auto* ctrl = win->ctrl;
    TempStr srcPath = str::DupTemp(ctrl->GetFilePath());
    // this happens e.g. for embedded documents and directories
    if (!file::Exists(srcPath)) {
        return;
    }

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    Str defExt = ctrl->GetDefaultFileExt();
    TempWStr defExtW = ToWStrTemp(defExt);
    str::Builder fileFilter(256);
    bool ok = AppendFileFilterForDoc(ctrl, fileFilter);
    ReportIf(!ok);
    fileFilter.Append(fmt("\1*%s\1", defExt));
    Str fileFilterStr = ToStr(fileFilter);
    str::TransCharsInPlace(fileFilterStr, StrL("\1"), StrL("\0"));

    WCHAR dstFilePathW[MAX_PATH];
    auto baseName = path::GetBaseNameTemp(srcPath);
    str::BufSet(dstFilePathW, dimof(dstFilePathW), baseName);
    // Remove the extension so that it can be re-added depending on the chosen filter
    if (wstr::EndsWithI(dstFilePathW, defExtW)) {
        int idx = len(dstFilePathW) - len(defExtW);
        dstFilePathW[idx] = '\0';
    }

    TempWStr srcPathW = ToWStrTemp(srcPath);
    TempWStr initDir = path::GetDirTemp(srcPathW);

    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFilePathW;
    ofn.nMaxFile = dimof(dstFilePathW);
    ofn.lpstrFilter = CWStrTemp(fileFilterStr);
    ofn.nFilterIndex = 1;
    // note: the other two dialogs are named "Open" and "Save As"
    auto s = _TRA("Rename To");
    ofn.lpstrTitle = CWStrTemp(s);
    ofn.lpstrInitialDir = initDir.s;
    ofn.lpstrDefExt = defExtW.s + 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    ok = GetSaveFileNameW(&ofn);
    if (!ok) {
        return;
    }
    TempStr dstFilePath = ToUtf8Temp(dstFilePathW);
    TempStr dstPathNormalized = path::NormalizeTemp(dstFilePath);
    TempStr srcPathNormalized = path::NormalizeTemp(srcPath);
    if (path::IsSame(srcPathNormalized, dstPathNormalized)) {
        return;
    }

    UpdateTabFileDisplayStateForTab(win->CurrentTab());
    CloseDocumentInCurrentTab(win, true, true);
    HwndSetFocus(win->hwndFrame);

    DWORD flags = MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING;
    BOOL moveOk = MoveFileExW(srcPathW.s, dstFilePathW, flags);
    if (!moveOk) {
        LogLastError();
        LoadArgs args(srcPath, win);
        args.forceReuse = true;
        LoadDocument(&args);
        NotificationCreateArgs nargs;
        nargs.hwndParent = win->hwndCanvas;
        nargs.msg = _TRA("Failed to rename the file!");
        nargs.warning = true;
        nargs.timeoutMs = 0;
        ShowNotification(nargs);
        return;
    }
    RenameFileInHistory(srcPath, dstPathNormalized);

    LoadArgs args(dstPathNormalized, win);
    args.forceReuse = true;
    LoadDocument(&args);

    // LoadDocument is async; forceReuse already updates tab path/title when the
    // load is queued. Repaint tabs/frame now so the new name is visible without
    // waiting for hover or load finish (#5863).
    if (IsMainWindowValid(win) && win->CurrentTab()) {
        WindowTab* tab = win->CurrentTab();
        TabsOnChangedDoc(win);
        SetFrameTitleForTab(tab, false);
        HwndSetText(win->hwndFrame, tab->frameTitle);
        if (win->tabsCtrl && win->tabsCtrl->hwnd) {
            HwndRepaintNow(win->tabsCtrl->hwnd);
        }
        // tabs-in-titlebar draws into the frame non-client area
        RedrawWindow(win->hwndFrame, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
    }
}

static void CreateLnkShortcut(MainWindow* win) {
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }

    auto* ctrl = win->ctrl;
    Str path = ctrl->GetFilePath();

    const TempWStr defExt = ToWStrTemp(ctrl->GetDefaultFileExt());

    WCHAR dstFileName[MAX_PATH] = {};
    // Remove the extension so that it can be replaced with .lnk
    auto name = path::GetBaseNameTemp(path);
    str::BufSet(dstFileName, dimof(dstFileName), name);
    WStr dstName(dstFileName);
    wstr::TransCharsInPlace(dstName, WStrL(L":"), WStrL(L"_"));
    if (wstr::EndsWithI(dstFileName, defExt)) {
        int idx = len(dstFileName) - len(defExt);
        dstFileName[idx] = '\0';
    }

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::Builder fileFilter;
    fileFilter.Append(fmt("%s\1*.lnk\1", _TRA("Bookmark Shortcuts")));
    Str fileFilterStr = ToStr(fileFilter);
    str::TransCharsInPlace(fileFilterStr, StrL("\1"), StrL("\0"));
    WCHAR* fileFilterW = CWStrTemp(fileFilterStr);

    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilterW;
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"lnk";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetSaveFileNameW(&ofn)) {
        return;
    }

    TempStr fileName = ToUtf8Temp(dstFileName);
    if (!str::EndsWithI(fileName, StrL(".lnk"))) {
        fileName = str::JoinTemp(fileName, StrL(".lnk"));
    }

    ScrollState ss(win->ctrl->CurrentPageNo(), 0, 0);
    if (win->AsFixed()) {
        ss = win->AsFixed()->GetScrollState();
    }
    Str viewMode = DisplayModeToString(ctrl->GetDisplayMode());
    TempStr zoomVirtual = fmt("%.2f", ctrl->GetZoomVirtual());
    if (kZoomFitPage == ctrl->GetZoomVirtual()) {
        zoomVirtual = "fitpage";
    } else if (kZoomFitWidth == ctrl->GetZoomVirtual()) {
        zoomVirtual = "fitwidth";
    } else if (kZoomFitHeight == ctrl->GetZoomVirtual()) {
        zoomVirtual = "fitheight";
    } else if (kZoomFitContent == ctrl->GetZoomVirtual()) {
        zoomVirtual = "fitcontent";
    }

    TempStr args = fmt("\"%s\" -page %d -view \"%s\" -zoom %s -scroll %d,%d", path, ss.page, viewMode, zoomVirtual,
                       (int)ss.x, (int)ss.y);
    TempStr label = ctrl->GetPageLabeTemp(ss.page);
    TempStr desc = fmt(_TRA("Bookmark shortcut to page %s of %s").s, label, path);
    auto exePath = GetSelfExePathTemp();
    CreateShortcut(fileName, exePath, args, desc, 1);
}

static TabState* NewTabStateFromTab(WindowTab* tab) {
    if (!tab || !tab->ctrl || !tab->filePath) {
        return nullptr;
    }

    FileState* fs = NewFileState(tab->filePath);
    tab->ctrl->GetDisplayState(fs);
    fs->showToc = tab->showToc;
    *fs->tocState = tab->tocState;

    TabState* state = NewTabState(fs);
    DeleteFileState(fs);
    return state;
}

void DuplicateTabInNewWindow(WindowTab* tab) {
    if (!tab || tab->IsAboutTab()) {
        return;
    }
    // so that the file is opened in the same state
    SaveSettings();

    Str path = tab->filePath;
    ReportIf(!path);
    if (!path) {
        return;
    }
    TabState* state = NewTabStateFromTab(tab);

    MainWindow* newWin = CreateAndShowMainWindow(nullptr);
    if (!newWin) {
        DeleteTabState(state);
        return;
    }

    LoadArgs args(path, newWin);
    args.showWin = true;
    args.noPlaceWindow = true;
    LoadDocument(&args);

    if (state) {
        SetTabState(newWin->CurrentTab(), state);
        DeleteTabState(state);
    }
}

// create a new window and load currently shown document into it
// meant to make it easy to compare 2 documents
static void DuplicateInNewWindow(MainWindow* win) {
    if (win->IsCurrentTabAbout()) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    DuplicateTabInNewWindow(tab);
}

// create a new tab in current window and load currently shown document into it
// meant to make it easy to compare 2 documents side by side
static void DuplicateInNewTab(MainWindow* win) {
    if (win->IsCurrentTabAbout()) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }
    WindowTab* currentTab = win->CurrentTab();
    if (!currentTab || !currentTab->filePath) {
        return;
    }

    Str path = currentTab->filePath;
    ReportIf(!path);
    if (!path) {
        return;
    }

    // Save current window/tab state before loading new tab
    TabState* state = NewTabStateFromTab(currentTab);
    SaveSettings();

    LoadArgs args(path, win);
    args.showWin = true;
    args.noPlaceWindow = true;
    args.forceReuse = false; // Force creation of new tab instead of reusing current
    LoadDocument(&args);

    if (state) {
        SetTabState(win->CurrentTab(), state);
        DeleteTabState(state);
    }
}

// File-type filters for IFileOpenDialog. Heap-owned wide strings stay alive
// for the whole Show() call (modal dialog pumps messages / temp arena).
struct OpenFileFilterList {
    Vec<WStr> names;
    Vec<WStr> patterns;
    Vec<COMDLG_FILTERSPEC> specs;

    ~OpenFileFilterList() {
        for (int i = 0; i < len(names); i++) {
            wstr::Free(names[i]);
        }
        for (int i = 0; i < len(patterns); i++) {
            wstr::Free(patterns[i]);
        }
    }

    void Add(Str name, Str pattern) {
        WStr nw = ToWStr(name);
        WStr pw = ToWStr(pattern);
        names.Append(nw);
        patterns.Append(pw);
        COMDLG_FILTERSPEC s{};
        s.pszName = nw.s;
        s.pszSpec = pw.s;
        specs.Append(s);
    }
};

static void BuildOpenFileFilters(OpenFileFilterList& out) {
    const struct {
        Str name;
        Str filter;
        bool available;
    } fileFormats[] = {
        {_TRA("PDF documents"), "*.pdf;*.p7m", true},
        {_TRA("XPS documents"), "*.xps;*.oxps", true},
        {_TRA("DjVu documents"), "*.djvu", true},
        {_TRA("PostScript documents"), "*.ps;*.eps", IsEnginePsAvailable()},
        {_TRA("Comic books"), "*.cbz;*.cbr;*.cb7;*.cbt", true},
        {_TRA("CHM documents"), "*.chm", true},
        {_TRA("SVG documents"), "*.svg", true},
        {_TRA("EPUB ebooks"), "*.epub", true},
        {_TRA("Markdown documents"), "*.md;*.markdown", true},
        {_TRA("Mobi documents"), "*.mobi", true},
        {_TRA("FictionBook documents"), "*.fb2;*.fb2z;*.zfb2;*.fb2.zip", true},
        {_TRA("PalmDoc documents"), "*.pdb;*.prc", true},
        {_TRA("Images"),
         "*.bmp;*.dib;*.gif;*.jpg;*.jpeg;*.jfif;*.jxr;*.hdp;*.wdp;*.png;*.tga;*.tif;*.tiff;*.webp;*.heic;*.heif;"
         "*.avif;*.jxl;*.jp2;*.j2k;*.jpx;*.jpf;*.jpm;*.j2c",
         true},
        {_TRA("Text documents"), "*.txt;*.log;*.nfo;file_id.diz;read.me;*.tcr", true},
    };

    str::Builder allPat;
    for (const auto& ff : fileFormats) {
        if (!ff.available) {
            continue;
        }
        if (!allPat.IsEmpty()) {
            allPat.AppendChar(';');
        }
        allPat.Append(ff.filter);
    }
    out.Add(_TRA("All supported documents"), ToStr(allPat));
    for (const auto& ff : fileFormats) {
        if (ff.available && ff.name) {
            out.Add(ff.name, ff.filter);
        }
    }
    out.Add(_TRA("All files"), StrL("*.*"));
}

// Standard Windows IFileOpenDialog multi-select open.
static void OpenFileWithOSFilePicker(MainWindow* win) {
    if (!CanAccessDisk()) {
        return;
    }

    // don't allow opening different files in plugin mode
    if (gPluginMode) {
        return;
    }

    ScopedComPtr<IFileOpenDialog> dlg;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) {
        logf("OpenFileWithOSFilePicker: CoCreateInstance(CLSID_FileOpenDialog) failed: 0x%x\n", (uint)hr);
        return;
    }

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT);

    OpenFileFilterList filters;
    BuildOpenFileFilters(filters);
    ReportIf(len(filters.specs) == 0);
    dlg->SetFileTypes((UINT)len(filters.specs), filters.specs.LendData());
    dlg->SetFileTypeIndex(1); // "All supported documents" (1-based)

    hr = dlg->Show(win->hwndFrame);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return;
    }
    if (FAILED(hr)) {
        logf("OpenFileWithOSFilePicker: IFileOpenDialog::Show failed: 0x%x\n", (uint)hr);
        return;
    }

    ScopedComPtr<IShellItemArray> results;
    hr = dlg->GetResults(&results);
    if (FAILED(hr) || !results) {
        return;
    }

    DWORD count = 0;
    hr = results->GetCount(&count);
    if (FAILED(hr) || count == 0) {
        return;
    }

    StrVec paths;
    for (DWORD i = 0; i < count; i++) {
        ScopedComPtr<IShellItem> item;
        hr = results->GetItemAt(i, &item);
        if (FAILED(hr) || !item) {
            continue;
        }
        PWSTR pathW = nullptr;
        hr = item->GetDisplayName(SIGDN_FILESYSPATH, &pathW);
        if (FAILED(hr) || !pathW) {
            continue;
        }
        TempStr path = ToUtf8Temp(WStr(pathW));
        CoTaskMemFree(pathW);
        if (len(path) == 0) {
            continue;
        }
        paths.Append(path);
    }
    StartLoadDocuments(paths, win);
}

// FilePicker: empty/os = Windows dialog; sumatrapdf = Navigate Files in Folder.
static bool FilePickerIsSumatraPDF() {
    return gGlobalPrefs && str::EqI(gGlobalPrefs->filePicker, StrL("sumatrapdf"));
}

static void ToggleFilePicker() {
    if (FilePickerIsSumatraPDF()) {
        str::ReplaceWithCopy(&gGlobalPrefs->filePicker, StrL("os"));
    } else {
        // empty or "os" (or anything else) → sumatrapdf
        str::ReplaceWithCopy(&gGlobalPrefs->filePicker, StrL("sumatrapdf"));
    }
    SaveSettings();
}

static void OpenFile(MainWindow* win) {
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }

    if (FilePickerIsSumatraPDF()) {
        ShowNavFilesInFolder(win);
        return;
    }
    // empty, "os", or unrecognized: Windows file picker
    OpenFileWithOSFilePicker(win);
}

static void RemoveFailedFiles(StrVec& files) {
    StrNode* curr = gFilesFailedToOpen;
    while (curr) {
        int idx = files.Find(curr->s);
        if (idx >= 0) {
            files.RemoveAt(idx);
        }
        curr = curr->next;
    }
}

static StrVec& CollectNextPrevFilesIfChanged(Str path) {
    StrVec& files = gNextPrevDirCache;

    TempStr dir = path::GetDirTemp(path);
    if (!path::IsSame(dir, gNextPrevDir)) {
        files.Reset();
        str::ReplaceWithCopy(&gNextPrevDir, dir);
        DirIter di{dir};
        for (DirIterEntry* de : di) {
            files.Append(de->filePath);
        }

        // remove unsupported files that have never been successfully loaded
        int nFiles = len(files);
        // remove unsupported files
        // traverse from the end so that removing doesn't change iterator
        for (int i = nFiles - 1; i >= 0; i--) {
            Str path2 = files[i];
            // files[] came from DirIter with the default includeDirs = false
            FileType kind = GuessFileTypeFromName(path2, true);
            bool isSupported = IsSupportedFileType(kind, true) || DocIsSupportedFileType(kind);
            bool inHistory = FileHistoryFindByPath(path2);
            if (isSupported || inHistory) {
                continue;
            }
            files.RemoveAt(i);
        }
    }
    // the set of failed files could have changed since the directory was read
    RemoveFailedFiles(files);
    // `path` itself is often one of the removed ones: it's the file we're
    // navigating away from and it may have just failed to load (the error page)
    // or be an unsupported type opened explicitly. Callers locate it in the list
    // to know where to continue from, so it has to be there (#5917)
    AppendIfNotExists(&files, path);
    SortNatural(&files);
    return files;
}

// at folder ends: forward = last file (next), !forward = first file (prev)
static void ShowNoFileToOpenNotif(MainWindow* win, bool forward) {
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.timeoutMs = kNotifDefaultTimeOut;
    nargs.corner = NotifCorner::BottomRight;
    Str tip = forward ? _TRA("Last file in folder.") : _TRA("First file in folder.");
    nargs.msg = fmt("%s [%s](CmdNavigateFilesInFolder)", tip, _TRA("Navigate"));
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
    // can't tell how far down it is. Comparing page numbers instead said "at
    // the end" for a document the user had barely started reading, which put
    // the open-next-file tip on screen on the first scroll down.
    return false;
}

// next openable file after the current tab's path (no wrap), or empty if none.
// outN/outM are 1-based index of the next file and total count when non-null.
static TempStr PeekNextFileInFolderTemp(MainWindow* win, int* outN = nullptr, int* outM = nullptr) {
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
    if (!tab || !tab->filePath) {
        return {};
    }
    Str path = tab->filePath;
    StrVec& files = CollectNextPrevFilesIfChanged(path);
    int nFiles = len(files);
    if (nFiles < 2) {
        return {};
    }
    int idx = files.Find(path);
    if (idx < 0 || idx + 1 >= nFiles) {
        return {}; // no wrap: already last
    }
    Str next = files[idx + 1];
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
    if (!win || !win->hwndCanvas) {
        return;
    }
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifNextFileHint);
}

// scroll-down at document end: show open-next-file tip; scroll-up: dismiss.
// Called after vertical scroll / page-change intents.
// vertical scroll intent for discoverability of "open next file in folder":
// scroll-down at document end may show a next-file hint; scroll-up dismisses it
void OnDocumentVerticalScrollIntent(MainWindow* win, bool down) {
    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        return;
    }
    if (!down) {
        DismissNextFileScrollHint(win);
        return;
    }
    if (!win->IsDocLoaded() || win->IsCurrentTabAbout()) {
        return;
    }
    if (!IsAtDocumentBottom(win)) {
        return;
    }
    int n = 0, m = 0;
    TempStr nextPath = PeekNextFileInFolderTemp(win, &n, &m);
    if (!nextPath) {
        return;
    }
    TempStr name = path::GetBaseNameTemp(nextPath);
    // (Kbd/(Key/...)): key-cap of the bound shortcut; filename and "browse" open
    // the navigate-files dialog (see ParseTip for (Kbd/)/(Key/) markup).
    // The file name comes from the file system, so it can't go through ParseTip
    // (a "](CmdExec ...)" in it would break out of the link and run a program -
    // GHSA-2wv2-qm2f-vmxh). Build the run instead: only our markup is parsed and
    // the name is added as plain words that happen to be a link.
    auto* rich = new VirtRichText();
    ParseTipInto(rich, fmt("(Kbd/(Key/CmdOpenNextFileInFolder)) %s", _TRA("open")));
    rich->AddPlainLink(name, StrL("CmdOpenNextFileInFolder"));
    // leading space so the "·" doesn't abut the file name
    ParseTipInto(rich, fmt(" · %d/%d · [%s](CmdNavigateFilesInFolder)", n, m, _TRA("browse")));
    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.groupId = kNotifNextFileHint;
    args.corner = NotifCorner::BottomRight;
    args.timeoutMs = kNotifNoTimeout;
    args.tab = win->CurrentTab();
    args.richMsg = rich;
    // what the window text (and thus NotificationGetMessageTemp) reports
    args.msg = fmt("%s %s · %d/%d · %s", _TRA("open"), name, n, m, _TRA("browse"));
    ShowNotification(args);
}

static void OpenNextPrevFileInFolder(MainWindow* win, bool forward, Str pathToDelete = {});

struct NextPrevFileInFolderData {
    MainWindow* win = nullptr;
    bool forward = true;
    Str path;         // the file we tried to load; owned
    Str pathToDelete; // deleted only after another document loads; owned
    ~NextPrevFileInFolderData() {
        str::Free(path);
        str::Free(pathToDelete);
    }
};

static void OnNextPrevFileInFolderLoaded(NextPrevFileInFolderData* d, bool ok) {
    AutoDelete delData(d);
    MainWindow* win = d->win;
    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        return;
    }
    // superseded: user advanced next/prev again (or navigated elsewhere) while loading
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->filePath && d->path && !str::EqI(tab->filePath, d->path) && !path::IsSame(tab->filePath, d->path)) {
        return;
    }
    if (ok) {
        if (d->pathToDelete) {
            DeleteFileFromDiskAndHistory(d->pathToDelete);
        }
        HwndRepaintNow(win->tabsCtrl->hwnd);
        return;
    }
    // remember the failure so CollectNextPrevFilesIfChanged skips this
    // file, then advance to the one after it in the same direction
    MarkFileFailedToOpen(d->path);
    OpenNextPrevFileInFolder(win, d->forward, d->pathToDelete);
}

// .md / .html open in a browser view whose "pages" are the sibling files in the
// folder - the same set next/prev walks. When the target is one of them, going
// to that page shows it right away; loading it as a document would re-scan the
// folder and rebuild the whole TOC to arrive at the same place (#5918).
// PageNoChanged() syncs the tab path and title, and the browser records the
// move, so Back still returns to the file we came from.
static bool GoToFileInBrowserView(MainWindow* win, Str path) {
    MarkdownModel* md = win->ctrl ? win->ctrl->AsMarkdown() : nullptr;
    if (!md) {
        return false;
    }
    for (int i = 0; i < len(md->pages); i++) {
        if (path::IsSame(md->pages[i], path)) {
            md->GoToPage(i + 1, true);
            return true;
        }
    }
    return false;
}

static void OpenNextPrevFileInFolder(MainWindow* win, bool forward, Str pathToDelete) {
    ReportIf(win->IsCurrentTabAbout());
    if (win->IsCurrentTabAbout()) {
        return;
    }
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }

    // dismiss document error notifications from the previous document
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifDocErrors);
    DismissNextFileScrollHint(win);

    WindowTab* tab = win->CurrentTab();
    bool didRetry = false;
again:
    Str path = tab->filePath;
    StrVec files = CollectNextPrevFilesIfChanged(path);
    if (len(files) < 2) {
        ShowNoFileToOpenNotif(win, forward);
        return;
    }

    int nFiles = len(files);
    int idx = files.Find(path);
    if (idx < 0) {
        ShowNoFileToOpenNotif(win, forward);
        return;
    }
    // do not wrap around at the ends of the folder
    if (forward) {
        if (idx + 1 >= nFiles) {
            ShowNoFileToOpenNotif(win, forward);
            return;
        }
        idx = idx + 1;
    } else {
        if (idx <= 0) {
            ShowNoFileToOpenNotif(win, forward);
            return;
        }
        idx = idx - 1;
    }
    path = files[idx];
    if (!file::Exists(path)) {
        if (didRetry) {
            ShowNoFileToOpenNotif(win, forward);
            return;
        }
        didRetry = true;
        str::Free(gNextPrevDir);
        gNextPrevDir = {}; // trigger re-reading the directory
        goto again;
    }

    // with pathToDelete the caller wants the old file deleted once the new one
    // loaded, which only the load path does
    if (!pathToDelete && GoToFileInBrowserView(win, path)) {
        return;
    }

    if (!MaybeSaveAnnotations(tab)) {
        return;
    }
    if (!IsMainWindowValid(win)) {
        return;
    }
    tab->askedToSaveAnnotations = false;
    UpdateTabFileDisplayStateForTab(tab);
    // load on a background thread; if the file fails to load, the
    // callback marks it as failed and advances to the next/prev file
    auto* d = new NextPrevFileInFolderData;
    d->win = win;
    d->forward = forward;
    d->path = str::Dup(path);
    d->pathToDelete = str::Dup(pathToDelete);
    LoadArgs args(path, win);
    args.forceReuse = true;
    args.onFinished = MkFunc1<NextPrevFileInFolderData, bool>(OnNextPrevFileInFolderLoaded, d);
    StartLoadDocument(&args);
}

static void DeleteCurrentFileAndOpenNext(MainWindow* win) {
    if (!CanAccessDisk() || !win->IsDocLoaded() || gPluginMode) {
        return;
    }
    TempStr path = str::DupTemp(win->ctrl->GetFilePath());
    // this happens e.g. for embedded documents and directories
    if (!file::Exists(path)) {
        return;
    }
    OpenNextPrevFileInFolder(win, true, path);
}

constexpr int kSidebarMinDx = 150;
constexpr int kTocMinDy = 100;

constexpr int kFrameBorderSize = 1;
// size (DIP) of the min/max/restore/close caption glyphs
constexpr int kCaptionGlyphDip = 10;

using UILayout = MainWindow::UIState::Layout;

static bool IsUiLayoutEq(UILayout* s1, UILayout* s2) {
    return s1->rc == s2->rc && s1->presentation == s2->presentation && s1->tabsInTitlebar == s2->tabsInTitlebar &&
           s1->isFullScreen == s2->isFullScreen && s1->tabsVisible == s2->tabsVisible &&
           s1->isToolbarVisible == s2->isToolbarVisible && s1->tocVisible == s2->tocVisible &&
           s1->showFavorites == s2->showFavorites && s1->favoritesAsTab == s2->favoritesAsTab &&
           s1->showMenuBarRebar == s2->showMenuBarRebar && s1->aiChatVisible == s2->aiChatVisible &&
           s1->aiChatDx == s2->aiChatDx && s1->sidebarOnRight == s2->sidebarOnRight;
}

// Favorites-only must not reserve a tab row (issue #5861)
static bool WinHasFileTabs(MainWindow* win) {
    for (WindowTab* tab : win->Tabs()) {
        if (!tab->IsNonDocumentTab()) {
            return true;
        }
    }
    return false;
}

static void BindSlot(HwndSlot* slot, HWND hwnd, DeferWinPosHelper* dh, bool move) {
    slot->hwnd = move ? hwnd : nullptr;
    slot->winPos = dh;
}

static void ClearSlotDefer(HwndSlot* slot) {
    slot->winPos = nullptr;
}

// sizes and shows the caption-tree children for the current mode (single row
// vs. menu-bar + tabs). RelayoutFrame measures the tree after this; the
// buttons' lastBounds become captionBtn[].rect
static void SyncCaptionLayout(MainWindow* win) {
    if (!win->captionLayout) {
        return;
    }
    bool maximized = IsZoomed(win->hwndFrame);
    bool twoRow = IsShowingMenuBarRebar(win);
    bool isRtl = IsUIRtl();
    bool needPad = !maximized && !twoRow;
    int tabHeight = GetTabbarHeight(win->hwndFrame);
    int pad = needPad ? kCaptionTopPadding : 0;
    int menuBarDy = twoRow ? GetMenuBarRebarHeight(win) : 0;
    bool hasFileTabs = WinHasFileTabs(win);

    win->captionRow1->rtl = isRtl;
    win->captionRow2->rtl = isRtl;

    int winBtn = twoRow ? menuBarDy : (pad + tabHeight + 2);
    // hamburger / app icon match the tab band; min/max/close span the pad too
    int tabBtn = twoRow ? menuBarDy : tabHeight;

    auto setBtn = [&](int id, bool vis, int sz) {
        win->capBtn[id]->idealSize = {sz, sz};
        SetVis(win->capBtn[id], vis);
        win->captionBtn[id].id = id;
        win->captionBtn[id].visible = vis;
    };
    setBtn(CB_SYSTEM_MENU, true, tabBtn);
    setBtn(CB_MENU, !twoRow, tabBtn);
    setBtn(CB_MINIMIZE, true, winBtn);
    setBtn(CB_MAXIMIZE, !maximized, winBtn);
    setBtn(CB_RESTORE, maximized, winBtn);
    setBtn(CB_CLOSE, true, winBtn);

    SetVis(win->capMenuSlot, twoRow);
    SetVis(win->capTabsRow1, !twoRow);
    SetVis(win->capDrag1, twoRow);
    SetVis(win->capGap, !twoRow);
    SetVis(win->captionRow2, twoRow && hasFileTabs);
    SetVis(win->capTabsRow2, twoRow && hasFileTabs);
    SetVis(win->capRow2Lead, twoRow && hasFileTabs && isRtl);
    SetVis(win->capRow2Trail, twoRow && hasFileTabs);

    win->capGap->dx = kTabsButtonGapX;
    win->capTabsRow1->dy = tabHeight + 2;
    win->capTabsRow2->dy = tabHeight;

    if (twoRow) {
        win->capMenuSlot->dy = menuBarDy;
        int rowDx = win->captionRect.dx;
        if (rowDx <= 0) {
            rowDx = HwndClientRect(win->hwndFrame).dx;
        }
        int menuMax = std::max(rowDx - (4 * winBtn), 0);
        int natural = menuMax;
        if (win->hwndMenuToolbar) {
            int btnCount = TbGetButtonCount(win->hwndMenuToolbar);
            if (btnCount > 0) {
                Rect lastBtn = TbGetItemRect(win->hwndMenuToolbar, btnCount - 1);
                natural = lastBtn.x + lastBtn.dx + (DpiGetSystemMetrics(SM_CXBORDER) * 2);
            }
        }
        win->capMenuSlot->dx = std::min(natural, menuMax);
        win->capRow2Lead->dx = winBtn;
        win->capRow2Trail->dx = 3 * winBtn;
        if (win->tabsCtrl) {
            win->tabsVisible = hasFileTabs;
            win->tabsCtrl->SetIsVisible(hasFileTabs);
        }
    }
}

static bool RelayoutFrame(MainWindow* win, bool updateToolbars, int sidebarDx) {
    DpiSetFromHwnd(win->hwndFrame);
    Rect rc = HwndClientRect(win->hwndFrame);
    // don't relayout while the window is minimized
    if (rc.IsEmpty()) {
        return false;
    }
    // build a snapshot of all state that affects layout
    UILayout curState;
    curState.rc = rc;
    curState.presentation = (int)win->presentation;
    curState.tabsInTitlebar = win->tabsInTitlebar;
    curState.isFullScreen = win->isFullScreen;
    curState.tabsVisible = win->tabsVisible;
    curState.isToolbarVisible = win->isToolbarVisible;
    curState.tocVisible = win->uiState.tocVisible;
    bool favAsTabNow = win->CurrentTab() && win->CurrentTab()->IsFavoritesTab();
    // showFavorites covers both sidebar panel and full-window tab; favoritesAsTab
    // must differ so switching between them never skips RelayoutFrame (otherwise
    // the tree stays at sidebar size / canvas stays hidden until another tab switch).
    curState.showFavorites = win->uiState.favVisible || favAsTabNow;
    curState.favoritesAsTab = favAsTabNow;
    curState.showMenuBarRebar = IsShowingMenuBarRebar(win);
    curState.aiChatVisible = win->uiState.aiChatVisible;
    curState.aiChatDx = win->aiChatDx;
    curState.sidebarOnRight = SidebarOnRightLayout();

    // skip redundant relayouts when all layout-affecting state is unchanged
    if (IsUiLayoutEq(&curState, &win->uiState.layout) && updateToolbars && sidebarDx == -1) {
        return false;
    }
    // live splitter drag: same width again (WM_SETCURSOR / synthetic
    // WM_MOUSEMOVE) must not re-run layout — that repaints both panes and
    // looks like a 1px shimmer. Fav-splitter calls pass sidebarDx == -1.
    if (sidebarDx > 0 && sidebarDx == win->sidebarDx && !updateToolbars) {
        return false;
    }
    // only cache for default calls; non-default calls (sidebar dragging etc.)
    // must not prevent a subsequent default call from running
    if (updateToolbars && sidebarDx == -1) {
        win->uiState.layout = curState;
    } else {
        win->uiState.layout = {};
    }

    // apply the desired visibility of the sidebar / AI chat panels (no-ops
    // when unchanged). All the inputs are part of the layout snapshot, so a
    // skipped relayout means the windows are already in the desired state.
    {
        const MainWindow::UIState& ui = win->uiState;
        WindowTab* cur = win->CurrentTab();
        bool favAsTab = cur && cur->IsFavoritesTab();
        bool favVis = favAsTab || ui.favVisible;
        bool tocVis = !favAsTab && ui.tocVisible;
        bool aiVis = !favAsTab && ui.aiChatVisible;
        win->sidebarSplitter->SetIsVisible(!favAsTab && (tocVis || favVis));
        HwndSetVisible(win->hwndTocBox, tocVis);
        win->favSplitter->SetIsVisible(tocVis && favVis);
        HwndSetVisible(win->hwndFavBox, favVis);
        // canvas stays sized under a Favorites tab (only hidden) so switching
        // back does not SetViewPortSize with a 0x0 canvas
        HwndSetVisible(win->hwndCanvas, !favAsTab);
        if (win->hwndAiChatBox) {
            HwndSetVisible(win->hwndAiChatBox, aiVis);
            win->aiChatSplitter->SetIsVisible(aiVis);
        }
    }

    if (gRedrawLog) {
        RECT r = ToRECT(rc);
        LogRedraw("RelayoutFrame", win->hwndFrame, &r);
    }

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        // make the black/white canvas cover the entire window
        HwndMoveWindow(win->hwndCanvas, &rc);
        return true;
    }

    // inset by border for resize hit-testing (only with custom caption, not when maximized/fullscreen)
    if (win->tabsInTitlebar && !IsZoomed(win->hwndFrame) && !win->isFullScreen && !win->presentation) {
        rc.x += kFrameBorderSize;
        // top border is kFrameBorderSize - 1 because 1px is already NC area
        // (WM_NCCALCSIZE keeps 1px NC to prevent DWM transparent flash)
        rc.y += kFrameBorderSize - 1;
        rc.dx -= 2 * kFrameBorderSize;
        rc.dy -= kFrameBorderSize + (kFrameBorderSize - 1);
    }

    // hide overlay scrollbars before relayout so they don't appear at
    // stale positions while child windows are being repositioned
    OverlayScrollbarHide(win->overlayScrollV);
    OverlayScrollbarHide(win->overlayScrollH);

    // Never send WM_SETREDRAW to a hidden frame: nothing paints while hidden
    // so there's nothing to suppress, and DefWindowProc's WM_SETREDRAW TRUE
    // handling *shows* the window, which would flash a normal-size standard-
    // caption window during startup, before ShowMainWindow / LoadDocument
    // show it with the intended state.
    // splitter drag (sidebarDx >= 0) is a live sibling resize: WM_SETREDRAW
    // would hide the frame on every mouse move and flash the TOC through the
    // transparent WebView2 in the strip the canvas just inherited
    bool isSplitterDrag = sidebarDx != -1;
    bool suppressIntermediateRedraws = !isSplitterDrag && !win->suppressFrameRedraw && HwndIsVisible(win->hwndFrame);
    if (suppressIntermediateRedraws) {
        // suppress intermediate repaints during relayout
        SendMessageW(win->hwndFrame, WM_SETREDRAW, FALSE, 0);
    }

    DeferWinPosHelper dh;

    // the splitters are positioned into this window's coordinates
    if (win->frameRoot) {
        win->frameRoot->SetBounds(HwndClientRect(win->hwndFrame));
    }

    WindowTab* curTab = win->CurrentTab();
    bool favAsTab = curTab && curTab->IsFavoritesTab();
    bool favVisible = favAsTab || win->uiState.favVisible;
    bool tocVisible = !favAsTab && win->uiState.tocVisible;
    bool sidebarVisible = !favAsTab && (tocVisible || win->uiState.favVisible);
    bool aiChatVisible = !favAsTab && win->uiState.aiChatVisible && win->hwndAiChatBox;
    bool showCaption = !win->presentation && !win->isFullScreen && win->tabsInTitlebar;
    bool showingMenuBar = IsShowingMenuBarRebar(win);
    bool showTabsBar = !win->presentation && !win->isFullScreen && !win->tabsInTitlebar && win->tabsVisible;
    bool showMenuRebar = showingMenuBar && (!win->tabsInTitlebar || win->isFullScreen);
    bool showToolbar = win->isToolbarVisible;
    bool toolbarBottom = showToolbar && ToolbarAtBottom();

    int tabHeight = GetTabbarHeight(win->hwndFrame);
    if (showCaption) {
        SyncCaptionLayout(win);
    }
    int captionHeight = showCaption ? win->captionLayout->Layout(ExpandInf()).dy : 0;
    int menuBarDy = showMenuRebar ? GetMenuBarRebarHeight(win) : 0;
    int rebarDy = 0;
    if (showToolbar && win->hwndToolbar) {
        rebarDy = HwndWindowRect(win->hwndToolbar).dy;
    }

    SetVis(win->captionLayout, showCaption);
    SetVis(win->tabsSlot, showTabsBar);
    SetVis(win->menuSlot, showMenuRebar);
    SetVis(win->toolbarTopSlot, showToolbar && !toolbarBottom);
    SetVis(win->toolbarBottomSlot, showToolbar && toolbarBottom);
    win->tabsSlot->dy = tabHeight;
    win->menuSlot->dy = menuBarDy;
    win->toolbarTopSlot->dy = rebarDy;
    win->toolbarBottomSlot->dy = rebarDy;

    // leave at least this much canvas for the document when sidebar is open
    constexpr int kMinDocCanvasDx = 200;
    int sidebarDxApplied = 0;
    if (sidebarVisible) {
        if (sidebarDx > 0) {
            win->sidebarDx = sidebarDx; // splitter drag
        }
        sidebarDxApplied = win->sidebarDx;
        if (0 == sidebarDxApplied) {
            // not laid out yet: width the toc box was created with
            // (gGlobalPrefs->sidebarDx, see CreateToc)
            sidebarDxApplied = HwndClientRect(win->hwndTocBox).dx;
        }
        if (0 == sidebarDxApplied) {
            sidebarDxApplied = rc.dx / 4;
        }
        // never too narrow; max leaves kMinDocCanvasDx for the document
        // (was hard-capped at half the frame, which cut long favorite names)
        int maxSidebarDx = std::max(kSidebarMinDx, rc.dx - kMinDocCanvasDx);
        sidebarDxApplied = limitValue(sidebarDxApplied, kSidebarMinDx, maxSidebarDx);
        win->sidebarDx = sidebarDxApplied; // remember what's applied
    }

    int chromeDy = captionHeight + (showTabsBar ? tabHeight : 0) + menuBarDy + rebarDy;
    int contentDy = std::max(rc.dy - chromeDy, 0);

    int tocDy = 0;
    if (tocVisible) {
        if (!win->uiState.favVisible) {
            tocDy = contentDy;
        } else {
            tocDy = gGlobalPrefs->tocDy;
            if (tocDy > 0) {
                tocDy = limitValue(gGlobalPrefs->tocDy, 0, contentDy);
            } else {
                tocDy = contentDy / 2;
            }
            tocDy = limitValue(tocDy, kTocMinDy, contentDy - kTocMinDy);
        }
    }

    int aiChatDx = 0;
    if (aiChatVisible) {
        aiChatDx = win->aiChatDx;
        if (aiChatDx <= 0) {
            aiChatDx = rc.dx * 3 / 8;
        }
        int availDx = rc.dx - (sidebarVisible ? sidebarDxApplied + kSplitterDx : 0);
        aiChatDx = limitValue(aiChatDx, kSidebarMinDx, availDx / 2);
        win->aiChatDx = aiChatDx;
    }

    // sidebar favorites vs. the full-window Favorites tab: same HWND, one slot
    bool sidebarFav = !favAsTab && win->uiState.favVisible;
    SetVis(win->tocSlot, tocVisible);
    SetVis(win->favSlot, sidebarFav);
    SetVis(win->fullFavSlot, favAsTab);
    SetVis(win->favSplitter, tocVisible && sidebarFav);
    SetVis(win->sidebarSplitter, sidebarVisible);
    SetVis(win->aiChatSplitter, aiChatVisible);
    SetVis(win->aiChatSlot, aiChatVisible);

    win->tocSlot->dx = sidebarDxApplied;
    win->tocSlot->dy = tocDy;
    win->favSlot->dx = sidebarDxApplied;
    win->aiChatSlot->dx = aiChatDx;
    if (win->frameLayout) {
        win->frameLayout->rtl = SidebarOnRightLayout();
    }

    // chrome HWNDs only move when updateToolbars (splitter drag skips them)
    bool capTwoRow = showCaption && showingMenuBar;
    bool capHasFileTabs = showCaption && WinHasFileTabs(win);
    BindSlot(win->tabsSlot, win->tabsCtrl ? win->tabsCtrl->hwnd : nullptr, &dh, updateToolbars && showTabsBar);
    BindSlot(win->menuSlot, win->hwndMenuReBar, &dh, updateToolbars && showMenuRebar);
    BindSlot(win->capMenuSlot, win->hwndMenuReBar, &dh, updateToolbars && capTwoRow);
    BindSlot(win->capTabsRow1, win->tabsCtrl ? win->tabsCtrl->hwnd : nullptr, &dh,
             updateToolbars && showCaption && !capTwoRow);
    BindSlot(win->capTabsRow2, win->tabsCtrl ? win->tabsCtrl->hwnd : nullptr, &dh,
             updateToolbars && capTwoRow && capHasFileTabs);
    BindSlot(win->toolbarTopSlot, win->hwndToolbar, &dh, updateToolbars && showToolbar && !toolbarBottom);
    BindSlot(win->toolbarBottomSlot, win->hwndToolbar, &dh, updateToolbars && showToolbar && toolbarBottom);
    BindSlot(win->tocSlot, win->hwndTocBox, &dh, tocVisible);
    BindSlot(win->favSlot, win->hwndFavBox, &dh, sidebarFav);
    BindSlot(win->fullFavSlot, win->hwndFavBox, &dh, favAsTab);
    BindSlot(win->canvasSlot, win->hwndCanvas, &dh, true);
    BindSlot(win->aiChatSlot, win->hwndAiChatBox, &dh, aiChatVisible);

    LayoutToSize(win->chromeLayout, {rc.dx, rc.dy});
    win->chromeLayout->SetBounds(rc);

    HwndSlot* chromeSlots[] = {win->tabsSlot,    win->menuSlot,    win->toolbarTopSlot, win->toolbarBottomSlot,
                               win->capMenuSlot, win->capTabsRow1, win->capTabsRow2,    win->tocSlot,
                               win->favSlot,     win->fullFavSlot, win->canvasSlot,     win->aiChatSlot};
    for (HwndSlot* s : chromeSlots) {
        ClearSlotDefer(s);
    }

    if (showCaption) {
        win->captionRect = win->captionLayout->lastBounds;
        if (IsRunningOnWine()) {
            logf(
                "RelayoutFrame: tabsInTitlebar tabHeight=%d captionHeight=%d captionRect=(%d,%d,%d,%d) "
                "showingMenuBar=%d\n",
                tabHeight, captionHeight, win->captionRect.x, win->captionRect.y, win->captionRect.dx,
                win->captionRect.dy, (int)showingMenuBar);
        }
        if (updateToolbars) {
            RelayoutCaption(win);
        }
    } else if (showTabsBar && IsRunningOnWine()) {
        logf("RelayoutFrame: tabsVisible tabHeight=%d\n", tabHeight);
    }

    // in overlay mode the toolbar floats over the canvas and is positioned
    // separately (see PositionOverlayToolbar below); don't touch its visibility
    // here so a relayout doesn't flash it on/off
    if (updateToolbars && !win->isToolbarOverlay) {
        ShowWindow(win->hwndToolbar, win->isToolbarVisible ? SW_SHOW : SW_HIDE);
    }

    dh.End();

    if (isSplitterDrag) {
        // EndDeferWindowPos should have sent WM_SIZE; send it again so the
        // tree/filter are clipped to the new width before the next paint
        if (win->hwndTocBox && HwndIsVisible(win->hwndTocBox)) {
            SendMessageW(win->hwndTocBox, WM_SIZE, 0, 0);
        }
        if (IsBrowserDocController(win->ctrl)) {
            FillCanvasThemeBackground(win->hwndCanvas);
        }
    }

    if (favAsTab) {
        // above the hidden canvas so mouse hits the tree
        SetWindowPos(win->hwndFavBox, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        LayoutFavoritesContainer(win);
    }

    // Canvas size/position may have changed (e.g. first open shows the ToC via
    // deferred ScheduleUiUpdate after "Errors in document" was created against
    // the full-width canvas). Re-anchor notifications now; UpdateCanvasSize may
    // not run if only position changed, and first-open layout used to skip
    // IsWindowVisible notifs while parents were mid-show.
    RelayoutNotifications(win->hwndCanvas);

    if (suppressIntermediateRedraws) {
        // re-enable redraw and invalidate once
        SendMessageW(win->hwndFrame, WM_SETREDRAW, TRUE, 0);
        // RDW_ALLCHILDREN ensures notification windows (children of canvas) also repaint.
        // RDW_FRAME repaints the canvas's non-client area, i.e. the window scrollbars:
        // WM_SETREDRAW FALSE clears WS_VISIBLE on the frame, so the SetScrollInfo /
        // ShowScrollBar done by the UpdateScrollbars that this relayout triggers has
        // nothing to paint on, and the bar was left a blank strip (issue #5850).
        // The frame's own RDW_FRAME below does not reach child windows.
        RedrawWindow(win->hwndCanvas, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
        // RDW_ALLCHILDREN for the frame too: WM_SETREDRAW FALSE above discards update
        // regions that were already pending on the tab bar and toolbar, and without it
        // nothing invalidates them again, so they kept whatever pixels were on screen —
        // the document showing through the caption row after leaving full screen
        // (issue #5866). Matches EndFrameRedrawSuppression, which has always done this.
        RedrawWindow(win->hwndFrame, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
    }
    if (tocVisible) {
        RedrawWindow(win->hwndTocBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    if (favVisible) {
        RedrawWindow(win->hwndFavBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    if (win->uiState.aiChatVisible && win->hwndAiChatBox) {
        RelayoutAIChatPanel(win);
    }
    if (tocVisible || favVisible) {
        win->sidebarSplitter->Invalidate();
    }
    if (tocVisible && favVisible) {
        win->favSplitter->Invalidate();
    }
    if (updateToolbars && win->isToolbarVisible) {
        RedrawWindow(win->hwndToolbar, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    // during a live splitter drag we must paint synchronously: WM_PAINT is
    // starved by the stream of WM_MOUSEMOVE messages
    if (isSplitterDrag) {
        RedrawWindow(win->hwndFrame, nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
    }
    if (updateToolbars && win->tabsInTitlebar && !win->isFullScreen) {
        HwndInvalidateRect(win->hwndFrame, win->captionRect, true);
        if (win->hwndMenuReBar && HwndIsVisible(win->hwndMenuReBar)) {
            RedrawWindow(win->hwndMenuReBar, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
        if (win->tabsCtrl && win->tabsCtrl->IsVisible()) {
            RedrawWindow(win->tabsCtrl->hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE);
        }
    }

    // TODO: if a document with ToC and a broken document are loaded
    //       and the first document is closed with the ToC still visible,
    //       we have tocVisible but !win->ctrl
    if (tocVisible && win->ctrl) {
        // the ToC selection may change due to resizing
        // (and SetSidebarVisibility relies on this for initialization)
        UpdateTocSelection(win, win->ctrl->CurrentPageNo());
    }

    // reposition overlay scrollbars after relayout (they were hidden at the
    // start to prevent stale positioning); skip during fullscreen transitions
    // where EndFrameRedrawSuppression handles this
    if (!win->suppressFrameRedraw) {
        UpdateOverlayScrollbarPositions(win);
    }

    // float the toolbar over the canvas last, after the canvas/frame repaints
    // above, so they don't paint over it; visibility is driven by mouse
    // proximity, not by relayout
    if (win->isToolbarOverlay && updateToolbars) {
        PositionOverlayToolbar(win);
        if (win->toolbarOverlayShown) {
            RedrawWindow(win->hwndToolbar, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
    }
    return true;
}

static void BeginFrameRedrawSuppression(MainWindow* win) {
    if (win->suppressFrameRedraw) {
        return;
    }
    win->suppressFrameRedraw = true;
    // only send WM_SETREDRAW to a visible frame: for a hidden one there's
    // nothing to suppress and re-enabling would show the window (DefWindowProc
    // implements WM_SETREDRAW TRUE by showing it)
    win->frameRedrawSuppressSent = HwndIsVisible(win->hwndFrame);
    if (win->frameRedrawSuppressSent) {
        SendMessageW(win->hwndFrame, WM_SETREDRAW, FALSE, 0);
    }
}

static void EndFrameRedrawSuppression(MainWindow* win) {
    if (!win->suppressFrameRedraw) {
        return;
    }
    win->suppressFrameRedraw = false;
    if (win->frameRedrawSuppressSent) {
        SendMessageW(win->hwndFrame, WM_SETREDRAW, TRUE, 0);
        win->frameRedrawSuppressSent = false;
    }
    HwndInvalidate(win->hwndCanvas);
    RedrawWindow(win->hwndFrame, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
}

static void UpdateOverlayScrollbarPositions(MainWindow* win) {
    if (win->overlayScrollV) {
        OverlayScrollbarUpdatePos(win->overlayScrollV);
    }
    if (win->overlayScrollH) {
        OverlayScrollbarUpdatePos(win->overlayScrollH);
    }
}

// perform all UI work requested via ScheduleUiUpdate since the last update
static void FrameUpdateUi(MainWindow* win) {
    if (!IsMainWindowValid(win)) {
        return;
    }
    MainWindow::UIState& ui = win->uiState;
    ui.updatePending = false;
    bool updateToolbars = ui.updateToolbars;
    int sidebarDx = ui.sidebarDx;
    ui.updateToolbars = false;
    ui.sidebarDx = -1;
    // RelayoutFrame skips when nothing layout-affecting changed (a force is
    // requested by clearing win->uiState.layout)
    bool didLayout = RelayoutFrame(win, updateToolbars, sidebarDx);
    if (didLayout) {
        // maximize/restore toggles DWM border (hide when maximized; issue #5851)
        UpdateWindowFrameBorderColor(win);
        // re-anchor the floating find bar over the (possibly moved) search icon
        FindBarReposition(win);
        if (win->presentation || win->isFullScreen) {
            Rect fullscreen = HwndGetFullscreenRect(win->hwndFrame);
            Rect rect = HwndWindowRect(win->hwndFrame);
            // Windows can alter the frame size on its own (display rotation,
            // XP-era quirks). MoveWindow is a no-op while WS_MAXIMIZE is set
            // (EnterFullScreen adds it), so use SetWindowPos (issue #1106).
            if (rect != fullscreen && rect != GetVirtualScreenRect()) {
                uint flags = SWP_NOACTIVATE | SWP_NOZORDER;
                SetWindowPos(win->hwndFrame, nullptr, fullscreen.x, fullscreen.y, fullscreen.dx, fullscreen.dy, flags);
            }
        }
    }
    if (ui.toolbarDirty) {
        ui.toolbarDirty = false;
        if (win->hwndToolbar) {
            RedrawWindow(win->hwndToolbar, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
    }
    if (ui.tabsDirty) {
        ui.tabsDirty = false;
        if (win->tabsCtrl && win->tabsCtrl->IsVisible()) {
            RedrawWindow(win->tabsCtrl->hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE);
        }
    }
    if (ui.sidebarDirty) {
        ui.sidebarDirty = false;
        bool tocVisible = ui.tocVisible;
        bool favVisible = ui.favVisible;
        if (tocVisible) {
            RedrawWindow(win->hwndTocBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
        if (favVisible) {
            RedrawWindow(win->hwndFavBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
        if (tocVisible || favVisible) {
            win->sidebarSplitter->Invalidate();
        }
        if (tocVisible && favVisible) {
            win->favSplitter->Invalidate();
        }
    }
}

// Request an async, coalesced UI update: records what needs to happen and
// posts one uitask; any further requests before it's handled are folded
// into the same pass. Prefer this over direct relayout/RedrawWindow calls
// to avoid excessive repaints. sidebarDx >= 0 relayouts with a new sidebar
// width (splitter dragging).
void ScheduleUiUpdate(MainWindow* win, u32 flags, int sidebarDx) {
    if (!win || !win->hwndFrame) {
        return;
    }
    MainWindow::UIState& ui = win->uiState;
    if (flags & kUiForceRelayout) {
        ui.layout = {};
    }
    if (!(flags & kUiNoToolbars)) {
        ui.updateToolbars = true;
    }
    if (sidebarDx >= 0) {
        ui.sidebarDx = sidebarDx; // last request wins
    }
    if (flags & kUiToolbarDirty) {
        ui.toolbarDirty = true;
    }
    if (flags & kUiTabsDirty) {
        ui.tabsDirty = true;
    }
    if (flags & kUiSidebarDirty) {
        ui.sidebarDirty = true;
    }
    if (ui.updatePending) {
        return; // one FrameUpdateUi is already queued; it'll pick this up
    }
    ui.updatePending = true;
    uitask::Post(MkFunc0(FrameUpdateUi, win), "FrameUpdateUi");
}

// Apply TOC/favorites fonts and TreeView row height for an explicit DPI
// (WM_DPICHANGED wParam; may differ from GetDpiForWindow during drag).
static void ApplySidebarDpiFonts(MainWindow* win, int dpi) {
    if (!win || dpi <= 0) {
        return;
    }
    PlatformFont* treeFont = GetAppTreeFontForDpi(dpi);
    PlatformFont* labelFont = GetAppSidebarLabelFontForDpi(dpi);
    PlatformFont* appFont = GetAppFontForDpi(dpi);

    if (win->tocTreeView && win->tocTreeView->hwnd) {
        HwndSetTreeFontForDpi(win->tocTreeView->hwnd, treeFont->GetHFont(), dpi);
    }
    if (win->favTreeView && win->favTreeView->hwnd) {
        HwndSetTreeFontForDpi(win->favTreeView->hwnd, treeFont->GetHFont(), dpi);
    }
    if (win->tocLabel) {
        win->tocLabel->font = labelFont;
    }
    if (win->favLabel) {
        win->favLabel->font = labelFont;
    }
    if (win->tocFilterEdit) {
        win->tocFilterEdit->SetFont(appFont);
    }
    if (win->favFilterEdit) {
        win->favFilterEdit->SetFont(appFont);
    }
    // re-layout VBox children in the sidebar boxes
    if (win->hwndTocBox) {
        SendMessageW(win->hwndTocBox, WM_SIZE, 0, 0);
    }
    if (win->hwndFavBox) {
        SendMessageW(win->hwndFavBox, WM_SIZE, 0, 0);
    }
}

// Full chrome refresh after a DPI change (or when drag settles). Ported from
// sumatrapdf-plus multi-monitor DPI handling: rebuild toolbar/menu fonts and
// icons, re-apply sidebar tree metrics, relayout caption/tabs/frame.
static void ApplyMainWindowDpiChromeRefresh(MainWindow* win, HWND hwnd) {
    if (!win || !hwnd) {
        return;
    }
    int dpi = win->frameDpi > 0 ? win->frameDpi : DpiGetForHwnd(hwnd);
    win->frameDpi = dpi;
    logf("ApplyMainWindowDpiChromeRefresh: dpi=%d\n", dpi);

    HideSelectionToolbar(win);
    DestroySvgPixmapIconsCache();
    for (MainWindow* other : gWindows) {
        if (other == win) {
            continue;
        }
        UpdateToolbarAfterThemeChange(other);
        RecreateFindBar(other);
        UpdateFindWindowTheme(other);
        RefreshSelectionToolbarIcons(other);
    }

    bool menuRebarVisible = IsShowingMenuBarRebar(win);
    if (menuRebarVisible) {
        DestroyMenuBarRebar(win);
    }

    RebuildMenuBarForWindow(win);
    ReCreateToolbar(win);
    RecreateFindBar(win);

    if (menuRebarVisible && IsMenubarVisible()) {
        CreateMenuBarRebar(win);
        ShowMenuBarRebar(win);
    }

    if (win->tabsCtrl) {
        win->tabsCtrl->SetFont(GetAppFontForDpi(dpi));
        UpdateTabWidth(win);
    }
    ApplySidebarDpiFonts(win, dpi);

    // window margin / page spacing are dpi-scaled, so they change too
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        dm->SetUiDpi(dpi);
    }

    win->uiState.layout = {};
    RelayoutFrame(win, true, -1);
    RelayoutCaption(win);
    UpdateOverlayScrollbarPositions(win);
    FindBarReposition(win);

    MainWindowRerender(win, true);
    uint flags = RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW;
    RedrawWindow(hwnd, nullptr, nullptr, flags);
    if (win->tabsInTitlebar && !win->captionRect.IsEmpty()) {
        RECT r = ToRECT(win->captionRect);
        RedrawWindow(hwnd, &r, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
    }
}

// Lightweight preview while the user is still dragging across monitors:
// update fonts/layout at the destination DPI without waiting for mouse-up.
// Full toolbar recreate still runs so icons match the new scale immediately.
static void ApplyMainWindowDpiMovePreview(MainWindow* win, HWND hwnd) {
    ApplyMainWindowDpiChromeRefresh(win, hwnd);
}

// WM_DPICHANGED: frame moved to a different DPI (or scaling changed).
// explicitDpi: LOWORD(wParam) from WM_DPICHANGED — trust it during cross-monitor
// drag when GetDpiForWindow can lag (sumatrapdf-plus / issue #5827).
// force: full refresh even when deferring (used after drag settles).
static void OnDpiChanged(MainWindow* win, RECT* suggested, int explicitDpi = 0, bool force = false) {
    if (!win || !win->hwndFrame) {
        return;
    }
    HWND hwnd = win->hwndFrame;
    int dpi;
    if (explicitDpi > 0) {
        dpi = RoundUp(explicitDpi, 4);
    } else {
        dpi = RoundUp(DpiGetForHwnd(hwnd), 4);
    }
    if (dpi <= 0) {
        dpi = 96;
    }

    if (suggested) {
        int dx = suggested->right - suggested->left;
        int dy = suggested->bottom - suggested->top;
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, dx, dy, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    bool dpiChanged = dpi != win->frameDpi;
    if (!force && dpi == win->frameDpi && !suggested) {
        return;
    }
    win->frameDpi = dpi;
    logf("OnDpiChanged: dpi=%d force=%d defer=%d\n", dpi, (int)force, (int)win->deferDpiChromeRefresh);

    if (win->deferDpiChromeRefresh && !force) {
        win->dpiChromeRefreshPending = true;
        if (dpiChanged) {
            ApplyMainWindowDpiMovePreview(win, hwnd);
        }
        return;
    }
    ApplyMainWindowDpiChromeRefresh(win, hwnd);
}

struct CollectTopWindowsCtx {
    Vec<HWND>* hwnds;
};

static BOOL CALLBACK CollectTopWindowsProc(HWND hwnd, LPARAM lp) {
    auto* ctx = (CollectTopWindowsCtx*)lp;
    if (HwndIsVisible(hwnd)) {
        ctx->hwnds->Append(hwnd);
    }
    return TRUE;
}

// Debug: pretend the app moved to a monitor with a different scaling. Cycles
// gDpiOverride 0 (system) -> 125% -> 150% -> 75% and sends every top-level
// window a WM_DPICHANGED with a suggested rect scaled the way Windows would, so
// DPI change handling can be exercised without a second monitor. 75% is there
// because the interesting bugs are in the direction where the window's DPI is
// *below* the system DPI (code that floors a per-window size with a system-DPI
// one only breaks that way).
// Debug builds only (gCommandsDebugOnly hides it from the palette elsewhere;
// this also covers a Shortcuts entry naming the command directly).
static void ToggleDpiOverride() {
    if (!gIsDebugBuild) {
        return;
    }
    int next = 125;
    if (gDpiOverride == 125) {
        next = 150;
    } else if (gDpiOverride == 150) {
        next = 75;
    } else if (gDpiOverride == 75) {
        next = 0;
    }

    Vec<HWND> hwnds;
    CollectTopWindowsCtx ctx{&hwnds};
    EnumThreadWindows(GetCurrentThreadId(), CollectTopWindowsProc, (LPARAM)&ctx);
    int n = len(hwnds);

    // snapshot geometry and DPI before switching: the suggested rect scales by
    // the ratio of the old to the new DPI
    Vec<Rect> rects;
    Vec<int> dpis;
    for (HWND hwnd : hwnds) {
        rects.Append(HwndWindowRect(hwnd));
        dpis.Append(DpiGetForHwnd(hwnd));
    }

    gDpiOverride = next;
    // not DpiGetForHwnd(HWND_DESKTOP): the override deliberately doesn't apply there,
    // so that still reports the (unchanged) system DPI
    int newDpi = n > 0 ? DpiGetForHwnd(hwnds[0]) : DpiGetForHwnd(HWND_DESKTOP);
    logf("ToggleDpiOverride: gDpiOverride=%d, windowDpi=%d systemDpi=%d\n", gDpiOverride, newDpi,
         DpiGetForHwnd(HWND_DESKTOP));

    for (int i = 0; i < n; i++) {
        int oldDpi = dpis[i] > 0 ? dpis[i] : 96;
        Rect r = rects[i];
        RECT suggested;
        suggested.left = r.x;
        suggested.top = r.y;
        suggested.right = r.x + MulDiv(r.dx, newDpi, oldDpi);
        suggested.bottom = r.y + MulDiv(r.dy, newDpi, oldDpi);
        SendMessageW(hwnds[i], WM_DPICHANGED, MAKEWPARAM(newDpi, newDpi), (LPARAM)&suggested);
    }
}

static void FinishDeferredMainWindowDpiRefresh(MainWindow* win) {
    if (!IsMainWindowValid(win)) {
        return;
    }
    win->deferDpiChromeRefresh = false;
    if (!win->dpiChromeRefreshPending) {
        return;
    }
    win->dpiChromeRefreshPending = false;
    // Keep the DPI from the last WM_DPICHANGED; do not re-query the monitor.
    int dpi = win->frameDpi > 0 ? win->frameDpi : DpiGetForHwnd(win->hwndFrame);
    OnDpiChanged(win, nullptr, dpi, true);
}

void SetCurrentLanguageAndRefreshUI(Str langCode) {
    if (!langCode || str::Eq(langCode, trans::GetCurrentLangCode())) {
        return;
    }
    SetCurrentLang(langCode);

    for (MainWindow* win : gWindows) {
        RebuildMenuBarForWindow(win);
        UpdateToolbarSidebarText(win);
        UpdateWindowRtlLayout(win);
    }

    SaveSettings();
}

// cycle the toolbar mode show -> overlay -> hide -> show
// (fullscreen uses Fullscreen.Toolbar; home page overlay has no effect, so only
// toggle show <-> hide there)
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
}

static void SetToolbarModeAndApply(int mode) {
    SetToolbarMode(mode);
    for (MainWindow* w : gWindows) {
        ShowOrHideToolbar(w);
    }
}

static void OnMenuChangeBackgroundColor(MainWindow* win) {
    ShowChangeBackgroundColorDialog(win);
}

// TODO: should use currently active window, but most of the time
// there's only one window
void MaybeRedrawHomePage() {
    if (len(gWindows) > 0 && gWindows[0]->IsCurrentTabAbout()) {
        gWindows[0]->RedrawAll(true);
    }
}

static void ShowOptionsDialog(MainWindow* win) {
    ShowSettingsDialog(win);
}

static void SetInverseSearch(MainWindow* win) {
    ShowInverseSearchDialog(win);
}

// toggles 'show pages continuously' state
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

static Point GetSelectionCenter(MainWindow* win) {
    bool hasSelection = win->showSelection && win->CurrentTab()->selectionOnPage;
    if (!hasSelection) {
        return {};
    }
    DisplayModel* dm = win->AsFixed();
    Rect selRect;
    for (SelectionOnPage& sel : *win->CurrentTab()->selectionOnPage) {
        selRect = selRect.Union(sel.GetRect(dm));
    }

    Rect rc = HwndClientRect(win->hwndCanvas);
    Point pt;
    pt.x = (2 * selRect.x) + selRect.dx - (rc.dx / 2);
    pt.y = (2 * selRect.y) + selRect.dy - (rc.dy / 2);
    pt.x = limitValue(pt.x, selRect.x, selRect.x + selRect.dx);
    pt.y = limitValue(pt.y, selRect.y, selRect.y + selRect.dy);
    return pt;
}

static Point GetFirstVisiblePageTopLeft(MainWindow* win) {
    DisplayModel* dm = win->AsFixed();
    int page = dm->FirstVisiblePageNo();
    PageInfo* pageInfo = dm->GetPageInfo(page);
    if (!pageInfo) {
        return {};
    }
    Rect visible = pageInfo->pageOnScreen.Intersect(win->canvasRc);
    return visible.TL();
}

static Point GetCanvasCenter(MainWindow* win) {
    Rect rc = HwndClientRect(win->hwndCanvas);
    auto x = rc.x + (rc.dx / 2);
    auto y = rc.y + (rc.dy / 2);
    return {x, y};
}

static bool IsPointOnPage(DisplayModel* dm, Point& pt) {
    if (!dm) {
        return false;
    }
    if (pt.IsEmpty()) {
        return false;
    }
    int pageNo = dm->GetPageNoByPoint(pt);
    if (!dm->ValidPageNo(pageNo)) {
        return false;
    }
    if (!dm->PageVisible(pageNo)) {
        return false;
    }
    return true;
}

static bool gZoomAroundCenterCanvas = true;

static Point GetSmartZoomPos(MainWindow* win, Point suggestdPoint) {
    // zoom around current selection takes precedence
    DisplayModel* dm = win->AsFixed();
    Point pt = GetSelectionCenter(win);
    if (IsPointOnPage(dm, pt)) {
        return pt;
    }
    // suggestedPoint is typically a current mouse position
    if (IsPointOnPage(dm, suggestdPoint)) {
        return suggestdPoint;
    }
    // or towards the top-left-most part of the first visible page
    // TODO: something better, like center of the screen?
    if (gZoomAroundCenterCanvas) {
        pt = GetCanvasCenter(win);
    } else {
        pt = GetFirstVisiblePageTopLeft(win);
    }
    if (IsPointOnPage(dm, pt)) {
        return pt;
    }
    return {};
}

static void ShowZoomNotification(MainWindow* win, float zoomLevel) {
    // don't show zoom info if showing page info
    NotificationWnd* wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (wnd) {
        return;
    }
    NotificationCreateArgs args;
    args.groupId = kNotifZoomOrView;
    args.timeoutMs = 2000;
    args.hwndParent = win->hwndCanvas;
    args.msg = BuildZoomString(zoomLevel);
    ShowNotification(args);
}

static void ShowViewModeNotification(MainWindow* win, int cmdId) {
    NotificationWnd* wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (wnd) {
        return;
    }
    Str viewName;
    if (cmdId == CmdSinglePageView) {
        viewName = _TRA("Single Page");
    } else if (cmdId == CmdFacingView) {
        viewName = _TRA("Facing");
    } else if (cmdId == CmdBookView) {
        viewName = _TRA("Book View");
    } else {
        return;
    }
    TempStr msg = fmt("%s: %s", _TRA("View"), viewName);
    NotificationCreateArgs args;
    args.groupId = kNotifZoomOrView;
    args.timeoutMs = 2000;
    args.hwndParent = win->hwndCanvas;
    args.msg = msg;
    ShowNotification(args);
}

// if suggestedPoint is provided, it's position on canvas and we'll try to preserve that point after zoom
// if suggestedPoint is nullptr we'll try to pick a smart point to zoom around if smartZoom is true
void SmartZoom(MainWindow* win, float factor, Point* pt, bool smartZoom) {
    if (!win->IsDocLoaded()) {
        return;
    }

    Point ptSmart;
    if (smartZoom) {
        ptSmart = GetSmartZoomPos(win, ptSmart);
        if (!ptSmart.IsEmpty()) {
            pt = &ptSmart;
        }
    }
    if (factor < 0) {
        // if factor is one of kZoomFit* constants, we don't do smartZoom
        // TODO: shouldn't happen if !smartZoom
        pt = nullptr;
    }

    win->ctrl->SetZoomVirtual(factor, pt);
    UpdateToolbarState(win);
    ShowZoomNotification(win, factor);
}

// Zoom so that the current selection (Ctrl + drag rectangle or selected text)
// fills the window, and centre it. The selection itself is left alone so it can
// still be copied afterwards, and a navigation point is added first so Back
// returns to the view you zoomed from (issue #1699).
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
    float newZoom = dm->GetZoomVirtual(true) * std::min(fx, fy);
    newZoom = limitValue(newZoom, kZoomMin, kZoomMax);

    // remember the zoom too, so Back undoes the whole "zoom to selection"
    dm->AddNavPoint(true);
    SmartZoom(win, newZoom, nullptr, false);

    // put the middle of the selection in the middle of the window
    Rect rc = dm->CvtToScreen(pageNo, selPage);
    viewPort = dm->GetViewPort();
    dm->ScrollXBy(rc.x + (rc.dx / 2) - (viewPort.dx / 2));
    dm->ScrollYBy(rc.y + (rc.dy / 2) - (viewPort.dy / 2), false);
}

/* Zoom document in window 'hwnd' to zoom level 'zoom'.
   'zoom' is given as a floating-point number, 1.0 is 100%, 2.0 is 200% etc.
*/
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

    if (mode != newMode || zoom != newZoom) {
        float prevZoom = win->CurrentTab()->prevZoomVirtual;
        DisplayMode prevMode = win->CurrentTab()->prevDisplayMode;

        if (mode != newMode) {
            SwitchToDisplayMode(win, newMode);
        }
        OnMenuZoom(win, CmdIdFromVirtualZoom(newZoom));

        // remember the previous values for when the toolbar button is unchecked
        if (kInvalidZoom == prevZoom) {
            win->CurrentTab()->prevZoomVirtual = zoom;
            win->CurrentTab()->prevDisplayMode = mode;
        } else {
            // keep the rememberd values when toggling between the two toolbar buttons
            win->CurrentTab()->prevZoomVirtual = prevZoom;
            win->CurrentTab()->prevDisplayMode = prevMode;
        }
    } else if (win->CurrentTab()->prevZoomVirtual != kInvalidZoom) {
        float prevZoom = win->CurrentTab()->prevZoomVirtual;
        SwitchToDisplayMode(win, win->CurrentTab()->prevDisplayMode);
        SmartZoom(win, prevZoom, nullptr, true);
    }
}

static void FocusPageNoEdit(HWND hwndPageEdit) {
    if (HwndIsFocused(hwndPageEdit)) {
        SendMessageW(hwndPageEdit, WM_SETFOCUS, 0, 0);
    } else {
        HwndSetFocus(hwndPageEdit);
    }
}

static void OnMenuGoToPage(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }

    // Don't show a dialog if we don't have to - use the Toolbar instead.
    // In overlay mode the toolbar is only visible while revealed, so reveal it
    // first; focusing the hidden page box did nothing at all (#5916).
    if (win->pageEdit && !win->presentation) {
        if (win->isToolbarOverlay) {
            RevealOverlayToolbar(win);
            FocusPageNoEdit(win->pageEdit->hwnd);
            return;
        }
        if (win->isToolbarVisible) {
            FocusPageNoEdit(win->pageEdit->hwnd);
            return;
        }
    }

    ShowGoToPageDialog(win);
}

void EnterFullScreen(MainWindow* win, bool presentation) {
    if (!HasPermission(Perm::FullscreenAccess) || gPluginMode) {
        return;
    }

    if (!HwndIsVisible(win->hwndFrame)) {
        return;
    }

    if (presentation ? win->presentation : win->isFullScreen) {
        return;
    }

    ReportIf(presentation ? win->isFullScreen : win->presentation);
    if (presentation) {
        ReportIf(!win->ctrl);
        if (!win->IsDocLoaded()) {
            return;
        }

        if (IsZoomed(win->hwndFrame)) {
            win->windowStateBeforePresentation = WIN_STATE_MAXIMIZED;
        } else {
            win->windowStateBeforePresentation = WIN_STATE_NORMAL;
        }
        win->presentation = PM_ENABLED;
        // hack: this tells OnMouseMove() to hide cursor immediately
        win->dragPrevPos = Point(-2, -3);
    } else {
        win->isFullScreen = true;
    }

    // Save window style and rect before hiding anything, since
    // SetMenu(nullptr) can alter the window style bits.
    long ws = GetWindowLong(win->hwndFrame, GWL_STYLE);
    if (!presentation || !win->isFullScreen) {
        win->nonFullScreenWindowStyle = ws;
    }
    win->nonFullScreenFrameRect = HwndWindowRect(win->hwndFrame);

    // Hide sidebar before suppressing redraws so the hide takes
    // visual effect immediately, preventing a flash of sidebar at
    // fullscreen size during the transition.
    // TODO: make showFavorites a per-window pref
    bool showFavoritesTmp = gGlobalPrefs->showFavorites;
    if (presentation && (win->uiState.tocVisible || gGlobalPrefs->showFavorites)) {
        SetSidebarVisibility(win, false, false);
    }

    // Set state flags; RelayoutFrame (triggered by SetWindowPos/WM_SIZE)
    // will handle the actual showing/hiding of toolbar and tabs.
    // Fullscreen.Toolbar mode (show / hide / overlay); presentation never has a toolbar.
    bool showMenubarInFS = !presentation && gGlobalPrefs->fullscreen.showMenubar;
    win->isToolbarVisible = !presentation && ShouldShowToolbar(win);
    win->isToolbarOverlay = !presentation && ShouldOverlayToolbar(win);
    win->toolbarOverlayShown = false;
    win->tabsCtrl->SetIsVisible(false);

    // suppress redraws before any operations that trigger WM_SIZE
    // (SetMenu, SetWindowLong, SetWindowPos all change non-client area)
    BeginFrameRedrawSuppression(win);

    // always remove native menu in fullscreen (WS_CAPTION is stripped, so SetMenu won't work)
    SetMenu(win->hwndFrame, nullptr);
    if (showMenubarInFS) {
        // use rebar-based menu bar which renders in the client area
        CreateMenuBarRebar(win);
    } else {
        DestroyMenuBarRebar(win);
    }

    // remove window styles that add to non-client area
    ws &= ~(WS_CAPTION | WS_THICKFRAME);
    ws |= WS_MAXIMIZE;
    Rect rect = HwndGetFullscreenRect(win->hwndFrame);

    UpdateWindowFrameBorderColor(win);
    // disable DWM rounded corners and border for true edge-to-edge fullscreen
    if (!IsRunningOnWine()) {
        SetWindowRoundedCorners(win->hwndFrame, false);
    }

    SetWindowLong(win->hwndFrame, GWL_STYLE, ws);
    uint flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER;
    SetWindowPos(win->hwndFrame, nullptr, rect.x, rect.y, rect.dx, rect.dy, flags);

    if (presentation) {
        win->ctrl->SetInPresentation(true);
    } else if (DisplayModel* dm = win->AsFixed()) {
        dm->ApplyFullscreenDisplayMode(true);
        UpdateToolbarState(win);
    }

    // Make sure that no toolbar/sidebar keeps the focus
    HwndSetFocus(win->hwndFrame);
    // restore gGlobalPrefs->showFavorites changed by SetSidebarVisibility()
    gGlobalPrefs->showFavorites = showFavoritesTmp;
    EndFrameRedrawSuppression(win);
    // ensure layout is correct after fullscreen transition
    RelayoutFrame(win);
    // show menu bar rebar after layout positions it correctly
    ShowMenuBarRebar(win);

    if (gGlobalPrefs->preventSleepInFullscreen) {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    }
}

// After display geometry changes (tablet rotation, resolution change), keep a
// fullscreen/presentation window covering the current monitor and refresh the
// saved restore rect so ExitFullScreen does not restore pre-rotation size.
// (The old code called EnterFullScreen again, but that early-returns when
// already in fullscreen — so rotation left a wrongly sized frame; issue #1106.)
static void ResizeFullScreenToCurrentDisplay(MainWindow* win) {
    if (!win || (!win->isFullScreen && !win->presentation)) {
        return;
    }

    HWND hwnd = win->hwndFrame;
    Rect fsRect = HwndGetFullscreenRect(hwnd);
    Rect cur = HwndWindowRect(hwnd);
    bool wasMaximized = (win->nonFullScreenWindowStyle & WS_MAXIMIZE) != 0;
    if (win->presentation && win->windowStateBeforePresentation == WIN_STATE_MAXIMIZED) {
        wasMaximized = true;
    }

    // Keep the non-fullscreen restore geometry valid for the new orientation.
    if (wasMaximized) {
        // ExitFullScreen re-maximizes; store the current work area as a
        // fallback if something still uses nonFullScreenFrameRect.
        win->nonFullScreenFrameRect = GetWorkAreaRect(fsRect, nullptr);
    } else {
        Rect restore = win->nonFullScreenFrameRect;
        Size limited = HwndLimitSizeToScreen(hwnd, {restore.dx, restore.dy});
        restore.dx = limited.dx;
        restore.dy = limited.dy;
        win->nonFullScreenFrameRect = ShiftRectToWorkArea(restore, nullptr, true);
    }

    if (fsRect == cur) {
        return;
    }

    uint flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER;
    SetWindowPos(hwnd, nullptr, fsRect.x, fsRect.y, fsRect.dx, fsRect.dy, flags);
    RelayoutFrame(win);
}

void ExitFullScreen(MainWindow* win) {
    if (!win->isFullScreen && !win->presentation) {
        return;
    }

    if (gGlobalPrefs->preventSleepInFullscreen) {
        SetThreadExecutionState(ES_CONTINUOUS);
    }

    bool wasPresentation = PM_DISABLED != win->presentation;
    if (wasPresentation) {
        win->presentation = PM_DISABLED;
        if (win->IsDocLoaded()) {
            win->ctrl->SetInPresentation(false);
        }
        // re-enable the auto-hidden cursor
        KillTimer(win->hwndCanvas, kHideCursorTimerID);
        SetCursorCached(IDC_ARROW);
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

    BeginFrameRedrawSuppression(win);
    bool tocVisible = win->CurrentTab() && win->CurrentTab()->showToc;
    SetSidebarVisibility(win, tocVisible, gGlobalPrefs->showFavorites);

    if (win->tabsVisible) {
        win->tabsCtrl->SetIsVisible(true);
    }
    win->isToolbarVisible = ShouldShowToolbar(win);
    win->isToolbarOverlay = ShouldOverlayToolbar(win);
    win->toolbarOverlayShown = false;
    if (win->isToolbarVisible) {
        ShowWindow(win->hwndToolbar, SW_SHOW);
    } else if (!win->isToolbarOverlay) {
        ShowWindow(win->hwndToolbar, SW_HIDE);
    }
    // destroy any fullscreen menu rebar before restoring normal menu
    DestroyMenuBarRebar(win);
    if (IsMenubarVisible()) {
        if (win->tabsInTitlebar) {
            CreateMenuBarRebar(win);
        } else {
            SetMenu(win->hwndFrame, win->menu);
        }
    }

    // restore DWM rounded corners and border
    if (!IsRunningOnWine()) {
        SetWindowRoundedCorners(win->hwndFrame, true);
    }
    UpdateWindowFrameBorderColor(win);

    // If we were maximized before fullscreen, re-maximize against the current
    // work area instead of restoring a stale pre-rotation maximized rect
    // (issue #1106). Same for presentation mode entered from maximized.
    bool wasMaximized = (win->nonFullScreenWindowStyle & WS_MAXIMIZE) != 0;
    if (wasPresentation && win->windowStateBeforePresentation == WIN_STATE_MAXIMIZED) {
        wasMaximized = true;
    }

    Rect cr = HwndClientRect(win->hwndFrame);
    long style = win->nonFullScreenWindowStyle;
    if (wasMaximized) {
        // Clear WS_MAXIMIZE so ShowWindow(SW_MAXIMIZE) applies cleanly.
        style &= ~WS_MAXIMIZE;
    }
    SetWindowLong(win->hwndFrame, GWL_STYLE, style);
    uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);

    if (wasMaximized) {
        ShowWindow(win->hwndFrame, SW_MAXIMIZE);
    } else {
        // Clamp restore geometry to the current work area (display may have
        // rotated or a monitor may have gone away while we were fullscreen).
        Rect restore = win->nonFullScreenFrameRect;
        Size limited = HwndLimitSizeToScreen(win->hwndFrame, {restore.dx, restore.dy});
        restore.dx = limited.dx;
        restore.dy = limited.dy;
        restore = ShiftRectToWorkArea(restore, nullptr, true);
        HwndMoveWindow(win->hwndFrame, &restore);
    }
    // We have to relayout here, because it isn't done in the SetWindowPos nor MoveWindow,
    // if the client rectangle hasn't changed.
    if (HwndClientRect(win->hwndFrame) == cr) {
        RelayoutFrame(win);
    }
    // show menu bar rebar after layout positions it correctly
    ShowMenuBarRebar(win);
    EndFrameRedrawSuppression(win);
}

void ToggleFullScreen(MainWindow* win, bool presentation) {
    bool enterFullScreen = presentation ? !win->presentation : !win->isFullScreen;

    if (win->presentation || win->isFullScreen) {
        ExitFullScreen(win);
    } else {
        RememberDefaultWindowPosition(win);
    }

    if (enterFullScreen && (!presentation || win->IsDocLoaded())) {
        EnterFullScreen(win, presentation);
    }
}

static void TogglePresentationMode(MainWindow* win) {
    // only DisplayModel currently supports an actual presentation mode
    ToggleFullScreen(win, win->AsFixed() != nullptr);
}

// make sure that idx falls within <0, max-1> inclusive range
// negative numbers wrap from the end
static int wrapIdx(int idx, int max) {
    for (; idx < 0; idx += max) {
        idx += max;
    }
    return idx % max;
}

void AdvanceFocus(MainWindow* win) {
    // Tab order: Frame -> Page -> Find -> ToC -> Favorites -> Frame -> ...

    bool hasToolbar = !win->isFullScreen && !win->presentation && gGlobalPrefs->showToolbar && win->IsDocLoaded();
    int direction = IsShiftPressed() ? -1 : 1;

    const int MAX_WINDOWS = 5;
    HWND tabOrder[MAX_WINDOWS] = {win->hwndFrame};
    int nWindows = 1;
    if (hasToolbar && win->pageEdit) {
        tabOrder[nWindows++] = win->pageEdit->hwnd;
    }
    // note: the find edit is no longer in the toolbar tab order; it lives in the
    // floating findBar and is reached via Ctrl+F / the search toolbar icon
    if (win->tocLoaded && win->uiState.tocVisible) {
        tabOrder[nWindows++] = win->tocTreeView->hwnd;
    }
    if (gGlobalPrefs->showFavorites) {
        tabOrder[nWindows++] = win->favTreeView->hwnd;
    }
    ReportIf(nWindows > MAX_WINDOWS);

    // find the currently focused element
    HWND focused = GetFocus();
    int i = 0;
    while (i < nWindows) {
        if (tabOrder[i] == focused) {
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
    HwndSetFocus(tabOrder[i]);
}

// allow to distinguish a '/' caused by VK_DIVIDE (rotates a document)
// from one typed on the main keyboard (focuses the find textbox)
static bool gIsDivideKeyDown = false;

static bool ChmForwardKey(WPARAM key) {
    if ((VK_LEFT == key) || (VK_RIGHT == key)) {
        return true;
    }
    if ((VK_UP == key) || (VK_DOWN == key)) {
        return true;
    }
    if ((VK_HOME == key) || (VK_END == key)) {
        return true;
    }
    if ((VK_PRIOR == key) || (VK_NEXT == key)) {
        return true;
    }
    if ((VK_MULTIPLY == key) || (VK_DIVIDE == key)) {
        return true;
    }
    return false;
}

static Annotation* GetAnnotionUnderCursor(WindowTab* tab, Annotation* annot, LPARAM lp = 0) {
    DisplayModel* dm = tab->AsFixed();
    if (!dm) return nullptr;
    Point pt = HwndGetCursorPos(tab->win->hwndCanvas);
    if (lp != 0) {
        // sent from the context menu: the right-click position is encoded in lp
        // (the live cursor is now over the menu, not the annotation)
        pt.x = GET_X_LPARAM(lp);
        pt.y = GET_Y_LPARAM(lp);
    }
    if (pt.IsEmpty()) return nullptr;
    int pageNoUnderCursor = dm->GetPageNoByPoint(pt);
    if (pageNoUnderCursor <= 0) {
        return nullptr;
    }
    annot = dm->GetAnnotationAtPos(pt, annot);
    return annot;
}

// Map a Windows virtual key to a portable selection extend (unit, delta).
// Returns false if the key is not a selection-extend key.
static bool TextSelectExtendFromVk(WPARAM key, TextSelectUnit& unit, int& delta) {
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
static bool TryExtendTextSelectionFromKey(MainWindow* win, WPARAM key) {
    if (!win || !IsShiftPressed() || IsCtrlPressed() || IsAltPressed()) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSelection || dm->textSelection->result.len <= 0) {
        return false;
    }
    TextSelectUnit unit;
    int delta = 0;
    if (!TextSelectExtendFromVk(key, unit, delta)) {
        return false;
    }
    if (!dm->textSelection->ExtendBy(unit, delta)) {
        return true; // handled, nothing more to move
    }
    UpdateTextSelection(win, false);
    ScheduleRepaint(win, 0);
    return true;
}

static bool FrameOnKeydown(MainWindow* win, WPARAM key, LPARAM lp) {
    // TODO: how does this interact with new accelerators?
    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        // black/white screen is disabled on any unmodified key press in FrameOnChar
        return true;
    }

    if (VK_ESCAPE == key) {
        CancelDrag(win);
        // and leave a selected annotation's edit mode (issue #5933)
        WindowTab* tabEsc = win->CurrentTab();
        if (tabEsc && tabEsc->selectedAnnotation) {
            SetSelectedAnnotation(tabEsc, nullptr);
        }
        return true;
    }

    bool isCtrl = IsCtrlPressed();
    bool isShift = IsShiftPressed();
    if (!win->IsDocLoaded()) {
        return false;
    }

    DisplayModel* dm = win->AsFixed();

    // some of the chm key bindings are different than the rest and we
    // need to make sure we don't break them
    bool isChm = IsBrowserDocController(win->ctrl);
    // TODO: not sure how this interacts with accelerators
#if 0
    bool isPageUp = (isCtrl && (VK_UP == key));
    if (!isChm) {
        isPageUp |= (VK_PRIOR == key) && !isCtrl;
    }

    bool isPageDown = (isCtrl && (VK_DOWN == key));
    if (!isChm) {
        isPageDown |= (VK_NEXT == key) && !isCtrl;
    }
#endif
    if (isChm) {
        if (ChmForwardKey(key)) {
            if (win->AsChm()) {
                win->AsChm()->PassUIMsg(WM_KEYDOWN, key, lp);
            } else {
                win->AsMarkdown()->PassUIMsg(WM_KEYDOWN, key, lp);
            }
            return true;
        }
    }
    // lf("key=%d,%c,shift=%d\n", key, (char)key, (int)WasKeyDown(VK_SHIFT));

    // while the keyboard selection caret is up, movement keys move it instead
    // of scrolling the view
    if (SelectTextWithKeyboardOnKeyDown(win, key)) {
        return true;
    }

    if (TryExtendTextSelectionFromKey(win, key)) {
        return true;
    }

    if (VK_MULTIPLY == key && dm) {
        // logf("VK_MULTIPLY\n");
        dm->RotateBy(90);
    } else if (VK_DIVIDE == key && dm) {
        // logf("VK_DIVIDE\n");
        dm->RotateBy(-90);
        gIsDivideKeyDown = true;
    } else if (VK_DELETE == key && !isCtrl && !isShift) {
        WindowTab* tab = win->CurrentTab();
        if (tab && tab->selectedAnnotation) {
            DeleteAnnotationAndUpdateUI(tab, tab->selectedAnnotation);
        }
    } else {
        return false;
    }

    return true;
}

static WCHAR SingleCharLowerW(WCHAR c) {
    WCHAR buf[2] = {c, 0};
    CharLowerBuffW(buf, 1);
    return buf[0];
}

static void OnFrameKeyEsc(MainWindow* win) {
    if (StopKeyboardLinkFollowing(win)) {
        return;
    }
    if (StopSelectTextWithKeyboard(win)) {
        return;
    }
    if (AbortFinding(win, true)) {
        return;
    }
    if (RemoveNotificationsForGroup(win->hwndCanvas, kNotifPersistentWarning)) {
        return;
    }
    if (RemoveNotificationsForGroup(win->hwndCanvas, kNotifPageInfo)) {
        win->pageInfoWanted = false;
        return;
    }
    if (RemoveNotificationsForGroup(win->hwndCanvas, kNotifCursorPos)) {
        return;
    }
    if (RemoveNotificationsForGroup(win->hwndCanvas, kNotifZoomOrView)) {
        return;
    }
    if (win->showSelection) {
        // clear the user's text/rect selection (ClearSearchResult only clears
        // find-match highlights since issue #5737, so it can't do this anymore)
        DeleteOldSelectionInfo(win, true);
        ClearSearchResult(win); // repaints; also drops any find-match highlights
        ToolbarUpdateStateForWindow(win, false);
        return;
    }
    if (gGlobalPrefs->escToExit && CanCloseWindow(win)) {
        CloseWindow(win, true, false);
        return;
    }
    if (win->presentation || win->isFullScreen) {
        ToggleFullScreen(win, win->presentation != PM_DISABLED);
        return;
    }
    if (gPluginMode) {
        // We're a child window of a host (Total Commander's Lister, a browser
        // plugin, ...). Closing our own window would leave the host with an
        // empty pane, and escToExit is forced off in plugin mode, so nothing
        // happened at all. Hand the key to the host instead -- Lister's
        // convention is that Esc closes the viewer, like its other plugins.
        HWND hwndParent = GetParent(win->hwndFrame);
        if (hwndParent) {
            PostMessageW(hwndParent, WM_KEYDOWN, VK_ESCAPE, 0);
        }
        return;
    }
}

static void OnFrameKeyB(MainWindow* win) {
    auto* ctrl = win->ctrl;
    bool isSinglePage = IsSingle(ctrl->GetDisplayMode());

    DisplayModel* dm = win->AsFixed();
    if (dm && !isSinglePage) {
        bool forward = !IsShiftPressed();
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
    } else if (false && !isSinglePage) {
        // "e-book view": flip a single page
        bool forward = !IsShiftPressed();
        int nextPage = ctrl->CurrentPageNo() + (forward ? 1 : -1);
        if (ctrl->ValidPageNo(nextPage)) {
            ctrl->GoToPage(nextPage, false);
        }
    } else if (win->presentation) {
        win->ChangePresentationMode(PM_BLACK_SCREEN);
    }
}

static void AddUniquePageNo(Vec<int>& v, int pageNo) {
    for (auto n : v) {
        if (n == pageNo) {
            return;
        }
    }
    v.Append(pageNo);
}

// create one or more annotations from current selection
// returns last created annotations
static Annotation* MakeAnnotationsFromSelection(WindowTab* tab, AnnotCreateArgs* args) {
    // converts current selection to annotation (or back to regular text
    // if it's already an annotation)
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
        args->content = GetSelectedTextTemp(tab, "\r\n", isTextOnlySelection);
    }

    Annotation* annot = nullptr;
    Vec<Annotation*> created;
    for (auto pageNo : pageNos) {
        Vec<RectF> rects;
        for (auto& sel : *s) {
            if (pageNo != sel.pageNo) {
                continue;
            }
            rects.Append(sel.rect);
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
        created.Append(annot);
    }
    UpdateAnnotationsList(tab->editAnnotsWindow);

    // copy selection to clipboard so that user can use Ctrl-V to set contents
    if (args->copyToClipboard) {
        CopySelectionToClipboard(win);
    }
    DeleteOldSelectionInfo(win, true);
    MainWindowRerender(win);
    ToolbarUpdateStateForWindow(win, true);
    return annot;
}

// what CmdToggleCursorPosition would switch to. The tip cycles pt -> mm -> in
// and then closes, so the command palette can't say true / false; naming the
// next unit here keeps it in step with ToggleCursorPositionInDoc() below
// next state of the cursor-position tip, for the command palette
Str NextCursorPositionUnitName(MainWindow* win) {
    if (!win || !win->AsFixed()) {
        return {};
    }
    if (!GetNotificationForGroup(win->hwndCanvas, kNotifCursorPos)) {
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
    auto* notif = GetNotificationForGroup(win->hwndCanvas, kNotifCursorPos);
    if (!notif) {
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.groupId = kNotifCursorPos;
        args.shrinkLimit = 0.7f;
        args.timeoutMs = 0;
        notif = ShowNotification(args);
        cursorPosUnit = MeasurementUnit::pt;
    } else {
        if (cursorPosUnit == MeasurementUnit::pt) {
            cursorPosUnit = MeasurementUnit::mm;
        } else if (cursorPosUnit == MeasurementUnit::mm) {
            cursorPosUnit = MeasurementUnit::in;
        } else if (cursorPosUnit == MeasurementUnit::in) {
            cursorPosUnit = MeasurementUnit::pt;
            RemoveNotificationsForGroup(win->hwndCanvas, kNotifCursorPos);
            return;
        } else {
            ReportIf(true);
        }
    }
    Point pt = HwndGetCursorPos(win->hwndCanvas);
    UpdateCursorPositionHelper(win, pt, notif);
}

static void FrameOnChar(MainWindow* win, WPARAM key, LPARAM info = 0) {
    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        win->ChangePresentationMode(PM_ENABLED);
        return;
    }

    bool isCtrl = IsCtrlPressed();
    bool isAlt = IsAltPressed();

    if (key >= 0x100 && info && !isCtrl && !isAlt) {
        // determine the intended keypress by scan code for non-Latin keyboard layouts
        uint vk = MapVirtualKeyW((info >> 16) & 0xFF, MAPVK_VSC_TO_VK);
        if ('A' <= vk && vk <= 'Z') {
            key = vk;
        }
    }

    switch (key) {
        case VK_ESCAPE:
            OnFrameKeyEsc(win);
            return;
        case VK_TAB:
            AdvanceFocus(win);
            break;
    }

    if (!win->IsDocLoaded()) {
        return;
    }

    // while keyboard link following is on, 1..9 pick a numbered link
    if (!isCtrl && !isAlt && KeyboardLinkFollowingOnChar(win, key)) {
        return;
    }

    // while the selection caret is up, 'v' toggles visual mode and 'y' copies
    if (!isCtrl && !isAlt && SelectTextWithKeyboardOnChar(win, key)) {
        return;
    }

    if (IsCharUpperW((WCHAR)key)) {
        key = (WPARAM)SingleCharLowerW((WCHAR)key);
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
        case 0xE4: {
            HwndSendCommand(win->hwndFrame, CmdZoomIn);
        } break;
        case '-': {
            HwndSendCommand(win->hwndFrame, CmdZoomOut);
        } break;
        case '/':
            if (!gIsDivideKeyDown) {
                FindFirst(win);
            }
            gIsDivideKeyDown = false;
            break;
        case 'b':
            OnFrameKeyB(win);
            break;
    }
}

static bool FrameOnSysChar(MainWindow* win, WPARAM key) {
    // use Alt+1 to Alt+8 for selecting the first 8 tabs and Alt+9 for the last tab
    if (win->tabsVisible && ('1' <= key && key <= '9')) {
        TabsSelect(win, key < '9' ? (int)(key - '1') : win->TabCount() - 1);
        return true;
    }
    // Alt + Space opens a sys menu
    if (key == ' ') {
        OpenSystemMenu(win);
        return true;
    }
    return false;
}

static void OnSidebarSplitterMove(VirtSplitter::MoveEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->w->GetHwnd());
    if (!win) {
        return;
    }

    Point pcur = HwndGetCursorPos(win->hwndFrame);
    Rect rFrame = HwndClientRect(win->hwndFrame);
    int sidebarDx = pcur.x; // without splitter
    if (SidebarOnRightLayout()) {
        sidebarDx = rFrame.dx - pcur.x;
    }

    // make sure to keep this in sync with the calculations in RelayoutFrame
    // note: without the min/max(..., curDx), the sidebar will be
    //       stuck at its width if it accidentally got too wide or too narrow
    int curDx = win->sidebarDx; // don't read the toc box rect, it can be stale
    int minDx = std::min(kSidebarMinDx, curDx);
    // match RelayoutFrame: allow wider than half window (long Favorites names)
    constexpr int kMinDocCanvasDx = 200;
    int maxDx = std::max(rFrame.dx - kMinDocCanvasDx, curDx);
    if (sidebarDx < minDx || sidebarDx > maxDx) {
        ev->resizeAllowed = false;
        return;
    }
    // SetCursor / a still mouse must not relayout (1px shimmer on both sides)
    if (ev->queryOnly || sidebarDx == win->sidebarDx) {
        return;
    }

    // coalesces a burst of splitter moves into one relayout
    ScheduleUiUpdate(win, kUiRelayout | kUiNoToolbars, sidebarDx);
}

static void OnFavSplitterMove(VirtSplitter::MoveEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->w->GetHwnd());
    if (!win) {
        return;
    }

    Point pcur = HwndGetCursorPos(win->hwndCanvas);
    int tocDy = pcur.y; // without splitter

    // make sure to keep this in sync with the calculations in RelayoutFrame.
    // the toc box is visible here (this splitter only exists when both toc
    // and favorites are showing), so its rect is current
    Rect rFrame = HwndClientRect(win->hwndFrame);
    Rect rToc = HwndClientRect(win->hwndTocBox);
    int minDy = std::min(kTocMinDy, rToc.dy);
    int maxDy = std::max(rFrame.dy - kTocMinDy, rToc.dy);
    if (tocDy < minDy || tocDy > maxDy) {
        ev->resizeAllowed = false;
        return;
    }
    if (ev->queryOnly || tocDy == gGlobalPrefs->tocDy) {
        return;
    }
    gGlobalPrefs->tocDy = tocDy;
    // the sidebar width is unchanged (win->sidebarDx); kUiNoToolbars makes
    // the relayout run unconditionally
    ScheduleUiUpdate(win, kUiRelayout | kUiNoToolbars);
}

// Records the desired sidebar visibility in UIState and schedules the
// deferred update, which shows/hides the sidebar windows and relayouts
// (see FrameUpdateUi).
void SetSidebarVisibility(MainWindow* win, bool tocVisible, bool showFavorites) {
    if (gPluginMode || !CanAccessDisk()) {
        showFavorites = false;
    }

    if (!win->IsDocLoaded() || !win->ctrl || !win->ctrl->HasToc()) {
        tocVisible = false;
    }

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        tocVisible = false;
        showFavorites = false;
    }

    if (tocVisible) {
        LoadTocTree(win);
        ReportIf(!win->tocLoaded);
    }

    if (showFavorites) {
        PopulateFavTreeIfNeeded(win);
    }

    if (!win->CurrentTab()) {
        ReportIf(tocVisible);
    } else if (!win->presentation) {
        win->CurrentTab()->showToc = tocVisible;
    } else if (PM_ENABLED == win->presentation) {
        win->CurrentTab()->showTocPresentation = tocVisible;
    }

    // TODO: make this a per-window setting as well?
    gGlobalPrefs->showFavorites = showFavorites;

    // When the Favorites tab is selected, the tree is focused there — don't
    // steal focus just because the sidebar panel is off.
    bool favTabActive = win->CurrentTab() && win->CurrentTab()->IsFavoritesTab();
    if ((!tocVisible && HwndIsFocused(win->tocTreeView->hwnd)) ||
        (!showFavorites && !favTabActive && HwndIsFocused(win->favTreeView->hwnd))) {
        HwndSetFocus(win->hwndFrame);
    }

    win->uiState.tocVisible = tocVisible;
    win->uiState.favVisible = showFavorites;
    ScheduleUiUpdate(win, kUiRelayout | kUiNoToolbars | kUiSidebarDirty);
}

constexpr const char* kUserLangStr = "${userlang}";
constexpr const char* kSelectionStr = "${selection}";

// https://github.com/sumatrapdfreader/sumatrapdf/issues/4368
// for Google translate tl= arg seems to be ISO-639 lang code
// and we seem to use ISO-3166 country code
// this translates between them but is a heuristic that might be wrong
// https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes
// https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes

// first entry is value in gLangCodes, second is ISO 639 lang code
// I made it manually by looking at trans_lang.go and
// https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes
// but not fully and it might be incorrect anyway wrt. to other translation websites
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
    args.hwndParent = tab->win->hwndCanvas;
    args.tab = tab;
    args.warning = true;
    args.timeoutMs = 5000;
    args.msg =
        _TRA("Selection was too long for a URL and was shortened. Use a POST selection handler to send all of it.");
    ShowNotification(args);
}

static void LaunchBrowserWithSelection(WindowTab* tab, Str urlPattern) {
    if (!tab || !HasPermission(Perm::InternetAccess) || !HasPermission(Perm::CopySelection)) {
        return;
    }

#if 0 // TODO: get selection from Chm
    if (tab->AsChm()) {
        tab->AsChm()->CopySelection();
    } else if (tab->AsMarkdown()) {
        tab->AsMarkdown()->CopySelection();
        return;
    }
#endif

    bool isTextOnlySelectionOut; // if false, a rectangular selection
    TempStr selText = GetSelectedTextTemp(tab, "\n", isTextOnlySelectionOut);
    if (!selText) {
        return;
    }
    // The budget is for the whole URL, so subtract the pattern around the
    // selection. (There used to be a second, 1024-*byte* cut applied to the raw
    // utf-8 before this, which both shortened the text far more than necessary
    // and could slice a multi-byte character in half.)
    int budget = kMaxUrlEncodedLen - len(urlPattern);
    bool didTruncate = false;
    TempStr encodedSelection = URLEncodeMayTruncateTemp(selText, budget, &didTruncate);
    if (didTruncate) {
        NotifyUrlSelectionTruncated(tab);
    }
    // ${userLang} and and ${selectin} are typed by user in settings file
    // to be shomewhat resilient against typos, we'll accept a different case
    Str lang = trans::GetCurrentLangCode();
    if (str::Eq(lang, StrL("kr"))) {
        lang = "ko";
    }
    TempStr contryCode = GetISO639LangCodeFromLangTemp(lang);
    TempStr uri = str::ReplaceNoCaseTemp(urlPattern, kUserLangStr, contryCode);
    uri = str::ReplaceNoCaseTemp(uri, kSelectionStr, encodedSelection);
    LaunchBrowser(uri);
}

// TODO: rather arbitrary divide of responsibility between this and CopySelectionToClipboard()
static void CopySelectionInTabToClipboard(WindowTab* tab) {
    // Don't break the shortcut for text boxes
    if (!tab || !tab->win) {
        return;
    }
    if ((tab->win->findEdit && tab->win->findEdit->IsFocused()) ||
        (tab->win->pageEdit && tab->win->pageEdit->IsFocused())) {
        SendMessageW(GetFocus(), WM_COPY, 0, 0);
        return;
    }
    if (!HasPermission(Perm::CopySelection)) {
        return;
    }
    if (tab->AsChm()) {
        tab->AsChm()->CopySelection();
    } else if (tab->AsMarkdown()) {
        tab->AsMarkdown()->CopySelection();
        return;
    }
    if (tab->selectionOnPage) {
        CopySelectionToClipboard(tab->win);
        return;
    }
    if (tab->AsFixed()) {
        NotificationCreateArgs args;
        args.hwndParent = tab->win->hwndCanvas;
        args.msg = _TRA("Select content with Ctrl+left mouse button");
        args.timeoutMs = 2000;
        ShowNotification(args);
    }
}

static void OnMenuCustomZoom(MainWindow* win) {
    ShowCustomZoomDialog(win);
}

// this is a directory for not important data, like downloaded symbols
// this directory is the same for installed / portable etc. versions
TempStr GetSumatraDataDirTemp() {
    TempStr dir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (!dir) {
        return {};
    }
    return path::JoinTemp(dir, StrL("SumatraPDF-data"));
}

TempStr GetSumatraBuildSpecificDirTemp() {
    TempStr dataDir = GetSumatraDataDirTemp();
    if (!dataDir) {
        return {};
    }
    char id[7] = "000000";
    Str sha1 = Sha1OfAppExe();
    if (sha1) {
        str::BufSet(Str(id, dimof(id)), sha1.s);
    }
    return path::JoinTemp(dataDir, id);
}

TempStr GetLogFilePathTemp() {
    TempStr buildDir = GetSumatraBuildSpecificDirTemp();
    if (!buildDir) {
        return {};
    }
    // TODO: maybe use unique name
    return path::JoinTemp(buildDir, StrL("sumatra-log.txt"));
}

TempStr GetCrashInfoDirTemp() {
    TempStr buildDir = GetSumatraBuildSpecificDirTemp();
    if (!buildDir) {
        return {};
    }
    return path::JoinTemp(buildDir, StrL("crashinfo"));
}

static void ShowLogFileSmart() {
    TempStr path = gLogFilePath;
    if (len(path) == 0) {
        path = GetLogFilePathTemp();
    }
    WriteCurrentLogToFile(path);
    LaunchFileIfExists(path);
}

static bool IsChmTab(WindowTab* tab) {
    if (!tab || !tab->IsDocLoaded()) {
        return false;
    }
    if (tab->AsChm()) {
        return true;
    }
    DisplayModel* dm = tab->AsFixed();
    return dm && dm->GetEngineType() == kindEngineChm;
}

static bool IsMarkdownTab(WindowTab* tab) {
    if (!tab || !tab->IsDocLoaded()) {
        return false;
    }
    return tab->AsMarkdown() != nullptr;
}

// collect file paths from all windows, closing all but the last
// returns the surviving window (with no documents)
static MainWindow* CollectPathsAndCloseWindows(StrVec& paths) {
    for (MainWindow* w : gWindows) {
        for (WindowTab* tab : w->Tabs()) {
            if (tab->IsAboutTab() || !tab->filePath) {
                continue;
            }
            paths.Append(tab->filePath);
        }
    }

    SaveSettings();

    // close all windows except the last; use quitIfLast=false to keep it alive
    Vec<MainWindow*> toClose(gWindows);
    for (MainWindow* w : toClose) {
        if (!CanCloseWindow(w)) {
            continue;
        }
        CloseWindow(w, false, false);
    }

    // the last window survives as an empty/about window
    if (len(gWindows) > 0) {
        return gWindows[0];
    }
    return nullptr;
}

// defined below; used by UseTabs transitions
static void ApplyMenuBarVisibility(MainWindow* win);

static void TransitionToNoTabs() {
    StrVec paths;

    // if no files are open, just relayout each window without tabs
    bool hasFiles = false;
    for (MainWindow* w : gWindows) {
        for (WindowTab* tab : w->Tabs()) {
            if (!tab->IsAboutTab() && tab->filePath) {
                hasFiles = true;
                break;
            }
        }
        if (hasFiles) {
            break;
        }
    }
    if (!hasFiles) {
        for (MainWindow* w : gWindows) {
            DestroyMenuBarRebar(w);
            SetTabsInTitlebar(w, false);
            ApplyMenuBarVisibility(w);
            ShowOrHideToolbar(w);
            ScheduleUiUpdate(w, kUiForceRelayout | kUiToolbarDirty | kUiTabsDirty);
            w->RedrawAllIncludingNonClient();
        }
        return;
    }

    MainWindow* surviving = CollectPathsAndCloseWindows(paths);

    // re-open each file in its own window, reuse the surviving window for the first file
    for (int i = 0; i < len(paths); i++) {
        Str path = paths[i];
        MainWindow* win;
        if (i == 0 && surviving) {
            win = surviving;
            DestroyMenuBarRebar(win);
            SetTabsInTitlebar(win, false);
            ApplyMenuBarVisibility(win);
            ShowOrHideToolbar(win);
            ScheduleUiUpdate(win, kUiForceRelayout | kUiToolbarDirty | kUiTabsDirty);
        } else {
            win = CreateAndShowMainWindow(nullptr);
            if (!win) {
                continue;
            }
        }
        LoadArgs args(path, win);
        args.showWin = true;
        args.forceReuse = true;
        LoadDocument(&args);
        win->RedrawAllIncludingNonClient();
    }
}

static void TransitionToTabs() {
    StrVec paths;

    // if no files are open, just relayout each window with tabs
    bool hasFiles = false;
    for (MainWindow* w : gWindows) {
        for (WindowTab* tab : w->Tabs()) {
            if (!tab->IsAboutTab() && tab->filePath) {
                hasFiles = true;
                break;
            }
        }
        if (hasFiles) {
            break;
        }
    }
    if (!hasFiles) {
        for (MainWindow* w : gWindows) {
            // drop native menu before switching to tabs-in-titlebar (menu becomes rebar)
            SetMenu(w->hwndFrame, nullptr);
            SetTabsInTitlebar(w, true);
            ApplyMenuBarVisibility(w);
            ShowOrHideToolbar(w);
            ScheduleUiUpdate(w, kUiForceRelayout | kUiToolbarDirty | kUiTabsDirty);
            w->RedrawAllIncludingNonClient();
        }
        return;
    }

    MainWindow* surviving = CollectPathsAndCloseWindows(paths);

    // open all files as tabs in the surviving window
    MainWindow* win = surviving;
    if (!win) {
        win = CreateAndShowMainWindow(nullptr);
        if (!win) {
            return;
        }
    }
    SetMenu(win->hwndFrame, nullptr);
    SetTabsInTitlebar(win, true);
    ApplyMenuBarVisibility(win);
    for (int i = 0; i < len(paths); i++) {
        Str path = paths[i];
        LoadArgs args(path, win);
        args.showWin = true;
        args.forceReuse = (i == 0);
        LoadDocument(&args);
    }
    ScheduleUiUpdate(win, kUiForceRelayout | kUiToolbarDirty | kUiTabsDirty);
    win->RedrawAllIncludingNonClient();
}

// set a window's menu bar visibility to match the current showMenubar pref
// (unlike ToggleMenuBar, which flips the pref). No-op in fullscreen /
// presentation, where the menu bar is governed by that mode.
static void ApplyMenuBarVisibility(MainWindow* win) {
    if (!win->menu || win->presentation || win->isFullScreen) {
        return;
    }
    bool visible = IsMenubarVisible();
    if (win->tabsInTitlebar) {
        bool showing = IsShowingMenuBarRebar(win);
        if (visible && !showing) {
            CreateMenuBarRebar(win);
        } else if (!visible && showing) {
            DestroyMenuBarRebar(win);
        }
        ScheduleUiUpdate(win);
        ShowMenuBarRebar(win);
    } else {
        SetMenu(win->hwndFrame, visible ? win->menu : nullptr);
    }
}

SettingsApplyState GetSettingsApplyState() {
    GlobalPrefs* p = gGlobalPrefs;
    SettingsApplyState s;
    s.useTabs = p->useTabs;
    s.showMenubar = p->showMenubar;
    s.showMenubarWithTabs = p->showMenubarWithTabs;
    s.disableAntiAlias = p->disableAntiAlias;
    s.chmUseFixedPageUI = p->chmUI.useFixedPageUI;
    s.markdownUseFixedPageUI = p->markdownUI.useFixedPageUI;
    return s;
}

// apply settings changes that need explicit handling beyond a settings reload,
// then re-layout all windows. `before` is a snapshot taken before the change.
void ApplyChangedSettingsAndRelayout(const SettingsApplyState& before) {
    GlobalPrefs* p = gGlobalPrefs;

    if (before.disableAntiAlias != p->disableAntiAlias) {
        for (MainWindow* w : gWindows) {
            DisplayModel* dm = w->AsFixed();
            if (dm) {
                dm->GetEngine()->disableAntiAlias = p->disableAntiAlias;
            }
        }
        RerenderFixedPage();
    }

    if (before.chmUseFixedPageUI != p->chmUI.useFixedPageUI) {
        for (MainWindow* w : gWindows) {
            if (IsChmTab(w->CurrentTab())) {
                ReloadDocument(w, false);
            }
        }
    }
    if (before.markdownUseFixedPageUI != p->markdownUI.useFixedPageUI) {
        for (MainWindow* w : gWindows) {
            if (IsMarkdownTab(w->CurrentTab())) {
                ReloadDocument(w, false);
            }
        }
    }

    bool menubarChanged =
        (before.showMenubar != p->showMenubar) || (before.showMenubarWithTabs != p->showMenubarWithTabs);
    if (menubarChanged) {
        for (MainWindow* w : gWindows) {
            ApplyMenuBarVisibility(w);
        }
    }

    // re-layout so toolbar / menu / findbox changes take effect
    ApplySettingsToOpenWindows();

    // UseTabs converts existing windows <-> tabs (closes and reopens windows);
    // post it so it runs after the settings dialog has been torn down
    if (before.useTabs != p->useTabs) {
        if (p->useTabs) {
            uitask::Post(MkFunc0Void(TransitionToTabs));
        } else {
            uitask::Post(MkFunc0Void(TransitionToNoTabs));
        }
    }
}

struct ListPrintersResult {
    HWND hwndParent = nullptr;
    Str text; // owned; freed in ListPrintersShowResult
};

static void ListPrintersShowResult(ListPrintersResult* d) {
    HWND parent = d->hwndParent;
    Str text = d->text;
    d->text = {};
    d->hwndParent = nullptr;
    delete d;

    RemoveNotificationsForGroup(parent, kNotifActionResponse);
    // ShowTextInWindow copies text into the edit control before returning.
    ShowTextInWindow("SumatraPDF - Printers", text);
    str::Free(text);
}

static void ListPrintersThread(HWND* hwndPtr) {
    str::Builder out;
    GetPrintersInfo(out);
    auto* d = new ListPrintersResult;
    d->hwndParent = *hwndPtr;
    d->text = str::Dup(ToStr(out));
    delete hwndPtr;
    uitask::Post(MkFunc0<ListPrintersResult>(ListPrintersShowResult, d));
}

static void ReopenLastClosedFile(MainWindow* win) {
    Str path = PopRecentlyClosedDocument();
    if (!path) {
        return;
    }
    LoadArgs args(path, win);
    LoadDocument(&args);
}

void CopyFilePath(WindowTab* tab) {
    if (!tab) {
        return;
    }
    Str path = tab->filePath;
    CopyTextToClipboard(path);
}

static Kind kNotifClearHistory = "clearHistry";

struct ClearHistoryData {
    MainWindow* win = nullptr;
    int nFiles = 0;
};

static void ClearHistoryFinish(ClearHistoryData* d) {
    AutoDelete delData(d);
    MainWindow* win = d->win;
    if (!IsMainWindowValid(win)) {
        return;
    }
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifClearHistory);
    HwndRepaintNow(win->hwndCanvas);
    TempStr msg2 = fmt(_TRA("Cleared history of %d files, deleted thumbnails.").s, d->nFiles);
    ShowTemporaryNotification(win->hwndCanvas, msg2, kNotif5SecsTimeOut);
}

static void ClearHistoryAsync(ClearHistoryData* d) {
    EmptyThumbnailCacheDirectory();
    TempStr symDir = GetCrashInfoDirTemp();
    dir::Empty(symDir);
    auto fn = MkFunc0<ClearHistoryData>(ClearHistoryFinish, d);
    uitask::Post(fn, "TaksClearHistoryAsyncPart");
    DestroyTempArena();
}

static void ClearHistory(MainWindow* win) {
    if (!win) {
        // TODO: find current active MainWindow ?
        return;
    }

    // there is no separate storage: FileHistoryStates() *is* gGlobalPrefs->fileStates.
    // LoadSettings() hands the vector to FileHistorySetStates() and the FileHistory*()
    // functions are just an API over it, so FileHistoryClear() deletes the FileStates
    // and empties that same vector -- gGlobalPrefs->fileStates ends up empty too.
    // Don't free/replace the vector here: gGlobalPrefs owns it and the history holds
    // the pointer. gGlobalPrefs->sessionData is deliberately left alone -- it describes
    // the currently open windows, not history, and SaveSettings() rebuilds it anyway.
    Vec<FileState*>* states = FileHistoryStates();
    int nFiles = states ? len(*states) : 0;
    FileHistoryClear(false);

    SaveSettings();

    NotificationCreateArgs args;
    args.groupId = kNotifClearHistory;
    args.hwndParent = win->hwndCanvas;
    args.timeoutMs = kNotif5SecsTimeOut;
    args.msg = _TRA("Clearing history...");
    ShowNotification(args);
    auto* data = new ClearHistoryData;
    data->win = win;
    data->nFiles = nFiles;
    auto fn = MkFunc0<ClearHistoryData>(ClearHistoryAsync, data);
    RunAsync(fn, "ClearHistoryAsync");
}

// looks through the file history and removes entries for files that no
// longer exist on disk. Done synchronously on the main thread for simplicity.
static void RemoveDeletedFilesFromHistory(MainWindow* win) {
    if (!win || !FileHistoryStates()) {
        return;
    }
    int nRemoved = 0;
    Vec<FileState*>* states = FileHistoryStates();
    // iterate from the end because removing changes indices
    for (int i = len(*states) - 1; i >= 0; i--) {
        FileState* fs = (*states)[i];
        Str path = fs->filePath;
        if (!path) {
            continue;
        }
        // files on network / removable drives can be temporarily missing,
        // so only remove files we're confident are really gone
        if (!path::IsOnFixedDrive(path)) {
            continue;
        }
        if (DocumentPathExists(path)) {
            continue;
        }
        // don't remove a file that's currently open in some tab
        if (FindTabByFile(path)) {
            continue;
        }
        DeleteThumbnailForFile(path);
        // drops the home page layout cache, which points at fs
        FileHistoryRemove(fs);
        DeleteFileState(fs);
        nRemoved++;
    }

    if (nRemoved > 0) {
        SaveSettings();
        MaybeRedrawHomePage();
    }
    TempStr msg = fmt(_TRA("Deleted files removed from history: %d").s, nRemoved);
    ShowTemporaryNotification(win->hwndCanvas, msg, kNotif5SecsTimeOut);
}

// Unconditionally delete all local copies of comic-book archives that were
// cached under <data>/cbx-cache/ when opening them from a network drive.
// Safe to call with no open document; open documents may still hold a lock
// on a cache file so some deletes can fail (logged).
static void DeleteCachedFiles(MainWindow* win) {
    int nDeleted = 0;
    int nFailed = 0;
    TempStr dataDir = GetSumatraDataDirTemp();
    if (dataDir) {
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
    if (!win || !win->hwndCanvas) {
        return;
    }
    TempStr msg;
    if (nDeleted == 0 && nFailed == 0) {
        msg = fmt("%s", _TRA("No cached comic book files."));
    } else if (nFailed == 0) {
        msg = fmt(_TRA("Deleted %d cached comic book files.").s, nDeleted);
    } else {
        msg = fmt(_TRA("Deleted %d cached comic book files, %d failed.").s, nDeleted, nFailed);
    }
    ShowTemporaryNotification(win->hwndCanvas, msg, kNotif5SecsTimeOut);
}

static void DownloadDebugSymbols() {
    TempStr msg = "Symbols were already downloaded";

    bool ok = AreSymbolsDownloaded(gSymbolsDir);
    if (ok) {
        goto ShowMessage;
    }
    ok = CrashHandlerDownloadSymbols();
    if (!ok) {
        msg = "Failed to download symbols";
        goto ShowMessage;
    }
    msg = fmt("Downloaded symbols to %s", gSymbolsDir);
    {
        bool didInitializeDbgHelp = InitializeDbgHelp(false);
        ReportIfFast(!didInitializeDbgHelp);
    }
ShowMessage:
    MessageBoxWarning(nullptr, msg, _TRA("Downloading symbols"));
}

// CmdDebugCorruptMemory, the only caller, is behind #if defined(DEBUG), so
// defining this in a release build leaves an unreferenced static: C4505, which
// /WX turns into an error
#if defined(DEBUG)
#if 1
static void DebugCorruptMemory() {}
#else
// try to trigger a crash due to corrupting allocator
// this is a different kind of a crash than just referencing invalid memory
// as corrupted memory migh prevent crash handler from working
// this can be used to test that crash handler still works
// TODO: maybe corrupt some more
static void DebugCorruptMemory() {
    char* s = (char*)malloc(23);
    char* d = (char*)malloc(34);
    free(s);
    free(d);
    // this triggers ntdll.dll!RtlReportCriticalFailure()
    // cppcheck-suppress doubleFree
    // the double free is deliberate; silence /analyze C6001
    free(s);
}
#endif

// a VirtImage doesn't own the Pixmap it shows, so a notification that
// generates one needs a wnd that frees it when the tree goes away
struct OwnedPixmapCtrl : VirtImage {
    ~OwnedPixmapCtrl() override { FreePixmap(pixmap); }
};

// a gradient, so that there is something recognizable to look at
static Pixmap* MakeDebugGradientPixmap(int dx, int dy) {
    Pixmap* px = AllocPixmap(dx, dy);
    if (!px) {
        return nullptr;
    }
    px->premultiplied = true;
    for (int y = 0; y < dy; y++) {
        u8* row = px->data + ((size_t)y * (size_t)px->stride);
        for (int x = 0; x < dx; x++) {
            u8* p = row + (x * 4);
            p[0] = (u8)(255 * x / dx);       // B
            p[1] = (u8)(255 * y / dy);       // G
            p[2] = (u8)(255 - 255 * x / dx); // R
            p[3] = 255;                      // A
        }
    }
    return px;
}

// content for a notification, built as a VirtCtrl tree: a generated Pixmap shown
// by a VirtCtrl, with a caption below it
static ILayout* MakeDebugPixmapNotifContent(HWND hwnd) {
    PlatformFont* font = GetAppBiggerFont();
    auto* box = new VBox();
    box->alignCross = CrossAxisAlign::CrossCenter;

    auto* img = new OwnedPixmapCtrl();
    img->pixmap = MakeDebugGradientPixmap(DpiScale(120), DpiScale(40));
    img->fitToBounds = false;
    box->AddChild(img);

    box->AddChild(new Spacer(0, DpiScale(6)));
    box->AddChild(new VirtText(StrL("a VirtCtrl-drawn Pixmap"), font));
    return box;
}
#endif

constexpr const char* kManualDefaultDocURI = "/SumatraPDF-documentation";
constexpr const char* kManualVirtualHost = "https://sumatrapdf.manual/";
constexpr const WCHAR* kManualVirtualHostW = L"https://sumatrapdf.manual/";

static SimpleBrowserWindow* gManualBrowserWindow = nullptr;

static void OnDestroyManualBrowserWindow(WindowBase::DestroyEvent* /*ev*/) {
    gManualBrowserWindow = nullptr;
}

static bool IsManualBrowserWindowOpen() {
    return gManualBrowserWindow && gManualBrowserWindow->hwnd && IsWindow(gManualBrowserWindow->hwnd);
}

static void DiscardManualBrowserWindowIfClosed() {
    if (!gManualBrowserWindow) {
        return;
    }
    if (!IsManualBrowserWindowOpen()) {
        delete gManualBrowserWindow;
        gManualBrowserWindow = nullptr;
    }
}

void DeleteManualBrowserWindow() {
    delete gManualBrowserWindow;
    gManualBrowserWindow = nullptr;
}

static TempStr ManualMimeFromPathTemp(Str path) {
    Str ext = str::SliceFromCharLast(path, '.');
    TempStr mime = MimeTypeFromExtTemp(ext);
    if (!mime) {
        mime = "text/html";
    }
    return mime;
}

static bool IsManualDocHtmlPage(Str path) {
    if (len(path) == 0 || !str::EndsWithI(path, StrL(".html"))) {
        return false;
    }
    if (str::EqI(path, StrL("manual.shell.html"))) {
        return false;
    }
    return true;
}

static TempStr ManualArchiveLookupPathTemp(Str path) {
    TempStr lookupPath = str::DupTemp(path);
    // embedded.dat stores names with backslashes (MakeLZSA convention) but WebView
    // requests use URL-style forward slashes.
    str::TransCharsInPlace(lookupPath, StrL("/"), StrL("\\"));
    return lookupPath;
}

static bool ManualGetResource(void* ctx, Str path, WebViewResourceResult* res) {
    auto* archive = (lzma::SimpleArchive*)ctx;
    if (!archive || !res || len(path) == 0) {
        return false;
    }

    Str mimePath = path;
    // Doc pages are rendered on demand from .md sources in WebView2.
    if (IsManualDocHtmlPage(path)) {
        path = "manual.shell.html";
        mimePath = path;
    }

    TempStr lookupPath = ManualArchiveLookupPathTemp(path);
    int idx = lzma::GetIdxFromName(archive, lookupPath);
    if (idx < 0) {
        return false;
    }

    u8* data = lzma::GetFileDataByIdx(archive, idx, nullptr);
    if (!data) {
        return false;
    }

    lzma::FileInfo* fi = &archive->files[idx];
    res->data = data;
    res->dataLen = fi->uncompressedSize;
    res->contentType = str::Dup(ManualMimeFromPathTemp(mimePath));
    res->ownsData = true;
    return true;
}

static WebViewResourceProvider ManualResourceProvider() {
    WebViewResourceProvider provider;
    provider.ctx = GetEmbeddedArchive();
    provider.getResource = ManualGetResource;
    return provider;
}

static bool EnsureManualArchiveLoaded() {
    // Manual assets live in the same LzSA as translations / JS runtimes.
    if (!EnsureEmbeddedArchiveLoaded()) {
        logf("EnsureManualArchiveLoaded(): embedded.dat not loaded\n");
        return false;
    }
    lzma::SimpleArchive* archive = GetEmbeddedArchive();
    if (!archive || archive->filesCount == 0) {
        return false;
    }
    // smoke-check a known manual entry
    if (lzma::GetIdxFromName(archive, StrL("manual.shell.html")) < 0) {
        logf("EnsureManualArchiveLoaded: manual.shell.html missing from embedded.dat\n");
        return false;
    }
    logf("EnsureManualArchiveLoaded(): using embedded.dat, %d files\n", archive->filesCount);
    return true;
}

static TempStr DocURIToLocalManualUrlTemp(Str docURI) {
    if (len(docURI) == 0) {
        docURI = kManualDefaultDocURI;
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

    TempStr url = str::JoinTemp(kManualVirtualHost, htmlFile);
    if (fragment) {
        url = str::JoinTemp(url, fragment);
    }
    return url;
}

static TempStr DocURIToWebUrlTemp(Str docURI) {
    if (len(docURI) == 0) {
        docURI = kManualDefaultDocURI;
    }
    if (len(docURI) > 0 && docURI.s[0] == '/') {
        return fmt("https://www.sumatrapdfreader.org/docs%s", docURI);
    }
    return fmt("https://www.sumatrapdfreader.org/docs/%s", docURI);
}

void LaunchDocumentation(Str docURI) {
    TempStr localUrl = DocURIToLocalManualUrlTemp(docURI);
    TempStr webUrl = DocURIToWebUrlTemp(docURI);

    if (HasWebView() && EnsureManualArchiveLoaded()) {
        DiscardManualBrowserWindowIfClosed();
        if (IsManualBrowserWindowOpen()) {
            gManualBrowserWindow->webView->resourceProvider = ManualResourceProvider();
            gManualBrowserWindow->webView->Navigate(localUrl);
            HWND hwnd = gManualBrowserWindow->hwnd;
            ShowWindow(hwnd, SW_SHOW);
            if (IsIconic(hwnd)) {
                ShowWindow(hwnd, SW_RESTORE);
            }
            SetForegroundWindow(hwnd);
            return;
        }

        SimpleBrowserCreateArgs args;
        args.title = "SumatraPDF Documentation";
        args.url = localUrl;
        args.resourceProvider = ManualResourceProvider();
        args.resourceUriPrefix = kManualVirtualHostW;
        gManualBrowserWindow = SimpleBrowserWindowCreate(args);
        if (gManualBrowserWindow != nullptr) {
            auto fn = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroyManualBrowserWindow);
            gManualBrowserWindow->onDestroy = fn;
            return;
        }
    }

    SumatraLaunchBrowser(webUrl);
}

// If url is a documentation URL (https://www.sumatrapdfreader.org/docs/<page>),
// open it in the embedded manual browser via LaunchDocumentation (which falls
// back to the external browser when WebView is unavailable) and return true.
// Returns false for non-docs URLs so the caller opens them externally. Used by
// the home-page and notification tip links.
bool MaybeLaunchDocumentation(Str url) {
    Str docsPrefix = StrL("https://www.sumatrapdfreader.org/docs");
    if (!str::TrimPrefix(url, docsPrefix)) {
        return false;
    }
    // remainder is "/<page>" (LaunchDocumentation's docURI convention)
    LaunchDocumentation(url);
    return true;
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

    args.opacity = GetCommandIntArg(cmd, kCmdArgOpacity, 100);
    setMinMax(args.opacity, 0, 100);

    args.textSize = GetCommandIntArg(cmd, kCmdArgTextSize, -1);
    if (args.textSize >= 0) {
        // set some reasonable limits
        setMinMax(args.textSize, 5, 128);
    }

    args.borderWidth = GetCommandIntArg(cmd, kCmdArgBorderWidth, -1);
    if (args.borderWidth >= 0) {
        // set some reasonable limits
        setMinMax(args.borderWidth, 0, 128);
    }

    args.quadding = QuaddingFromName(GetCommandStringArg(cmd, kCmdArgAlignment, {}));
}

static void SetAnnotCreateArgs(AnnotCreateArgs& args, CustomCommand* cmd) {
    // note: test the arguments, not `cmd->id != cmd->origId`. A command without
    // arguments usually keeps its original id, but not always: a Shortcuts entry
    // that would collide with an earlier one gets a generated id (#5869).
    if (cmd && cmd->firstArg) {
        // a command definition doesn't use values from settings
        // must specify everything explicitly
        SetAnnotCreateArgsFromCommand(args, cmd);
        return;
    }
    auto& a = gGlobalPrefs->annotations;
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
    } else if (typ == AnnotationType::Stamp || typ == AnnotationType::Caret || typ == AnnotationType::Square ||
               typ == AnnotationType::Circle || typ == AnnotationType::Line) {
        // MuPDF defaults these to red on create; no separate prefs color.
        // Leave args.col unset so we keep MuPDF's default.
    } else {
        logf("SetAnnotCreateArgs: unexpected type %d for default prefs color\n", (int)typ);
        // ReportIf(true);
    }
    if (col && col->parsedOk) {
        args.col = *col;
    }
}

static void PasteImageFromClipboard(MainWindow* win) {
    if (!OpenClipboard(nullptr)) {
        return;
    }
    HBITMAP hbmp = (HBITMAP)GetClipboardData(CF_BITMAP);
    if (!hbmp) {
        CloseClipboard();
        return;
    }
    // create GDI+ bitmap from clipboard HBITMAP
    Gdiplus::Bitmap gdipBmp(hbmp, nullptr);
    CloseClipboard();

    if (gdipBmp.GetWidth() == 0 || gdipBmp.GetHeight() == 0) {
        return;
    }

    // generate unique path in our data dir: clipboard.png, clipboard.1.png, etc.
    TempStr dataDir = GetAppDataDirTemp();
    dir::CreateAll(dataDir);
    TempStr basePath = path::JoinTemp(dataDir, StrL("clipboard.png"));
    TempStr destPath = MakeUniqueFilePathTemp(basePath);

    // save as PNG
    CLSID pngClsid = GetGdiPlusEncoderClsid(L"image/png");
    WCHAR* destW = CWStrTemp(destPath);
    Gdiplus::Status status = gdipBmp.Save(destW, &pngClsid, nullptr);
    if (status != Gdiplus::Ok) {
        return;
    }
    OptimizePngFileAsync(destPath);

    // load the saved file
    if (win) {
        LoadArgs args(destPath, win);
        StartLoadDocument(&args);
    }
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

// for toggle commands that accept an optional "state" bool arg (issue #5067):
// returns false if the command asked for a state that already matches the
// current one (so the toggle should be skipped); true otherwise (no explicit
// state given, or the requested state differs and a flip is needed)
static bool ShouldToggle(CustomCommand* cmd, bool curState) {
    if (!GetCommandArg(cmd, kCmdArgState)) {
        return true; // no explicit state: always toggle
    }
    return GetCommandBoolArg(cmd, kCmdArgState, !curState) != curState;
}

// The image file the current tab is showing, or empty when it isn't showing
// one. The image editor takes a path rather than reaching into the tab itself.
static Str CurrentImageTabPathTemp(MainWindow* win) {
    if (!win) {
        return {};
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
        return {};
    }
    if (tab->GetEngineType() != kindEngineImage) {
        return {};
    }
    return tab->filePath;
}

// Run the print dialog from the message loop rather than from whatever loop
// delivered the WM_COMMAND. TranslateAcceleratorW sends it from inside a win32k
// user-mode callback, and menu commands come from the menu's modal loop, so
// calling PrintCurrentFile() directly nests the modal dialog inside one of
// those. That matters more than it used to: on Windows 11 PrintDlgExW shows the
// out-of-process unified print dialog and blocks on it with
// CoWaitForMultipleHandles, and it has been seen to never return - the dialog
// vanishes and the app hangs with PrintDlgExW still on the stack.
static void PrintCurrentFileDeferred(MainWindow* win) {
    // the window can be closed between posting this and running it
    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        return;
    }
    PrintCurrentFile(win);
}

static LRESULT FrameOnCommand(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    int cmdId = LOWORD(wp);
    bool openAnnotationEdit = false;

    if (cmdId >= 0xF000) {
        // handle system menu messages for the Window menu (needed for Tabs in Titlebar)
        return SendMessageW(hwnd, WM_SYSCOMMAND, wp, lp);
    }

    if (win && HandleMenuBarCommand(win, cmdId)) {
        return 0;
    }

    if (win && HandleReadAloudMenuCommand(win, cmdId)) {
        return 0;
    }

    if (CanAccessDisk()) {
        // check if the menuId belongs to an entry in the list of
        // recently opened files and load the referenced file if it does
        if ((cmdId >= CmdFileHistoryFirst) && (cmdId <= CmdFileHistoryLast)) {
            int idx = cmdId - (int)CmdFileHistoryFirst;
            FileState* state = FileHistoryGet(idx);
            if (state) {
                LoadArgs args(state->filePath, win);
                LoadDocument(&args);
            }
            return 0;
        }
    }

    // 10 submenus max with 10 items each max (=100) plus generous buffer => 200
    static_assert(CmdFavoriteLast - CmdFavoriteFirst == 256, "wrong number of favorite menu ids");
    if ((cmdId >= CmdFavoriteFirst) && (cmdId <= CmdFavoriteLast)) {
        GoToFavoriteByMenuId(win, cmdId);
        return 0;
    }

    if (!win || win->isBeingClosed) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    WindowTab* tab = win->CurrentTab();
    if (!win->IsCurrentTabAbout()) {
        if (CmdOpenWithKnownExternalViewerFirst < cmdId && cmdId < CmdOpenWithKnownExternalViewerLast) {
            ViewWithKnownExternalViewer(tab, cmdId);
            return 0;
        }
    }

    auto* ctrl = win->ctrl;
    DisplayModel* dm = win->AsFixed();

    Annotation* lastCreatedAnnot = nullptr;

    CustomCommand* cmd = FindCustomCommand(cmdId);
    if (cmd != nullptr) {
        cmdId = cmd->origId;
    }

    AnnotationType annotType = CmdIdToAnnotationType(cmdId);

    // most of them require a win, the few exceptions are no-ops
    switch (cmdId) {
        case CmdViewWithExternalViewer: {
            Str cmdLine = GetCommandStringArg(cmd, kCmdArgCommandLine, nullptr);
            if (!cmdLine || !CanAccessDisk() || !tab || !file::Exists(tab->filePath)) {
                return 0;
            }
            Str filter = GetCommandStringArg(cmd, kCmdArgFilter, nullptr);
            RunWithExe(tab, cmdLine, filter);
            return 0;
        }

        case CmdSetTheme: {
            auto theme = GetCommandStringArg(cmd, kCmdArgTheme, nullptr);
            if (theme) {
                SetTheme(theme);
                SaveSettings();
            }
            return 0;
        }

        case CmdFixDefaultApp: {
            // open OS "Open with" / Default apps UI for this extension
            Str ext = GetCommandStringArg(cmd, kCmdArgExt, {});
            if (len(ext) == 0) {
                return 0;
            }
            LaunchDefaultAppDialogForExtension(win->hwndFrame, ext);
            return 0;
        }

        case CmdSelectionHandler: {
            if (!HasPermission(Perm::CopySelection)) {
                return 0;
            }
            auto exe = GetCommandStringArg(cmd, kCmdArgExe, nullptr);
            if (exe) {
                // ${selectionfile} lets a helper program read arbitrarily long
                // text without going near a command-line length limit
                bool isTextOnly;
                TempStr sel = GetSelectedTextTemp(tab, "\n", isTextOnly);
                if (!sel) {
                    return 0;
                }
                TempStr cmdLine = ExpandSelectionVarsTemp(exe, sel, false);
                RunWithExe(tab, cmdLine, nullptr);
                return 0;
            }
            auto url = GetCommandStringArg(cmd, kCmdArgURL, nullptr);
            if (!url) {
                return 0;
            }
            // try to auto-fix url
            bool isValidURL = (str::Contains(url, StrL("://")));
            if (!isValidURL) {
                url = str::JoinTemp(StrL("https://"), url);
            }
            auto method = ParseSelectionSendMethod(GetCommandStringArg(cmd, kCmdArgMethod, nullptr));
            if (method == SelectionSendMethod::Get) {
                LaunchBrowserWithSelection(tab, url);
                return 0;
            }
            if (!HasPermission(Perm::InternetAccess) || !HasPermission(Perm::CopySelection)) {
                return 0;
            }
            bool isTextOnlySelection;
            TempStr selText = GetSelectedTextTemp(tab, "\n", isTextOnlySelection);
            if (!selText) {
                return 0;
            }
            auto body = GetCommandStringArg(cmd, kCmdArgBody, nullptr);
            if (method == SelectionSendMethod::PostViaBrowser) {
                SelectionHandlerPostViaBrowser(tab, url, body, selText);
                return 0;
            }
            auto contentType = GetCommandStringArg(cmd, kCmdArgContentType, nullptr);
            auto headers = GetCommandStringArg(cmd, kCmdArgHeaders, nullptr);
            SelectionHandlerPost(tab, url, body, contentType, headers, selText);
            return 0;
        }

        case CmdExec: {
            auto filter = GetCommandStringArg(cmd, kCmdArgFilter, nullptr);
            auto cmdLine = GetCommandStringArg(cmd, kCmdArgExe, nullptr);
            if (len(cmdLine) == 0) {
                return 0;
            }
            RunWithExe(tab, cmdLine, filter);
            return 0;
        }

        case CmdNewWindow:
            CreateAndShowMainWindow(nullptr);
            break;

        case CmdTabGroupSave:
            ShowSaveTabGroupDialog(win);
            break;

        case CmdTabGroupRestore:
            ShowOpenTabGroupDialog(win);
            break;

        case CmdDuplicateInNewWindow:
            DuplicateInNewWindow(win);
            break;

        case CmdDuplicateInNewTab:
            DuplicateInNewTab(win);
            break;

        case CmdOpenFile:
            OpenFile(win);
            break;

        case CmdOpenFileWithOSFilePicker:
            OpenFileWithOSFilePicker(win);
            break;

        case CmdToggleFilePicker:
            ToggleFilePicker();
            for (MainWindow* w : gWindows) {
                UpdateAppMenu(w, w->menu);
            }
            break;

        case CmdToggleBoolSetting: {
            // e.g. [CmdToggleBoolSetting Fullscreen.ShowMenubar] in Shortcuts
            Str settingName = GetCommandStringArg(cmd, kCmdArgName, {});
            if (len(settingName) == 0) {
                MaybeDelayedWarningNotification(StrL(
                    "CmdToggleBoolSetting requires a setting name, e.g. CmdToggleBoolSetting Fullscreen.ShowMenubar"));
                break;
            }
            bool* p = FindGlobalPrefsBoolSetting(settingName);
            if (!p) {
                MaybeDelayedWarningNotification(fmt("CmdToggleBoolSetting: unknown boolean setting '%s'", settingName));
                break;
            }
            *p = !*p;
            SaveSettings();
            // selection toolbar respects the setting on next show; hide if turned off
            if (str::EqI(settingName, StrL("SelectionToolbar")) && !*p) {
                for (MainWindow* w : gWindows) {
                    HideSelectionToolbar(w);
                }
            }
            break;
        }

        case CmdShowInFolder:
            ShowCurrentFileInFolder(win);
            break;

        case CmdShowGeneratedHTML:
            ShowGeneratedMarkdownHtml(win);
            break;

        case CmdOpenPrevFileInFolder:
        case CmdOpenNextFileInFolder:
            if (!win->IsCurrentTabAbout()) {
                // folder browsing should also work when an error page is displayed,
                // so special-case it before the win->IsDocLoaded() check
                bool forward = cmdId == CmdOpenNextFileInFolder;
                OpenNextPrevFileInFolder(win, forward);
            }
            break;

        case CmdNavigateFilesInFolder:
            // also works on the home page, where it starts in the folder of the
            // most recently opened document
            ShowNavFilesInFolder(win);
            break;

        case CmdRenameFile:
            RenameCurrentFile(win);
            break;

        case CmdDeleteFile:
            DeleteCurrentFile(win);
            break;

        case CmdDeleteFileAndOpenNext:
            DeleteCurrentFileAndOpenNext(win);
            break;

        case CmdSaveAs:
            SaveCurrentFileAs(win);
            break;

        case CmdPrint:
            // not PrintCurrentFile(win): see PrintCurrentFileDeferred
            uitask::Post(MkFunc0(PrintCurrentFileDeferred, win), "CmdPrint");
            break;

        case CmdCopyFilePath:
            CopyFilePath(tab);
            break;

        case CmdCommandPalette: {
            Str mode = {};
            if (cmd) {
                mode = GetCommandStringArg(cmd, kCmdArgMode, nullptr);
            }
            RunCommandPalette(win, mode, 0);
        } break;

        case CmdCommandPaletteTOC:
            // alias for `CmdCommandPalette *`: open the palette in TOC mode
            RunCommandPalette(win, kPalettePrefixTOC, 0);
            break;

        case CmdCommandPaletteFavorites:
            // alias for `CmdCommandPalette $`: open the palette in favorites mode
            RunCommandPalette(win, kPalettePrefixFavorites, 0);
            break;

        case CmdAIChatWithClaudeCode:
            logf("CmdAIChatWithClaudeCode dispatched\n");
            OnAIChatToggle(win, (int)AIChatBackend::Claude);
            break;

        case CmdAIChatWithGrokBuild:
            logf("CmdAIChatWithGrokBuild dispatched\n");
            OnAIChatToggle(win, (int)AIChatBackend::Grok);
            break;

        case CmdAIChatWithOpenAICodex:
            logf("CmdAIChatWithOpenAICodex dispatched\n");
            OnAIChatToggle(win, (int)AIChatBackend::Codex);
            break;

        case CmdAIChatWithAntiGravity:
            logf("CmdAIChatWithAntiGravity dispatched\n");
            OnAIChatToggle(win, (int)AIChatBackend::AntiGravity);
            break;

        case CmdClearHistory:
            ClearHistory(win);
            break;

        case CmdRemoveDeletedFilesFromHistory:
            RemoveDeletedFilesFromHistory(win);
            break;

        case CmdDeleteCachedFiles:
            DeleteCachedFiles(win);
            break;

        case CmdReopenLastClosedFile:
            ReopenLastClosedFile(win);
            break;

        case CmdShowLog:
            ShowLogFileSmart();
            break;

        case CmdScreenshot:
            TakeScreenshots();
            break;

        case CmdSetScreenshotHotkey:
            ShowSetScreenshotHotkeyDialog(win->hwndFrame);
            break;

        case CmdCropImage:
            ShowImageEditWindow(win->hwndFrame, ImageEditMode::Crop, CurrentImageTabPathTemp(win));
            break;

        case CmdResizeImage:
            ShowImageEditWindow(win->hwndFrame, ImageEditMode::Resize, CurrentImageTabPathTemp(win));
            break;

        case CmdConvertImageToPdf:
            ShowImageEditWindow(win->hwndFrame, ImageEditMode::Save, CurrentImageTabPathTemp(win), nullptr,
                                /* selectPdf */ true);
            break;

        case CmdPasteClipboardImage:
            PasteImageFromClipboard(win);
            break;

        case CmdListPrinters: {
            NotificationCreateArgs nargs;
            nargs.hwndParent = win->hwndCanvas;
            nargs.msg = _TRA("Collecting list of printers");
            ShowNotification(nargs);
            auto* data = new HWND(win->hwndCanvas);
            RunAsync(MkFunc0<HWND>(ListPrintersThread, data), "ListPrinters");
            break;
        }

        case CmdNextTab:
        case CmdPrevTab: {
            bool reverse = cmdId == CmdPrevTab;
            TabsOnCtrlTab(win, reverse);
        } break;

        case CmdNextTabSmart:
        case CmdPrevTabSmart: {
            if (gGlobalPrefs->ctrlTabSimple) {
                // simple (pre-3.6) behavior: switch tabs immediately, in tab-strip order
                TabsOnCtrlTab(win, cmdId == CmdPrevTabSmart);
                break;
            }
            if (win && win->TabCount() > 1) {
                int advance = cmdId == CmdNextTabSmart ? 1 : -1;
                RunCommandPalette(win, kPalettePrefixTabs, advance);
            }
        } break;

        case CmdMoveTabRight:
        case CmdMoveTabLeft: {
            int dir = (cmdId == CmdMoveTabRight) ? 1 : -1;
            MoveTab(win, dir);
        } break;

        case CmdCloseAllTabs: {
            CloseAllTabs(win);
            break;
        }
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
            for (WindowTab* t : toClose) {
                CloseTab(t, false);
            }
        } break;

        case CmdExit:
            OnMenuExit();
            break;

        case CmdReloadDocument:
            ReloadDocument(win, false);
            break;

        case CmdCreateShortcutToFile:
            CreateLnkShortcut(win);
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
                return 0;
            }
            float towards = (cmdId == CmdZoomIn) ? kZoomMax : kZoomMin;
            auto zoom = ctrl->GetNextZoomStep(towards);
            Point mousePos = HwndGetCursorPos(win->hwndCanvas);
            SmartZoom(win, zoom, &mousePos, true);
        } break;

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
        case CmdZoomShrinkToFit:
        case CmdZoomActualSize:
            OnMenuZoom(win, cmdId);
            break;

        case CmdZoomCustom: {
            if (cmd && cmd->firstArg) {
                float virtZoom = cmd->firstArg->floatVal;
                SmartZoom(win, virtZoom, nullptr, true);
            } else {
                OnMenuCustomZoom(win);
            }
        } break;

        case CmdZoomToSelection:
            ZoomToSelection(win);
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

        case CmdToggleContinuousView: {
            bool cur = win->ctrl && IsContinuous(win->ctrl->GetDisplayMode());
            if (ShouldToggle(cmd, cur)) {
                ToggleContinuousView(win);
            }
            break;
        }

        case CmdToggleMangaMode:
            ToggleMangaMode(win);
            break;

        case CmdToggleToolbar:
            if (GetCommandArg(cmd, kCmdArgState)) {
                // explicit state: on -> show (pinned), off -> hide
                bool on = GetCommandBoolArg(cmd, kCmdArgState, true);
                int mode = on ? kToolbarShow : kToolbarHide;
                if (win->isFullScreen) {
                    SetFullscreenToolbarMode(mode);
                    for (MainWindow* w : gWindows) {
                        ShowOrHideToolbar(w);
                    }
                } else {
                    SetToolbarModeAndApply(mode);
                }
            } else {
                OnMenuViewShowHideToolbar(win);
            }
            break;

        case CmdToggleToolbarShowReadAloud: {
            bool show = !gGlobalPrefs->toolbarShowReadAloud;
            if (GetCommandArg(cmd, kCmdArgState)) {
                show = GetCommandBoolArg(cmd, kCmdArgState, true);
            }
            gGlobalPrefs->toolbarShowReadAloud = show;
            for (MainWindow* w : gWindows) {
                ToolbarUpdateStateForWindow(w, true);
            }
            SaveSettings();
            break;
        }

        case CmdChangeScrollbar:
            ShowChangeScrollbarDialog(win);
            break;

        case CmdChangeBackgroundColor:
            OnMenuChangeBackgroundColor(win);
            break;

        case CmdSaveAnnotations: {
            SaveAnnotationsToExistingFile(tab);
            break;
        }

        case CmdReadAloud: {
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

        case CmdInvokeInverseSearch: {
            InvokeInverseSearch(tab);
            break;
        }

        case CmdSetInverseSearch:
            SetInverseSearch(win);
            break;

        case CmdSaveAnnotationsNewFile: {
            SaveAnnotationsToMaybeNewPdfFile(tab);
            break;
        }

        case CmdToggleMenuBar: {
            if (ShouldToggle(cmd, gGlobalPrefs->showMenubar)) {
                ToggleMenuBar(win, false);
            }
            break;
        }

        case CmdToggleWindowsPreviewer: {
            PreviousInstallationInfo info;
            GetPreviousInstallInfo(&info);
            if (info.installationDir) {
                if (IsPreviewInstalled()) {
                    UnRegisterPreviewer();
                } else {
                    RegisterPreviewer(info.allUsers, info.installationDir);
                }
            }
            break;
        }

        case CmdToggleWindowsSearchFilter: {
            PreviousInstallationInfo info;
            GetPreviousInstallInfo(&info);
            if (info.installationDir) {
                if (IsSearchFilterInstalled()) {
                    UnRegisterSearchFilter();
                } else {
                    RegisterSearchFilter(info.allUsers, info.installationDir);
                }
            }
            break;
        }

        case CmdChangeLanguage:
            ShowChangeLanguageDialog(win);
            break;

        case CmdToggleBookmarks:
        case CmdToggleTableOfContents:
            if (ShouldToggle(cmd, win->uiState.tocVisible)) {
                ToggleTocBox(win);
            }
            break;

        case CmdExpandToCurrentPage:
            ExpandTocToCurrentPage(win);
            break;

        case CmdStartAutoScroll:
            // start middle-click-style auto-scroll without needing a middle button
            StartAutoScrollAtCursor(win);
            break;

        case CmdScrollUpHalfPage: {
            if (win->IsCurrentTabAbout()) {
                HomePageOnVScroll(win, SB_PAGEUP);
                return 0;
            }
            if (!win->IsDocLoaded()) {
                return 0;
            }
            bool isCont = IsContinuous(win->ctrl->GetDisplayMode());
            int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
            SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_HALF_PAGEUP, 0);
            if (isCont && GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos) {
                win->ctrl->GoToPrevPage(true);
                OnDocumentVerticalScrollIntent(win, false);
            }
            ReadAloudOnUserViewChanged(win);
        } break;

        // TODO: do I need both CmdScrollUpPage and CmdGoToPrevPage
        case CmdScrollUpPage: {
            if (win->IsCurrentTabAbout()) {
                HomePageOnVScroll(win, SB_PAGEUP);
                return 0;
            }
            if (!win->IsDocLoaded()) {
                return 0;
            }
            int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
            if (win->ctrl->GetZoomVirtual() != kZoomFitContent) {
                SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_PAGEUP, 0);
            }
            if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos) {
                win->ctrl->GoToPrevPage(true);
                OnDocumentVerticalScrollIntent(win, false);
            }
            ReadAloudOnUserViewChanged(win);
        } break;

        case CmdScrollDown:
        case CmdScrollUp: {
            if (win->IsCurrentTabAbout()) {
                HomePageOnVScroll(win, cmdId == CmdScrollUp ? SB_LINEUP : SB_LINEDOWN);
                return 0;
            }
            if (!win->IsDocLoaded()) {
                return 0;
            }
            if (dm && dm->NeedVScroll() && dm->GetZoomVirtual() != kZoomFitContent) {
                int n = GetCommandIntArg(cmd, kCmdArgN, 1);
                WPARAM dir = (cmdId == CmdScrollUp) ? SB_LINEUP : SB_LINEDOWN;
                for (int i = 0; i < n; i++) {
                    SendMessageW(win->hwndCanvas, WM_VSCROLL, dir, 0);
                }
            } else {
                // in single page view or fit content, scrolls by page
                if (cmdId == CmdScrollUp) {
                    win->ctrl->GoToPrevPage(true);
                } else {
                    win->ctrl->GoToNextPage();
                }
                // VSCROLL path already reports intent; page flips here need it
                OnDocumentVerticalScrollIntent(win, cmdId == CmdScrollDown);
            }
            ReadAloudOnUserViewChanged(win);
        } break;

        case CmdGoToPrevPage:
        case CmdGoToNextPage: {
            if (!win->IsDocLoaded()) {
                return 0;
            }
            int n = GetCommandIntArg(cmd, kCmdArgN, 1);
            for (int i = 0; i < n; i++) {
                if (cmdId == CmdGoToPrevPage) {
                    ctrl->GoToPrevPage();
                } else {
                    ctrl->GoToNextPage();
                }
            }
            OnDocumentVerticalScrollIntent(win, cmdId == CmdGoToNextPage);
            ReadAloudOnUserViewChanged(win);
            break;
        }

        case CmdScrollDownHalfPage: {
            if (win->IsCurrentTabAbout()) {
                HomePageOnVScroll(win, SB_PAGEDOWN);
                return 0;
            }
            if (!win->IsDocLoaded()) {
                return 0;
            }
            bool isCont = IsContinuous(win->ctrl->GetDisplayMode());
            int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
            SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_HALF_PAGEDOWN, 0);
            if (isCont && GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos) {
                win->ctrl->GoToNextPage();
                OnDocumentVerticalScrollIntent(win, true);
            }
            ReadAloudOnUserViewChanged(win);
        } break;

        case CmdScrollDownPage: {
            if (win->IsCurrentTabAbout()) {
                HomePageOnVScroll(win, SB_PAGEDOWN);
                return 0;
            }
            if (!win->IsDocLoaded()) {
                return 0;
            }
            int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
            if (win->ctrl->GetZoomVirtual() != kZoomFitContent) {
                SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_PAGEDOWN, 0);
            }
            if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos) {
                win->ctrl->GoToNextPage();
                OnDocumentVerticalScrollIntent(win, true);
            }
            ReadAloudOnUserViewChanged(win);
        } break;

        // TODO: rename CmdScrollLeftOrPrevPage
        case CmdScrollLeft: {
            if (!win->IsDocLoaded()) {
                return 0;
            }
            if (dm && dm->NeedHScroll()) {
                SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_LINELEFT, 0);
            } else if (dm) {
                // manga (R2L): Left advances (issue #3964)
                // toRight=false → goNext when R2L (same as GoToPageHorizontal)
                bool goNext = dm->GetDisplayR2L();
                dm->GoToPageHorizontal(false);
                // same next-file tip path as Page Up / Up: leave end dismisses
                OnDocumentVerticalScrollIntent(win, goNext);
            } else {
                win->ctrl->GoToPrevPage();
                OnDocumentVerticalScrollIntent(win, false);
            }
            ReadAloudOnUserViewChanged(win);
        } break;

        case CmdScrollLeftPage: {
            SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_PAGELEFT, 0);
        } break;

        case CmdScrollRight: {
            if (!win->IsDocLoaded()) {
                return 0;
            }
            if (dm && dm->NeedHScroll()) {
                SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_LINERIGHT, 0);
            } else if (dm) {
                // manga (R2L): Right goes back (issue #3964)
                // toRight=true → goNext when LTR
                bool goNext = !dm->GetDisplayR2L();
                dm->GoToPageHorizontal(true);
                OnDocumentVerticalScrollIntent(win, goNext);
            } else {
                win->ctrl->GoToNextPage();
                OnDocumentVerticalScrollIntent(win, true);
            }
            ReadAloudOnUserViewChanged(win);
        } break;

        case CmdScrollRightPage: {
            SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_PAGERIGHT, 0);
        } break;

        case CmdGoToFirstPage:
            if (win->IsCurrentTabAbout()) {
                HomePageOnVScroll(win, SB_TOP);
                return 0;
            }
            if (!win->IsDocLoaded()) {
                return 0;
            }
            ctrl->GoToFirstPage();
            OnDocumentVerticalScrollIntent(win, false);
            ReadAloudOnUserViewChanged(win);
            break;

        case CmdGoToLastPage:
            if (win->IsCurrentTabAbout()) {
                HomePageOnVScroll(win, SB_BOTTOM);
                return 0;
            }
            if (!win->IsDocLoaded()) {
                return 0;
            }
            if (!ctrl->GoToLastPage()) {
                SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_BOTTOM, 0);
            }
            OnDocumentVerticalScrollIntent(win, true);
            ReadAloudOnUserViewChanged(win);
            break;

        case CmdGoToPage:
            OnMenuGoToPage(win);
            break;

        case CmdTogglePresentationMode:
            if (ShouldToggle(cmd, win->presentation != PM_DISABLED)) {
                TogglePresentationMode(win);
            }
            break;

        case CmdToggleFullscreen:
            if (ShouldToggle(cmd, win->isFullScreen)) {
                ToggleFullScreen(win);
            }
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

        case CmdFindFirst:
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

        case CmdHelpVisitWebsite:
            SumatraLaunchBrowser(kWebsiteURL);
            break;

        case CmdHelpOpenManual:
            LaunchDocumentation("/SumatraPDF-documentation");
            break;

        case CmdHelpOpenKeyboardShortcuts:
            LaunchDocumentation("/Keyboard-shortcuts");
            break;

        case CmdToggleKeyboardHelp:
            ToggleKeyboardHelp(win);
            break;

        case CmdHelpOpenManualOnWebsite:
            SumatraLaunchBrowser(kManualURL);
            break;

        case CmdContributeTranslation:
            SumatraLaunchBrowser(kContributeTranslationsURL);
            break;

        case CmdHelpAbout:
            ShowAboutWindow(win);
            break;

        case CmdCheckUpdate:
            StartAsyncUpdateCheck(win, UpdateCheck::UserInitiated);
            break;

        case CmdInstallPrereleaseUpdate:
            DownloadAndInstallPendingUpdate(win);
            break;

        case CmdTogglePdfPreviewLogging: {
            bool enabled = !IsPdfPreviewLoggingEnabled();
            SetPdfPreviewLoggingEnabled(enabled);
            TempStr notifMsg = nullptr;
            if (enabled) {
                TempStr dir = GetPdfPreviewLogDirTemp();
                notifMsg = fmt("PDF preview logging enabled.\nLogs: %s", dir ? dir : StrL("(unknown)"));
            } else {
                notifMsg = str::DupTemp("PDF preview logging disabled.");
            }
            NotificationCreateArgs nargs;
            nargs.hwndParent = win->hwndCanvas;
            nargs.msg = notifMsg;
            nargs.timeoutMs = 8000;
            ShowNotification(nargs);
        } break;

        case CmdOptions:
            ShowOptionsDialog(win);
            break;

        case CmdAdvancedOptions:
        case CmdAdvancedSettings:
            ShowAdvancedSettingsDialog(win);
            break;

        case CmdChangeTheme:
            ShowChangeThemeDialog(win);
            break;

        case CmdSendByEmail:
            SendAsEmailAttachment(tab, win->hwndFrame);
            break;

        case CmdProperties: {
            ShowProperties(win->hwndFrame, win->ctrl);
            break;
        }

        case CmdPdShowInfo: {
            if (tab && tab->filePath && CouldBePDFDoc(tab)) {
                if (tab->hwndPDFInfo && IsWindow(tab->hwndPDFInfo)) {
                    SetForegroundWindow(tab->hwndPDFInfo);
                } else {
                    TempStr info = EngineMupdfGetPdfInfo(tab->filePath);
                    if (info) {
                        tab->hwndPDFInfo = ShowTextInWindow("PDF Info", info, &tab->hwndPDFInfo);
                    }
                }
            }
            break;
        }

        case CmdShowErrors: {
            EngineBase* engine = dm ? dm->GetEngine() : nullptr;
            if (engine && engine->HasErrors()) {
                // GetErrorsTextTemp is the engine's internal buffer; ShowTextInWindow
                // copies it before returning, so it must not be kept past this frame.
                TempStr text = engine->GetErrorsTextTemp();
                ShowTextInWindow("Errors", text);
            }
            break;
        }

        case CmdDocumentShowOutline: {
            if (tab && tab->ctrl && tab->ctrl->HasToc()) {
                if (tab->hwndPDFOutline && IsWindow(tab->hwndPDFOutline)) {
                    SetForegroundWindow(tab->hwndPDFOutline);
                } else if (tab->filePath && CouldBePDFDoc(tab)) {
                    TempStr outline = EngineMupdfGetPdfOutline(tab->filePath);
                    if (outline) {
                        tab->hwndPDFOutline = ShowTextInWindow("Document Outline", outline, &tab->hwndPDFOutline);
                    }
                } else {
                    TocTree* tocTree = tab->ctrl->GetToc();
                    if (tocTree && tocTree->root) {
                        str::Builder s;
                        TocItemToText(s, tocTree->root, 0);
                        tab->hwndPDFOutline = ShowTextInWindow("Document Outline", ToStr(s), &tab->hwndPDFOutline);
                    }
                }
            }
            break;
        }

        case CmdPdfBake:
            ShowPdfBakeDialog(win);
            break;

        case CmdConvertToPDF:
            ShowConvertToPdfDialog(win);
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

        case CmdMoveFrameFocus:
            if (!HwndIsFocused(win->hwndFrame)) {
                HwndSetFocus(win->hwndFrame);
            } else if (win->uiState.tocVisible) {
                HwndSetFocus(win->tocTreeView->hwnd);
            }
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
            LaunchBrowserWithSelection(tab, "https://www.google.com/search?q=${selection}");
            break;

        case CmdSearchSelectionWithBing:
            LaunchBrowserWithSelection(tab, "https://www.bing.com/search?q=${selection}");
            break;

        case CmdSearchSelectionWithWikipedia:
            LaunchBrowserWithSelection(tab, "https://wikipedia.org/w/index.php?search=${selection}");
            break;

        case CmdSearchSelectionWithGoogleScholar:
            LaunchBrowserWithSelection(tab, "https://scholar.google.com/scholar?q=${selection}");
            break;

        case CmdCopySelection:
            CopySelectionInTabToClipboard(tab);
            break;

        case CmdSelectAll:
            OnSelectAll(win);
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

        case CmdDebugToggleRtl:
            gForceRtl = !gForceRtl;
            for (auto* w : gWindows) {
                UpdateWindowRtlLayout(w);
            }
            break;

        case CmdDebugToggleDpiOverride:
            ToggleDpiOverride();
            break;

        case CmdDebugDownloadSymbols:
            DownloadDebugSymbols();
            break;

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

        case CmdToggleLinks:
            gGlobalPrefs->showLinks = !gGlobalPrefs->showLinks;
            for (auto& w : gWindows) {
                w->RedrawAll(true);
            }
            break;

        case CmdToggleDisableLinks:
            if (ShouldToggle(cmd, gGlobalPrefs->disableLinks)) {
                gGlobalPrefs->disableLinks = !gGlobalPrefs->disableLinks;
                SaveSettings();
                if (gGlobalPrefs->disableLinks) {
                    for (MainWindow* w : gWindows) {
                        RefHoverHide(w->refHover, w->hwndCanvas);
                        StopKeyboardLinkFollowing(w);
                    }
                }
            }
            break;

        case CmdToggleImages:
            // not a setting like showLinks: this is a debug aid, so it lasts
            // for the session and doesn't end up in everyone's settings file
            ToggleShowImageOutlines();
            for (auto& w : gWindows) {
                w->RedrawAll(true);
            }
            break;

        case CmdDebugShowFitContentArea:
            // like CmdToggleImages: session-only debug aid, not a setting
            ToggleShowFitContentArea();
            for (auto& w : gWindows) {
                w->RedrawAll(true);
            }
            break;

        case CmdToggleShowAnnotations:
            if (tab) {
                tab->hideAnnotations = !tab->hideAnnotations;
                EngineBase* engine = tab->GetEngine();
                if (engine) {
                    engine->hideAnnotations = tab->hideAnnotations;
                }
                MainWindowRerender(win);
            }
            break;

        case CmdShowAnnotations:
            if (tab && tab->hideAnnotations) {
                tab->hideAnnotations = false;
                EngineBase* engine = tab->GetEngine();
                if (engine) {
                    engine->hideAnnotations = false;
                }
                MainWindowRerender(win);
            }
            break;

        case CmdHideAnnotations:
            if (tab && !tab->hideAnnotations) {
                tab->hideAnnotations = true;
                EngineBase* engine = tab->GetEngine();
                if (engine) {
                    engine->hideAnnotations = true;
                }
                MainWindowRerender(win);
            }
            break;

#if defined(DEBUG)
        case CmdDebugTestApp:
            extern void TestApp(HINSTANCE hInstance);
            extern void TestBrowser();
            // TestApp(GetModuleHandle(nullptr));
            TestBrowser();
            break;

        case CmdDebugStartStressTest: {
            if (!win) {
                return 0;
            }

            // TODO: ideally would ask user for the cmd-line args but this will do
            Flags f;
            // f.stressTestPath = str::Dup("C:\\Users\\kjk\\!sumatra\\all formats");
            f.stressTestPath = str::Dup(StrL("D:\\sumstress"));
            f.stressRandomizeFiles = true;
            f.stressTestMax = 25;
            StartStressTest(&f, win);
        } break;

        case CmdDebugShowNotif: {
            {
                NotificationCreateArgs args;
                args.hwndParent = win->hwndCanvas;
                args.groupId = kNotifPersistentWarning;
                args.msg = "This is a second notification\nMy friend.";
                args.warning = false;
                args.timeoutMs = kNotifDefaultTimeOut;
                ShowNotification(args);
            }
            {
                NotificationCreateArgs args;
                args.hwndParent = win->hwndCanvas;
                args.groupId = kNotifAdHoc;
                args.msg = "This is a second notification\nMy friend.";
                args.warning = false;
                args.timeoutMs = 0;
                ShowNotification(args);
            }

            {
                NotificationCreateArgs args;
                args.hwndParent = win->hwndCanvas;
                args.msg = "This is a notification";
                args.groupId = kNotifAdHoc;
                args.warning = true;
                args.timeoutMs = 0;
                ShowNotification(args);
            }

            {
                NotificationCreateArgs args;
                args.hwndParent = win->hwndCanvas;
                args.groupId = kNotifAdHoc;
                args.warning = false;
                args.timeoutMs = 0;
                auto wnd = ShowNotification(args);
                UpdateNotificationProgress(wnd, "Progress", 50);
            }

            // a notification whose content is a VirtCtrl tree we build here
            ShowCustomNotification(win->hwndCanvas, MakeDebugPixmapNotifContent(win->hwndCanvas));
        } break;

        case CmdDebugCrashMe:
            CrashMe();
            break;

        case CmdDebugCorruptMemory:
            DebugCorruptMemory();
            break;
#endif

        case CmdFavoriteAdd:
            AddFavoriteForCurrentPage(win);
            break;

        case CmdFavoriteDel:
            if (win->IsDocLoaded()) {
                auto path = ctrl->GetFilePath();
                DelFavorite(path, win->currPageNo);
            }
            break;

        case CmdFavoriteToggle:
            ToggleFavorites(win);
            break;
        case CmdFavoriteShowInTab:
            ToggleFavoritesTab(win);
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

        case CmdTogglePageInfo: {
            // "page info" tip: make figuring out current page and
            // total pages count a one-key action (unless they're already visible)
            TogglePageInfoHelper(win);
        } break;

        case CmdInvertColors: {
            // swaps the page colors for this session, whatever they are and
            // whatever the theme is. Use CmdSetDocumentColorsFollowTheme to
            // change how (or whether) pages follow the theme (issue #5887)
            SetInvertPageColors(!GetInvertPageColors());
            UpdateDocumentColors();
            break;
        }

        case CmdToggleEngineeringDrawingEnhance: {
            DisplayModel* fixedDm = win->AsFixed();
            if (fixedDm) {
                EngineMupdfToggleCadEnhance(fixedDm->GetEngine());
                MainWindowRerender(win, true);
            }
            break;
        }

        case CmdTogglePreservePdfImages: {
            SetPreservePdfImagesInDarkMode(!GetPreservePdfImagesInDarkMode());
            UpdateDocumentColors();
            break;
        }

        case CmdSetDocumentColorsFollowTheme:
            ShowSetDocumentColorsFollowThemeDialog(win);
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

        case CmdToggleZoom:
            win->ToggleZoom();
            break;

        case CmdToggleCursorPosition:
            ToggleCursorPositionInDoc(win);
            break;

        case CmdToggleKeyboardLinkFollowing:
            ToggleKeyboardLinkFollowing(win);
            break;

        case CmdToggleLaserPointer:
            // the cursor itself is the feedback, so no notification
            ToggleLaserPointer(win);
            break;

        case CmdToggleHoverPreview:
            if (gGlobalPrefs->citationHoverDelay < 0) {
                gGlobalPrefs->citationHoverDelay = 300;
            } else {
                gGlobalPrefs->citationHoverDelay = -1;
                for (MainWindow* w : gWindows) {
                    RefHoverHide(w->refHover, w->hwndCanvas);
                }
            }
            SaveSettings();
            break;

        case CmdSelectTextViaKeyboard:
            ToggleSelectTextWithKeyboard(win);
            break;

        case CmdPresentationBlackBackground:
            if (win->presentation) {
                // toggle: pressing the key again restores the slide (so a
                // presenter remote bound to '.' can black out and back) (#2820)
                bool isBlack = win->presentation == PM_BLACK_SCREEN;
                win->ChangePresentationMode(isBlack ? PM_ENABLED : PM_BLACK_SCREEN);
            }
            break;
        case CmdPresentationWhiteBackground:
            if (win->presentation) {
                bool isWhite = win->presentation == PM_WHITE_SCREEN;
                win->ChangePresentationMode(isWhite ? PM_ENABLED : PM_WHITE_SCREEN);
            }
            break;

        case CmdClose: {
            CloseCurrentTab(win, false /* quitIfLast */);
            break;
        }

        case CmdCloseCurrentDocument: {
            gDontSaveSettings = true;
            CloseCurrentTab(win, true /* quitIfLast */);
            gDontSaveSettings = false;
            SaveSettings();
            break;
        }

        case CmdEditAnnotations: {
            if (!tab) return 0;
            Annotation* annot = nullptr;
            Point pt = HwndGetCursorPos(win->hwndCanvas);
            if (lp != 0) {
                // when sending from Menu.cpp mouse position is encoded as LPARAM
                pt.x = GET_X_LPARAM(lp);
                pt.y = GET_Y_LPARAM(lp);
                // pt = HwndMapWindowPoint(win->hwndCanvas, HWND_DESKTOP, pt);
            }
            int pageNoUnderCursor = dm->GetPageNoByPoint(pt);
            if (pageNoUnderCursor > 0) {
                annot = dm->GetAnnotationAtPos(pt, nullptr);
            }
            ShowEditAnnotationsWindow(tab, annot);
            return 0;
        }

        case CmdDeleteAnnotation: {
            if (!tab) return 0;
            Annotation* annot = tab->selectedAnnotation;
            if (!annot) annot = GetAnnotionUnderCursor(tab, nullptr, lp);
            if (!annot) return 0;
            DeleteAnnotationAndUpdateUI(tab, annot);
            return 0;
        } break;

        case CmdDiscardChanges: {
            // revert to the on-disk version, discarding unsaved changes (same as
            // the tab context menu); makes it work from the command palette too
            if (tab && win->IsDocLoaded()) {
                ReloadDocument(win, false);
            }
            return 0;
        }

        case CmdSetTabColor: {
            // lp carries the WindowTab* when forwarded from the tab context menu
            // (Tabs.cpp); the command palette sends 0, so use the current tab
            WindowTab* colorTab = lp ? (WindowTab*)lp : tab;
            ShowSetTabColorDialog(win, colorTab);
            return 0;
        }

        case CmdCreateAnnotHighlight:
            [[fallthrough]];
        case CmdCreateAnnotSquiggly:
            [[fallthrough]];
        case CmdCreateAnnotStrikeOut:
            [[fallthrough]];
        case CmdCreateAnnotUnderline: {
            if (!win || !tab) {
                return 0;
            }
            AnnotCreateArgs args{annotType};
            SetAnnotCreateArgs(args, cmd);
            lastCreatedAnnot = MakeAnnotationsFromSelection(tab, &args);
            if (cmd) {
                // for custom commands must explicitly provide "openedit" argument
                openAnnotationEdit = GetCommandBoolArg(cmd, kCmdArgOpenEdit, false);
            } else {
                // for built-in shortcuts, Shift opens edit window
                openAnnotationEdit = IsShiftPressed();
            }
        } break;

            // Note: duplicated in OnWindowContextMenu because slightly different handling
        case CmdCreateAnnotText:
            [[fallthrough]];
        case CmdCreateAnnotFreeText:
            [[fallthrough]];
        case CmdCreateAnnotStamp:
            [[fallthrough]];
        case CmdCreateAnnotCaret:
            [[fallthrough]];
        case CmdCreateAnnotSquare:
            [[fallthrough]];
        case CmdCreateAnnotLine:
            [[fallthrough]];
        case CmdCreateAnnotCircle: {
            if (!win || !tab || !dm) {
                return 0;
            }
            EngineBase* engine = dm->GetEngine();
            if (!engine) {
                return 0;
            }
            if (!EngineSupportsAnnotations(engine)) {
                return 0;
            }
            Point pt = HwndGetCursorPos(win->hwndCanvas);
            if (lp != 0) {
                // when sending from Menu.cpp mouse position is encoded as LPARAM
                pt.x = GET_X_LPARAM(lp);
                pt.y = GET_Y_LPARAM(lp);
            }
            int pageNoUnderCursor = dm->GetPageNoByPoint(pt);
            if (pageNoUnderCursor < 0) {
                if (!cmd) return 0;
                // this is a case of custom command invoked by clicking toolbar button
                // in which case we don't know where to place the annotation
                // so we guess it as y = 20 px of hwndFrame and x being in the middle of window
                // it's a heuristic so might not be what user expects
                // TODO: ideally creating those annotations should be more visual
                // i.e. we start interactive process of creating an annotation via mouse
                auto r = HwndWindowRect(win->hwndCanvas);
                pt.x = r.dx / 2;
                pt.y = 20;
                pageNoUnderCursor = dm->GetPageNoByPoint(pt);
                if (pageNoUnderCursor < 0) return 0;
            }
            PointF ptOnPage = dm->CvtFromScreen(pt, pageNoUnderCursor);
            pt = HwndMapWindowPoint(win->hwndCanvas, HWND_DESKTOP, pt);
            AnnotCreateArgs args{annotType};
            SetAnnotCreateArgs(args, cmd);
            lastCreatedAnnot = EngineMupdfCreateAnnotation(engine, pageNoUnderCursor, ptOnPage, &args);
            openAnnotationEdit = GetCommandBoolArg(cmd, kCmdArgOpenEdit, false);
        } break;

        case CmdCreateAnnotImageFromClipboard: {
            if (!win || !tab || !dm) {
                return 0;
            }
            EngineBase* engine = dm->GetEngine();
            if (!engine || !EngineSupportsAnnotations(engine)) {
                return 0;
            }
            Pixmap* image = GetClipboardImageAsPixmap();
            if (!image) {
                NotificationCreateArgs nargs;
                nargs.hwndParent = win->hwndCanvas;
                nargs.timeoutMs = 3000;
                nargs.msg = _TRA("No image in the clipboard");
                ShowNotification(nargs);
                return 0;
            }
            Point pt = HwndGetCursorPos(win->hwndCanvas);
            if (lp != 0) {
                // when sent from the context menu, the click position is in LPARAM
                pt.x = GET_X_LPARAM(lp);
                pt.y = GET_Y_LPARAM(lp);
            }
            int pageNoUnderCursor = dm->GetPageNoByPoint(pt);
            if (pageNoUnderCursor < 0) {
                // invoked without a position (palette / shortcut): place near top
                auto r = HwndWindowRect(win->hwndCanvas);
                pt.x = r.dx / 2;
                pt.y = 20;
                pageNoUnderCursor = dm->GetPageNoByPoint(pt);
            }
            if (pageNoUnderCursor < 0) {
                FreePixmap(image);
                return 0;
            }
            PointF ptOnPage = dm->CvtFromScreen(pt, pageNoUnderCursor);
            AnnotCreateArgs args{AnnotationType::Stamp};
            args.stampImage = image;
            lastCreatedAnnot = EngineMupdfCreateAnnotation(engine, pageNoUnderCursor, ptOnPage, &args);
            FreePixmap(image);
        } break;

        case CmdToggleLightDarkTheme:
            ToggleLightDarkTheme();
            SaveSettings();
            break;

        case CmdToggleInverseSearch:
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/5289
            // allow to temporarily disable invoking tex inverse search
            // with left mouse click
            extern bool gDisableInteractiveInverseSearch;
            gDisableInteractiveInverseSearch = !gDisableInteractiveInverseSearch;
            break;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    if (!lastCreatedAnnot) {
        return 0;
    }
    UpdateAnnotationsList(tab->editAnnotsWindow);

    EditAnnotFocus focusTarget = EditAnnotFocus::Default;
    if (GetCommandBoolArg(cmd, kCmdArgFocusEdit, false)) {
        focusTarget = EditAnnotFocus::Edit;
    } else if (GetCommandBoolArg(cmd, kCmdArgFocusList, false)) {
        focusTarget = EditAnnotFocus::List;
    }

    if (openAnnotationEdit) {
        ShowEditAnnotationsWindow(tab, lastCreatedAnnot, focusTarget);
        return 0;
    }

    // proper action for a given annotation type
    switch (lastCreatedAnnot->type) {
        case AnnotationType::Highlight:
        case AnnotationType::Squiggly:
        case AnnotationType::StrikeOut:
        case AnnotationType::Underline: {
            MainWindowRerender(win);
            ToolbarUpdateStateForWindow(win, false);
            return 0;
        }
        case AnnotationType::FreeText: {
            // for FreeText you want to edit text so show edit window
            ShowEditAnnotationsWindow(tab, lastCreatedAnnot, focusTarget);
            return 0;
        } break;
    }

    // mark as selected so it can be moved / resized
    // isNew: page was already under the cursor/selection; do not scroll the view
    SetSelectedAnnotation(tab, lastCreatedAnnot, true);
    return 0;
}

// minimum size of the window
constexpr LONG kWinMinDx = 500;
constexpr LONG kWinMinDy = 320;

static LRESULT OnFrameGetMinMaxInfo(MINMAXINFO* info) {
    // limit windows min width to prevent render loop when siderbar is too big
    info->ptMinTrackSize.x = kWinMinDx - kSidebarMinDx + gGlobalPrefs->sidebarDx;
    info->ptMinTrackSize.y = kWinMinDy;
    return 0;
}

// --- Caption code (moved from Caption.cpp) ---

#define UNDOCUMENTED_MENU_CLASS_NAME L"#32768"
#define DO_NOT_REOPEN_MENU_TIMER_ID 1
#define DO_NOT_REOPEN_MENU_DELAY_IN_MS 1
#define CBS_INACTIVE 5
#define NON_CLIENT_BAND 1

static HMENU GetUpdatedSystemMenu(HWND hwnd, bool changeDefaultItem) {
    HMENU menu = GetSystemMenu(hwnd, FALSE);
    HwndSetWindowStyle(hwnd, WS_VISIBLE, false);

    bool maximized = IsZoomed(hwnd);
    EnableMenuItem(menu, SC_SIZE, maximized ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(menu, SC_MOVE, maximized ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(menu, SC_MINIMIZE, MF_ENABLED);
    EnableMenuItem(menu, SC_MAXIMIZE, maximized ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(menu, SC_CLOSE, MF_ENABLED);
    EnableMenuItem(menu, SC_RESTORE, maximized ? MF_ENABLED : MF_GRAYED);
    if (changeDefaultItem) {
        SetMenuDefaultItem(menu, maximized ? SC_RESTORE : SC_MAXIMIZE, FALSE);
    } else {
        SetMenuDefaultItem(menu, SC_CLOSE, FALSE);
    }

    HwndSetWindowStyle(hwnd, WS_VISIBLE, true);
    return menu;
}

static void TrackCaptionPopupMenu(MainWindow* win, HMENU menu, Rect btnRect) {
    Rect rs = HwndMapLtrClientRectToScreen(win->hwndFrame, btnRect);
    TPMPARAMS tpm{};
    tpm.cbSize = sizeof(TPMPARAMS);
    tpm.rcExclude = ToRECT(rs);

    uint flags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_VERTICAL;
    int x = rs.x;
    int y = rs.y + rs.dy;
    if (IsUIRtl()) {
        x = rs.x + rs.dx;
        flags = TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_VERTICAL | TPM_LAYOUTRTL;
    }
    TrackPopupMenuEx(menu, flags, x, y, win->hwndFrame, &tpm);
}

void OpenSystemMenu(MainWindow* win) {
    Rect r = win->captionBtn[CB_SYSTEM_MENU].rect;
    HMENU systemMenu = GetUpdatedSystemMenu(win->hwndFrame, false);
    TrackCaptionPopupMenu(win, systemMenu, r);
}

static int CaptionButtonAt(MainWindow* win, Point pt) {
    UnmirrorRtl(win->hwndFrame, pt);
    for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
        if (win->captionBtn[i].visible && win->captionBtn[i].rect.Contains(pt)) {
            return i;
        }
    }
    return -1;
}

static void RepaintButton(HWND hwnd, int btnIdx, MainWindow* win) {
    if (false) {
        HwndInvalidateRect(hwnd, win->captionBtn[btnIdx].rect, false);
        UpdateWindow(hwnd);
    } else {
        HwndInvalidate(hwnd);
    }
}

static void ClearAllHighlights(MainWindow* win) {
    for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
        if (win->captionBtn[i].highlighted || win->captionBtn[i].pressed) {
            win->captionBtn[i].highlighted = false;
            win->captionBtn[i].pressed = false;
            RepaintButton(win->hwndFrame, i, win);
        }
    }
}

static void MenuBarAsPopupMenu(MainWindow* win, Rect btnRect) {
    int count = GetMenuItemCount(win->menu);
    if (count <= 0) {
        return;
    }
    HMENU popup = CreatePopupMenu();

    MENUITEMINFO mii{};
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_SUBMENU | MIIM_STRING;
    for (int i = 0; i < count; i++) {
        mii.dwTypeData = nullptr;
        GetMenuItemInfo(win->menu, i, TRUE, &mii);
        if (!mii.hSubMenu || !mii.cch) {
            continue;
        }
        mii.cch++;
        WCHAR* subMenuName = AllocArrayTemp<WCHAR>((int)mii.cch);
        mii.dwTypeData = subMenuName;
        GetMenuItemInfo(win->menu, i, TRUE, &mii);
        AppendMenuW(popup, MF_POPUP | MF_STRING, (UINT_PTR)mii.hSubMenu, subMenuName);
    }

    MarkMenuOwnerDraw(popup);
    TrackCaptionPopupMenu(win, popup, btnRect);
    FreeMenuOwnerDrawInfoData(popup);

    while (count > 0) {
        --count;
        RemoveMenu(popup, count, MF_BYPOSITION);
    }
    DestroyMenu(popup);
}

static void HandleCaptionClick(MainWindow* win, int btnIdx) {
    switch (btnIdx) {
        case CB_MINIMIZE:
            PostMessageW(win->hwndFrame, WM_SYSCOMMAND, SC_MINIMIZE, 0);
            break;
        case CB_MAXIMIZE:
            PostMessageW(win->hwndFrame, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
            break;
        case CB_RESTORE:
            PostMessageW(win->hwndFrame, WM_SYSCOMMAND, SC_RESTORE, 0);
            break;
        case CB_CLOSE:
            PostMessageW(win->hwndFrame, WM_SYSCOMMAND, SC_CLOSE, 0);
            break;
        case CB_MENU:
            if (!KillTimer(win->hwndFrame, DO_NOT_REOPEN_MENU_TIMER_ID) && !win->isMenuOpen) {
                Rect r = win->captionBtn[CB_MENU].rect;
                win->isMenuOpen = true;
                RepaintButton(win->hwndFrame, CB_MENU, win);
                MenuBarAsPopupMenu(win, r);
                win->isMenuOpen = false;
                RepaintButton(win->hwndFrame, CB_MENU, win);
                SetTimer(win->hwndFrame, DO_NOT_REOPEN_MENU_TIMER_ID, DO_NOT_REOPEN_MENU_DELAY_IN_MS, nullptr);
            }
            HwndSetFocus(win->hwndFrame);
            break;
        case CB_SYSTEM_MENU:
            OpenSystemMenu(win);
            break;
    }
}

void RelayoutCaption(MainWindow* win) {
    if (!win->captionLayout || !win->tabsInTitlebar) {
        return;
    }
    DpiSetFromHwnd(win->hwndFrame);
    SyncCaptionLayout(win);
    Rect rc = win->captionRect;
    if (rc.IsEmpty()) {
        return;
    }
    bool twoRow = IsShowingMenuBarRebar(win);
    bool hasFileTabs = WinHasFileTabs(win);
    DeferWinPosHelper dh;
    BindSlot(win->capMenuSlot, win->hwndMenuReBar, &dh, twoRow);
    BindSlot(win->capTabsRow1, win->tabsCtrl ? win->tabsCtrl->hwnd : nullptr, &dh, !twoRow);
    BindSlot(win->capTabsRow2, win->tabsCtrl ? win->tabsCtrl->hwnd : nullptr, &dh, twoRow && hasFileTabs);
    int dy = win->captionLayout->MinIntrinsicHeight(rc.dx);
    if (dy <= 0) {
        dy = rc.dy;
    }
    LayoutToSize(win->captionLayout, {rc.dx, dy});
    win->captionLayout->SetBounds({rc.x, rc.y, rc.dx, dy});
    win->captionRect = {rc.x, rc.y, rc.dx, dy};
    BindSlot(win->capMenuSlot, nullptr, nullptr, false);
    BindSlot(win->capTabsRow1, nullptr, nullptr, false);
    BindSlot(win->capTabsRow2, nullptr, nullptr, false);
    dh.End();
    UpdateTabWidth(win);
    if (IsRunningOnWine()) {
        logf("RelayoutCaption: captionRect=(%d,%d,%d,%d) twoRow=%d hasFileTabs=%d\n", win->captionRect.x,
             win->captionRect.y, win->captionRect.dx, win->captionRect.dy, (int)twoRow, (int)hasFileTabs);
    }
}

static void DrawCaptionButton(MainWindow* win, HDC hdc, ButtonInfo* bi) {
    int button = bi->id;
    if (!bi->visible) {
        return;
    }
    Rect rButton = bi->rect;
    Rect rc = rButton;

    bool isSysButton = (button == CB_MINIMIZE || button == CB_MAXIMIZE || button == CB_RESTORE || button == CB_CLOSE);

    int stateId;
    if (bi->pressed) {
        stateId = CBS_PUSHED;
    } else if (bi->highlighted) {
        stateId = CBS_HOT;
    } else if (bi->inactive) {
        stateId = CBS_INACTIVE;
    } else {
        stateId = CBS_NORMAL;
    }

    Graphics gfx(hdc);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeNone);

    if (isSysButton) {
        Color bgc = ThemeControlBackgroundColor();
        SolidBrush bgBrNormal(GdiRgbFromColor(bgc));
        gfx.FillRectangle(&bgBrNormal, rButton.x, rButton.y, rButton.dx, rButton.dy);

        bool isClose = (button == CB_CLOSE);
        bool isHot = (stateId == CBS_HOT);
        bool isPushed = (stateId == CBS_PUSHED);
        bool isInactive = (stateId == CBS_INACTIVE);

        if (isHot || isPushed) {
            Gdiplus::Color bgCol;
            if (isClose) {
                bgCol = isPushed ? Gdiplus::Color(200, 196, 43, 28) : Gdiplus::Color(255, 196, 43, 28);
            } else {
                Color hotBg = isPushed ? AccentColor(bgc, 40) : AccentColor(bgc, 20);
                bgCol = GdiRgbFromColor(hotBg);
            }
            SolidBrush bgBr(bgCol);
            int x = rButton.x;
            int y = rButton.y;
            int w = rButton.dx;
            int h = rButton.dy;
            // leave the frame-border pixel visible at the outer top corner;
            // only the outer edge borders the frame, the bottom is interior
            if (isClose && !IsZoomed(win->hwndFrame)) {
                if (IsUIRtl()) {
                    x += kFrameBorderSize;
                    w -= kFrameBorderSize;
                } else {
                    w -= kFrameBorderSize;
                }
            }
            gfx.FillRectangle(&bgBr, x, y, w, h);
        }

        Color iconCol;
        if (isInactive) {
            iconCol = MkRgb(153, 153, 153);
        } else if (isClose && (isHot || isPushed)) {
            iconCol = kColWhite;
        } else {
            iconCol = ThemeWindowTextColor();
        }

        // Windows 11 style caption glyphs (Segoe Fluent Icons outlines)
        CaptionSysButtonKind kind = CaptionSysButtonKind::Close;
        switch (button) {
            case CB_MINIMIZE:
                kind = CaptionSysButtonKind::Minimize;
                break;
            case CB_MAXIMIZE:
                kind = CaptionSysButtonKind::Maximize;
                break;
            case CB_RESTORE:
                kind = CaptionSysButtonKind::Restore;
                break;
        }
        int iconPx = DpiScale(kCaptionGlyphDip);
        DrawCaptionSysButtonGlyph(hdc, kind, rc, iconCol, iconPx);
    } else if (button == CB_MENU) {
        SolidBrush bgBrMenu(GdiRgbFromColor(ThemeControlBackgroundColor()));
        gfx.FillRectangle(&bgBrMenu, rButton.x, rButton.y, rButton.dx, rButton.dy);

        if (win->isMenuOpen) {
            stateId = CBS_PUSHED;
        }
        u8 buttonRGB = 1;
        if (CBS_PUSHED == stateId) {
            buttonRGB = 0;
        } else if (CBS_HOT == stateId) {
            buttonRGB = 255;
        }

        if (buttonRGB != 1) {
            if (GetLightness(ThemeWindowTextColor()) > GetLightness(ThemeControlBackgroundColor())) {
                buttonRGB ^= 0xff;
            }
            u8 buttonAlpha = u8((255 - abs((int)GetLightness(ThemeControlBackgroundColor()) - buttonRGB)) / 2);
            SolidBrush br(Gdiplus::Color(buttonAlpha, buttonRGB, buttonRGB, buttonRGB));
            gfx.FillRectangle(&br, rc.x, rc.y, rc.dx, rc.dy);
        }
        Color c = ThemeWindowTextColor();
        u8 r, g, b;
        UnpackColor(c, r, g, b);
        float width = floorf((float)rc.dy / 8.0f);
        Pen p(Gdiplus::Color(r, g, b), width);
        rc.Inflate(-(int)lroundf((float)rc.dx * 0.2f), -(int)lroundf((float)rc.dy * 0.3f));
        for (int i = 0; i < 3; i++) {
            gfx.DrawLine(&p, rc.x, rc.y + (i * rc.dy / 2), rc.x + rc.dx, rc.y + (i * rc.dy / 2));
        }
    } else if (button == CB_SYSTEM_MENU) {
        SolidBrush bgBrSys(GdiRgbFromColor(ThemeControlBackgroundColor()));
        gfx.FillRectangle(&bgBrSys, rButton.x, rButton.y, rButton.dx, rButton.dy);
        int xIcon = DpiGetSystemMetrics(SM_CXSMICON);
        int yIcon = DpiGetSystemMetrics(SM_CYSMICON);
        HICON hIcon = (HICON)GetClassLongPtr(win->hwndFrame, GCLP_HICONSM);
        int x = rButton.x + ((rButton.dx - xIcon) / 2);
        int y = rButton.y + ((rButton.dy - yIcon) / 2);
        DrawIconEx(hdc, x, y, hIcon, xIcon, yIcon, 0, nullptr, DI_NORMAL);
    }
}

static WCHAR gMenuAccelPressed = 0;

static LRESULT CustomCaptionFrameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool* callDef, MainWindow* win) {
    switch (msg) {
        case WM_SETTINGCHANGE:
            if (wp == SPI_SETNONCLIENTMETRICS) {
                RelayoutCaption(win);
            }
            break;

        case WM_NCPAINT: {
            if (win->isFullScreen || win->presentation) {
                *callDef = false;
                return 0;
            }
            // Paint residual NC strips (top 1px for DWM; bottom only if present).
            // Leaving them unpainted shows as a white/wrong-color glitch (#5851).
            HDC hdc = GetWindowDC(hwnd);
            if (hdc) {
                Rect wr = HwndWindowRect(hwnd);
                Rect cr = HwndClientRect(hwnd);
                // client origin in window coordinates (window DC origin = top-left of frame)
                Point clientScreen = HwndClientToScreen(hwnd, Point(0, 0));
                int clientX = clientScreen.x - wr.x;
                int clientY = clientScreen.y - wr.y;
                HBRUSH br = CreateSolidBrush(ThemeControlBackgroundColor());
                if (clientY > 0) {
                    RECT rc = {0, 0, wr.dx, clientY};
                    HdcFillRect(hdc, ToRect(rc), br);
                }
                int bottomNcTop = clientY + cr.dy;
                if (bottomNcTop < wr.dy) {
                    RECT rc = {0, bottomNcTop, wr.dx, wr.dy};
                    HdcFillRect(hdc, ToRect(rc), br);
                }
                // side NC (left/right frame borders when not maximized)
                if (clientX > 0) {
                    RECT rc = {0, clientY, clientX, bottomNcTop};
                    HdcFillRect(hdc, ToRect(rc), br);
                }
                int rightNcLeft = clientX + cr.dx;
                if (rightNcLeft < wr.dx) {
                    RECT rc = {rightNcLeft, clientY, wr.dx, bottomNcTop};
                    HdcFillRect(hdc, ToRect(rc), br);
                }
                DeleteObject(br);
                ReleaseDC(hwnd, hdc);
            }
            *callDef = false;
            return 0;
        }

        case WM_PAINT: {
            if (win->isFullScreen || win->presentation) {
                break; // no custom caption painting in fullscreen
            }
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            LogRedraw("WM_PAINT", hwnd, &ps.rcPaint);

            Rect cr = win->captionRect;
            // span the full client width so the right frame-border column is painted
            Rect captionArea = {0, 0, HwndClientRect(hwnd).dx, cr.y + cr.dy};
            DoubleBuffer buffer(hwnd, captionArea);
            HDC memDC = buffer.GetDC();
            // RTL windows mirror DC coordinates; use explicit LTR coords for caption painting
            bool isRtl = IsUIRtl();
            if (isRtl) {
                SetLayout(memDC, 0);
            }
            {
                HBRUSH brCap = CreateSolidBrush(ThemeControlBackgroundColor());
                RECT rcFill = ToRECT(captionArea);
                HdcFillRect(memDC, ToRect(rcFill), brCap);
                DeleteObject(brCap);
            }
            for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
                DrawCaptionButton(win, memDC, &win->captionBtn[i]);
            }
            if (isRtl) {
                SetLayout(hdc, 0);
            }
            buffer.Flush(hdc);

            // paint the 3px border outside the caption area
            // (WS_CLIPCHILDREN prevents painting over child windows)
            {
                RECT rcCaption = ToRECT(captionArea);
                ExcludeClipRect(hdc, rcCaption.left, rcCaption.top, rcCaption.right, rcCaption.bottom);
                HBRUSH brBorder = CreateSolidBrush(ThemeControlBackgroundColor());
                HdcFillRect(hdc, ToRect(ps.rcPaint), brBorder);
                DeleteObject(brBorder);
            }
            // the splitters are virtual controls of this window
            if (win->frameRoot) {
                GfxHdc gfx(hdc);
                win->frameRoot->Paint(&gfx, ToRect(ps.rcPaint));
            }

            EndPaint(hwnd, &ps);
            *callDef = false;
            return 0;
        }

        case WM_NCACTIVATE:
            for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
                win->captionBtn[i].inactive = wp == FALSE;
            }
            if (!IsIconic(hwnd)) {
                RECT rc = ToRECT(win->captionRect);
                if (IsCurrentThemeDefault()) {
                    rc.bottom += NON_CLIENT_BAND;
                }
                uint flags = RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW;
                RedrawWindow(hwnd, &rc, nullptr, flags);
                *callDef = false;
                return TRUE;
            }
            break;

        case WM_TIMER:
            if (wp == DO_NOT_REOPEN_MENU_TIMER_ID) {
                KillTimer(hwnd, DO_NOT_REOPEN_MENU_TIMER_ID);
                *callDef = false;
                return 0;
            }
            if (wp == kFindDebounceTimerId) {
                FindDebounceTimerFired(win);
                *callDef = false;
                return 0;
            }
            break;

        case WM_THEMECHANGED:
            break;

        case WM_NCCALCSIZE: {
            RECT* r = wp == TRUE ? &((NCCALCSIZE_PARAMS*)lp)->rgrc[0] : (RECT*)lp;
            if (IsRunningOnWine()) {
                logf("WM_NCCALCSIZE: before=(%ld,%ld,%ld,%ld) zoomed=%d\n", r->left, r->top, r->right, r->bottom,
                     (int)IsZoomed(hwnd));
            }
            bool isFullScreen = win->isFullScreen || win->presentation;
            if (IsZoomed(hwnd) && !isFullScreen) {
                // The maximized outer frame extends beyond the work area. Global
                // system metrics can still describe the old monitor during a
                // cross-DPI move, leaving an unpainted strip at the screen edge.
                // The proposed rect identifies the destination monitor reliably.
                // Client fills the full work area — do not shrink by NON_CLIENT_BAND
                // at the bottom (that left an unpainted 1px gap above the taskbar
                // with tabs-in-titlebar; issue #5851).
                HMONITOR monitor = MonitorFromRect(r, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                if (monitor && GetMonitorInfoW(monitor, &mi)) {
                    *r = mi.rcWork;
                } else {
                    int frameX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                    int frameY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                    r->left += frameX;
                    r->top += frameY;
                    r->right -= frameX;
                    r->bottom -= frameY;
                }
            } else if (!isFullScreen) {
                // keep 1px non-client area at top so DWM preserves content
                // during resize (returning 0 makes DWM clear the surface)
                r->top += 1;
            }
            if (IsRunningOnWine()) {
                logf("WM_NCCALCSIZE: after=(%ld,%ld,%ld,%ld) clientDy=%ld cyFrame=%d cyCaption=%d\n", r->left, r->top,
                     r->right, r->bottom, r->bottom - r->top, GetSystemMetrics(SM_CYFRAME),
                     GetSystemMetrics(SM_CYCAPTION));
            }
            *callDef = false;
            return 0;
        }

        case WM_NCHITTEST: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            Rect wrc = HwndWindowRect(hwnd);

            // use a larger hit-test area than the visible border for easier resizing
            if (!IsZoomed(hwnd) && !win->isFullScreen && !win->presentation) {
                int b = kFrameResizeHitTest;
                bool onLeft = (x - wrc.x) < b;
                bool onRight = (wrc.x + wrc.dx - x) <= b;
                bool onTop = (y - wrc.y) < b;
                bool onBottom = (wrc.y + wrc.dy - y) <= b;

                if (onTop && onLeft) {
                    *callDef = false;
                    return HTTOPLEFT;
                }
                if (onTop && onRight) {
                    *callDef = false;
                    return HTTOPRIGHT;
                }
                if (onBottom && onLeft) {
                    *callDef = false;
                    return HTBOTTOMLEFT;
                }
                if (onBottom && onRight) {
                    *callDef = false;
                    return HTBOTTOMRIGHT;
                }
                if (onLeft) {
                    *callDef = false;
                    return HTLEFT;
                }
                if (onRight) {
                    *callDef = false;
                    return HTRIGHT;
                }
                if (onTop) {
                    *callDef = false;
                    return HTTOP;
                }
                if (onBottom) {
                    *callDef = false;
                    return HTBOTTOM;
                }
            }

            {
                Point ptClient = HwndScreenToClient(hwnd, Point(x, y));
                int btnIdx = CaptionButtonAt(win, ptClient);
                if (btnIdx >= 0) {
                    if (btnIdx == CB_MAXIMIZE || btnIdx == CB_RESTORE) {
                        *callDef = false;
                        return HTMAXBUTTON;
                    }
                    *callDef = false;
                    return HTCLIENT;
                }
            }

            {
                Point pt{x, y};
                Rect rClient = HwndMapRectToWindow(HwndClientRect(hwnd), hwnd, HWND_DESKTOP);
                Rect rCaption = HwndMapRectToWindow(win->captionRect, hwnd, HWND_DESKTOP);
                if (rClient.Contains(pt) && pt.y < rCaption.y + rCaption.dy) {
                    *callDef = false;
                    return HTCAPTION;
                }
            }
        } break;

        case WM_NCLBUTTONDOWN:
            if (wp == HTMAXBUTTON) {
                *callDef = false;
                return 0;
            }
            break;

        case WM_NCLBUTTONUP:
            if (wp == HTMAXBUTTON) {
                WPARAM cmd = IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE;
                PostMessageW(hwnd, WM_SYSCOMMAND, cmd, 0);
                *callDef = false;
                return 0;
            }
            break;

        case WM_NCMOUSEMOVE: {
            int btnIdx = IsZoomed(hwnd) ? CB_RESTORE : CB_MAXIMIZE;
            if (wp == HTMAXBUTTON) {
                if (!win->captionBtn[btnIdx].highlighted) {
                    win->captionBtn[btnIdx].highlighted = true;
                    RepaintButton(hwnd, btnIdx, win);
                }
            } else {
                if (win->captionBtn[btnIdx].highlighted) {
                    win->captionBtn[btnIdx].highlighted = false;
                    RepaintButton(hwnd, btnIdx, win);
                }
            }
        } break;

        case WM_NCMOUSELEAVE:
            ClearAllHighlights(win);
            break;

        case WM_MOUSEMOVE: {
            Point ptm{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int btnIdx = CaptionButtonAt(win, ptm);
            for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
                bool shouldHighlight = (i == btnIdx);
                if (win->captionBtn[i].highlighted != shouldHighlight) {
                    win->captionBtn[i].highlighted = shouldHighlight;
                    RepaintButton(hwnd, i, win);
                }
            }
            if (btnIdx >= 0) {
                TrackMouseLeave(hwnd);
            }
        } break;

        case WM_MOUSELEAVE:
            ClearAllHighlights(win);
            break;

        case WM_LBUTTONDOWN: {
            Point ptd{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int btnIdx = CaptionButtonAt(win, ptd);
            if (btnIdx >= 0) {
                win->captionBtn[btnIdx].pressed = true;
                RepaintButton(hwnd, btnIdx, win);
                SetCapture(hwnd);
                *callDef = false;
                return 0;
            }
        } break;

        case WM_LBUTTONUP: {
            bool anyPressed = false;
            for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
                anyPressed |= win->captionBtn[i].pressed;
            }
            if (!anyPressed) {
                // not ours: whoever else is tracking the mouse (a splitter
                // drag) has to see the button go up, or it keeps dragging
                break;
            }
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            Point ptu{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int btnIdx = CaptionButtonAt(win, ptu);
            for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
                if (win->captionBtn[i].pressed) {
                    win->captionBtn[i].pressed = false;
                    RepaintButton(hwnd, i, win);
                    if (i == btnIdx) {
                        HandleCaptionClick(win, i);
                    }
                }
            }
            *callDef = false;
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            Point ptdc{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int btnIdx = CaptionButtonAt(win, ptdc);
            if (btnIdx == CB_SYSTEM_MENU) {
                PostMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
                *callDef = false;
                return 0;
            }
        } break;

        case WM_NCRBUTTONUP:
            if (wp == HTCAPTION) {
                HMENU menu = GetUpdatedSystemMenu(hwnd, true);
                uint flags = TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD;
                if (GetSystemMetrics(SM_MENUDROPALIGNMENT)) {
                    flags |= TPM_RIGHTALIGN;
                }
                WPARAM cmd = TrackPopupMenu(menu, flags, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 0, hwnd, nullptr);
                if (cmd) {
                    PostMessageW(hwnd, WM_SYSCOMMAND, cmd, 0);
                }
                *callDef = false;
                return 0;
            }
            break;

        case WM_SYSCOMMAND:
            if (wp == SC_KEYMENU) {
                if (IsShowingMenuBarRebar(win)) {
                    // activate the rebar menu bar directly
                    ActivateMenuBarByAccel(win, (WCHAR)lp);
                    *callDef = false;
                    return 0;
                }
                gMenuAccelPressed = (WCHAR)lp;
                if (' ' == gMenuAccelPressed) {
                    Str after;
                    if (str::CutChar(_TRA("&Window"), '&', nullptr, &after) && after.len > 0) {
                        char c = after.s[0];
                        gMenuAccelPressed = (WCHAR)c;
                    }
                }
                HandleCaptionClick(win, CB_MENU);
                *callDef = false;
                return 0;
            }
            break;

        case WM_INITMENUPOPUP:
            // apply dark mode to popup menu window
            DarkModeApplyToMenuWindow(FindWindow(UNDOCUMENTED_MENU_CLASS_NAME, nullptr));
            if (gMenuAccelPressed) {
                HWND hMenu = FindWindow(UNDOCUMENTED_MENU_CLASS_NAME, nullptr);
                if (hMenu) {
                    if ('a' <= gMenuAccelPressed && gMenuAccelPressed <= 'z') {
                        gMenuAccelPressed -= 'a' - 'A';
                    }
                    if ('A' <= gMenuAccelPressed && gMenuAccelPressed <= 'Z') {
                        PostMessageW(hMenu, WM_KEYDOWN, gMenuAccelPressed, 0);
                    } else {
                        PostMessageW(hMenu, WM_CHAR, gMenuAccelPressed, 0);
                    }
                }
                gMenuAccelPressed = 0;
            }
            break;

        case WM_SYSCOLORCHANGE:
            break;
    }

    *callDef = true;
    return 0;
}

// --- End caption code ---

HWND gLastActiveFrameHwnd = nullptr;

// Text-to-speech/read-aloud integration
static constexpr UINT WM_TTS_EVENT = WM_APP + 0x421;

static WindowTab* gReadAloudSourceTab = nullptr;
static WindowTab* gReadAloudSessionTab = nullptr;
static HMENU gReadAloudAppSubmenu = nullptr;
static HMENU gReadAloudContextSubmenu = nullptr;

void SetReadAloudAppSubmenu(HMENU menu) {
    gReadAloudAppSubmenu = menu;
}

bool IsReadAloudAppSubmenu(HMENU menu) {
    return menu && menu == gReadAloudAppSubmenu;
}

void SetReadAloudContextSubmenu(HMENU menu) {
    gReadAloudContextSubmenu = menu;
}

bool IsReadAloudContextSubmenu(HMENU menu) {
    return menu && menu == gReadAloudContextSubmenu;
}

HMENU GetReadAloudContextSubmenu() {
    return gReadAloudContextSubmenu;
}

static void ReadAloudShowNotif(WindowTab* tab, Str msg);

static void ReadAloudSaveVoicePref(Str voiceId) {
    if (!gGlobalPrefs) {
        return;
    }
    str::ReplaceWithCopy(&gGlobalPrefs->readAloudVoiceId, voiceId);
    SaveSettings();
}

// WinRT speech synthesis is too slow for whole-document requests; speak in chunks.
static constexpr int kReadAloudMaxChunkLen = 1024;

static int ReadAloudFindChunkEnd(Str text, int start, int maxLen) {
    int textLen = text.len;
    if (start >= textLen) {
        return textLen;
    }

    int end = start + maxLen;
    if (end >= textLen) {
        return textLen;
    }

    while (end > start && text.s[end] != ' ') {
        end--;
    }
    if (end <= start) {
        end = start + maxLen;
        end = std::min(end, textLen);
    }
    return end;
}

static bool ReadAloudHasMoreChunks(WindowTab* tab) {
    if (!tab || len(tab->readAloudText) == 0) {
        return false;
    }
    return tab->readAloudChunkEnd < tab->readAloudText.len;
}

static void ReadAloudFinishSession(WindowTab* tab, MainWindow* win) {
    if (!tab) {
        return;
    }

    logf("ReadAloud: FinishSession\n");
    if (tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
        HwndInvalidate(tab->win->hwndCanvas);
        ReadAloudPlaybackBarHide(tab->win);
    }
    str::Free(tab->readAloudText);
    tab->readAloudText = {};
    tab->readAloudResumePos = -1;
    tab->readAloudChunkStart = 0;
    tab->readAloudChunkEnd = 0;
    if (tab->readAloudHighlight) {
        ReadAloudHighlightFree(tab->readAloudHighlight);
        delete tab->readAloudHighlight;
        tab->readAloudHighlight = nullptr;
    }
    tab->readAloudHighlightBase = 0;
    tab->readAloudAutoScroll = false;
    tab->readAloudScope = 0;
    ReadAloudClearSourceTab();
    if (gReadAloudSessionTab == tab) {
        gReadAloudSessionTab = nullptr;
    }
    if (win) {
        ToolbarUpdateStateForWindow(win, true);
    }
}

static bool ReadAloudSpeakChunk(WindowTab* tab, Str errMsg) {
    if (!tab || len(tab->readAloudText) == 0) {
        return false;
    }

    int start = tab->readAloudChunkEnd;
    int textLen = tab->readAloudText.len;
    int end = ReadAloudFindChunkEnd(tab->readAloudText, start, kReadAloudMaxChunkLen);
    if (start >= end) {
        return false;
    }

    int chunkLen = end - start;
    TempStr chunk = str::DupTemp(Str(tab->readAloudText.s + start, (int)((size_t)chunkLen)));
    logf("ReadAloud: SpeakChunk: %d..%d of %d (mapBase=%d)\n", start, end, textLen, tab->readAloudHighlightBase);

    if (!TtsSpeakUtf8(chunk)) {
        logf("ReadAloud: SpeakChunk: TtsSpeakUtf8 failed\n");
        ReadAloudShowNotif(tab, errMsg);
        return false;
    }

    tab->readAloudChunkStart = start;
    tab->readAloudChunkEnd = end;
    ToolbarUpdateStateForWindow(tab->win, true);
    HwndInvalidate(tab->win->hwndCanvas);
    return true;
}

// Text cleanup for speech
static bool IsReadAloudLowerAscii(char c) {
    return c >= 'a' && c <= 'z';
}

static bool IsReadAloudLineBreak(char c) {
    return c == '\r' || c == '\n';
}

static bool IsReadAloudHorizontalSpace(char c) {
    return c == ' ' || c == '\t';
}

static TempStr CleanReadAloudTextTemp(Str text) {
    if (len(text) == 0) {
        return {};
    }

    str::Builder out;
    int i = 0;
    bool lastWasSpace = false;

    while (i < text.len) {
        char c = text.s[i];

        // Remove likely soft hyphenation caused by PDF line wrapping:
        // "cap-\nturing" -> "capturing"
        //
        // Conservative rule: only join lowercase ASCII on both sides.
        // This avoids damaging many intentional hyphen cases.
        if (c == '-' && i + 1 < text.len && IsReadAloudLineBreak(text.s[i + 1])) {
            int after = i + 1;

            while (after < text.len && IsReadAloudLineBreak(text.s[after])) {
                after++;
            }

            bool prevIsLower = i > 0 && IsReadAloudLowerAscii(text.s[i - 1]);
            bool nextIsLower = after < text.len && IsReadAloudLowerAscii(text.s[after]);

            if (prevIsLower && nextIsLower) {
                i = after;
                lastWasSpace = false;
                continue;
            }
        }

        // Convert extracted visual line breaks into spaces.
        if (IsReadAloudLineBreak(c)) {
            int lineBreaks = 0;

            while (i < text.len && IsReadAloudLineBreak(text.s[i])) {
                if (text.s[i] == '\n') {
                    lineBreaks++;
                }
                i++;
            }

            while (i < text.len && IsReadAloudHorizontalSpace(text.s[i])) {
                i++;
            }

            if (!lastWasSpace && len(out) > 0) {
                out.AppendChar(' ');
                lastWasSpace = true;
            }

            // Keep a slightly stronger pause for paragraph breaks.
            if (lineBreaks >= 2) {
                out.AppendChar(' ');
            }

            continue;
        }

        // Collapse spaces and tabs.
        if (IsReadAloudHorizontalSpace(c)) {
            if (!lastWasSpace && len(out) > 0) {
                out.AppendChar(' ');
                lastWasSpace = true;
            }

            i++;
            continue;
        }

        out.AppendChar(c);
        lastWasSpace = false;
        i++;
    }

    if (out.IsEmpty()) {
        return {};
    }
    return ToStrTemp(out);
}

// Read-aloud lifetime and commands
static void ReadAloudSetSourceTab(WindowTab* tab) {
    gReadAloudSourceTab = tab;
}

static void ReadAloudClearSourceTab() {
    gReadAloudSourceTab = nullptr;
}

static void StopReadAloudIfSourceTab(WindowTab* tab) {
    if (!tab || gReadAloudSourceTab != tab) {
        return;
    }

    if (TtsIsSpeaking()) {
        TtsStop();
    }

    if (tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
        HwndInvalidate(tab->win->hwndCanvas);
    }
    ReadAloudClearSourceTab();
}

// last-resort guard: whatever a close path forgets, a tab that is being
// destroyed can't be left pointed at by the read-aloud state
void ReadAloudForgetTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    if (gReadAloudSourceTab == tab) {
        gReadAloudSourceTab = nullptr;
    }
    if (gReadAloudSessionTab == tab) {
        gReadAloudSessionTab = nullptr;
    }
    for (MainWindow* win : gWindows) {
        ReadAloudPlaybackBarForgetTab(win, tab);
    }
}

static void StopReadAloudIfSourceWindow(MainWindow* win) {
    if (!win || !gReadAloudSourceTab || gReadAloudSourceTab->win != win) {
        return;
    }

    if (TtsIsSpeaking()) {
        TtsStop();
    }

    ReadAloudClearSourceTab();
}

// reset "Continue reading" state, called when its document goes away
static void ResetReadAloudStateForTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    StopReadAloudIfSourceTab(tab);
    str::Free(tab->readAloudText);
    tab->readAloudText = {};
    tab->readAloudResumePos = -1;
    if (tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
    }
    if (tab->readAloudHighlight) {
        ReadAloudHighlightFree(tab->readAloudHighlight);
        delete tab->readAloudHighlight;
        tab->readAloudHighlight = nullptr;
    }
    tab->readAloudHighlightBase = 0;
    tab->readAloudChunkStart = 0;
    tab->readAloudChunkEnd = 0;
    tab->readAloudAutoScroll = false;
    tab->readAloudScope = 0;
    if (gReadAloudSessionTab == tab) {
        gReadAloudSessionTab = nullptr;
    }
    if (tab->win) {
        ReadAloudPlaybackBarHide(tab->win);
    }
}

// stop reading and remember where we stopped so that "Continue reading"
// can pick up from there
static void ReadAloudStopRememberPos() {
    // drain pending word-boundary events for an accurate position
    TtsProcessEvents();
    WindowTab* tab = gReadAloudSourceTab;
    if (tab && TtsIsSpeaking()) {
        int pos = TtsGetSpokenPosUtf8();
        if (pos >= 0) {
            int absPos = tab->readAloudHighlightBase + tab->readAloudChunkStart + pos;
            int maxPos = tab->readAloudHighlightBase + tab->readAloudText.len;
            if (absPos > 0 && absPos < maxPos) {
                tab->readAloudResumePos = absPos;
            }
        }
    }
    TtsStop();
    ReadAloudClearSourceTab();
    if (tab && tab->win) {
        ReadAloudHighlightTimerStop(tab->win);
        HwndInvalidate(tab->win->hwndCanvas);
        ReadAloudPlaybackBarUpdateSession(tab);
    }
}

void ReadAloudPlaybackPauseOrResume() {
    WindowTab* tab = gReadAloudSessionTab;
    if (!tab) {
        tab = GetReadAloudSourceTab();
    }
    if (!tab || !tab->win) {
        return;
    }

    if (TtsIsSpeaking() && GetReadAloudSourceTab() == tab) {
        ReadAloudStopRememberPos();
        ToolbarUpdateStateForWindow(tab->win, true);
    } else if (CanContinueReadAloud(tab)) {
        ReadAloudContinueInTab(tab);
    }
}

// preset playback speeds offered in the Speed menu and cycled by the speed
// button on the playback bar
constexpr float kReadAloudSpeeds[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f};

// e.g. "1x", "0.75x", "1.5x"
TempStr ReadAloudSpeedLabelTemp(float speed) {
    int hundredths = (int)lroundf(speed * 100.0f);
    int whole = hundredths / 100;
    int frac = hundredths % 100;
    if (frac == 0) {
        return fmt("%dx", whole);
    }
    if (frac % 10 == 0) {
        return fmt("%d.%dx", whole, frac / 10);
    }
    return fmt("%d.%02dx", whole, frac);
}

// index of the preset closest to the current speed (the setting can hold
// an arbitrary value edited by hand)
static int ReadAloudClosestSpeedIdx() {
    float curr = TtsGetSpeed();
    int idx = 0;
    float bestDist = -1;
    for (int i = 0; i < dimofi(kReadAloudSpeeds); i++) {
        float dist = kReadAloudSpeeds[i] - curr;
        if (dist < 0) {
            dist = -dist;
        }
        if (bestDist < 0 || dist < bestDist) {
            bestDist = dist;
            idx = i;
        }
    }
    return idx;
}

static void ReadAloudSetSpeed(float speed) {
    TtsSetSpeed(speed);
    gGlobalPrefs->readAloudSpeed = TtsGetSpeed();
    logf("ReadAloud: SetSpeed: %s\n", ReadAloudSpeedLabelTemp(TtsGetSpeed()));
    SaveSettings();

    // the WinRT backend applies the new speed only to newly synthesized
    // chunks, so re-speak from the current position
    WindowTab* tab = GetReadAloudSourceTab();
    if (tab && TtsIsSpeaking()) {
        ReadAloudStopRememberPos();
        if (CanContinueReadAloud(tab)) {
            ReadAloudContinueInTab(tab);
        }
    }
}

// dir is +1 (next speed) or -1 (previous speed), wraps around
void ReadAloudPlaybackCycleSpeed(int dir) {
    int n = dimofi(kReadAloudSpeeds);
    int idx = (ReadAloudClosestSpeedIdx() + dir + n) % n;
    ReadAloudSetSpeed(kReadAloudSpeeds[idx]);
}

void ReadAloudPlaybackStop() {
    WindowTab* tab = gReadAloudSessionTab;
    if (!tab) {
        tab = GetReadAloudSourceTab();
    }
    if (!tab) {
        return;
    }
    if (TtsIsSpeaking()) {
        TtsStop();
    }
    ReadAloudFinishSession(tab, tab->win);
}

static void ReadAloudShowNotif(WindowTab* tab, Str msg) {
    NotificationCreateArgs args;
    args.hwndParent = tab->win->hwndCanvas;
    args.msg = msg;
    args.timeoutMs = 2000;
    ShowNotification(args);
}

// remembers cleaned text on the tab and starts speaking it in TTS-sized chunks
static void ReadAloudStartText(WindowTab* tab, Str cleaned, ReadAloudHighlightMap* newMap, int highlightBase,
                               Str errMsg) {
    if (len(cleaned) == 0) {
        logf("ReadAloud: StartText: empty cleaned text\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    int cleanedLen = cleaned.len;
    int mapLen = newMap ? newMap->len : -1;
    logf("ReadAloud: StartText: cleanedLen=%d mapLen=%d highlightBase=%d\n", cleanedLen, mapLen, highlightBase);

    if (newMap) {
        if (!tab->readAloudHighlight) {
            tab->readAloudHighlight = new ReadAloudHighlightMap{};
        }
        ReadAloudHighlightFree(tab->readAloudHighlight);
        if (newMap->len > 0 && newMap->locs) {
            *tab->readAloudHighlight = *newMap;
            newMap->locs = nullptr;
            newMap->len = 0;
            newMap->cap = 0;
        } else {
            logf("ReadAloud: StartText: highlight map empty (len=%d locs=%p)\n", newMap->len, newMap->locs);
        }
    } else if (highlightBase == 0 && tab->readAloudHighlight) {
        ReadAloudHighlightFree(tab->readAloudHighlight);
        delete tab->readAloudHighlight;
        tab->readAloudHighlight = nullptr;
    }

    str::ReplaceWithCopy(&tab->readAloudText, cleaned);
    tab->readAloudHighlightBase = highlightBase;
    tab->readAloudChunkStart = 0;
    tab->readAloudChunkEnd = 0;
    tab->readAloudResumePos = -1;
    tab->readAloudAutoScroll = true;
    gReadAloudSessionTab = tab;
    ReadAloudSetSourceTab(tab);
    ReadAloudHighlightTimerStart(tab->win);

    if (!ReadAloudSpeakChunk(tab, errMsg)) {
        ReadAloudFinishSession(tab, tab->win);
        return;
    }
    ReadAloudPlaybackBarUpdateSession(tab);
    logf("ReadAloud: StartText: started speaking\n");
}

static void ReadAloudStartFromViewportTop(WindowTab* tab, Str errMsg) {
    logf("ReadAloud: StartFromViewportTop\n");
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        logf("ReadAloud: StartFromViewportTop: not a fixed-layout document\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    int startPage = 0;
    int startGlyph = 0;
    if (!ReadAloudGetViewportStart(dm, &startPage, &startGlyph)) {
        logf("ReadAloud: StartFromViewportTop: GetViewportStart failed\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    str::Builder cleaned;
    ReadAloudHighlightMap map{};
    if (!ReadAloudHighlightBuildFromDocument(dm, startPage, startGlyph, &map, cleaned)) {
        logf("ReadAloud: StartFromViewportTop: BuildFromDocument failed (page=%d glyph=%d)\n", startPage, startGlyph);
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    ReadAloudStartText(tab, ToStr(cleaned), &map, 0, errMsg);
}

static void ReadAloudStartFromSelection(WindowTab* tab, Str errMsg) {
    DisplayModel* dm = tab->AsFixed();
    if (!dm || dm->textSelection->result.len <= 0) {
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    str::Builder cleaned;
    ReadAloudHighlightMap map{};
    if (!ReadAloudHighlightBuildFromTextSelection(dm->textSelection, &map, cleaned)) {
        bool isTextOnlySelection = false;
        TempStr text = GetSelectedTextTemp(tab, "\r\n", isTextOnlySelection);
        TempStr cleanedStr = CleanReadAloudTextTemp(text);
        ReadAloudStartText(tab, cleanedStr, nullptr, 0, errMsg);
        return;
    }

    ReadAloudStartText(tab, ToStr(cleaned), &map, 0, errMsg);
}

static void ReadAloudInTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        logf("ReadAloud: InTab: null tab or window\n");
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("ReadAloud: InTab: CopySelection permission denied\n");
        return;
    }

    bool isTextOnlySelection = false;
    TempStr text = GetSelectedTextTemp(tab, "\r\n", isTextOnlySelection);

    if (len(text) > 0 && isTextOnlySelection) {
        logf("ReadAloud: InTab: using selection path (len=%d)\n", len(text));
        tab->readAloudScope = WindowTab::ReadAloudScopeSmart;
        ReadAloudStartFromSelection(tab, _TRA("No text available to read aloud"));
    } else {
        logf("ReadAloud: InTab: using viewport-top path (hasSelection=%d isTextOnly=%d)\n", len(text) > 0,
             isTextOnlySelection);
        tab->readAloudScope = WindowTab::ReadAloudScopeSmart;
        ReadAloudStartFromViewportTop(tab, _TRA("No text available to read aloud"));
    }
}

static void ReadAloudStartFromCursor(WindowTab* tab, Point screenPt, Str errMsg) {
    logf("ReadAloud: StartFromCursor\n");
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        logf("ReadAloud: StartFromCursor: not a fixed-layout document\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    int startPage = 0;
    int startGlyph = 0;
    if (!ReadAloudGetCursorStart(dm, screenPt, &startPage, &startGlyph)) {
        logf("ReadAloud: StartFromCursor: GetCursorStart failed\n");
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    str::Builder cleaned;
    ReadAloudHighlightMap map{};
    if (!ReadAloudHighlightBuildFromDocument(dm, startPage, startGlyph, &map, cleaned)) {
        logf("ReadAloud: StartFromCursor: BuildFromDocument failed (page=%d glyph=%d)\n", startPage, startGlyph);
        ReadAloudShowNotif(tab, errMsg);
        return;
    }

    ReadAloudStartText(tab, ToStr(cleaned), &map, 0, errMsg);
}

static void ReadAloudFromCursorInTab(WindowTab* tab, Point screenPt) {
    if (!tab || !tab->win) {
        logf("ReadAloud: FromCursorInTab: null tab or window\n");
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("ReadAloud: FromCursorInTab: CopySelection permission denied\n");
        return;
    }

    tab->readAloudScope = WindowTab::ReadAloudScopeCursor;
    ReadAloudStartFromCursor(tab, screenPt, _TRA("No text available to read aloud"));
}

static void ReadAloudFromViewportTopInTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        logf("ReadAloud: FromViewportTopInTab: null tab or window\n");
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        logf("ReadAloud: FromViewportTopInTab: CopySelection permission denied\n");
        return;
    }

    tab->readAloudScope = WindowTab::ReadAloudScopeViewport;
    ReadAloudStartFromViewportTop(tab, _TRA("No text available to read aloud"));
}

static void ReadAloudSelectionInTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }

    if (!HasPermission(Perm::CopySelection)) {
        return;
    }

    tab->readAloudScope = WindowTab::ReadAloudScopeSelection;
    ReadAloudStartFromSelection(tab, _TRA("No text available to read aloud"));
}

// true if read aloud was paused and can be resumed in this tab
bool CanContinueReadAloud(WindowTab* tab) {
    if (!tab || len(tab->readAloudText) == 0) {
        return false;
    }
    int pos = tab->readAloudResumePos;
    int maxPos = tab->readAloudHighlightBase + tab->readAloudText.len;
    return pos > 0 && pos < maxPos;
}

static void ReadAloudContinueInTab(WindowTab* tab) {
    if (!CanContinueReadAloud(tab) || !tab->win) {
        return;
    }

    int resumeInText = tab->readAloudResumePos - tab->readAloudHighlightBase;
    tab->readAloudChunkEnd = resumeInText;
    tab->readAloudChunkStart = resumeInText;
    tab->readAloudResumePos = -1;
    tab->readAloudAutoScroll = true;
    ReadAloudSetSourceTab(tab);
    ReadAloudHighlightTimerStart(tab->win);

    if (!ReadAloudSpeakChunk(tab, _TRA("No text available to read aloud"))) {
        ReadAloudFinishSession(tab, tab->win);
        return;
    }
    ReadAloudPlaybackBarUpdateSession(tab);
}

WindowTab* GetReadAloudSourceTab() {
    return gReadAloudSourceTab;
}

// Voice selection menu
static TempStr TtsLangIdToLocaleNameTemp(Str lang) {
    if (len(lang) == 0) {
        return str::DupTemp("unknown");
    }

    // Windows.Media.SpeechSynthesis voices report a locale name like "en-US",
    // SAPI voices a hex language id like "409"
    if (str::ContainsChar(lang, '-')) {
        return str::DupTemp(lang);
    }

    char* langZ = CStrTemp(lang);
    char* end = nullptr;
    unsigned long langId = strtoul(langZ, &end, 16);
    if (end == langZ || langId == 0) {
        return str::DupTemp(lang);
    }

    WCHAR localeName[LOCALE_NAME_MAX_LENGTH] = {};
    int n = LCIDToLocaleName((LCID)langId, localeName, dimof(localeName), 0);
    if (n <= 0) {
        return str::DupTemp(lang);
    }

    return ToUtf8Temp(localeName);
}

static void BuildReadAloudVoiceMenuItems(HMENU voiceMenu) {
    if (!voiceMenu) {
        return;
    }

    Str currentVoiceId = TtsGetVoiceId();

    UINT defaultFlags = MF_STRING;
    if (len(currentVoiceId) == 0) {
        defaultFlags |= MF_CHECKED;
    }

    AppendMenuW(voiceMenu, defaultFlags, CmdTtsVoiceDefault, L"System default");
    AppendMenuW(voiceMenu, MF_SEPARATOR, 0, nullptr);

    Vec<TtsVoiceInfo> voices = TtsGetVoices();

    Str lastLang = {};

    UINT cmd = CmdTtsVoiceFirst;
    for (TtsVoiceInfo& voice : voices) {
        if (cmd > CmdTtsVoiceLast) {
            break;
        }

        Str lang = len(voice.lang) == 0 ? StrL("") : voice.lang;

        if (lastLang && !str::EqI(lastLang, lang)) {
            AppendMenuW(voiceMenu, MF_SEPARATOR, 0, nullptr);
        }

        UINT flags = MF_STRING;
        if (str::Eq(voice.id, currentVoiceId)) {
            flags |= MF_CHECKED;
        }

        TempStr localeName = TtsLangIdToLocaleNameTemp(voice.lang);
        TempStr label = fmt("%s - %s", voice.name, localeName);
        AppendMenuW(voiceMenu, flags, cmd, CWStrTemp(label));

        lastLang = lang;
        cmd++;
    }

    TtsFreeVoices(voices);
    RemoveBadMenuSeparators(voiceMenu);
}

static void BuildReadAloudMenuItems(HMENU menu, MainWindow* win, bool includeCursorItem, bool canReadFromCursor) {
    WindowTab* currTab = win ? win->CurrentTab() : nullptr;
    bool isSpeaking = TtsIsSpeaking();
    bool canContinue = CanContinueReadAloud(currTab);
    bool hasSelection = currTab && win->showSelection && currTab->selectionOnPage && len(*currTab->selectionOnPage) > 0;

    if (isSpeaking) {
        AppendMenuW(menu, MF_STRING, CmdTtsMenuPauseReading, CWStrTemp(_TRA("Pause Reading")));
        AppendMenuW(menu, MF_STRING, CmdTtsMenuStopReading, CWStrTemp(_TRA("Stop Reading")));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    } else if (canContinue) {
        AppendMenuW(menu, MF_STRING, CmdTtsMenuContinueReading, CWStrTemp(_TRA("Continue Reading")));
        AppendMenuW(menu, MF_STRING, CmdTtsMenuStopReading, CWStrTemp(_TRA("Stop Reading")));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(menu, MF_STRING, CmdTtsMenuReadCurrentPage, CWStrTemp(_TRA("Start Reading From Top")));
    if (includeCursorItem) {
        AppendMenuW(menu, canReadFromCursor ? MF_STRING : MF_STRING | MF_GRAYED, CmdTtsMenuReadFromCursor,
                    CWStrTemp(_TRA("Start Reading From Cursor Position")));
    }
    AppendMenuW(menu, hasSelection ? MF_STRING : MF_STRING | MF_GRAYED, CmdTtsMenuReadSelection,
                CWStrTemp(_TRA("Start Reading Selection")));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    HMENU voiceMenu = CreatePopupMenu();
    if (voiceMenu) {
        BuildReadAloudVoiceMenuItems(voiceMenu);
        AppendMenuW(menu, MF_POPUP | MF_STRING, (UINT_PTR)voiceMenu, CWStrTemp(_TRA("Voice")));
    }

    HMENU speedMenu = CreatePopupMenu();
    if (speedMenu) {
        int currIdx = ReadAloudClosestSpeedIdx();
        for (int i = 0; i < dimofi(kReadAloudSpeeds); i++) {
            UINT flags = MF_STRING;
            if (i == currIdx) {
                flags |= MF_CHECKED;
            }
            TempStr label = ReadAloudSpeedLabelTemp(kReadAloudSpeeds[i]);
            AppendMenuW(speedMenu, flags, CmdTtsSpeedFirst + (UINT)i, CWStrTemp(label));
        }
        AppendMenuW(menu, MF_POPUP | MF_STRING, (UINT_PTR)speedMenu, CWStrTemp(_TRA("Speed")));
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    UINT showFlags = MF_STRING;
    if (gGlobalPrefs->toolbarShowReadAloud) {
        showFlags |= MF_CHECKED;
    }
    AppendMenuW(menu, showFlags, CmdToggleToolbarShowReadAloud, CWStrTemp(_TRA("Show In Toolbar")));
}

void RebuildReadAloudMenu(MainWindow* win, HMENU menu, bool includeCursorItem, bool canReadFromCursor) {
    if (!menu || !win) {
        return;
    }
    MenuEmpty(menu);
    BuildReadAloudMenuItems(menu, win, includeCursorItem, canReadFromCursor);
    RemoveBadMenuSeparators(menu);
}

static void HandleReadAloudMenuSelection(MainWindow* win, UINT selected) {
    if (!win || selected == 0) {
        return;
    }

    WindowTab* currTab = win->CurrentTab();

    if (selected == CmdTtsMenuPauseReading) {
        ReadAloudStopRememberPos();
        ToolbarUpdateStateForWindow(win, true);
    } else if (selected == CmdTtsMenuStopReading) {
        ReadAloudPlaybackStop();
    } else if (selected == CmdTtsMenuReadCurrentPage) {
        if (currTab) {
            if (TtsIsSpeaking()) {
                TtsStop();
            }
            ReadAloudFromViewportTopInTab(currTab);
        }
    } else if (selected == CmdTtsMenuReadFromCursor) {
        if (currTab && win->contextMenuPtValid) {
            if (TtsIsSpeaking()) {
                TtsStop();
            }
            ReadAloudFromCursorInTab(currTab, win->contextMenuPt);
        }
    } else if (selected == CmdTtsMenuContinueReading) {
        if (TtsIsSpeaking()) {
            TtsStop();
        }
        ReadAloudContinueInTab(currTab);
    } else if (selected == CmdTtsMenuReadSelection) {
        if (TtsIsSpeaking()) {
            TtsStop();
        }
        ReadAloudSelectionInTab(currTab);
    } else if (selected == CmdTtsVoiceDefault) {
        if (TtsSetVoiceById("")) {
            ReadAloudSaveVoicePref("");
        }
    } else if (selected >= CmdTtsVoiceFirst && selected <= CmdTtsVoiceLast) {
        Vec<TtsVoiceInfo> voices = TtsGetVoices();
        int voiceIndex = (int)(selected - CmdTtsVoiceFirst);
        if (voiceIndex >= 0 && voiceIndex < len(voices)) {
            if (TtsSetVoiceById(voices[voiceIndex].id)) {
                ReadAloudSaveVoicePref(voices[voiceIndex].id);
            }
        }
        TtsFreeVoices(voices);
    } else if (selected >= CmdTtsSpeedFirst && selected <= CmdTtsSpeedLast) {
        int speedIndex = (int)(selected - CmdTtsSpeedFirst);
        if (speedIndex >= 0 && speedIndex < dimofi(kReadAloudSpeeds)) {
            ReadAloudSetSpeed(kReadAloudSpeeds[speedIndex]);
        }
    }
}

bool HandleReadAloudMenuCommand(MainWindow* win, int cmdId) {
    if (cmdId == CmdTtsVoiceDefault || (cmdId >= CmdTtsMenuReadCurrentPage && cmdId <= CmdTtsMenuStopReading) ||
        (cmdId >= CmdTtsVoiceFirst && cmdId <= CmdTtsVoiceLast) ||
        (cmdId >= CmdTtsSpeedFirst && cmdId <= CmdTtsSpeedLast)) {
        HandleReadAloudMenuSelection(win, (UINT)cmdId);
        return true;
    }
    return false;
}

// the menu shown by the dropdown arrow on the Read Aloud toolbar button
void ShowTtsVoiceMenu(MainWindow* win, Rect buttonScreen) {
    if (!win || buttonScreen.IsEmpty()) {
        return;
    }

    RECT rc = ToRECT(buttonScreen);

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    BuildReadAloudMenuItems(menu, win, false, false);

    UINT selected = (UINT)TrackPopupMenu(menu, TPM_RETURNCMD, rc.left, rc.bottom, 0, win->hwndFrame, nullptr);
    // the click that dismissed the menu is delivered to the toolbar afterwards;
    // this is what makes the toolbar ignore it instead of re-opening the menu
    ToolbarNoteDropdownClosed();

    DestroyMenu(menu);
    if (selected == 0) {
        return;
    }
    // the menu also carries real Cmd* ids (Show In Toolbar), which
    // HandleReadAloudMenuSelection knows nothing about - let the frame have them
    if (HandleReadAloudMenuCommand(win, (int)selected)) {
        return;
    }
    HwndSendCommand(win->hwndFrame, (int)selected);
}

// Drop tabs-in-titlebar / menu rebar chrome after an external host reparents
// our frame as WS_CHILD (e.g. Total Commander lister). Must not run while
// nested under RelayoutCaption/EndDeferWindowPos paint: destroying the menu
// rebar mid-paint crashes in comctl32!DrawScrollBar (null +0x10).
// Posted via uitask::Post so it runs after the current message finishes.
static void ApplyEmbeddedWindowChrome(MainWindow* win) {
    if (!IsMainWindowValid(win) || !win->hwndFrame || !::IsWindow(win->hwndFrame)) {
        return;
    }
    logf("ApplyEmbeddedWindowChrome\n");
    SetTabsInTitlebar(win, false);
    DestroyMenuBarRebar(win);
    if (::IsWindow(win->hwndFrame)) {
        SetMenu(win->hwndFrame, nullptr);
    }
    UpdateTabWidth(win);
    ScheduleUiUpdate(win, kUiForceRelayout | kUiToolbarDirty | kUiTabsDirty);
}

LRESULT CALLBACK WndProcSumatraFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DpiSetFromHwnd(hwnd);
    MainWindow* win = FindMainWindowByHwnd(hwnd);

    // DbgLogMsg("frame:", hwnd, msg, wp, lp);
    // detect when an external host (e.g. Total Commander's lister) embeds us
    // by reparenting our window as WS_CHILD. Only set the flag here and post
    // chrome teardown: this handler can re-enter under EndDeferWindowPos.
    bool isChildWindow = HwndIsWindowStyleSet(hwnd, WS_CHILD);
    if (win && !gMyWindowWasEmbedded && isChildWindow) {
        logf("Detected window embedded in another window\n");
        gMyWindowWasEmbedded = true;
        str::ReplaceWithCopy(&gGlobalPrefs->scrollbars, "windows");
        uitask::Post(MkFunc0(ApplyEmbeddedWindowChrome, win), "ApplyEmbeddedWindowChrome");
    }
    // custom caption is incompatible with WS_CHILD hosts; skip even before
    // deferred ApplyEmbeddedWindowChrome clears tabsInTitlebar
    if (win && win->tabsInTitlebar && !gMyWindowWasEmbedded) {
        bool callDefault = true;
        LRESULT res = CustomCaptionFrameProc(hwnd, msg, wp, lp, &callDefault, win);
        if (!callDefault) {
            return res;
        }
    }

    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    // the splitters between the panes are virtual controls: they get the mouse
    // (dragging one captures it) and set the resize cursor
    if (win && win->frameRoot) {
        LRESULT vres = 0;
        if (VirtTreeOnMessage(hwnd, win->frameRoot, msg, wp, lp, vres)) {
            return vres;
        }
        if (msg == WM_PAINT) {
            // BeginPaint erases with the class brush first; we only add the
            // splitters on top (the custom-caption path paints its own)
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            GfxHdc gfx(hdc);
            win->frameRoot->Paint(&gfx, ToRect(ps.rcPaint));
            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    switch (msg) {
        case WM_CTLCOLORSTATIC: {
            // The Bookmarks and Favorites panels are plain WC_STATIC containers,
            // so a static paints its background with the brush its parent
            // returns here - including the strips its children don't cover, like
            // the gap between the filter field and the tree. Without this the
            // default COLOR_BTNFACE brush puts a bright #f0f0f0 band across a
            // dark sidebar, which reads as a light divider (issue #5893)
            HWND hwndCtl = (HWND)lp;
            if (!win || (hwndCtl != win->hwndTocBox && hwndCtl != win->hwndFavBox)) {
                break;
            }
            if (!win->brControlBgColor) {
                win->brControlBgColor = CreateSolidBrush(ThemeControlBackgroundColor());
            }
            SetTextColor((HDC)wp, ThemeWindowTextColor());
            SetBkMode((HDC)wp, TRANSPARENT);
            return (LRESULT)win->brControlBgColor;
        }

        case WM_CREATE:
            // do nothing
            TtsSetNotifyWindow(hwnd, WM_TTS_EVENT, 0, 0);
            goto InitMouseWheelInfo;

        case WM_SIZE:
            if (win && SIZE_MINIMIZED == wp) {
                // Track-mode canvas tips (home file path, page links) are
                // topmost popups and must be dismissed on minimize or they
                // stick on the desktop (issue #5928).
                win->DeleteToolTip();
                break;
            }
            if (win) {
                RememberDefaultWindowPosition(win);
                // UIState.layout.rc remembers the last laid-out client size;
                // the scheduled update relayouts only when the size actually
                // changed, and a burst of WM_SIZE does the work once
                ScheduleUiUpdate(win);
            }
            break;

        case WM_GETMINMAXINFO:
            return OnFrameGetMinMaxInfo((MINMAXINFO*)lp);

        case WM_ENTERSIZEMOVE:
            if (win) {
                win->deferDpiChromeRefresh = true;
                win->dpiChromeRefreshPending = false;
            }
            return 0;

        case WM_EXITSIZEMOVE:
            if (win) {
                if (win->dpiChromeRefreshPending) {
                    uitask::Post(MkFunc0(FinishDeferredMainWindowDpiRefresh, win), "DpiSettled");
                } else {
                    win->deferDpiChromeRefresh = false;
                }
            }
            return 0;

        case WM_DPICHANGED:
            DpiSet((int)LOWORD(wp), (int)HIWORD(wp));
            if (win) {
                // Trust wParam DPI during cross-monitor drag (GetDpiForWindow can lag).
                OnDpiChanged(win, (RECT*)lp, (int)LOWORD(wp));
                return 0;
            }
            break;

        case WM_MOVE:
            if (win) {
                RememberDefaultWindowPosition(win);
                UpdateOverlayScrollbarPositions(win);
                // keep the floating find bar anchored over the search icon
                FindBarReposition(win);
            }
            break;

        case WM_INITMENUPOPUP:
            // apply dark mode to popup menu window
            DarkModeApplyToMenuWindow(FindWindow(UNDOCUMENTED_MENU_CLASS_NAME, nullptr));
            // TODO: should I just build the menu from scratch every time?
            if (win) {
                UpdateAppMenu(win, (HMENU)wp);
            }
            break;

        case WM_HOTKEY:
            if (wp == kScreenshotHotkeyId) {
                TakeScreenshots();
                return 0;
            }
            break;

        case WM_COMMAND:
            return FrameOnCommand(win, hwnd, msg, wp, lp);

        case WM_MEASUREITEM:
            if (ThemeColorizeControls()) {
                MenuCustomDrawMesureItem(hwnd, (MEASUREITEMSTRUCT*)lp);
                return TRUE;
            }
            break;

        case WM_DRAWITEM:
            if (ThemeColorizeControls()) {
                MenuCustomDrawItem(hwnd, (DRAWITEMSTRUCT*)lp);
                return TRUE;
            }
            break;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            // document keyboard focus is on the frame; repaint canvas focus ring (#4644)
            if (win) {
                InvalidateCanvasKeyboardFocus(win);
            }
            break;

        case WM_ACTIVATE:
            if (wp != WA_INACTIVE) {
                gLastActiveFrameHwnd = hwnd;
                // restore home-page keyboard-selection tooltip if applicable
                if (win) {
                    HomePageOnWindowActivate(win, true);
                }
            } else if (win) {
                // hide the topmost citation-hover popup when switching to
                // another application (no WM_MOUSELEAVE is generated then)
                RefHoverHide(win->refHover, win->hwndCanvas);
                // home-page thumbnail tip is topmost track-mode; hide on deactivate
                HomePageOnWindowActivate(win, false);
            }
            break;

        case WM_APPCOMMAND:
            // both keyboard and mouse drivers should produce WM_APPCOMMAND
            // messages for their special keys, so handle these here and return
            // TRUE so as to not make them bubble up further
            switch (GET_APPCOMMAND_LPARAM(lp)) {
                case APPCOMMAND_BROWSER_BACKWARD:
                    HwndSendCommand(hwnd, CmdNavigateBack);
                    return TRUE;
                case APPCOMMAND_BROWSER_FORWARD:
                    HwndSendCommand(hwnd, CmdNavigateForward);
                    return TRUE;
                case APPCOMMAND_BROWSER_REFRESH:
                    HwndSendCommand(hwnd, CmdReloadDocument);
                    return TRUE;
                case APPCOMMAND_BROWSER_SEARCH:
                    HwndSendCommand(hwnd, CmdFindFirst);
                    return TRUE;
                case APPCOMMAND_BROWSER_FAVORITES:
                    HwndSendCommand(hwnd, CmdToggleBookmarks);
                    return TRUE;
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_CHAR:
            if (win && !win->isBeingClosed) {
                FrameOnChar(win, wp, lp);
            }
            break;

        case WM_KEYDOWN:
            if (win && !win->isBeingClosed) {
                FrameOnKeydown(win, wp, lp);
            }
            break;

        case WM_SYSKEYUP:
            // pressing and releasing the Alt key focuses the menu even if
            // the wheel has been used for scrolling horizontally, so we
            // have to suppress that effect explicitly in this situation
            if (VK_MENU == wp && gSupressNextAltMenuTrigger) {
                gSupressNextAltMenuTrigger = false;
                return 0;
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_SYSCHAR:
            if (win && FrameOnSysChar(win, wp)) {
                return 0;
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_MENUSELECT:
            if (win) {
                UpdateCustomMenuBarMenuSelect(win, wp, lp);
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_SYSCOMMAND:
            // temporarily show the menu bar if it has been hidden
            if (wp == SC_KEYMENU && win && !IsMenubarVisible()) {
                ToggleMenuBar(win, true);
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_ENTERMENULOOP:
            gOverlayScrollbarSuppressThick = true;
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_EXITMENULOOP:
            gOverlayScrollbarSuppressThick = false;
            // hide the menu bar again if it was shown only temporarily
            if (!wp && win && !IsMenubarVisible()) {
                SetMenu(hwnd, nullptr);
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_CONTEXTMENU: {
            // opening the context menu with a keyboard doesn't call the canvas'
            // WM_CONTEXTMENU, as it never has the focus (mouse right-clicks are
            // handled as expected)
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            if (win && (x == -1) && (y == -1) && !HwndIsFocused(win->tocTreeView->hwnd)) {
                return SendMessageW(win->hwndCanvas, WM_CONTEXTMENU, wp, lp);
            }
            return DefWindowProc(hwnd, msg, wp, lp);
        }

        case WM_DISPLAYCHANGE:
            // Screen rotation / resolution change (tablets, display settings).
            // Keep fullscreen covering the monitor and fix the restore rect.
            if (win) {
                ResizeFullScreenToCurrentDisplay(win);
            }
            return 0;

        case WM_SETTINGCHANGE:
            // Windows switched between light and dark mode: re-resolve the
            // System theme (no-op unless Theme = System)
            if (lp && str::EqI(ToUtf8Temp((const WCHAR*)lp), StrL("ImmersiveColorSet"))) {
                UpdateThemeAfterSystemColorChange();
            }
            // high contrast can be toggled at any time (Alt+Shift+PrtScr);
            // SPI_SETHIGHCONTRAST arrives here (no-op unless it changed)
            UpdateThemeAfterHighContrastChange();
        InitMouseWheelInfo:
            UpdateDeltaPerLine();

            if (win) {
                // Work area can change without WM_DISPLAYCHANGE (taskbar move,
                // some tablet rotation paths). Resize fullscreen if needed.
                ResizeFullScreenToCurrentDisplay(win);
            }

            return 0;

        case WM_SYSCOLORCHANGE:
            if (gGlobalPrefs->useSysColors) {
                UpdateDocumentColors();
            }
            break;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            if (!win || !win->IsDocLoaded()) {
                break;
            }
            if (win->AsChm()) {
                return win->AsChm()->PassUIMsg(msg, wp, lp);
            }
            if (win->AsMarkdown()) {
                return win->AsMarkdown()->PassUIMsg(msg, wp, lp);
            }
            ReportIf(!win->AsFixed());
            // Pass the message to the canvas' window procedure
            // (required since the canvas itself never has the focus and thus
            // never receives WM_MOUSEWHEEL messages)
            return SendMessageW(win->hwndCanvas, msg, wp, lp);

        case WM_CLOSE: {
            if (!win) {
                logf("WM_CLOSE to 0x%p, but didn't find MainWindow for it\n", hwnd);
            }
            if (CanCloseWindow(win)) {
                CloseWindow(win, true, false);
            }
            return 0;
        }

        case WM_DESTROY: {
            // WM_DESTROY is generated by windows when close button is pressed
            // or if we explicitly call DestroyWindow().
            // It might be sent as a result of File\Close, in which
            // case CloseWindow() has already been called.
            // It's also sent when a parent window (e.g. Total Commander's lister)
            // destroys our embedded window.
            UnregisterScreenshotHotkey(hwnd);
            FreeMenuOwnerDrawInfoData(GetMenu(hwnd));
            if (win) {
                CloseWindow(win, true, true);
            }
        } break;

        case WM_ENDSESSION:
            // TODO: check for unfinished print jobs in WM_QUERYENDSESSION?
            SaveSettings();
            gDontSaveSettings = true;
            if (wp == TRUE) {
                CloseWindow(win, true, true);
            }
            return 0;

        case WM_DDE_INITIATE:
            if (gPluginMode) {
                break;
            }
            return OnDDEInitiate(hwnd, wp, lp);
        case WM_DDE_EXECUTE:
            return OnDDExecute(hwnd, wp, lp);
        case WM_DDE_REQUEST:
            return OnDDERequest(hwnd, wp, lp);
        case WM_DDE_TERMINATE:
            return OnDDETerminate(hwnd, wp, lp);

        case WM_COPYDATA:
            return OnCopyData(hwnd, wp, lp);

        case WM_TIMER:
            if (win && win->stressTest) {
                OnStressTestTimer(win, (int)wp);
            }
            break;

        case WM_MOUSEACTIVATE:
            if (win && win->presentation && hwnd != GetForegroundWindow()) {
                return MA_ACTIVATEANDEAT;
            }
            return MA_ACTIVATE;

        case WM_TTS_EVENT:
            TtsProcessEvents();

            if (TtsIsSpeaking() && gReadAloudSourceTab && gReadAloudSourceTab->win) {
                HwndInvalidate(gReadAloudSourceTab->win->hwndCanvas);
                ReadAloudPlaybackBarUpdateSession(gReadAloudSourceTab);
            }

            // also gets here for word boundary events while still speaking;
            // only the end of speech needs handling
            if (!TtsIsSpeaking() && gReadAloudSourceTab) {
                WindowTab* raTab = gReadAloudSourceTab;
                if (ReadAloudHasMoreChunks(raTab)) {
                    if (!ReadAloudSpeakChunk(raTab, _TRA("No text available to read aloud"))) {
                        ReadAloudFinishSession(raTab, win);
                    }
                } else {
                    ReadAloudFinishSession(raTab, win);
                }
            }

            return 0;
        case WM_ERASEBKGND:
            // not sure why it's needed but it causes
            // flash of caption area in choco theme when resizing sidebar
#if 0
            LogRedraw("WM_ERASEBKGND", hwnd);
            if (win && win->tabsInTitlebar && !IsCurrentThemeDefault()) {
                HDC hdc = (HDC)wp;
                HBRUSH br = CreateSolidBrush(ThemeMainWindowBackgroundColor());
                HdcFillRect(hdc, HwndClientRect(hwnd), br);
                DeleteObject(br);
                if (!win->captionRect.IsEmpty()) {
                    RECT rcCaption = ToRECT(win->captionRect);
                    HBRUSH brCaption = CreateSolidBrush(ThemeControlBackgroundColor());
                    HdcFillRect(hdc, ToRect(rcCaption), brCaption);
                    DeleteObject(brCaption);
                }
            }
#endif
            return TRUE;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

static TempStr GetFileSizeAsStrTemp(Str path) {
    i64 fileSize = file::GetSize(path);
    return str::FormatFileSizeTemp(fileSize);
}

void GetProgramInfo(str::Builder& s) {
    s.Append(fmt("Crash file: %s\r\n", gCrashFilePath));

    TempStr exePath = GetSelfExePathTemp();
    auto fileSizeExe = GetFileSizeAsStrTemp(exePath);
    s.Append(fmt("Exe: %s %s\r\n", exePath, fileSizeExe));
    if (IsDllBuild()) {
        // show the size of the dll so that we can verify it's the
        // correct size for the given version
        TempStr dir = path::GetDirTemp(exePath);
        TempStr dllPath = path::JoinTemp(dir, StrL("libsumatrapdf.dll"));
        auto fileSizeDll = GetFileSizeAsStrTemp(dllPath);
        s.Append(fmt("Dll: %s %s\r\n", dllPath, fileSizeDll));
    }
    TempStr signer = GetExecutableSignerTemp(exePath);
    s.Append(fmt("Signer: %s\r\n", signer ? signer : StrL("(not signed)")));
    if (builtOn) {
        s.Append(fmt("BuiltOn: %s\n", builtOn));
    }
    Str exeType = IsDllBuild() ? "dll" : "static";
    Str instType = IsRunningInPortableMode() ? "portable" : "installed";
    s.Append(fmt("ExeType: %s, %s\r\n", exeType, instType));
    s.Append(fmt("Ver: %s", currentVersion));
    if (gIsPreReleaseBuild) {
        s.Append(fmt(" pre-release"));
    }
    if (IsProcess64()) {
        s.Append(" 64-bit");
    } else {
        s.Append(" 32-bit");
        if (IsRunningInWow64()) {
            s.Append(" Wow64");
        }
    }
    if (gIsDebugBuild) {
        if (!str::Contains(ToStr(s), StrL(" (dbg)"))) {
            s.Append(" (dbg)");
        }
    }
    if (gPluginMode) {
        s.Append(" [plugin]");
    }
    s.Append("\r\n");

    if (gitCommidId) {
        s.Append(
            fmt("Git: %s (https://github.com/sumatrapdfreader/sumatrapdf/commit/%s)\r\n", gitCommidId, gitCommidId));
    }
}

bool CrashHandlerCanUseNet() {
    return HasPermission(Perm::InternetAccess);
}

void ShowCrashHandlerMessage() {
    log("ShowCrashHandlerMessage\n");
    // don't show a message box in restricted use, as the user most likely won't be
    // able to do anything about it anyway and it's up to the application provider
    // to fix the unexpected behavior (of which for a restricted set of documents
    // there should be much less, anyway)
    if (!CanAccessDisk()) {
        log("ShowCrashHandlerMessage: skipping because !CanAccessDisk()\n");
        return;
    }

#if 0
    int res = MsgBox(nullptr, _TRA("Sorry, that shouldn't have happened!\n\nPlease press 'Cancel', if you want to help us fix the cause of this crash."), _TRA("SumatraPDF crashed"), MB_ICONERROR | MB_OKCANCEL | MbRtlReadingMaybe());
    if (IDCANCEL == res) {
        LaunchBrowser(CRASH_REPORT_URL);
    }
#endif

    Str msg = _TRA("We're sorry, SumatraPDF crashed.\n\nPress 'Cancel' to see crash report.");
    uint flags = MB_ICONERROR | MB_OK | MB_OKCANCEL | MbRtlReadingMaybe();
    flags |= MB_SETFOREGROUND | MB_TOPMOST;

    int res = MsgBox(nullptr, msg, _TRA("SumatraPDF crashed"), flags);
    if (IDCANCEL != res) {
        log("ShowCrashHandlerMessage: res != IDCANCEL\n");
        return;
    }
    if (!gCrashFilePath) {
        log("ShowCrashHandlerMessage: !gCrashFilePath\n");
        return;
    }
    LaunchFileIfExists(gCrashFilePath);
    const auto* url = "https://www.sumatrapdfreader.org/docs/Submit-crash-report.html";
    LaunchFileShell(url, nullptr, "open");
}

TempStr PageInfoOverlayResultTemp(Str pathTwoPages, Str pathOnePage, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (len(pathTwoPages) == 0 || len(pathOnePage) == 0) {
        return fail("ERROR missing-paths");
    }
    if (len(gWindows) == 0) {
        return fail("NOTREADY no-window");
    }
    MainWindow* win = gWindows[0];
    if (!win) {
        return fail("NOTREADY no-window");
    }

    LoadArgs args2(pathTwoPages, win);
    args2.forceReuse = true;
    args2.noSavePrefs = true;
    LoadDocument(&args2);
    if (!win->IsDocLoaded() || win->ctrl->PageCount() != 2) {
        return fail("ERROR two-page-load");
    }

    TogglePageInfoHelper(win);
    NotificationWnd* wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (!wnd) {
        return fail("ERROR no-overlay");
    }
    TempStr msg = NotificationGetMessageTemp(wnd);
    if (!str::Contains(msg, StrL("/ 2"))) {
        out.Append(fmt("FAIL before-reload msg=%s\n", msg));
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }

    EngineBase* engine = CreateEngineFromFile(pathOnePage, nullptr, true);
    if (!engine || engine->PageCount() != 1) {
        SafeEngineRelease(&engine);
        return fail("ERROR one-page-engine");
    }
    DocController* ctrl = CreateControllerForEngineOrFile(engine, pathOnePage, nullptr, win);
    if (!ctrl) {
        return fail("ERROR one-page-ctrl");
    }
    LoadArgs args1(pathOnePage, win);
    args1.noSavePrefs = true;
    ReplaceDocumentInCurrentTab(&args1, ctrl, nullptr);
    if (!win->IsDocLoaded() || win->ctrl->PageCount() != 1) {
        return fail("ERROR one-page-load");
    }
    wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (!wnd) {
        return fail("ERROR overlay-gone");
    }
    msg = NotificationGetMessageTemp(wnd);
    bool ok = str::Contains(msg, StrL("/ 1")) && !str::Contains(msg, StrL("/ 2"));
    if (ok) {
        out.Append(fmt("OK msg=%s\n", msg));
    } else {
        out.Append(fmt("FAIL after-reload msg=%s\n", msg));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

// Verifies maximized WindowState is not downgraded while a document is still
// loading, and that a plain empty/home window still may record NORMAL.
// Used by tests/issue-5529.ts.
TempStr WindowStateDuringLoadResultTemp(int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code = 1) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !HwndIsVisible(win->hwndFrame)) {
        return fail(StrL("NOTREADY window-not-visible"), 2);
    }
    if (win->IsDocLoaded()) {
        return fail(StrL("NOTREADY doc-already-loaded"), 2);
    }

    int prevState = gGlobalPrefs->windowState;
    // Force a non-maximized frame so the bug path is exercised: prefs say
    // maximized, but the visible window is still at restored size (as during
    // slow load). Without the #5529 guard this rewrites WindowState to NORMAL.
    if (IsZoomed(win->hwndFrame)) {
        ShowWindow(win->hwndFrame, SW_RESTORE);
    }

    // --- mid-load: keep maximized WindowState ---
    // Empty -for-testing launch has no tabs until a document is opened, so the
    // old harness (set Loading on CurrentTab) was a no-op and always "failed"
    // after the guard required WindowHasDocumentLoading. Create a temporary
    // loading document tab when needed.
    gGlobalPrefs->windowState = WIN_STATE_MAXIMIZED;
    WindowTab* tab = win->CurrentTab();
    if (!tab && win->TabCount() > 0) {
        tab = win->GetTab(0);
    }
    bool createdTempTab = false;
    WindowTab::LoadState prevLoad = WindowTab::LoadState::None;
    if (!tab) {
        tab = new WindowTab(win);
        tab->SetFilePath(StrL("C:\\__sumatra_issue_5529_loading__.pdf"));
        tab->loadState = WindowTab::LoadState::Loading;
        AddTabToWindow(win, tab);
        createdTempTab = true;
    } else {
        prevLoad = tab->loadState;
        tab->loadState = WindowTab::LoadState::Loading;
    }
    RememberDefaultWindowPosition(win);
    int observedLoading = gGlobalPrefs->windowState;
    if (createdTempTab) {
        RemoveTab(tab);
        delete tab;
        tab = nullptr;
    } else if (tab) {
        tab->loadState = prevLoad;
    }

    // --- empty home (no loading tab): may record NORMAL ---
    gGlobalPrefs->windowState = WIN_STATE_MAXIMIZED;
    RememberDefaultWindowPosition(win);
    int observedEmpty = gGlobalPrefs->windowState;

    gGlobalPrefs->windowState = prevState;

    bool okLoading = observedLoading == WIN_STATE_MAXIMIZED;
    // Empty non-maximized frame should be allowed to write NORMAL (home-window
    // fix after #5529). If the frame is still maximized for some reason, skip.
    bool okEmpty = IsZoomed(win->hwndFrame) || observedEmpty == WIN_STATE_NORMAL;
    if (okLoading && okEmpty) {
        out.Append(fmt("OK loading preserved maximized; empty records normal\n"));
        if (exitCodeOut) {
            *exitCodeOut = 0;
        }
        return ToStrTemp(out);
    }
    if (!okLoading) {
        out.Append(fmt("FAIL loading windowState=%d expected=%d\n", observedLoading, WIN_STATE_MAXIMIZED));
    }
    if (!okEmpty) {
        out.Append(fmt("FAIL empty windowState=%d expected=%d\n", observedEmpty, WIN_STATE_NORMAL));
    }
    if (exitCodeOut) {
        *exitCodeOut = 1;
    }
    return ToStrTemp(out);
}

void ShutdownCleanup() {
    TtsRelease();
    FreeHomePageTips();
    DestroySvgPixmapIconsCache();
    DisconnectLastDragDataObject();

    gAllowedFileTypes.Reset();
    gAllowedLinkProtocols.Reset();
}
