/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/WinDynCalls.h"
#include "base/DirIter.h"
#include "base/Dpi.h"
#include "base/File.h"
#include "base/FileWatcher.h"
#include "base/GuessFileType.h"
#include "base/SquareTreeParser.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/GdiPlus.h"
#include "base/Archive.h"
#include "base/Timer.h"
#include "base/LzmaSimpleArchive.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"

#include "wingui/LabelWithCloseWnd.h"
#include "wingui/FrameRateWnd.h"

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
#include "SelectionToolbar.h"
#include "Screenshot.h"
#include "ImageSaveCropResize.h"
#include "StressTesting.h"
#include "HomePage.h"
#include "SumatraDialogs.h"
#include "SumatraProperties.h"
#include "TabGroupsManage.h"
#include "TableOfContents.h"
#include "Tabs.h"
#include "Toolbar.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "Translations.h"
#include "uia/Provider.h"
#include "SumatraConfig.h"
#include "EditAnnotations.h"
#include "AIChatCommon.h"
#include "ClaudeCode.h"
#include "GrokBuild.h"
#include "SelectionTranslate.h"
#include "CodexBuild.h"
#include "CommandPalette.h"
#include "AdvancedSettingsDialog.h"
#include "ChangeThemeDialog.h"
#include "NavFilesInFolder.h"
#include "Installer.h"
#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"
#include "Theme.h"
#include "DarkModeSubclass.h"
#include "TextToSpeech.h"
#include "ReadAloudHighlight.h"
#include "ReadAloudPlaybackBar.h"
#include "SumatraLog.h"

using Gdiplus::Color;
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
bool gShowFrameRate = false;

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

// message for deferred, coalesced UI updates (see ScheduleUiUpdate)
constexpr UINT WM_UPDATE_UI = WM_APP + 0x400;

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

static void CloseDocumentInCurrentTab(MainWindow*, bool keepUIEnabled, bool deleteModel);
static void SetFrameTitleForTab(WindowTab* tab, bool needRefresh);
static void OnSidebarSplitterMove(Splitter::MoveEvent*);
static void OnFavSplitterMove(Splitter::MoveEvent*);

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
    res->forceReuse = this->forceReuse;
    res->noSavePrefs = this->noSavePrefs;
    res->onFinished = this->onFinished;
    res->loadingNotifCorner = this->loadingNotifCorner;
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
        ReportIf(gWindows.empty());
        if (gWindows.empty()) {
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
    if (str::EndsWithI(path, ".htm") || str::EndsWithI(path, ".html") || str::EndsWithI(path, ".xhtml")) {
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

static WindowTab* FindTabByController(DocController* ctrl) {
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
    auto win = tab->win;
    if (tab == win->CurrentTab()) {
        return;
    }
    TabsSelect(win, win->GetTabIdx(tab));
}

// Find the first window showing a given PDF file
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

// Find the first window that has been produced from <file>
MainWindow* FindMainWindowBySyncFile(Str path, bool focusTab) {
    for (MainWindow* win : gWindows) {
        Vec<Rect> rects;
        int page;
        auto dm = win->AsFixed();
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

bool gShowPassword = false;

class HwndPasswordUI : public PasswordUI {
    HWND hwnd;
    int pwdIdx;
    bool triedCliPwd = false;

  public:
    explicit HwndPasswordUI(HWND hwnd) : hwnd(hwnd), pwdIdx(0) {}

    Str GetPassword(Str fileName, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) override;
};

/* Get password for a given 'fileName', can be nullptr if user cancelled the
   dialog box or if the encryption key has been filled in instead.
   Caller needs to free() the result. */
Str HwndPasswordUI::GetPassword(Str path, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) {
    FileState* fileFromHistory = gFileHistory.FindByName(path, nullptr);
    if (fileFromHistory && fileFromHistory->decryptionKey && fileDigest && decryptionKeyOut) {
        TempStr fingerprint = str::MemToHexTemp(Str((const char*)fileDigest, 16));
        *saveKey = str::StartsWith(fileFromHistory->decryptionKey, fingerprint);
        if (*saveKey && str::HexToMem(fileFromHistory->decryptionKey.s + 32, Str((char*)decryptionKeyOut, 32))) {
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
    return Dialog_GetPassword(hwnd, path, rememberPwd, &gShowPassword);
}

// update global windowState for next default launch when either
// no pdf is opened or a document without window dimension information
void RememberDefaultWindowPosition(MainWindow* win) {
    // ignore spurious WM_SIZE and WM_MOVE messages happening during initialization
    if (!IsWindowVisible(win->hwndFrame)) {
        return;
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
    gGlobalPrefs->sidebarDx = win->sidebarDx > 0 ? win->sidebarDx : WindowRect(win->hwndTocBox).dx;

    if (IsIconic(win->hwndFrame) || win->presentation) {
        return;
    }

    if (WIN_STATE_NORMAL == gGlobalPrefs->windowState) {
        gGlobalPrefs->windowPos = WindowRect(win->hwndFrame);
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
    FileState* fs = gFileHistory.FindByName(fp, nullptr);
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

    if (win->tocLabelWithClose) win->tocLabelWithClose->Layout();
    if (win->favLabelWithClose) win->favLabelWithClose->Layout();

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
        WCHAR* oldName = AllocArrayTemp<WCHAR>(oldMii.cch);
        WCHAR* newName = AllocArrayTemp<WCHAR>(newMii.cch);
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
        InvalidateRect(win->hwndFrame, &r, TRUE);
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
        gFileHistory.GetFrequencyOrder(list);
    } else {
        gFileHistory.GetRecentlyOpenedOrder(list);
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
    void RenderThumbnail(DisplayModel* dm, Size size, const OnBitmapRendered*) override;
    void GotoLink(IPageDestination* dest) override { win->linkHandler->GotoLink(dest); }
    void FocusFrame(bool always) override;
    void SaveDownload(Str url, Str) override;
    void FindResultReceived(int gen, int current, int total) override {
        BrowserFindResultReceived(win, gen, current, total);
    }
    void FindAllResultReceived(Str payload) override { BrowserFindAllResultReceived(win, payload); }
};

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
    auto engine = dm->GetEngine();
    RectF pageRect = engine->PageMediabox(1);
    if (pageRect.IsEmpty()) {
        // saveThumbnail must always be called for clean-up code
        saveThumbnail->Call(nullptr);
        return;
    }

    pageRect = engine->Transform(pageRect, 1, 1.0f, 0);
    float zoom = size.dx / (float)pageRect.dx;
    if (pageRect.dy > (float)size.dy / zoom) {
        pageRect.dy = (float)size.dy / zoom;
    }
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
    ~CreateThumbnailFromFileData() { str::Free(filePath); }
};

static void CreateThumbnailFromFileFinish(CreateThumbnailFromFileData* d) {
    if (d->bmp) {
        FileState* fs = gFileHistory.FindByPath(d->filePath);
        SetThumbnail(fs, RenderedBitmapFromPixmap(d->bmp));
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
    float zoom = (float)kThumbnailDx / (float)pageRect.dx;
    if (pageRect.dy > (float)kThumbnailDy / zoom) {
        pageRect.dy = (float)kThumbnailDy / zoom;
    }
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
    gRenderCache->CancelRendering(dm);
    gRenderCache->FreeForDisplayModel(dm);
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
    // keep the legacy bool in sync so old versions and fullscreen logic stay sane
    gGlobalPrefs->showToolbar = (mode != kToolbarHide);
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

void ControllerCallbackHandler::UpdateScrollbars(Size canvas) {
    ReportIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();

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
        ShowScrollBar(win->hwndCanvas, SB_HORZ, showHScroll);
        SetScrollInfo(win->hwndCanvas, SB_HORZ, &si, TRUE);
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
                si.nMax -= viewPort.dy - si.nPage;
            }
        }
        showVScroll = (viewPort.dy < canvas.dy);
    }
    bool showScrollbar = !hideScrollbar;
    BOOL showWinScrollbar = showScrollbar && !useOverlay;
    BOOL showOverScrollbar = showScrollbar && useOverlay;

    if (useOverlay || hideScrollbar) {
        SetScrollInfo(win->hwndCanvas, SB_VERT, &si, FALSE);
        // hide the window scrollbar explicitly (see SB_HORZ note above)
        ShowScrollBar(win->hwndCanvas, SB_VERT, FALSE);
    } else {
        ShowScrollBar(win->hwndCanvas, SB_VERT, showWinScrollbar);
        SetScrollInfo(win->hwndCanvas, SB_VERT, &si, showWinScrollbar);
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
    pageInfo = str::JoinTemp(pageInfo, StrL(" "), zoomStr);
    NotificationUpdateMessage(wnd, pageInfo);
}

static void TogglePageInfoHelper(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    NotificationWnd* wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (wnd) {
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifPageInfo);
        return;
    }
    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.timeoutMs = 0;
    args.msg = "";
    args.groupId = kNotifPageInfo;
    wnd = ShowNotification(args);
    UpdatePageInfoHelper(win->ctrl, wnd, -1);
}

void ControllerCallbackHandler::ZoomChanged(DocController* ctrl, float zoomVirtual) {
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

    if (kInvalidPageNo != pageNo) {
        TempStr label = win->ctrl->GetPageLabeTemp(pageNo);
        // HwndSetText is a no-op when the text is unchanged, so this no longer
        // repaints (flickers) the page box on e.g. Back without a page change
        HwndSetText(win->hwndPageEdit, label);
        ToolbarUpdateStateForWindow(win, false);
        if (win->ctrl->HasPageLabels()) {
            UpdateToolbarPageText(win, win->ctrl->PageCount(), true);
        }
    }

    MarkdownModel* md = ctrl->AsMarkdown();
    if (md && kInvalidPageNo != pageNo && md->ValidPageNo(pageNo)) {
        WindowTab* tab = win->CurrentTab();
        if (tab) {
            TempStr name = path::GetBaseNameTemp(md->pages[pageNo - 1]);
            if (name && !str::Eq(tab->displayName, name)) {
                tab->SetDisplayName(name);
                TabsOnChangedDoc(win);
                SetFrameTitleForTab(tab, true);
                HwndSetText(win->hwndFrame, tab->frameTitle);
            }
        }
    }

    NotificationWnd* wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (pageNo == win->currPageNo) {
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
    bool mdInFixedUI = gGlobalPrefs->markdownUI.useFixedPageUI;
    if (!mdInFixedUI && !engine) {
        FileType kind = GuessFileTypeFromName(path);
        if (MarkdownModel::IsSupportedFileType(kind)) {
            auto mdCtrl = CreateControllerForMarkdown(path, win);
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
        auto ctrl = CreateControllerForChm(path, pwdUI, win);
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

    NotificationWnd* pageInfoWnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (pageInfoWnd) {
        UpdatePageInfoHelper(win->ctrl, pageInfoWnd, -1);
    }

    UpdateFindbox(win);

    HwndSetText(win->hwndFrame, win->CurrentTab()->frameTitle);

    bool onlyNumbers = !win->ctrl || !win->ctrl->HasPageLabels();
    SetWindowStyle(win->hwndPageEdit, ES_NUMBER, onlyNumbers);
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

// Document is represented as DocController. Replace current DocController (if any) with ctrl
// in current tab.
// meaning of the internal values of LoadArgs:
// isNewWindow : if true then 'win' refers to a newly created window that needs
//   to be resized and placed
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
    ReportIf(!tab);

    // Never load settings from a preexisting state if the user doesn't wish to
    // (unless we're just refreshing the document, i.e. only if state && !state->useDefaultState)
    if (!fs && gGlobalPrefs->rememberStatePerDocument) {
        Str fn = args->FilePath();
        fs = gFileHistory.FindByPath(fn);
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
        }
    }

    AbortFinding(args->win, true);

    DocController* prevCtrl = win->ctrl;
    tab->ctrl = ctrl;
    win->ctrl = tab->ctrl;

    EngineBase* engine = tab->GetEngine();
    if (engine) {
        engine->hideAnnotations = tab->hideAnnotations;
        float imageZoom = gGlobalPrefs->imageUI.defaultZoomFloat;
        if (engine->kind == kindEngineImage && imageZoom != 0) {
            zoomVirtual = imageZoom;
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
            dm->SetInitialViewSettings(displayMode, ss.page, win->GetViewPortSize(), dpi);
            // TODO: also expose Manga Mode for image folders?
            if (tab->GetEngineType() == kindEngineComicBooks || tab->GetEngineType() == kindEngineImageDir) {
                dm->SetDisplayR2L(fs ? fs->displayR2L : gGlobalPrefs->comicBookUI.cbxMangaMode);
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
            win->ctrl->SetDisplayMode(displayMode);
            ss.page = limitValue(ss.page, 1, win->ctrl->PageCount());
            if (fs) {
                RectF r(fs->scrollPos.x, fs->scrollPos.y, 0, 0);
                if (win->AsChm()) {
                    win->AsChm()->ScrollTo(ss.page, r, kInvalidZoom);
                } else {
                    win->AsMarkdown()->ScrollTo(ss.page, r, kInvalidZoom);
                }
            } else {
                win->ctrl->GoToPage(ss.page, false);
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
    if (win->AsFixed()) {
        win->AsFixed()->Relayout(zoomVirtual, rotation);
    } else if (win && win->ctrl && win->IsDocLoaded()) {
        win->ctrl->SetZoomVirtual(zoomVirtual, nullptr);
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
            MoveWindow(win->hwndFrame, rect);
        }
        if (args->showWin) {
            ShowWindow(win->hwndFrame, showType);
            if (IsRunningOnWine()) {
                Rect wr = WindowRect(win->hwndFrame);
                Rect cr = ClientRect(win->hwndFrame);
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
            HwndEnsureVisible(win->hwndFrame);
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
    // restore scroll state after the canvas size has been restored
    if ((args->showWin || ss.page != 1) && win->AsFixed()) {
        win->AsFixed()->SetScrollState(ss);
    }

    TabsOnChangedDoc(win);

    if (!win->IsDocLoaded()) {
        win->RedrawAll(true);
        return;
    }

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
        nargs.tab = win->CurrentTab(); // only show while this tab is active
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
    if (engineErr && len(engineErr->errors) > 0) {
        TempStr msg = fmt("[%s](CmdShowErrors) %s", _TRA("Errors"), _TRA("in document"));
        NotificationCreateArgs nargs;
        nargs.hwndParent = win->hwndCanvas;
        nargs.warning = true;
        nargs.timeoutMs = 16 * 1000; // auto-dismiss after 16 seconds
        nargs.groupId = kNotifDocErrors;
        nargs.msg = msg;
        nargs.tab = win->CurrentTab(); // only show while this tab is active
        nargs.corner = NotifCorner::BottomRight;
        nargs.xMargin = 2;
        nargs.yMargin = 2;
        ShowNotification(nargs);
    }

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

void ReloadDocument(MainWindow* win, bool autoRefresh) {
    WindowTab* tab = win->CurrentTab();

    if (!tab) {
        return;
    }
    // TODO: maybe should ensure it never is called for IsAboutTab() ?
    // This only happens if gLazyLoading is true
    if (tab->IsAboutTab()) {
        return;
    }

    // tear down any in-place form-field edit before the engine is deleted below,
    // so the overlay's widget pointer can't dangle into freed memory
    CommitFormFieldEdit(false);

    tab->selectedAnnotation = nullptr;
    win->annotationBeingDragged = nullptr;
    win->annotationBeingResized = false;
    win->annotationUnderCursor = nullptr;
    tab->ignoreNextAutoReload = false;

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

    HwndPasswordUI pwdUI(win->hwndFrame);
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
        FileState* state = gFileHistory.FindByPath(fs->filePath);
        if (state) {
            CreateThumbnailForFile(win, state);
        }
    }

    if (tab->AsFixed()) {
        // save a newly remembered password into file history so that
        // we don't ask again at the next refresh
        Str decryptionKey = tab->AsFixed()->GetEngine()->decryptionKey;
        if (decryptionKey) {
            FileState* fs2 = gFileHistory.FindByName(fs->filePath, nullptr);
            if (fs2 && !str::Eq(fs2->decryptionKey, decryptionKey)) {
                str::ReplaceWithCopy(&fs2->decryptionKey, decryptionKey);
            }
        }
    }

    DeleteFileState(fs);
}

static void CreateSidebar(MainWindow* win) {
    {
        Splitter::CreateArgs args;
        args.parent = win->hwndFrame;
        args.type = SplitterType::Vert;
        win->sidebarSplitter = new Splitter();
        win->sidebarSplitter->onMove = MkFunc1Void(OnSidebarSplitterMove);
        win->sidebarSplitter->Create(args);
    }

    CreateToc(win);

    {
        Splitter::CreateArgs args;
        args.parent = win->hwndFrame;
        args.type = SplitterType::Horiz;
        win->favSplitter = new Splitter();
        win->favSplitter->onMove = MkFunc1Void(OnFavSplitterMove);
        win->favSplitter->Create(args);
    }

    CreateFavorites(win);

    CreateClaudePanel(win);
    CreateGrokPanel(win);
    CreateCodexPanel(win);

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

    win->tocLabelWithClose->SetLabel(_TRA("Bookmarks"));
    win->favLabelWithClose->SetLabel(_TRA("Favorites"));
}

static void UpdateWindowFrameBorderColor(MainWindow* win);

static MainWindow* CreateMainWindow() {
    Rect windowPos = gGlobalPrefs->windowPos;
    if (!windowPos.IsEmpty()) {
        EnsureAreaVisibility(windowPos);
    } else {
        windowPos = GetDefaultWindowPos();
    }
    // we don't want the windows to overlap so shift each window by a bit
    int nShift = len(gWindows);
    windowPos.x += (nShift * 15); // TODO: DPI scale

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

    // WM_NCCALCSIZE returning 0 disables DWM rounded corners; re-enable them.
    if (!IsRunningOnWine()) {
        dwm::SetWindowRoundedCorners(hwndFrame, true);
    }

    ReportIf(nullptr != FindMainWindowByHwnd(hwndFrame));
    MainWindow* win = new MainWindow(hwndFrame);
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
    Rect rcFrame = ClientRect(hwndFrame);
    win->hwndCanvas =
        CreateWindowExW(0, clsName.s, nullptr, style, 0, 0, rcFrame.dx, rcFrame.dy, hwndFrame, nullptr, h, nullptr);
    if (!win->hwndCanvas) {
        delete win;
        return nullptr;
    }

    if (gShowFrameRate) {
        win->frameRateWnd = new FrameRateWnd();
        win->frameRateWnd->Create(win->hwndCanvas);
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
    args.font = GetAppFont(win->hwndCanvas);
    args.isRtl = IsUIRtl();

    win->infotip = new Tooltip();
    win->infotip->Create(args);

    CreateTabbar(win);
    CreateToolbar(win);
    // create the floating find bar hidden; it owns win->hwndFindEdit
    win->findBar = CreateFindBar(win);
    CreateSidebar(win);
    UpdateFindbox(win);
    if (CanAccessDisk() && !gPluginMode) {
        RegisterCanvasDropTarget(win->hwndCanvas);
    }

    if (gWindows.IsEmpty() && !NeedsWindowEmbeddingHacks()) {
        RegisterScreenshotHotkey(win->hwndFrame);
    }
    gWindows.Append(win);
    ShowMaybeDelayedNotifications(win->hwndCanvas);
    // needed for RTL languages
    UpdateWindowRtlLayout(win);
    UpdateToolbarSidebarText(win);

    if (touch::SupportsGestures()) {
        GESTURECONFIG gc = {0, GC_ALLGESTURES, 0};
        touch::SetGestureConfig(win->hwndCanvas, 0, 1, &gc, sizeof(GESTURECONFIG));
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
    if (UseDarkModeLib() && !IsCurrentThemeDefault()) {
        DarkMode::setDarkTitleBarEx(win->hwndFrame, true);
        DarkMode::setChildCtrlsSubclassAndTheme(win->hwndFrame);
        DarkMode::removeTabCtrlSubclass(win->tabsCtrl->hwnd);
        DarkMode::setDarkScrollBar(win->hwndCanvas);
        DarkMode::setWindowMenuBarSubclass(win->hwndFrame);
        // TODO: this over-rides the font in the control
        // this will only happen with themes
        // could custom paint instead of using DarkMode
        // DarkMode::setDarkTooltips(win->infotip->hwnd, (int)DarkMode::ToolTipsType::tooltip);
    }

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
    HwndEnsureVisible(win->hwndFrame);

    if (IsRunningOnWine()) {
        Rect wr = WindowRect(win->hwndFrame);
        Rect cr = ClientRect(win->hwndFrame);
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
        InvalidateRect(win->hwndFrame, &r, TRUE);
        RedrawWindow(win->hwndFrame, &r, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
        if (win->hwndMenuReBar && IsWindowVisible(win->hwndMenuReBar)) {
            RedrawWindow(win->hwndMenuReBar, nullptr, nullptr,
                         RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        }
        if (win->tabsCtrl && win->tabsCtrl->IsVisible()) {
            RedrawWindow(win->tabsCtrl->hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
        }
    }
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
        MoveWindow(win->hwndFrame, rect);
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
    ImageList_Destroy((HIMAGELIST)SendMessageW(win->hwndToolbar, TB_GETIMAGELIST, 0, 0));
    RevokeCanvasDropTarget(win->hwndCanvas);

    ReportIf(win->findThread && WaitForSingleObject(win->findThread, 0) == WAIT_TIMEOUT);
    ReportIf(win->printThread && WaitForSingleObject(win->printThread, 0) == WAIT_TIMEOUT);

    if (win->uiaProvider) {
        // tell UIA to release all objects cached in its store
        UiaReturnRawElementProvider(win->hwndCanvas, 0, 0, nullptr);
    }

    delete win;
}

static COLORREF DwmFrameBorderColorForCurrentTheme() {
    return IsCurrentThemeDefault() ? (COLORREF)DWMWA_COLOR_DEFAULT : ThemeControlBackgroundColor();
}

static void UpdateWindowFrameBorderColor(MainWindow* win) {
    dwm::SetWindowBorderColor(win->hwndFrame, DwmFrameBorderColorForCurrentTheme());
}

void UpdateAfterThemeChange() {
    for (auto win : gWindows) {
        DeleteObject(win->brControlBgColor);
        win->brControlBgColor = CreateSolidBrush(ThemeControlBackgroundColor());

        UpdateControlsColors(win);
        RebuildMenuBarForWindow(win);
        // TODO: probably leaking toolbar image list
        UpdateToolbarAfterThemeChange(win);
        RecreateFindBar(win);
        UpdateFindWindowTheme(win);
        if (UseDarkModeLib()) {
            DarkMode::setDarkTitleBarEx(win->hwndFrame, true);
            DarkMode::setChildCtrlsTheme(win->hwndFrame);
            if (win->tabsCtrl) {
                DarkMode::removeTabCtrlSubclass(win->tabsCtrl->hwnd);
            }
            DarkMode::setDarkScrollBar(win->hwndCanvas);
            DarkMode::setWindowMenuBarSubclass(win->hwndFrame);
            // DarkMode::setDarkTooltips(win->infotip->hwnd, (int)DarkMode::ToolTipsType::tooltip);
        }
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
    FileState* fs = gFileHistory.FindByPath(newPath);
    bool oldIsPinned = false;
    int oldOpenCount = 0;
    if (fs) {
        oldIsPinned = fs->isPinned;
        oldOpenCount = fs->openCount;
        gFileHistory.Remove(fs);
        // TODO: merge favorites as well?
        if (len(*fs->favorites) > 0) {
            UpdateFavoritesTreeForAllWindows();
        }
        DeleteFileState(fs);
    }
    fs = gFileHistory.FindByName(oldPath, nullptr);
    if (fs) {
        SetFileStatePath(fs, newPath);
        // merge Frequently Read data, so that a file
        // doesn't accidentally vanish from there
        fs->isPinned = fs->isPinned || oldIsPinned;
        fs->openCount += oldOpenCount;
        // the thumbnail is recreated by LoadDocument
        delete fs->thumbnail;
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

// return true if adjustd path
static bool AdjustPathForMaybeMovedFile(LoadArgs* args) {
    Str path = args->FilePath();
    if (DocumentPathExists(path)) {
        return false;
    }
    bool failEarly = args->win && !args->forceReuse && !args->engine;
    bool fileInHistory = gFileHistory.FindByPath(path) != nullptr;
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
        ShowWindow(win->hwndFrame, SW_SHOW);
    }

    // display the notification ASAP (SaveSettings() can introduce a notable delay)
    win->RedrawAll(true);

    if (!gFileHistory.MarkFileInexistent(path)) {
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

static void ShowFileNotFound(MainWindow* win, Str path, bool noSavePrefs, bool showWin) {
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.warning = true;
    nargs.msg = fmt(_TRA("File %s not found").s, path);
    ShowNotification(nargs);
    LoadDocumentMarkNotExist(win, path, noSavePrefs, showWin);
}

void ShowErrorLoadingNotification(MainWindow* win, Str path, bool noSavePrefs, bool showWin) {
    // TODO: same message as in Canvas.cpp to not introduce
    // new translation. Find a better message e.g. why failed.
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.msg = fmt(_TRA("Error loading %s").s, path);
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

    bool openNewTab = SettingsUseTabs() && !args->forceReuse;
    ReportIf(openNewTab && args->forceReuse);

    if (win->IsCurrentTabAbout()) {
        // TODO: probably need to do it when switching tabs
        // invalidate the links on the Frequently Read page
        DeleteVecMembers(win->staticLinks);
        Rect rc = {};
        // TODO: a hack, need a way to clear tooltips
        win->infotip->Delete();
        win->DeleteToolTip();
        // there's no tab to reuse at this point
        args->forceReuse = false;
    } else {
        if (openNewTab) {
            SaveCurrentWindowTab(args->win);
        }
        CloseDocumentInCurrentTab(win, true, args->forceReuse);
    }
    if (!args->forceReuse) {
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

    auto currTab = win->CurrentTab();
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
    // TODO: figure why we hit this.
    // happens when opening 3 files via "Open With"
    // the first file is loaded via cmd-line arg, the rest
    // via DDE Open command.
    ReportIf(currTab->watcher);

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
        FileState* ds = gFileHistory.MarkFileLoaded(fullPath);
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
    }

    // Remove Zone.Identifier (Mark of the Web) so that Windows Explorer
    // will show previews/thumbnails for this file without security warnings
    if (CanAccessDisk() && !gPluginMode) {
        file::DeleteZoneIdentifier(fullPath);
    }

    return win;
}

static NotificationWnd* ShowLoadingNotif(MainWindow* win, Str path, NotifCorner corner = NotifCorner::TopLeft) {
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.groupId = path.s;
    nargs.corner = corner;
    nargs.msg = fmt(_TRA("Loading %s ...").s, path::GetBaseNameTemp(path));
    return ShowNotification(nargs);
}

static MainWindow* MaybeCreateWindowForFileLoad(LoadArgs* args) {
    MainWindow* win = args->win;
    bool openNewTab = SettingsUseTabs() && !args->forceReuse;
    if (openNewTab && !args->win) {
        // modify the args so that we always reuse the same window
        // TODO: enable the tab bar if tabs haven't been initialized
        if (!gWindows.empty()) {
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
    NotificationWnd* wndNotif = nullptr;
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

static void StartLoadDocumentThread(LoadDocumentAsyncData* data) {
    // show the "Loading ..." notification only now that we're actually
    // loading, not while the file was sitting in the queue
    LoadArgs* args = data->args;
    data->wndNotif = ShowLoadingNotif(args->win, args->FilePath(), args->loadingNotifCorner);
    gLoadThreadsActive++;
    auto fn = MkFunc0<LoadDocumentAsyncData>(LoadDocumentAsync, data);
    RunAsync(fn, "LoadDocumentThread");
}

// start a background load now if a thread slot is free, otherwise queue it
static void StartOrQueueLoadDocument(LoadDocumentAsyncData* data) {
    if (gMaxLoadThreads == 0) {
        // at most min(4, CpuCoreCount()) concurrent loads
        int n = CpuCoreCount();
        gMaxLoadThreads = n < 4 ? n : 4;
    }
    if (gLoadThreadsActive < gMaxLoadThreads) {
        StartLoadDocumentThread(data);
    } else {
        gLoadQueue.Append(data);
    }
}

// called on the UI thread when a background load finishes; frees its slot
// and starts the next queued load, if any
static void OnLoadDocumentThreadFinished() {
    gLoadThreadsActive--;
    ReportIf(gLoadThreadsActive < 0);
    while (len(gLoadQueue) > 0) {
        LoadDocumentAsyncData* next = gLoadQueue.PopAt(0);
        MainWindow* win = next->args->win;
        if (!IsMainWindowValid(win) || win->isBeingClosed) {
            // the target window is gone, e.g. it was closed or the app is
            // shutting down (uitask::Destroy drains queued finish tasks after
            // all windows were deleted). Starting the load would leak it: its
            // finish task would be posted to the already-destroyed dispatch
            // window and never run.
            next->args->onFinished.Call(false);
            delete next;
            continue;
        }
        StartLoadDocumentThread(next);
        break;
    }
}

static void LoadDocumentAsyncFinish(LoadDocumentAsyncData* d) {
    AutoDelete delData(d);
    OnLoadDocumentThreadFinished();

    auto args = d->args;
    RemoveNotification(d->wndNotif);
    MainWindow* win = args->win;
    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        DeleteOrphanedController(win, args->ctrl);
        args->onFinished.Call(false);
        return;
    }
    Str path = args->FilePath();
    if (!args->ctrl) {
        ShowErrorLoadingNotification(win, path, args->noSavePrefs, args->showWin);
        // re-sync win->ctrl with current tab after ShowErrorLoadingNotification
        // which can pump messages and change tab selection
        WindowTab* currTab = win->CurrentTab();
        win->ctrl = currTab ? currTab->ctrl : nullptr;
        args->onFinished.Call(false);
        return;
    }
    args->activateExisting = false;
    LoadDocumentFinish(args);
    args->onFinished.Call(true);
}

// Progress notification payload posted from archive extraction (worker
// thread) to the UI thread. The NotificationWnd* is safe to use without
// a separate validity check because every progress task is enqueued
// before LoadDocumentAsyncFinish (which calls RemoveNotification), and
// uitask runs posted tasks in FIFO order on the UI thread.
struct ExtractProgressUITask {
    NotificationWnd* wnd;
    Str path; // owned by the task; duped on the worker thread
    int nDecoded;
    int nTotal;
};

static void UpdateLoadingNotifUI(ExtractProgressUITask* task) {
    AutoDelete delTask(task);
    if (task->wnd) {
        // basename points into task->path, so free path only after building msg
        TempStr basename = path::GetBaseNameTemp(task->path);
        TempStr msg;
        if (task->nTotal > 0) {
            msg = fmt(_TRA("Loading %s %d of %d").s, basename, task->nDecoded, task->nTotal);
        } else {
            msg = fmt(_TRA("Loading %s %d").s, basename, task->nDecoded);
        }
        NotificationUpdateMessage(task->wnd, msg);
    }
    str::Free(task->path);
}

struct ExtractProgressState {
    NotificationWnd* wnd;
    Str path;
    u64 lastUpdate = 0;
};

static void OnExtractProgress(ExtractProgressState* s, ArchiveExtractProgress* p) {
    // throttle: the last callback (with nDecoded == nTotal) always posts
    // so the final count is displayed; intermediate callbacks post at
    // most once every 100 ms.
    bool isFinal = (p->nTotal > 0 && p->nDecoded == p->nTotal);
    u64 now = GetTickCount64();
    if (!isFinal && (now - s->lastUpdate) < 100) {
        return;
    }
    s->lastUpdate = now;

    auto* task = new ExtractProgressUITask;
    task->wnd = s->wnd;
    task->path = str::Dup(s->path);
    task->nDecoded = p->nDecoded;
    task->nTotal = p->nTotal;
    auto fn = MkFunc0<ExtractProgressUITask>(UpdateLoadingNotifUI, task);
    uitask::Post(fn, "ExtractProgress");
}

// Progress payload for the network-drive copy step, rendered as
// "Copying <name>: 12.38 MB / 45.00 MB" so users can see large copies
// making progress. nDecoded/nTotal from OnExtractProgress's task carry
// counts; this task carries byte totals instead.
struct CopyProgressUITask {
    NotificationWnd* wnd;
    Str path;
    i64 bytesCopied;
    i64 bytesTotal;
};

static void UpdateCopyNotifUI(CopyProgressUITask* task) {
    AutoDelete delTask(task);
    if (task->wnd) {
        // basename points into task->path, so free path only after building msg
        TempStr basename = path::GetBaseNameTemp(task->path);
        TempStr copied = str::FormatSizeShortTemp(task->bytesCopied, nullptr);
        TempStr msg;
        if (task->bytesTotal > 0) {
            TempStr total = str::FormatSizeShortTemp(task->bytesTotal, nullptr);
            msg = fmt(_TRA("Copying %s: %s / %s").s, basename, copied, total);
        } else {
            msg = fmt(_TRA("Copying %s: %s").s, basename, copied);
        }
        NotificationUpdateMessage(task->wnd, msg);
    }
    str::Free(task->path);
}

struct CopyProgressState {
    NotificationWnd* wnd;
    Str path;
};

static void OnFileCopyProgress(CopyProgressState* s, file::CopyProgress* p) {
    auto* task = new CopyProgressUITask;
    task->wnd = s->wnd;
    task->path = str::Dup(s->path);
    task->bytesCopied = p->bytesCopied;
    task->bytesTotal = p->bytesTotal;
    auto fn = MkFunc0<CopyProgressUITask>(UpdateCopyNotifUI, task);
    uitask::Post(fn, "CopyProgress");
}

static void LoadDocumentAsync(LoadDocumentAsyncData* d) {
    auto args = d->args;
    AtomicIntInc(&gDangerousThreadCount);
    DocController* ctrl = nullptr;
    MainWindow* win = args->win;
    HwndPasswordUI pwdUI(win->hwndFrame ? win->hwndFrame : nullptr);
    Str path = args->FilePath();
    EngineBase* engine = args->engine;

    // wire up the archive extraction progress callback so eager-load
    // archives (small cbx / epub / fb2z) can update the "Loading ..."
    // notification.
    ExtractProgressState progState;
    progState.wnd = d->wndNotif;
    progState.path = path;
    progState.lastUpdate = 0;
    gArchiveProgressCb = MkFunc1<ExtractProgressState, ArchiveExtractProgress*>(OnExtractProgress, &progState);

    // also wire up the file-copy progress callback so the cbx
    // network-drive caching step (runs before archive open) reports bytes
    // copied into the same loading notification.
    CopyProgressState copyState;
    copyState.wnd = d->wndNotif;
    copyState.path = path;
    file::gFileCopyProgressCb = MkFunc1<CopyProgressState, file::CopyProgress*>(OnFileCopyProgress, &copyState);

    args->ctrl = CreateControllerForEngineOrFile(engine, path, &pwdUI, win);

    gArchiveProgressCb = {};
    file::gFileCopyProgressCb = {};

    if (args->ctrl && gIsDebugBuild) {
        //::Sleep(5000);
    }

    auto fn = MkFunc0<LoadDocumentAsyncData>(LoadDocumentAsyncFinish, d);
    uitask::Post(fn, "TaskLoadDocumentAsyncFinish");
    AtomicIntDec(&gDangerousThreadCount);
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
    }

    win = MaybeCreateWindowForFileLoad(argsIn);
    if (!win) {
        argsIn->onFinished.Call(false);
        return;
    }

    LoadArgs* args = argsIn->Clone();

    // when using mshtml to display CHM files, we can't load in a thread
    // TODO: that's because we create web control on a thread which
    // violates threading rules and that happens as part of CreateControllerForEngineOrFile()
    // we could probably delay creating web control but that's more complicated
    if (!gGlobalPrefs->chmUI.useFixedPageUI || !gGlobalPrefs->markdownUI.useFixedPageUI) {
        FileType kind = GuessFileTypeFromName(path);
        bool isChm = !gGlobalPrefs->chmUI.useFixedPageUI && ChmModel::IsSupportedFileType(kind);
        bool isMd = !gGlobalPrefs->markdownUI.useFixedPageUI && MarkdownModel::IsSupportedFileType(kind);
        if (isChm || isMd) {
            // TODO: repeating the code below
            auto wndNotif = ShowLoadingNotif(win, path, args->loadingNotifCorner);
            DocController* ctrl = nullptr;
            HwndPasswordUI pwdUI(win->hwndFrame ? win->hwndFrame : nullptr);
            EngineBase* engine = args->engine;
            args->ctrl = CreateControllerForEngineOrFile(engine, path, &pwdUI, win);
            RemoveNotification(wndNotif);
            if (!args->ctrl) {
                ShowErrorLoadingNotification(win, path, args->noSavePrefs, args->showWin);
                // re-sync win->ctrl with current tab after ShowErrorLoadingNotification
                // which can pump messages and change tab selection
                WindowTab* currTab = win->CurrentTab();
                win->ctrl = currTab ? currTab->ctrl : nullptr;
                args->onFinished.Call(false);
                delete args;
                return;
            }
            args->activateExisting = false;
            LoadDocumentFinish(args);
            args->onFinished.Call(true);
            delete args;
            return;
        }
    }

    auto data = new LoadDocumentAsyncData;
    data->args = args;
    StartOrQueueLoadDocument(data);
}

// remember which files failed to open so that a failure to
// open a file doesn't block next/prev file in
static StrVec gFilesFailedToOpen;

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
                AppendIfNotExists(&gFilesFailedToOpen, path);
            }
        }

        if (!ctrl) {
            // ensure window is visible even if loading failed
            // (it may have been created hidden during startup)
            if (!IsWindowVisible(win->hwndFrame)) {
                ShowMainWindow(win, gGlobalPrefs->windowState);
            }
            ShowErrorLoadingNotification(win, path, args->noSavePrefs, args->showWin);
            // re-sync win->ctrl with current tab after ShowErrorLoadingNotification
            // which can pump messages and change tab selection
            WindowTab* currTab = win->CurrentTab();
            win->ctrl = currTab ? currTab->ctrl : nullptr;
            return win;
        }
    }
    args->ctrl = ctrl;
    return LoadDocumentFinish(args);
}

// Loads document data into the MainWindow.
void LoadModelIntoTab(WindowTab* tab) {
    if (!tab) {
        return;
    }

    MainWindow* win = tab->win;
    if (gGlobalPrefs->lazyLoading && win->ctrl && !tab->ctrl && !tab->IsAboutTab()) {
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.msg = fmt(_TRA("Please wait - loading...").s);
        args.warning = true;
        ShowNotification(args);
        ShowWindow(win->hwndFrame, SW_SHOW);
        // display the notification ASAP
        win->RedrawAll(true);
    }
    // ShowWindow / RedrawAll can pump messages, potentially destroying win
    if (!IsMainWindowValid(win)) {
        return;
    }
    CloseDocumentInCurrentTab(win, true, false);

    win->currentTabTemp = tab;
    win->ctrl = tab->ctrl;

    if (win->AsChm()) {
        win->AsChm()->SetParentHwnd(win->hwndCanvas);
    } else if (win->AsMarkdown()) {
        win->AsMarkdown()->SetParentHwnd(win->hwndCanvas);
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

    DisplayModel* dm = win->AsFixed();
    if (dm) {
        if (tab->canvasRc != win->canvasRc) {
            auto viewPort = win->GetViewPortSize();
            win->ctrl->SetViewPortSize(viewPort);
        } else {
            // avoid double setting of scroll state -> it gets triggered by SetViewPortSize();
            dm->SetScrollState(dm->GetScrollState());
        }
        if (dm->InPresentation() != win->InPresentation()) {
            dm->SetInPresentation(win->InPresentation());
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
        if (gGlobalPrefs->lazyLoading && !tab->ctrl) {
            ReloadDocument(win, false);
        } else {
            if (tab->reloadOnFocus) {
                tab->reloadOnFocus = false;
                ReloadDocument(win, true);
            }
        }
    }
    InvalidateRect(win->hwndCanvas, nullptr, FALSE);
    UpdateWindow(win->hwndCanvas);

    // show/hide notifications that are tied to a specific tab
    ShowNotificationsForActiveTab(win->hwndCanvas, tab);

    if (IsMainWindowValid(win)) {
        bool claudeWas = win->claudeVisible;
        bool grokWas = win->grokVisible;
        bool codexWas = win->codexVisible;
        AIChatSyncPanelsToCurrentTab(win);
        if (claudeWas != win->claudeVisible || grokWas != win->grokVisible || codexWas != win->codexVisible) {
            ScheduleUiUpdate(win);
        }
        OnClaudeTabChanged(win);
        OnGrokTabChanged(win);
        OnCodexTabChanged(win);
    }
}

enum class MeasurementUnit {
    pt,
    mm,
    in
};

static TempStr FormatCursorPositionTemp(EngineBase* engine, PointF pt, MeasurementUnit unit) {
    if (pt.x < 0) {
        pt.x = 0;
    }
    if (pt.y < 0) {
        pt.y = 0;
    }
    pt.x /= engine->GetFileDPI();
    pt.y /= engine->GetFileDPI();

    // for MeasurementUnit::in
    float factor = 1;
    Str unitName = "in";
    switch (unit) {
        case MeasurementUnit::pt:
            factor = 72;
            unitName = "pt";
            break;

        case MeasurementUnit::mm:
            factor = 25.4f;
            unitName = "mm";
            break;
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
    gRenderCache->CancelRendering(dm);
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
                gRenderCache->CancelRendering(dm);
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
    COLORREF bg;
    COLORREF text = ThemePageRenderColors(bg);
    bool pagesDark = !IsLightColor(bg);
    COLORREF link = pagesDark ? ThemeWindowLinkColor() : 0;

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
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            DisplayModel* dm = tab->AsFixed();
            if (!dm) {
                continue;
            }
            EngineMupdfInvalidateDarkMode(dm->GetEngine());
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
    if (win->AsChm()) {
        win->AsChm()->RemoveParentHwnd();
    } else if (win->AsMarkdown()) {
        win->AsMarkdown()->RemoveParentHwnd();
    }
    ClearTocBox(win);
    // stop render threads before waiting on find: they hold pagesLock/renderLock
    // that the find thread needs for text extraction (issue: stress-test hang in
    // AbortFinding while RenderCacheThread holds engine locks).
    if (DisplayModel* dm = win->AsFixed()) {
        gRenderCache->CancelRendering(dm);
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
    }
    if (deleteModel) {
        if (currentTab) {
            delete currentTab->ctrl;
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

    // TODO: this can cause a mouse capture to stick around when called from LoadModelIntoTab (cf. OnSelectionStop)
    win->mouseAction = MouseAction::None;

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
    ShowNotification(nargs);
}

static void ShowSavedAnnotationsFailedNotification(HWND hwndParent, Str path, Str mupdfErr) {
    str::Builder msg;
    msg.Append(fmt(_TRA("Saving of '%s' failed with: '%s'").s, path, mupdfErr));
    ShowWarningNotification(hwndParent, ToStr(msg), 0);
}

struct ShowErrorData {
    WindowTab* tab;
    Str path;
};

static void ShowSaveAnnotationError(ShowErrorData* d, Str err) {
    auto tab = d->tab;
    auto path = d->path;
    ShowSavedAnnotationsFailedNotification(tab->win->hwndCanvas, path, err);
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

    // have to re-open edit annotations window because the current has
    // a reference to deleted Engine
    bool hadEditAnnotations = CloseAndDeleteEditAnnotationsWindow(tab);
    ReloadDocument(tab->win, false);
    if (hadEditAnnotations) {
        // TODO: improve by remembering which annotation was selected and restoring it after  we reload
        ShowEditAnnotationsWindow(tab, nullptr);
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

    // TODO: automatically construct "foo.pdf" => "foo Copy.pdf"
    EngineBase* engine = tab->AsFixed()->GetEngine();
    TempStr srcFileName = str::Dup(engine->FilePath());
    TempWStr srcFileNameW = ToWStrTemp(srcFileName);
    wstr::BufSet(WStr(dstFileName, dimof(dstFileName)), srcFileNameW);

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

    // have to re-open edit annotations window because the current has
    // a reference to deleted Engine
    bool hadEditAnnotations = CloseAndDeleteEditAnnotationsWindow(tab);

    auto win = tab->win;
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
        // TODO: improve by remembering which annotation was selected and restoring it after reload
        // could do it by index
        ShowEditAnnotationsWindow(tab, nullptr);
    }
    return true;
}

enum class SaveChoice {
    Discard,
    SaveNew,
    SaveExisting,
    Cancel,
};

SaveChoice ShouldSaveAnnotationsDialog(HWND hwndParent, Str filePath) {
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
    dialogConfig.dwFlags = flags;
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
            bool ok = EngineMupdfSaveUpdated(engine, nullptr, fn);
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

    // Stop eventual TTS reading
    StopReadAloudIfSourceTab(tab);

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
    // TODO: use ctrl->GetDefaultFileExt()
    Kind type = nullptr;
    if (ctrl->AsFixed()) {
        type = ctrl->AsFixed()->engineType;
    } else if (ctrl->AsChm()) {
        type = kindEngineChm;
    }
    // markdown has no engine kind; it falls through to the default filter below

    auto ext = ctrl->GetDefaultFileExt();
    if (str::EqI(ext, ".xps")) {
        fileFilter.Append(_TRA("XPS documents"));
    } else if (str::EqI(ext, ".epub")) {
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
        fileFilter.Append(_TRA("Postscript documents"));
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
    file::DeleteFileToTrash(path);
    DeleteThumbnailForFile(path);
    FileState* fs = gFileHistory.FindByPath(path);
    if (fs) {
        gFileHistory.Remove(fs);
        DeleteFileState(fs);
    }
    SaveSettings();
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
    if (!str::EndsWithI(fileName, ".lnk")) {
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

#if 0
// code adapted from https://support.microsoft.com/kb/131462/en-us
static UINT_PTR CALLBACK FileOpenHook(HWND hDlg, UINT uiMsg, WPARAM wp, LPARAM lp)
{
    switch (uiMsg) {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)lp);
        break;
    case WM_NOTIFY:
        if (((LPOFNOTIFY)lp)->hdr.code == CDN_SELCHANGE) {
            LPOPENFILENAME lpofn = (LPOPENFILENAME)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            // make sure that the filename buffer is large enough to hold
            // all the selected filenames
            int cbLength = CommDlg_OpenSave_GetSpec(GetParent(hDlg), nullptr, 0) + MAX_PATH;
            if (cbLength >= 0 && lpofn->nMaxFile < (DWORD)cbLength) {
                WCHAR* oldBuffer = lpofn->lpstrFile;
                lpofn->lpstrFile = (LPWSTR)realloc(lpofn->lpstrFile, cbLength * sizeof(WCHAR));
                if (lpofn->lpstrFile)
                    lpofn->nMaxFile = cbLength;
                else
                    lpofn->lpstrFile = oldBuffer;
            }
        }
        break;
    }

    return 0;
}
#endif

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

static void GetFilesFromGetOpenFileName(OPENFILENAMEW* ofn, StrVec& filesOut) {
    WCHAR* dir = ofn->lpstrFile;
    WCHAR* file = ofn->lpstrFile + ofn->nFileOffset;
    // only a single file, full path
    TempStr path;
    if (file[-1] != 0) {
        path = ToUtf8Temp(dir);
        filesOut.Append(path);
        return;
    }
    // the layout of lpstrFile is:
    // <dir> 0 <file1> 0 <file2> 0 0
    while (*file) {
        path = ToUtf8Temp(path::JoinTemp(dir, file));
        filesOut.Append(path);
        file += len(file) + 1;
    }
}

static TempWStr GetFileFilterTemp() {
    const struct {
        Str name; /* empty if only to include in "All supported documents" */
        Str filter;
        bool available;
    } fileFormats[] = {
        {_TRA("PDF documents"), "*.pdf;*.p7m", true},
        {_TRA("XPS documents"), "*.xps;*.oxps", true},
        {_TRA("DjVu documents"), "*.djvu", true},
        {_TRA("Postscript documents"), "*.ps;*.eps", IsEnginePsAvailable()},
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
    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::Builder fileFilter;
    fileFilter.Append(_TRA("All supported documents"));
    fileFilter.AppendChar('\1');
    for (int i = 0; i < dimof(fileFormats); i++) {
        if (fileFormats[i].available) {
            fileFilter.Append(fileFormats[i].filter);
            fileFilter.AppendChar(';');
        }
    }
    ReportIf(fileFilter.LastChar() != ';');
    fileFilter.Last() = '\1';
    for (int i = 0; i < dimof(fileFormats); i++) {
        if (fileFormats[i].available && fileFormats[i].name) {
            fileFilter.Append(fileFormats[i].name);
            fileFilter.AppendChar('\1');
            fileFilter.Append(fileFormats[i].filter);
            fileFilter.AppendChar('\1');
        }
    }
    fileFilter.Append(_TRA("All files"));
    fileFilter.Append("\1*.*\1");
    Str fileFilterStr = ToStr(fileFilter);
    str::TransCharsInPlace(fileFilterStr, StrL("\1"), StrL("\0"));
    return ToWStrTemp(fileFilterStr);
}

static void OpenFile(MainWindow* win) {
    if (!CanAccessDisk()) {
        return;
    }

    // don't allow opening different files in plugin mode
    if (gPluginMode) {
        return;
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;

    ofn.lpstrFilter = GetFileFilterTemp().s;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    // OFN_ENABLEHOOK disables the new Open File dialog under Windows Vista
    // and later, so don't use it and just allocate enough memory to contain
    // several dozen file paths and hope that this is enough
    // TODO: Use IFileOpenDialog instead (requires a Vista SDK, though)
    ofn.nMaxFile = MAX_PATH * 100;
    // the `false &&` disable is deliberate; silence /analyze C6237
#pragma warning(suppress : 6237)
    if (false && !IsWindowsVistaOrGreater()) {
#if 0
        ofn.lpfnHook = FileOpenHook;
        ofn.Flags |= OFN_ENABLEHOOK;
        ofn.nMaxFile = MAX_PATH / 2;
#endif
    }
    // note: ofn.lpstrFile can be reallocated by GetOpenFileName -> FileOpenHook

    TempWStr file = AllocArrayTemp<WCHAR>(ofn.nMaxFile);
    ofn.lpstrFile = file.s;

    if (!GetOpenFileNameW(&ofn)) {
        return;
    }

    StrVec files;
    GetFilesFromGetOpenFileName(&ofn, files);
    for (Str path : files) {
        LoadArgs args(path, win);
        args.activateExisting = true;
        args.activateExistingInWindow = true;
        LoadDocument(&args);
    }
}

static void RemoveFailedFiles(StrVec& files) {
    for (Str path : gFilesFailedToOpen) {
        int idx = files.Find(path);
        if (idx >= 0) {
            files.RemoveAt(idx);
        }
    }
}

static StrVec& CollectNextPrevFilesIfChanged(Str path) {
    StrVec& files = gNextPrevDirCache;

    TempStr dir = path::GetDirTemp(path);
    if (path::IsSame(dir, gNextPrevDir)) {
        // failed files could have changed
        RemoveFailedFiles(files);
        return files;
    }
    files.Reset();
    str::ReplaceWithCopy(&gNextPrevDir, dir);
    DirIter di{dir};
    for (DirIterEntry* de : di) {
        files.Append(de->filePath);
    }
    RemoveFailedFiles(files);

    // remove unsupported files that have never been successfully loaded
    int nFiles = len(files);
    // remove unsupported files
    // traverse from the end so that removing doesn't change iterator
    for (int i = nFiles - 1; i >= 0; i--) {
        Str path2 = files[i];
        FileType kind = GuessFileTypeFromName(path2);
        bool isSupported = IsSupportedFileType(kind, true) || DocIsSupportedFileType(kind);
        bool inHistory = gFileHistory.FindByPath(path2);
        if (isSupported || inHistory) {
            continue;
        }
        files.RemoveAt(i);
    }
    AppendIfNotExists(&files, path);
    SortNatural(&files);
    return files;
}

static void ShowNoFileToOpenNotif(MainWindow* win) {
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.warning = true;
    nargs.timeoutMs = kNotifDefaultTimeOut;
    nargs.corner = NotifCorner::BottomRight;
    nargs.msg = _TRA("No file to open in this folder");
    ShowNotification(nargs);
}

static void OpenNextPrevFileInFolder(MainWindow* win, bool forward);

struct NextPrevFileInFolderData {
    MainWindow* win = nullptr;
    bool forward = true;
    Str path; // the file we tried to load; owned
    ~NextPrevFileInFolderData() { str::Free(path); }
};

static void OnNextPrevFileInFolderLoaded(NextPrevFileInFolderData* d, bool ok) {
    AutoDelete delData(d);
    MainWindow* win = d->win;
    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        return;
    }
    if (ok) {
        HwndRepaintNow(win->tabsCtrl->hwnd);
        return;
    }
    // remember the failure so CollectNextPrevFilesIfChanged skips this
    // file, then advance to the one after it in the same direction
    AppendIfNotExists(&gFilesFailedToOpen, d->path);
    OpenNextPrevFileInFolder(win, d->forward);
}

static void OpenNextPrevFileInFolder(MainWindow* win, bool forward) {
    ReportIf(win->IsCurrentTabAbout());
    if (win->IsCurrentTabAbout()) {
        return;
    }
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }

    // dismiss document error notifications from the previous document
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifDocErrors);

    WindowTab* tab = win->CurrentTab();
    bool didRetry = false;
again:
    Str path = tab->filePath;
    StrVec files = CollectNextPrevFilesIfChanged(path);
    if (len(files) < 2) {
        ShowNoFileToOpenNotif(win);
        return;
    }

    int nFiles = len(files);
    int idx = files.Find(path);
    if (forward) {
        idx = (idx + 1) % nFiles;
    } else {
        idx = (idx + nFiles - 1) % nFiles;
    }
    path = files[idx];
    if (!file::Exists(path)) {
        if (didRetry) {
            ShowNoFileToOpenNotif(win);
            return;
        }
        didRetry = true;
        str::Free(gNextPrevDir);
        gNextPrevDir = {}; // trigger re-reading the directory
        goto again;
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
    auto d = new NextPrevFileInFolderData;
    d->win = win;
    d->forward = forward;
    d->path = str::Dup(path);
    LoadArgs args(path, win);
    args.forceReuse = true;
    args.loadingNotifCorner = NotifCorner::BottomRight;
    args.onFinished = MkFunc1<NextPrevFileInFolderData, bool>(OnNextPrevFileInFolderLoaded, d);
    StartLoadDocument(&args);
}

constexpr int kSplitterDx = 5;
constexpr int kSplitterDy = 4;
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
           s1->showFavorites == s2->showFavorites && s1->showMenuBarRebar == s2->showMenuBarRebar &&
           s1->claudeVisible == s2->claudeVisible && s1->grokVisible == s2->grokVisible &&
           s1->codexVisible == s2->codexVisible && s1->aiChatDx == s2->aiChatDx;
}

static bool RelayoutFrame(MainWindow* win, bool updateToolbars, int sidebarDx) {
    Rect rc = ClientRect(win->hwndFrame);
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
    curState.showFavorites = win->uiState.favVisible;
    curState.showMenuBarRebar = IsShowingMenuBarRebar(win);
    curState.claudeVisible = win->claudeVisible;
    curState.grokVisible = win->grokVisible;
    curState.codexVisible = win->codexVisible;
    curState.aiChatDx = win->aiChatDx;

    // skip redundant relayouts when all layout-affecting state is unchanged
    if (IsUiLayoutEq(&curState, &win->uiState.layout) && updateToolbars && sidebarDx == -1) {
        return false;
    }
    // only cache for default calls; non-default calls (sidebar dragging etc.)
    // must not prevent a subsequent default call from running
    if (updateToolbars && sidebarDx == -1) {
        win->uiState.layout = curState;
    } else {
        win->uiState.layout = {};
    }
    if (gRedrawLog) {
        RECT r = ToRECT(rc);
        LogRedraw("RelayoutFrame", win->hwndFrame, &r);
    }

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        // make the black/white canvas cover the entire window
        MoveWindow(win->hwndCanvas, rc);
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
    bool suppressIntermediateRedraws = !win->suppressFrameRedraw && IsWindowVisible(win->hwndFrame);
    if (suppressIntermediateRedraws) {
        // suppress intermediate repaints during relayout
        SendMessageW(win->hwndFrame, WM_SETREDRAW, FALSE, 0);
    }

    DeferWinPosHelper dh;

    // Tabbar and toolbar at the top
    if (!win->presentation && !win->isFullScreen) {
        if (win->tabsInTitlebar) {
            bool showingMenuBar = IsShowingMenuBarRebar(win);
            // Add a visible gap above the caption for window dragging.
            // Skip when menu bar is showing (it goes all the way to the top).
            if (!IsZoomed(win->hwndFrame) && !showingMenuBar) {
                rc.y += kCaptionTopPadding;
                rc.dy -= kCaptionTopPadding;
            }
            int tabHeight = GetTabbarHeight(win->hwndFrame);
            int captionHeight = tabHeight + 2;
            if (showingMenuBar) {
                int menuBarDy = GetMenuBarRebarHeight(win);
                // check if there are actual file tabs to show
                bool hasFileTabs = false;
                for (WindowTab* tab : win->Tabs()) {
                    if (!tab->IsAboutTab()) {
                        hasFileTabs = true;
                        break;
                    }
                }
                // menu bar row + optional tabs row
                captionHeight = menuBarDy + (hasFileTabs ? tabHeight : 0);
            }
            win->captionRect = {rc.x, rc.y, rc.dx, captionHeight};
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
            rc.y += captionHeight;
            rc.dy -= captionHeight;
        } else if (win->tabsVisible) {
            int tabHeight = GetTabbarHeight(win->hwndFrame);
            if (IsRunningOnWine()) {
                logf("RelayoutFrame: tabsVisible tabHeight=%d\n", tabHeight);
            }
            if (updateToolbars) {
                int tabX = MapChildXForRtlParent(win->hwndFrame, rc.x, rc.dx);
                dh.SetWindowPos(win->tabsCtrl->hwnd, nullptr, tabX, rc.y, rc.dx, tabHeight, SWP_NOZORDER);
            }
            rc.y += tabHeight;
            rc.dy -= tabHeight;
        }
    }
    bool showMenuRebar = IsShowingMenuBarRebar(win) && (!win->tabsInTitlebar || win->isFullScreen);
    if (showMenuRebar) {
        // menu bar rebar in client area (non-titlebar case, or fullscreen where there's no titlebar)
        int menuBarDy = GetMenuBarRebarHeight(win);
        if (updateToolbars) {
            dh.SetWindowPos(win->hwndMenuReBar, nullptr, rc.x, rc.y, rc.dx, menuBarDy, SWP_NOZORDER);
        }
        rc.y += menuBarDy;
        rc.dy -= menuBarDy;
    }
    if (win->isToolbarVisible) {
        Rect rcRebar = WindowRect(win->hwndReBar);
        int rebarDy = rcRebar.dy;
        bool atBottom = ToolbarAtBottom();
        int rebarY = atBottom ? (rc.y + rc.dy - rebarDy) : rc.y;
        if (updateToolbars) {
            dh.SetWindowPos(win->hwndReBar, nullptr, rc.x, rebarY, rc.dx, rebarDy, SWP_NOZORDER);
        }
        // reserve the toolbar's space; from the bottom of the content area when
        // placed at the bottom, otherwise from the top
        rc.dy -= rebarDy;
        if (!atBottom) {
            rc.y += rebarDy;
        }
    }
    // in overlay mode the toolbar floats over the canvas and is positioned
    // separately (see PositionOverlayToolbar below); don't touch its visibility
    // here so a relayout doesn't flash it on/off
    if (updateToolbars && !win->isToolbarOverlay) {
        ShowWindow(win->hwndReBar, win->isToolbarVisible ? SW_SHOW : SW_HIDE);
    }

    // ToC and Favorites sidebars at the left
    // desired state, normalized by SetSidebarVisibility
    bool favVisible = win->uiState.favVisible;
    bool tocVisible = win->uiState.tocVisible;
    if (tocVisible || favVisible) {
        if (sidebarDx > 0) {
            win->sidebarDx = sidebarDx; // splitter drag
        }
        Size toc(win->sidebarDx, 0);
        if (0 == toc.dx) {
            // not laid out yet: width the toc box was created with
            // (gGlobalPrefs->sidebarDx, see CreateToc)
            toc.dx = ClientRect(win->hwndTocBox).dx;
        }
        if (0 == toc.dx) {
            toc.dx = rc.dx / 4;
        }
        // make sure that the sidebar is never too wide or too narrow
        // note: requires that the main frame is at least 2 * kSidebarMinDx
        //       wide (cf. OnFrameGetMinMaxInfo)
        toc.dx = limitValue(toc.dx, kSidebarMinDx, rc.dx / 2);
        win->sidebarDx = toc.dx; // remember what's applied

        toc.dy = 0;
        if (tocVisible) {
            if (!favVisible) {
                toc.dy = rc.dy;
            } else {
                toc.dy = gGlobalPrefs->tocDy;
                if (toc.dy > 0) {
                    toc.dy = limitValue(gGlobalPrefs->tocDy, 0, rc.dy);
                } else {
                    toc.dy = rc.dy / 2; // default value
                }
            }
        }

        if (tocVisible && favVisible) {
            toc.dy = limitValue(toc.dy, kTocMinDy, rc.dy - kTocMinDy);
        }

        if (tocVisible) {
            Rect rToc(rc.TL(), toc);
            dh.MoveWindow(win->hwndTocBox, rToc);
            if (favVisible) {
                Rect rSplitV(rc.x, rc.y + toc.dy, toc.dx, kSplitterDy);
                dh.MoveWindow(win->favSplitter->hwnd, rSplitV);
                toc.dy += kSplitterDy;
            }
        }
        if (favVisible) {
            Rect rFav(rc.x, rc.y + toc.dy, toc.dx, rc.dy - toc.dy);
            dh.MoveWindow(win->hwndFavBox, rFav);
        }
        Rect rSplitH(rc.x + toc.dx, rc.y, kSplitterDx, rc.dy);
        dh.MoveWindow(win->sidebarSplitter->hwnd, rSplitH);

        rc.x += toc.dx + kSplitterDx;
        rc.dx -= toc.dx + kSplitterDx;
    }

    HWND hwndAIChatBox = nullptr;
    Splitter* aiChatSplitter = nullptr;
    if (win->codexVisible && win->hwndCodexBox) {
        hwndAIChatBox = win->hwndCodexBox;
        aiChatSplitter = win->codexSplitter;
    } else if (win->grokVisible && win->hwndGrokBox) {
        hwndAIChatBox = win->hwndGrokBox;
        aiChatSplitter = win->grokSplitter;
    } else if (win->claudeVisible && win->hwndClaudeBox) {
        hwndAIChatBox = win->hwndClaudeBox;
        aiChatSplitter = win->claudeSplitter;
    }
    if (hwndAIChatBox && aiChatSplitter) {
        int aiChatDx = win->aiChatDx;
        if (aiChatDx <= 0) {
            aiChatDx = rc.dx * 3 / 8;
        }
        aiChatDx = limitValue(aiChatDx, kSidebarMinDx, rc.dx / 2);
        win->aiChatDx = aiChatDx;

        Rect rSplitter(rc.x + rc.dx - aiChatDx - kSplitterDx, rc.y, kSplitterDx, rc.dy);
        dh.MoveWindow(aiChatSplitter->hwnd, rSplitter);

        Rect rAIChat(rc.x + rc.dx - aiChatDx, rc.y, aiChatDx, rc.dy);
        dh.MoveWindow(hwndAIChatBox, rAIChat);
        rc.dx -= aiChatDx + kSplitterDx;
    }

    dh.MoveWindow(win->hwndCanvas, rc);

    dh.End();

    if (suppressIntermediateRedraws) {
        // re-enable redraw and invalidate once
        SendMessageW(win->hwndFrame, WM_SETREDRAW, TRUE, 0);
        // RDW_ALLCHILDREN ensures notification windows (children of canvas) also repaint
        RedrawWindow(win->hwndCanvas, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        RedrawWindow(win->hwndFrame, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME);
    }
    if (tocVisible) {
        RedrawWindow(win->hwndTocBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    if (favVisible) {
        RedrawWindow(win->hwndFavBox, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    if (win->codexVisible && win->hwndCodexBox) {
        RelayoutCodexPanel(win);
    } else if (win->grokVisible && win->hwndGrokBox) {
        RelayoutGrokPanel(win);
    } else if (win->claudeVisible && win->hwndClaudeBox) {
        RelayoutClaudePanel(win);
    }
    if (tocVisible || favVisible) {
        InvalidateRect(win->sidebarSplitter->hwnd, nullptr, TRUE);
    }
    if (tocVisible && favVisible) {
        InvalidateRect(win->favSplitter->hwnd, nullptr, TRUE);
    }
    if (updateToolbars && win->isToolbarVisible) {
        RedrawWindow(win->hwndReBar, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    // during a live splitter drag we must paint synchronously: WM_PAINT is
    // starved by the stream of WM_MOUSEMOVE messages and the next relayout's
    // WM_SETREDRAW FALSE would discard the pending invalidation
    bool isSplitterDrag = sidebarDx != -1;
    if (isSplitterDrag) {
        RedrawWindow(win->hwndFrame, nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
    }
    if (updateToolbars && win->tabsInTitlebar && !win->isFullScreen) {
        RECT r = ToRECT(win->captionRect);
        InvalidateRect(win->hwndFrame, &r, TRUE);
        if (win->hwndMenuReBar && IsWindowVisible(win->hwndMenuReBar)) {
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
            RedrawWindow(win->hwndReBar, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
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
    win->frameRedrawSuppressSent = IsWindowVisible(win->hwndFrame);
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
    InvalidateRect(win->hwndCanvas, nullptr, FALSE);
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

// handle WM_UPDATE_UI: perform all UI work requested via ScheduleUiUpdate
// since the last update in one pass
static void FrameUpdateUi(MainWindow* win) {
    MainWindow::UIState& ui = win->uiState;
    ui.updatePending = false;
    bool updateToolbars = ui.updateToolbars;
    int sidebarDx = ui.sidebarDx;
    ui.updateToolbars = false;
    ui.sidebarDx = -1;
    // apply the desired sidebar visibility (no-ops when unchanged)
    if (win->sidebarSplitter) {
        HwndSetVisibility(win->sidebarSplitter->hwnd, ui.tocVisible || ui.favVisible);
        HwndSetVisibility(win->hwndTocBox, ui.tocVisible);
        HwndSetVisibility(win->favSplitter->hwnd, ui.tocVisible && ui.favVisible);
        HwndSetVisibility(win->hwndFavBox, ui.favVisible);
    }
    // RelayoutFrame skips when nothing layout-affecting changed (a force is
    // requested by clearing win->uiState.layout)
    bool didLayout = RelayoutFrame(win, updateToolbars, sidebarDx);
    if (didLayout) {
        // re-anchor the floating find bar over the (possibly moved) search icon
        FindBarReposition(win);
        if (win->presentation || win->isFullScreen) {
            Rect fullscreen = GetFullscreenRect(win->hwndFrame);
            Rect rect = WindowRect(win->hwndFrame);
            // Windows XP sometimes seems to change the window size on it's own
            if (rect != fullscreen && rect != GetVirtualScreenRect()) {
                MoveWindow(win->hwndFrame, fullscreen);
            }
        }
    }
    if (ui.toolbarDirty) {
        ui.toolbarDirty = false;
        if (win->hwndReBar) {
            RedrawWindow(win->hwndReBar, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
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
            InvalidateRect(win->sidebarSplitter->hwnd, nullptr, TRUE);
        }
        if (tocVisible && favVisible) {
            InvalidateRect(win->favSplitter->hwnd, nullptr, TRUE);
        }
    }
}

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
        return; // one WM_UPDATE_UI is already queued; it'll pick this up
    }
    ui.updatePending = true;
    PostMessageW(win->hwndFrame, WM_UPDATE_UI, 0, 0);
}

// WM_DPICHANGED: the frame moved to a monitor with a different DPI (or the
// monitor's scaling changed). Resize to the rectangle Windows suggests for the
// new DPI and rebuild the DPI-scaled chrome (menu bar fonts, toolbar icons,
// find bar) so the UI re-scales instead of staying at the old monitor's size
// (issue #1832). DpiScale() reads the live per-window DPI, so relaying out is
// enough for everything else.
static void OnDpiChanged(MainWindow* win, RECT* suggested) {
    if (!win || !win->hwndFrame) {
        return;
    }
    // rebuild the DPI-dependent controls first so the resize below lays them out
    // at the new DPI (GetDpiForWindow already reports the new value here)
    RebuildMenuBarForWindow(win);
    ReCreateToolbar(win);
    RecreateFindBar(win);

    // re-apply fonts to controls that keep an HFONT: app fonts are cached
    // per DPI, so these get fonts sized for the new monitor
    HWND hwndFrame = win->hwndFrame;
    if (win->tabsCtrl) {
        win->tabsCtrl->SetFont(GetAppFont(hwndFrame));
        UpdateTabWidth(win);
    }
    if (win->tocLabelWithClose) {
        win->tocLabelWithClose->SetFont(GetAppSidebarLabelFont(hwndFrame));
    }
    if (win->tocFilterEdit) {
        win->tocFilterEdit->SetFont(GetAppFont(hwndFrame));
    }
    if (win->tocTreeView) {
        win->tocTreeView->SetFont(GetAppTreeFont(hwndFrame));
    }
    if (win->favLabelWithClose) {
        win->favLabelWithClose->SetFont(GetAppSidebarLabelFont(hwndFrame));
    }
    if (win->favTreeView) {
        win->favTreeView->SetFont(GetAppTreeFont(hwndFrame));
    }

    if (suggested) {
        int dx = suggested->right - suggested->left;
        int dy = suggested->bottom - suggested->top;
        SetWindowPos(win->hwndFrame, nullptr, suggested->left, suggested->top, dx, dy, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    win->uiState.layout = {};
    ScheduleUiUpdate(win);
    MainWindowRerender(win, true);
    uint flags = RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW;
    RedrawWindow(win->hwndFrame, nullptr, nullptr, flags);
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

static void OnMenuChangeLanguage(HWND hwnd) {
    Str newLangCode = Dialog_ChangeLanguge(hwnd, trans::GetCurrentLangCode());
    SetCurrentLanguageAndRefreshUI(newLangCode);
}

// cycle the toolbar mode show -> overlay -> hide -> show
// (in fullscreen, toggle the separate pinned-toolbar setting instead; on the
// home page overlay has no effect, so only toggle show <-> hide there)
static void OnMenuViewShowHideToolbar(MainWindow* win) {
    if (win->isFullScreen) {
        gGlobalPrefs->fullscreen.showToolbar = !gGlobalPrefs->fullscreen.showToolbar;
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
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->ctrl) {
        return;
    }
    auto* engine = tab->GetEngine();
    bool isImage = engine && engine->IsImageCollection();
    bool isCbx = engine && engine->kind == kindEngineComicBooks;
    bool isEbook = engine && engine->kind == kindEngineMupdf && !str::EqI(engine->defaultExt, ".pdf");

    COLORREF curColor;
    bool isCheckered;
    if (tab->bgColorCheckered) {
        curColor = kColorUnset;
        isCheckered = true;
    } else if (tab->bgColor != kColorUnset) {
        curColor = tab->bgColor;
        isCheckered = false;
    } else {
        // no per-document override: FixedPageUI/etc. WindowBgCol takes priority over theme
        ParsedColor* bgOverride = nullptr;
        if (isCbx) {
            bgOverride = GetPrefsColor(gGlobalPrefs->comicBookUI.windowBgCol);
        } else if (isImage) {
            bgOverride = GetPrefsColor(gGlobalPrefs->imageUI.windowBgCol);
        } else if (isEbook) {
            bgOverride = GetPrefsColor(gGlobalPrefs->eBookUI.windowBgCol);
        } else {
            bgOverride = GetPrefsColor(gGlobalPrefs->fixedPageUI.windowBgCol);
        }
        if (bgOverride->parsedOk) {
            curColor = bgOverride->col;
            isCheckered = (bgOverride->col == kColorUnset);
        } else {
            COLORREF bg;
            ThemeDocumentColors(bg);
            curColor = bg;
            isCheckered = false;
        }
    }

    Str allFilesLabel = "For all &PDF files";
    if (isCbx) {
        allFilesLabel = "For all &comic books";
    } else if (isImage) {
        allFilesLabel = "For all &images";
    } else if (isEbook) {
        allFilesLabel = "For all &ebooks";
    }

    BgColorResult result;
    if (!Dialog_ChangeBackgroundColor(win->hwndFrame, curColor, isCheckered, allFilesLabel, result)) {
        return;
    }

    Str colorStr;
    if (result.isCheckered) {
        colorStr = "checkered";
    } else {
        colorStr = SerializeColorTemp(result.color);
    }
    COLORREF newColor = result.isCheckered ? kColorUnset : result.color;

    if (result.applyToAllFiles) {
        if (isCbx) {
            str::ReplaceWithCopy(&gGlobalPrefs->comicBookUI.windowBgCol, colorStr);
            gGlobalPrefs->comicBookUI.windowBgColParsed.wasParsed = false;
        } else if (isImage) {
            str::ReplaceWithCopy(&gGlobalPrefs->imageUI.windowBgCol, colorStr);
            gGlobalPrefs->imageUI.windowBgColParsed.wasParsed = false;
        } else if (isEbook) {
            str::ReplaceWithCopy(&gGlobalPrefs->eBookUI.windowBgCol, colorStr);
            gGlobalPrefs->eBookUI.windowBgColParsed.wasParsed = false;
        } else {
            str::ReplaceWithCopy(&gGlobalPrefs->fixedPageUI.windowBgCol, colorStr);
            gGlobalPrefs->fixedPageUI.windowBgColParsed.wasParsed = false;
        }
        // clear per-file override so it inherits the global setting
        FileState* fs = gFileHistory.FindByPath(tab->filePath);
        if (fs) {
            str::ReplaceWithCopy(&fs->bgCol, "");
        }
        tab->bgColor = kColorUnset;
        tab->bgColorCheckered = false;
        SaveSettings();
    } else {
        // apply to this file only
        FileState* fs = gFileHistory.FindByPath(tab->filePath);
        if (fs) {
            str::ReplaceWithCopy(&fs->bgCol, colorStr);
            fs->bgColParsed.wasParsed = false;
        }
        tab->bgColor = newColor;
        tab->bgColorCheckered = result.isCheckered;
        SaveSettings();
    }
    // trigger repaint
    InvalidateRect(win->hwndCanvas, nullptr, TRUE);
}

static void OnMenuChangeScrollbar(HWND hwnd) {
    if (Dialog_ChangeScrollbar(hwnd)) {
        UpdateFixedPageScrollbarsVisibility();
    }
}

static void ShowOptionsDialog(HWND hwnd) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }

    if (IDOK != Dialog_Settings(hwnd, gGlobalPrefs)) {
        return;
    }

    if (!SettingsRememberOpenedFiles()) {
        gFileHistory.Clear(true);
        DeleteThumbnailCacheDirectory();
    }
    UpdateDocumentColors();
    ApplySettingsToOpenWindows();

    // note: ideally we would also update state for useTabs changes but that's complicated since
    // to do it right we would have to convert tabs to windows. When moving no tabs -> tabs,
    // there's no problem. When moving tabs -> no tabs, a half solution would be to only
    // call SetTabsInTitlebar() for windows that have only one tab, but that's somewhat inconsistent
    SaveSettings();
}

// TODO: should use currently active window, but most of the time
// there's only one window
void MaybeRedrawHomePage() {
    if (!gWindows.empty() && gWindows[0]->IsCurrentTabAbout()) {
        gWindows[0]->RedrawAll(true);
    }
}

static void ShowOptionsDialog(MainWindow* win) {
    ShowOptionsDialog(win->hwndFrame);
    MaybeRedrawHomePage();
}

static void SetInverseSearch(MainWindow* win) {
    if (Dialog_SetInverseSearch(win->hwndFrame, gGlobalPrefs)) {
        SaveSettings();
    }
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

    Rect rc = ClientRect(win->hwndCanvas);
    Point pt;
    pt.x = 2 * selRect.x + selRect.dx - rc.dx / 2;
    pt.y = 2 * selRect.y + selRect.dy - rc.dy / 2;
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
    Rect rc = ClientRect(win->hwndCanvas);
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
    switch (cmdId) {
        case CmdSinglePageView:
            viewName = _TRA("Single Page");
            break;
        case CmdFacingView:
            viewName = _TRA("Facing");
            break;
        case CmdBookView:
            viewName = _TRA("Book View");
            break;
        default:
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
void SmartZoom(MainWindow* win, float newZoom, Point* suggestedPoint, bool smartZoom) {
    if (!win->IsDocLoaded()) {
        return;
    }
    Point* pt = suggestedPoint;

    Point ptSmart;
    if (smartZoom) {
        ptSmart = GetSmartZoomPos(win, ptSmart);
        if (!ptSmart.IsEmpty()) {
            pt = &ptSmart;
        }
    }
    if (newZoom < 0) {
        // if newZoom is one of kZoomFit* constants, we don't do smartZoom
        // TODO: shouldn't happen if !smartZoom
        pt = nullptr;
    }

    win->ctrl->SetZoomVirtual(newZoom, pt);
    UpdateToolbarState(win);
    ShowZoomNotification(win, newZoom);
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

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs->showToolbar && !win->isFullScreen && !win->presentation) {
        FocusPageNoEdit(win->hwndPageEdit);
        return;
    }

    auto* ctrl = win->ctrl;
    TempStr label = ctrl->GetPageLabeTemp(ctrl->CurrentPageNo());
    Str pageLabelResult = Dialog_GoToPage(win->hwndFrame, label, ctrl->PageCount(), !ctrl->HasPageLabels());
    if (!pageLabelResult) {
        return;
    }

    int newPageNo = ctrl->GetPageByLabel(pageLabelResult);
    if (ctrl->ValidPageNo(newPageNo)) {
        ctrl->GoToPage(newPageNo, true);
    }
    str::Free(pageLabelResult);
}

void EnterFullScreen(MainWindow* win, bool presentation) {
    if (!HasPermission(Perm::FullscreenAccess) || gPluginMode) {
        return;
    }

    if (!IsWindowVisible(win->hwndFrame)) {
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
    win->nonFullScreenFrameRect = WindowRect(win->hwndFrame);

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
    bool showToolbarInFS = !presentation && gGlobalPrefs->fullscreen.showToolbar && gGlobalPrefs->showToolbar;
    bool showMenubarInFS = !presentation && gGlobalPrefs->fullscreen.showMenubar;
    win->isToolbarVisible = showToolbarInFS;
    // no floating overlay toolbar in fullscreen / presentation
    win->isToolbarOverlay = false;
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
    Rect rect = GetFullscreenRect(win->hwndFrame);

    UpdateWindowFrameBorderColor(win);
    // disable DWM rounded corners and border for true edge-to-edge fullscreen
    if (!IsRunningOnWine()) {
        dwm::SetWindowRoundedCorners(win->hwndFrame, false);
    }

    SetWindowLong(win->hwndFrame, GWL_STYLE, ws);
    uint flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER;
    SetWindowPos(win->hwndFrame, nullptr, rect.x, rect.y, rect.dx, rect.dy, flags);

    if (presentation) {
        win->ctrl->SetInPresentation(true);
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
        ShowWindow(win->hwndReBar, SW_SHOW);
    } else if (!win->isToolbarOverlay) {
        ShowWindow(win->hwndReBar, SW_HIDE);
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
        dwm::SetWindowRoundedCorners(win->hwndFrame, true);
    }
    UpdateWindowFrameBorderColor(win);

    Rect cr = ClientRect(win->hwndFrame);
    SetWindowLong(win->hwndFrame, GWL_STYLE, win->nonFullScreenWindowStyle);
    uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);
    MoveWindow(win->hwndFrame, win->nonFullScreenFrameRect);
    // TODO: this ReportIf() fires in pre-release e.g. 64011
    // ReportIf(WindowRect(win.hwndFrame) != win.nonFullScreenFrameRect);
    // We have to relayout here, because it isn't done in the SetWindowPos nor MoveWindow,
    // if the client rectangle hasn't changed.
    if (ClientRect(win->hwndFrame) == cr) {
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
    if (hasToolbar) {
        tabOrder[nWindows++] = win->hwndPageEdit;
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

static bool FrameOnKeydown(MainWindow* win, WPARAM key, LPARAM lp) {
    // TODO: how does this interact with new accelerators?
    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        // black/white screen is disabled on any unmodified key press in FrameOnChar
        return true;
    }

    if (VK_ESCAPE == key) {
        CancelDrag(win);
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
    if (AbortFinding(win, true)) {
        return;
    }
    if (RemoveNotificationsForGroup(win->hwndCanvas, kNotifPersistentWarning)) {
        return;
    }
    if (RemoveNotificationsForGroup(win->hwndCanvas, kNotifPageInfo)) {
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
    auto engine = dm->GetEngine();
    bool supportsAnnots = EngineSupportsAnnotations(engine);
    MainWindow* win = tab->win;
    bool ok = supportsAnnots && win->showSelection && tab->selectionOnPage;
    if (!ok) {
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
    if (pageNos.empty()) {
        return 0;
    }

    if (args->setContentToSelection) {
        bool isTextOnlySelection = false;
        args->content = GetSelectedTextTemp(tab, "\r\n", isTextOnlySelection);
    }

    int nCreated = 0;
    Annotation* annot = nullptr;
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
            // TODO: leaking if created annots before
            return nullptr;
        }
        SetQuadPointsAsRect(annot, rects);
        annot->bounds = GetBounds(annot);
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

static void ToggleCursorPositionInDoc(MainWindow* win) {
    // "cursor position" tip: make figuring out the current
    // cursor position in cm/in/pt possible (for exact layouting)
    if (!win->AsFixed()) {
        return;
    }
    auto notif = GetNotificationForGroup(win->hwndCanvas, kNotifCursorPos);
    if (!notif) {
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.groupId = kNotifCursorPos;
        args.shrinkLimit = 0.7f;
        args.timeoutMs = 0;
        notif = ShowNotification(args);
        cursorPosUnit = MeasurementUnit::pt;
    } else {
        switch (cursorPosUnit) {
            case MeasurementUnit::pt:
                cursorPosUnit = MeasurementUnit::mm;
                break;
            case MeasurementUnit::mm:
                cursorPosUnit = MeasurementUnit::in;
                break;
            case MeasurementUnit::in:
                cursorPosUnit = MeasurementUnit::pt;
                RemoveNotificationsForGroup(win->hwndCanvas, kNotifCursorPos);
                return;
            default:
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

    if (IsCharUpperW((WCHAR)key)) {
        key = (WPARAM)SingleCharLowerW((WCHAR)key);
    }

    DocController* ctrl = win->ctrl;

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

static void OnSidebarSplitterMove(Splitter::MoveEvent* ev) {
    Splitter* splitter = ev->w;
    HWND hwnd = splitter->hwnd;
    MainWindow* win = FindMainWindowByHwnd(hwnd);

    Point pcur = HwndGetCursorPos(win->hwndFrame);
    int sidebarDx = pcur.x; // without splitter

    // make sure to keep this in sync with the calculations in RelayoutFrame
    // note: without the min/max(..., curDx), the sidebar will be
    //       stuck at its width if it accidentally got too wide or too narrow
    Rect rFrame = ClientRect(win->hwndFrame);
    int curDx = win->sidebarDx; // don't read the toc box rect, it can be stale
    int minDx = std::min(kSidebarMinDx, curDx);
    int maxDx = std::max(rFrame.dx / 2, curDx);
    if (sidebarDx < minDx || sidebarDx > maxDx) {
        ev->resizeAllowed = false;
        return;
    }

    // coalesces a burst of splitter moves into one relayout
    ScheduleUiUpdate(win, kUiRelayout | kUiNoToolbars, sidebarDx);
}

static void OnFavSplitterMove(Splitter::MoveEvent* ev) {
    Splitter* splitter = ev->w;
    HWND hwnd = splitter->hwnd;
    MainWindow* win = FindMainWindowByHwnd(hwnd);

    Point pcur = HwndGetCursorPos(win->hwndCanvas);
    int tocDy = pcur.y; // without splitter

    // make sure to keep this in sync with the calculations in RelayoutFrame.
    // the toc box is visible here (this splitter only exists when both toc
    // and favorites are showing), so its rect is current
    Rect rFrame = ClientRect(win->hwndFrame);
    Rect rToc = ClientRect(win->hwndTocBox);
    int minDy = std::min(kTocMinDy, rToc.dy);
    int maxDy = std::max(rFrame.dy - kTocMinDy, rToc.dy);
    if (tocDy < minDy || tocDy > maxDy) {
        ev->resizeAllowed = false;
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

    if (!win->IsDocLoaded() || !win->ctrl->HasToc()) {
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

    if ((!tocVisible && HwndIsFocused(win->tocTreeView->hwnd)) ||
        (!showFavorites && HwndIsFocused(win->favTreeView->hwnd))) {
        HwndSetFocus(win->hwndFrame);
    }

    win->uiState.tocVisible = tocVisible;
    win->uiState.favVisible = showFavorites;
    ScheduleUiUpdate(win, kUiRelayout | kUiNoToolbars | kUiSidebarDirty);
}

// if url-encoded s is bigger than a reasonable URL path,
// we don't want to fail but truncate and encode less
TempStr URLEncodeMayTruncateTemp(Str s) {
    constexpr int kMaxURLLen = 1500;

    HRESULT hr;
    DWORD diff;
    WCHAR buf[kMaxURLLen + 1]{};
    DWORD flags = URL_ESCAPE_AS_UTF8;
    TempWStr ws = ToWStrTemp(s);
    // we can't predict the length of encoded string so we try
    // with increasingly smaller input strings, from 1500 down to 1000
    int maxLen = kMaxURLLen;
    for (int i = 0; i < 10; i++) {
        if (len(ws) >= maxLen) {
            ws.s[maxLen - 1] = 0;
        }
        DWORD cchSizeInOut = kMaxURLLen;
        hr = UrlEscapeW(ws.s, buf, &cchSizeInOut, flags);
        if (SUCCEEDED(hr)) {
            return ToUtf8Temp(buf);
        }
        // cchSizeInOut involves url-encoded characters
        // we can reduce ws by less characters than that
        // but don't know how many, so we use conservative guess
        diff = cchSizeInOut - kMaxURLLen;
        if (diff > 10) {
            diff = (diff * 2) / 3;
        }
        if ((int)diff >= maxLen) {
            // can't reduce further
            return {};
        }
        maxLen -= diff;
    }
    return {};
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

    // TODO: limit the size of the selection to e.g. 1 kB?
    bool isTextOnlySelectionOut; // if false, a rectangular selection
    TempStr selText = GetSelectedTextTemp(tab, "\n", isTextOnlySelectionOut);
    if (!selText) {
        return;
    }
    TempStr encodedSelection = URLEncodeMayTruncateTemp(selText);
    // ${userLang} and and ${selectin} are typed by user in settings file
    // to be shomewhat resilient against typos, we'll accept a different case
    Str lang = trans::GetCurrentLangCode();
    if (str::Eq(lang, "kr")) {
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
    if (HwndIsFocused(tab->win->hwndFindEdit) || HwndIsFocused(tab->win->hwndPageEdit)) {
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
    if (!win->IsDocLoaded()) {
        return;
    }

    float virtZoom = win->ctrl->GetZoomVirtual();
    if (!Dialog_CustomZoom(win->hwndFrame, IsBrowserDocController(win->ctrl), &virtZoom)) {
        return;
    }
    SmartZoom(win, virtZoom, nullptr, true);
}

// this is a directory for not important data, like downloaded symbols
// this directory is the same for installed / portable etc. versions
TempStr GetNotImportantDataDirTemp() {
    TempStr dir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (!dir) {
        return {};
    }
    return path::JoinTemp(dir, StrL("SumatraPDF-data"));
}

TempStr GetBuildDirNameTemp() {
    TempStr dataDir = GetNotImportantDataDirTemp();
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
    TempStr buildDir = GetBuildDirNameTemp();
    if (!buildDir) {
        return {};
    }
    // TODO: maybe use unique name
    return path::JoinTemp(buildDir, StrL("sumatra-log.txt"));
}

TempStr GetCrashInfoDirTemp() {
    TempStr buildDir = GetBuildDirNameTemp();
    if (!buildDir) {
        return {};
    }
    return path::JoinTemp(buildDir, StrL("crashinfo"));
}

void ShowLogFileSmart() {
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
            if (IsMenubarVisible()) {
                SetMenu(w->hwndFrame, w->menu);
            }
            ShowOrHideToolbar(w);
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
            SetTabsInTitlebar(w, true);
            ShowOrHideToolbar(w);
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
    SetTabsInTitlebar(win, true);
    for (int i = 0; i < len(paths); i++) {
        Str path = paths[i];
        LoadArgs args(path, win);
        args.showWin = true;
        args.forceReuse = (i == 0);
        LoadDocument(&args);
    }
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
    HWND hwndParent;
    Str text;
};

static void ListPrintersShowResult(ListPrintersResult* d) {
    RemoveNotificationsForGroup(d->hwndParent, kNotifActionResponse);
    ShowTextInWindow("SumatraPDF - Printers", d->text);
    str::Free(d->text);
    delete d;
}

static void ListPrintersThread(HWND* hwndPtr) {
    str::Builder out;
    GetPrintersInfo(out);
    auto d = new ListPrintersResult{*hwndPtr, str::Dup(ToStr(out))};
    delete hwndPtr;
    uitask::Post(MkFunc0<ListPrintersResult>(ListPrintersShowResult, d));
}

void ReopenLastClosedFile(MainWindow* win) {
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

Kind kNotifClearHistory = "clearHistry";

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
    DeleteThumbnailCacheDirectory();
    TempStr symDir = GetCrashInfoDirTemp();
    dir::RemoveAll(symDir);
    auto fn = MkFunc0<ClearHistoryData>(ClearHistoryFinish, d);
    uitask::Post(fn, "TaksClearHistoryAsyncPart");
    DestroyTempArena();
}

void ClearHistory(MainWindow* win) {
    if (!win) {
        // TODO: find current active MainWindow ?
        return;
    }

    // TODO: what is relation between gFileHistory and gGlobalPrefs->fileStates?
    int nFiles = 0;
    if (gFileHistory.states) {
        nFiles = len(*gFileHistory.states);
    }
    gFileHistory.Clear(false);

    /*
    Vec<FileState*>* files = gGlobalPrefs->fileStates;
    int nFiles = 0;
    if (files) {
        nFiles = files->Size();
        DeleteVecMembers(*files);
        delete files;
    }
    gGlobalPrefs->fileStates = new Vec<FileState*>();
    if (gGlobalPrefs->sessionData) {
        DeleteVecMembers(*gGlobalPrefs->sessionData);
        delete gGlobalPrefs->sessionData;
    }
    gGlobalPrefs->sessionData = new Vec<SessionData*>();
    */

    SaveSettings();

    NotificationCreateArgs args;
    args.groupId = kNotifClearHistory;
    args.hwndParent = win->hwndCanvas;
    args.timeoutMs = kNotif5SecsTimeOut;
    args.msg = _TRA("Clearing history...");
    ShowNotification(args);
    auto data = new ClearHistoryData;
    data->win = win;
    data->nFiles = nFiles;
    auto fn = MkFunc0<ClearHistoryData>(ClearHistoryAsync, data);
    RunAsync(fn, "ClearHistoryAsync");
}

// looks through the file history and removes entries for files that no
// longer exist on disk. Done synchronously on the main thread for simplicity.
void RemoveDeletedFilesFromHistory(MainWindow* win) {
    if (!win || !gFileHistory.states) {
        return;
    }
    int nRemoved = 0;
    Vec<FileState*>* states = gFileHistory.states;
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
        states->RemoveAt(i);
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

static void TogglePredictiveRender(MainWindow* win) {
    gPredictiveRender = !gPredictiveRender;
    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.msg = gPredictiveRender ? "Enabled predictive render" : "Disabled predictie render";
    args.timeoutMs = 3000;
    ShowNotification(args);
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

// try to trigger a crash due to corrupting allocator
// this is a different kind of a crash than just referencing invalid memory
// as corrupted memory migh prevent crash handler from working
// this can be used to test that crash handler still works
// TODO: maybe corrupt some more
void DebugCorruptMemory() {
    if (!gIsDebugBuild) {
        return;
    }
    char* s = (char*)malloc(23);
    char* d = (char*)malloc(34);
    free(s);
    free(d);
    // this triggers ntdll.dll!RtlReportCriticalFailure()
    // cppcheck-suppress doubleFree
    // the double free is deliberate; silence /analyze C6001
#pragma warning(suppress : 6001)
    free(s);
}

constexpr const char* kManualDefaultDocURI = "/SumatraPDF-documentation";
constexpr const char* kManualVirtualHost = "https://sumatrapdf.manual/";
constexpr const WCHAR* kManualVirtualHostW = L"https://sumatrapdf.manual/";

static LoadedDataResource gManualArchiveData;
static lzma::SimpleArchive gManualArchive{};
static SimpleBrowserWindow* gManualBrowserWindow = nullptr;

static void OnDestroyManualBrowserWindow(Wnd::DestroyEvent*) {
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
    if (len(path) == 0 || !str::EndsWithI(path, ".html")) {
        return false;
    }
    if (str::EqI(path, "manual.shell.html")) {
        return false;
    }
    return true;
}

static TempStr ManualArchiveLookupPathTemp(Str path) {
    TempStr lookupPath = str::DupTemp(path);
    // manual.dat stores names with backslashes (MakeLZSA convention) but WebView
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
    provider.ctx = &gManualArchive;
    provider.getResource = ManualGetResource;
    return provider;
}

static bool EnsureManualArchiveLoaded() {
    if (gManualArchive.filesCount > 0) {
        return true;
    }

    bool ok = LockDataResource(IDR_MANUAL_PAK, &gManualArchiveData);
    if (!ok) {
        logf("EnsureManualArchiveLoaded(): LockDataResource() failed\n");
        return false;
    }
    auto data = gManualArchiveData.data;
    auto size = gManualArchiveData.dataSize;
    ok = lzma::ParseSimpleArchive(data, size, &gManualArchive);
    if (!ok) {
        logf("EnsureManualArchiveLoaded: lzma::ParseSimpleArchive() failed\n");
        return false;
    }
    logf("EnsureManualArchiveLoaded(): opened manual.dat, %d files\n", gManualArchive.filesCount);
    return gManualArchive.filesCount > 0;
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
    if (!str::EndsWithI(htmlFile, ".html")) {
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
            auto fn = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroyManualBrowserWindow);
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
    if (!str::StartsWith(url, docsPrefix)) {
        return false;
    }
    // remainder is "/<page>" (LaunchDocumentation's docURI convention)
    LaunchDocumentation(Str(url.s + docsPrefix.len, url.len - docsPrefix.len));
    return true;
}

static void SetAnnotCreateArgsFromCommand(AnnotCreateArgs& args, CustomCommand* cmd) {
    args.copyToClipboard = GetCommandBoolArg(cmd, kCmdArgCopyToClipboard, false);
    args.setContentToSelection = GetCommandBoolArg(cmd, kCmdArgSetContent, false);

    auto col = GetCommandArg(cmd, kCmdArgColor);
    if (col && col->colorVal.parsedOk) {
        args.col = col->colorVal;
    }

    auto bgCol = GetCommandArg(cmd, kCmdArgBgColor);
    if (bgCol && bgCol->colorVal.parsedOk) {
        args.bgCol = bgCol->colorVal;
    }

    auto interiorCol = GetCommandArg(cmd, kCmdArgInteriorColor);
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
}

static void SetAnnotCreateArgs(AnnotCreateArgs& args, CustomCommand* cmd) {
    if (cmd && (cmd->id != cmd->origId)) {
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
        col = GetParsedColor(a.textIconColor, a.textIconColorParsed);
    } else if (typ == AnnotationType::Underline) {
        col = GetParsedColor(a.underlineColor, a.underlineColorParsed);
    } else if (typ == AnnotationType::Highlight) {
        col = GetParsedColor(a.highlightColor, a.highlightColorParsed);
    } else if (typ == AnnotationType::Squiggly) {
        col = GetParsedColor(a.squigglyColor, a.squigglyColorParsed);
    } else if (typ == AnnotationType::StrikeOut) {
        col = GetParsedColor(a.strikeOutColor, a.strikeOutColorParsed);
    } else if (typ == AnnotationType::FreeText) {
        col = GetParsedColor(a.freeTextColor, a.freeTextColorParsed);
        bgCol = GetParsedColor(a.freeTextBackgroundColor, a.freeTextBackgroundColorParsed);
        if (bgCol && bgCol->parsedOk) {
            args.bgCol = *bgCol;
        }
        args.opacity = a.freeTextOpacity;
        args.textSize = a.freeTextSize;
        args.borderWidth = a.freeTextBorderWidth;
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

    // get Downloads folder
    WCHAR* downloadsW = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &downloadsW);
    if (FAILED(hr) || !downloadsW) {
        CoTaskMemFree(downloadsW);
        return;
    }
    TempStr downloadsDir = ToUtf8Temp(downloadsW);
    CoTaskMemFree(downloadsW);

    // generate unique path: clipboard.png, clipboard.1.png, etc.
    TempStr basePath = path::JoinTemp(downloadsDir, StrL("clipboard.png"));
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
            FileState* state = gFileHistory.Get(idx);
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

        case CmdSelectionHandler: {
            // TODO: handle kCmdArgExe
            auto url = GetCommandStringArg(cmd, kCmdArgURL, nullptr);
            if (!url) {
                return 0;
            }
            // try to auto-fix url
            bool isValidURL = (str::Contains(url, StrL("://")));
            if (!isValidURL) {
                url = str::JoinTemp(StrL("https://"), url);
            }
            LaunchBrowserWithSelection(tab, url);
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

        case CmdShowInFolder:
            ShowCurrentFileInFolder(win);
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
            if (!win->IsCurrentTabAbout()) {
                ShowNavFilesInFolder(win);
            }
            break;

        case CmdRenameFile:
            RenameCurrentFile(win);
            break;

        case CmdDeleteFile:
            DeleteCurrentFile(win);
            break;

        case CmdSaveAs:
            SaveCurrentFileAs(win);
            break;

        case CmdPrint:
            PrintCurrentFile(win);
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
            OnAIChatWithClaudeCode(win);
            break;

        case CmdAIChatWithGrokBuild:
            OnAIChatWithGrokBuild(win);
            break;

        case CmdAIChatWithOpenAICodex:
            OnAIChatWithOpenAICodex(win);
            break;

        case CmdClearHistory:
            ClearHistory(win);
            break;

        case CmdRemoveDeletedFilesFromHistory:
            RemoveDeletedFilesFromHistory(win);
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
            ShowImageEditWindow(win, ImageEditMode::Crop);
            break;

        case CmdResizeImage:
            ShowImageEditWindow(win, ImageEditMode::Resize);
            break;

        case CmdConvertImageToPdf:
            ShowImageEditWindow(win, ImageEditMode::Save, nullptr, nullptr, /* selectPdf */ true);
            break;

        case CmdPasteClipboardImage:
            PasteImageFromClipboard(win);
            break;

        case CmdListPrinters: {
            NotificationCreateArgs nargs;
            nargs.hwndParent = win->hwndCanvas;
            nargs.msg = _TRA("Collecting list of printers");
            ShowNotification(nargs);
            auto data = new HWND(win->hwndCanvas);
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
            if (!win->isFullScreen && GetCommandArg(cmd, kCmdArgState)) {
                // explicit state: on -> show (pinned), off -> hide
                bool on = GetCommandBoolArg(cmd, kCmdArgState, true);
                SetToolbarModeAndApply(on ? kToolbarShow : kToolbarHide);
            } else if (win->isFullScreen) {
                if (ShouldToggle(cmd, gGlobalPrefs->fullscreen.showToolbar)) {
                    OnMenuViewShowHideToolbar(win);
                }
            } else {
                OnMenuViewShowHideToolbar(win);
            }
            break;

        case CmdChangeScrollbar:
            OnMenuChangeScrollbar(win->hwndFrame);
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
            OnMenuChangeLanguage(win->hwndFrame);
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
            } else {
                win->ctrl->GoToPrevPage();
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
            } else {
                win->ctrl->GoToNextPage();
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
            if (engine && len(engine->errors) > 0) {
                Str text = Join(&engine->errors, "");
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

        case CmdDebugToggleRtl:
            gForceRtl = !gForceRtl;
            for (auto w : gWindows) {
                UpdateWindowRtlLayout(w);
            }
            break;

        case CmdDebugDownloadSymbols:
            DownloadDebugSymbols();
            break;

        case CmdDebugTogglePredictiveRender:
            TogglePredictiveRender(win);
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
            DocumentColorsFollowTheme mode = GetDocumentColorsFollowTheme();
            if (mode == DocumentColorsFollowTheme::Off) {
                SetDocumentColorsFollowTheme(DocumentColorsFollowTheme::Smart);
            } else {
                SetDocumentColorsFollowTheme(DocumentColorsFollowTheme::Off);
            }
            UpdateDocumentColors();
            UpdateControlsColors(win);
            SaveSettings();
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
                // MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
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

        case CmdDiscardAnnotations: {
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
            if (!colorTab || !colorTab->ctrl) {
                return 0;
            }
            COLORREF curColor = colorTab->tabColor;
            bool isUnset = (curColor == kColorUnset);
            if (isUnset) {
                curColor = ThemeControlBackgroundColor();
            }
            COLORREF newColor;
            bool newIsUnset;
            if (!Dialog_SetTabColor(win->hwndFrame, curColor, isUnset, newColor, newIsUnset)) {
                return 0;
            }
            colorTab->tabColor = newIsUnset ? kColorUnset : newColor;
            // update TabInfo
            TabInfo* ti = win->tabsCtrl->GetTab(win->GetTabIdx(colorTab));
            if (ti) {
                ti->tabColor = colorTab->tabColor;
            }
            // persist to FileState
            FileState* fs = gFileHistory.FindByPath(colorTab->filePath);
            if (fs) {
                if (newIsUnset) {
                    str::ReplaceWithCopy(&fs->tabCol, "");
                } else {
                    TempStr colorStr = SerializeColorTemp(newColor);
                    str::ReplaceWithCopy(&fs->tabCol, colorStr);
                }
                fs->tabColParsed.wasParsed = false;
            }
            SaveSettings();
            win->tabsCtrl->ScheduleRepaint();
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
                auto r = WindowRect(win->hwndCanvas);
                pt.x = r.dx / 2;
                pt.y = 20;
                pageNoUnderCursor = dm->GetPageNoByPoint(pt);
                if (pageNoUnderCursor < 0) return 0;
            }
            PointF ptOnPage = dm->CvtFromScreen(pt, pageNoUnderCursor);
            MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
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
            Str img = GetClipboardImageBmp();
            if (len(img) == 0) {
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
                auto r = WindowRect(win->hwndCanvas);
                pt.x = r.dx / 2;
                pt.y = 20;
                pageNoUnderCursor = dm->GetPageNoByPoint(pt);
            }
            if (pageNoUnderCursor < 0) {
                str::Free(img);
                return 0;
            }
            PointF ptOnPage = dm->CvtFromScreen(pt, pageNoUnderCursor);
            AnnotCreateArgs args{AnnotationType::Stamp};
            args.stampImage = img;
            lastCreatedAnnot = EngineMupdfCreateAnnotation(engine, pageNoUnderCursor, ptOnPage, &args);
            str::Free(img);
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
    SetSelectedAnnotation(tab, lastCreatedAnnot);
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
    SetWindowStyle(hwnd, WS_VISIBLE, false);

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

    SetWindowStyle(hwnd, WS_VISIBLE, true);
    return menu;
}

static void TrackCaptionPopupMenu(MainWindow* win, HMENU menu, Rect btnRect) {
    Rect rs = MapLtrClientRectToScreen(win->hwndFrame, btnRect);
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
        RECT rc = ToRECT(win->captionBtn[btnIdx].rect);
        InvalidateRect(hwnd, &rc, FALSE);
        UpdateWindow(hwnd);
    } else {
        InvalidateRect(hwnd, nullptr, FALSE);
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
        WCHAR* subMenuName = AllocArrayTemp<WCHAR>(mii.cch);
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

constexpr int kTabsButtonGapX = 32;

void RelayoutCaption(MainWindow* win) {
    for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
        win->captionBtn[i].id = i;
    }
    Rect rc = win->captionRect;
    bool maximized = IsZoomed(win->hwndFrame);
    bool showingMenuBar = IsShowingMenuBarRebar(win);
    int tabHeight = GetTabbarHeight(win->hwndFrame);
    bool isRtl = IsUIRtl();
    if (IsRunningOnWine()) {
        logf("RelayoutCaption: captionRect=(%d,%d,%d,%d) tabHeight=%d showingMenuBar=%d maximized=%d\n", rc.x, rc.y,
             rc.dx, rc.dy, tabHeight, (int)showingMenuBar, (int)maximized);
    }

    if (showingMenuBar) {
        // Two-row layout:
        //   Row 1 (top): CB_SYSTEM_MENU, menu bar rebar, [drag area], min/max/close
        //   Row 2: tabs, [drag area]
        // Menu bar goes all the way to the top for compactness.

        int menuBarDy = GetMenuBarRebarHeight(win);

        int row1Y = rc.y;
        int row2Y = rc.y + menuBarDy;

        // window buttons match menu bar size: dy = menuBarDy, dx = menuBarDy
        int btnDy = menuBarDy;
        int btnDx = menuBarDy;

        int row1X = 0;
        int row1Dx = 0;
        int buttonsWidth = 3 * btnDx;
        int tabsX = rc.x;
        int tabsDx = rc.dx;

        if (isRtl) {
            int x = rc.x;
            win->captionBtn[CB_CLOSE].rect = {x, row1Y, btnDx, btnDy};
            win->captionBtn[CB_CLOSE].visible = true;
            x += btnDx;

            win->captionBtn[CB_RESTORE].rect = {x, row1Y, btnDx, btnDy};
            win->captionBtn[CB_RESTORE].visible = maximized;
            if (maximized) {
                x += btnDx;
            }

            win->captionBtn[CB_MAXIMIZE].rect = {x, row1Y, btnDx, btnDy};
            win->captionBtn[CB_MAXIMIZE].visible = !maximized;
            if (!maximized) {
                x += btnDx;
            }

            win->captionBtn[CB_MINIMIZE].rect = {x, row1Y, btnDx, btnDy};
            win->captionBtn[CB_MINIMIZE].visible = true;
            x += btnDx;

            int right = rc.x + rc.dx;
            right -= btnDx;
            win->captionBtn[CB_SYSTEM_MENU].rect = {right, row1Y, btnDx, btnDy};
            win->captionBtn[CB_SYSTEM_MENU].visible = true;

            row1X = x;
            row1Dx = right - x;
            tabsX = rc.x + buttonsWidth;
            tabsDx = rc.dx - buttonsWidth - btnDx;
        } else {
            win->captionBtn[CB_CLOSE].rect = {rc.x + rc.dx - btnDx, row1Y, btnDx, btnDy};
            win->captionBtn[CB_CLOSE].visible = true;
            rc.dx -= btnDx;

            win->captionBtn[CB_RESTORE].rect = {rc.x + rc.dx - btnDx, row1Y, btnDx, btnDy};
            win->captionBtn[CB_RESTORE].visible = maximized;

            win->captionBtn[CB_MAXIMIZE].rect = {rc.x + rc.dx - btnDx, row1Y, btnDx, btnDy};
            win->captionBtn[CB_MAXIMIZE].visible = !maximized;
            rc.dx -= btnDx;

            win->captionBtn[CB_MINIMIZE].rect = {rc.x + rc.dx - btnDx, row1Y, btnDx, btnDy};
            win->captionBtn[CB_MINIMIZE].visible = true;
            rc.dx -= btnDx;

            // Row 1 left: system menu (sized to match window buttons)
            win->captionBtn[CB_SYSTEM_MENU].rect = {rc.x, row1Y, btnDx, btnDy};
            win->captionBtn[CB_SYSTEM_MENU].visible = true;
            row1X = rc.x + btnDx;
            row1Dx = rc.dx - btnDx;
            tabsX = rc.x;
            tabsDx = rc.dx;
        }

        // CB_MENU hidden when menu bar rebar is showing
        win->captionBtn[CB_MENU].rect = {row1X, row1Y, menuBarDy, menuBarDy};
        win->captionBtn[CB_MENU].visible = false;

        // Menu bar rebar in row 1 after system menu, natural width
        int menuBarWidth = row1Dx; // default: fill available
        if (win->hwndMenuToolbar) {
            int btnCount = (int)SendMessageW(win->hwndMenuToolbar, TB_BUTTONCOUNT, 0, 0);
            if (btnCount > 0) {
                RECT lastBtn;
                SendMessageW(win->hwndMenuToolbar, TB_GETITEMRECT, btnCount - 1, (LPARAM)&lastBtn);
                int naturalWidth = lastBtn.right + GetSystemMetrics(SM_CXBORDER) * 2;
                if (naturalWidth < row1Dx) {
                    menuBarWidth = naturalWidth;
                }
            }
        }

        // check if there are actual file tabs to show
        bool hasFileTabs = false;
        for (WindowTab* tab : win->Tabs()) {
            if (!tab->IsAboutTab()) {
                hasFileTabs = true;
                break;
            }
        }

        DeferWinPosHelper dh;
        int menuBarX = MapChildXForRtlParent(win->hwndFrame, row1X, menuBarWidth);
        dh.SetWindowPos(win->hwndMenuReBar, nullptr, menuBarX, row1Y, menuBarWidth, menuBarDy, SWP_NOZORDER);

        if (hasFileTabs) {
            // Row 2: tabs
            win->tabsCtrl->SetIsVisible(true);
            int tabBarX = MapChildXForRtlParent(win->hwndFrame, tabsX, tabsDx);
            dh.SetWindowPos(win->tabsCtrl->hwnd, nullptr, tabBarX, row2Y, tabsDx, tabHeight, SWP_NOZORDER);
        } else {
            // no file tabs: hide tab bar, single-row caption
            win->tabsCtrl->SetIsVisible(false);
        }
        dh.End();
    } else {
        // Single-row layout
        int btnDy = rc.y + rc.dy;
        int btnDx = btnDy;

        // tabs fill the full caption height (rc.dy)
        int tabDy = rc.dy;
        int tabY = rc.y + rc.dy - tabDy;

        int tabsX = 0;
        int tabsDx = 0;

        if (isRtl) {
            int x = rc.x;
            win->captionBtn[CB_CLOSE].rect = {x, 0, btnDx, btnDy};
            win->captionBtn[CB_CLOSE].visible = true;
            x += btnDx;

            win->captionBtn[CB_RESTORE].rect = {x, 0, btnDx, btnDy};
            win->captionBtn[CB_RESTORE].visible = maximized;
            if (maximized) {
                x += btnDx;
            }

            win->captionBtn[CB_MAXIMIZE].rect = {x, 0, btnDx, btnDy};
            win->captionBtn[CB_MAXIMIZE].visible = !maximized;
            if (!maximized) {
                x += btnDx;
            }

            win->captionBtn[CB_MINIMIZE].rect = {x, 0, btnDx, btnDy};
            win->captionBtn[CB_MINIMIZE].visible = true;
            x += btnDx;

            int right = rc.x + rc.dx;
            right -= tabDy;
            win->captionBtn[CB_SYSTEM_MENU].rect = {right, tabY, tabDy, tabDy};
            win->captionBtn[CB_SYSTEM_MENU].visible = true;
            right -= tabDy;
            win->captionBtn[CB_MENU].rect = {right, tabY, tabDy, tabDy};
            win->captionBtn[CB_MENU].visible = true;
            right -= kTabsButtonGapX;

            tabsX = x;
            tabsDx = right - x;
        } else {
            win->captionBtn[CB_CLOSE].rect = {rc.x + rc.dx - btnDx, 0, btnDx, btnDy};
            win->captionBtn[CB_CLOSE].visible = true;
            rc.dx -= btnDx;

            win->captionBtn[CB_RESTORE].rect = {rc.x + rc.dx - btnDx, 0, btnDx, btnDy};
            win->captionBtn[CB_RESTORE].visible = maximized;

            win->captionBtn[CB_MAXIMIZE].rect = {rc.x + rc.dx - btnDx, 0, btnDx, btnDy};
            win->captionBtn[CB_MAXIMIZE].visible = !maximized;
            rc.dx -= btnDx;

            win->captionBtn[CB_MINIMIZE].rect = {rc.x + rc.dx - btnDx, 0, btnDx, btnDy};
            win->captionBtn[CB_MINIMIZE].visible = true;
            rc.dx -= btnDx;

            win->captionBtn[CB_SYSTEM_MENU].rect = {rc.x, tabY, tabDy, tabDy};
            win->captionBtn[CB_SYSTEM_MENU].visible = true;
            rc.x += tabDy;
            rc.dx -= tabDy;

            win->captionBtn[CB_MENU].rect = {rc.x, tabY, tabDy, tabDy};
            win->captionBtn[CB_MENU].visible = true;
            rc.x += tabDy;
            rc.dx -= tabDy;

            // leave a gap between the tab bar and the minimize button
            rc.dx -= kTabsButtonGapX;
            tabsX = rc.x;
            tabsDx = rc.dx;
        }

        DeferWinPosHelper dh;
        int tabBarX = MapChildXForRtlParent(win->hwndFrame, tabsX, tabsDx);
        dh.SetWindowPos(win->tabsCtrl->hwnd, nullptr, tabBarX, tabY, tabsDx, tabDy, SWP_NOZORDER);
        dh.End();
        if (IsRunningOnWine()) {
            logf("RelayoutCaption: singleRow btnDy=%d tabY=%d tabDy=%d tabsDx=%d\n", btnDy, tabY, tabDy, tabsDx);
        }
    }

    UpdateTabWidth(win);

    for (int i = CB_BTN_FIRST; i < CB_BTN_COUNT; i++) {
        if (win->captionBtn[i].visible) {
            RECT r = ToRECT(win->captionBtn[i].rect);
            InvalidateRect(win->hwndFrame, &r, FALSE);
        }
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
        COLORREF bgc = ThemeControlBackgroundColor();
        SolidBrush bgBrNormal(GdiRgbFromCOLORREF(bgc));
        gfx.FillRectangle(&bgBrNormal, rButton.x, rButton.y, rButton.dx, rButton.dy);

        bool isClose = (button == CB_CLOSE);
        bool isHot = (stateId == CBS_HOT);
        bool isPushed = (stateId == CBS_PUSHED);
        bool isInactive = (stateId == CBS_INACTIVE);

        if (isHot || isPushed) {
            Color bgCol;
            if (isClose) {
                bgCol = isPushed ? Color(200, 196, 43, 28) : Color(255, 196, 43, 28);
            } else {
                COLORREF hotBg = bgc;
                hotBg = isPushed ? AccentColor(bgc, 40) : AccentColor(bgc, 20);
                bgCol = GdiRgbFromCOLORREF(hotBg);
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

        COLORREF iconCol;
        if (isInactive) {
            iconCol = RGB(153, 153, 153);
        } else if (isClose && (isHot || isPushed)) {
            iconCol = RGB(255, 255, 255);
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
        int iconPx = DpiScale(win->hwndFrame, kCaptionGlyphDip);
        DrawCaptionSysButtonGlyph(hdc, kind, rc, iconCol, iconPx);
    } else if (button == CB_MENU) {
        SolidBrush bgBrMenu(GdiRgbFromCOLORREF(ThemeControlBackgroundColor()));
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
            SolidBrush br(Color(buttonAlpha, buttonRGB, buttonRGB, buttonRGB));
            gfx.FillRectangle(&br, rc.x, rc.y, rc.dx, rc.dy);
        }
        COLORREF c = ThemeWindowTextColor();
        u8 r, g, b;
        UnpackColor(c, r, g, b);
        float width = floorf((float)rc.dy / 8.0f);
        Pen p(Color(r, g, b), width);
        rc.Inflate(-int(rc.dx * 0.2f + 0.5f), -int(rc.dy * 0.3f + 0.5f));
        for (int i = 0; i < 3; i++) {
            gfx.DrawLine(&p, rc.x, rc.y + i * rc.dy / 2, rc.x + rc.dx, rc.y + i * rc.dy / 2);
        }
    } else if (button == CB_SYSTEM_MENU) {
        SolidBrush bgBrSys(GdiRgbFromCOLORREF(ThemeControlBackgroundColor()));
        gfx.FillRectangle(&bgBrSys, rButton.x, rButton.y, rButton.dx, rButton.dy);
        int xIcon = GetSystemMetrics(SM_CXSMICON);
        int yIcon = GetSystemMetrics(SM_CYSMICON);
        HICON hIcon = (HICON)GetClassLongPtr(win->hwndFrame, GCLP_HICONSM);
        int x = rButton.x + (rButton.dx - xIcon) / 2;
        int y = rButton.y + (rButton.dy - yIcon) / 2;
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
            // paint the 1px NC strip at top with the correct color
            HDC hdc = GetWindowDC(hwnd);
            if (hdc) {
                Rect wr = WindowRect(hwnd);
                // window DC is in window coordinates (origin at top-left of window)
                RECT rc = {0, 0, wr.dx, 1};
                HBRUSH br = CreateSolidBrush(ThemeControlBackgroundColor());
                FillRect(hdc, &rc, br);
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
            Rect captionArea = {0, 0, ClientRect(hwnd).dx, cr.y + cr.dy};
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
                FillRect(memDC, &rcFill, brCap);
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
                FillRect(hdc, &ps.rcPaint, brBorder);
                DeleteObject(brBorder);
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
            if (wp == kHideOverlayToolbarTimerId) {
                OverlayToolbarHideTimerFired(win);
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
                int frameX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                int frameY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                r->left += frameX;
                r->top += frameY;
                r->right -= frameX;
                r->bottom -= frameY;
                r->bottom -= NON_CLIENT_BAND;
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
            RECT wrc;
            GetWindowRect(hwnd, &wrc);

            // use a larger hit-test area than the visible border for easier resizing
            if (!IsZoomed(hwnd) && !win->isFullScreen && !win->presentation) {
                int b = kFrameResizeHitTest;
                bool onLeft = (x - wrc.left) < b;
                bool onRight = (wrc.right - x) <= b;
                bool onTop = (y - wrc.top) < b;
                bool onBottom = (wrc.bottom - y) <= b;

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
                Point ptClient{x, y};
                HwndScreenToClient(hwnd, ptClient);
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
                Rect rClient = MapRectToWindow(ClientRect(hwnd), hwnd, HWND_DESKTOP);
                Rect rCaption = MapRectToWindow(win->captionRect, hwnd, HWND_DESKTOP);
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
            if (UseDarkModeLib() && DarkMode::isEnabled()) {
                HWND hMenu = FindWindow(UNDOCUMENTED_MENU_CLASS_NAME, nullptr);
                if (hMenu) {
                    DarkMode::setDarkTitleBarEx(hMenu, false);
                }
            }
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
        if (end > textLen) {
            end = textLen;
        }
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
        InvalidateRect(tab->win->hwndCanvas, nullptr, FALSE);
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
    InvalidateRect(tab->win->hwndCanvas, nullptr, FALSE);
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
        InvalidateRect(tab->win->hwndCanvas, nullptr, FALSE);
    }
    ReadAloudClearSourceTab();
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
        InvalidateRect(tab->win->hwndCanvas, nullptr, FALSE);
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
    int hundredths = (int)(speed * 100.0f + 0.5f);
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

static void ShowTtsVoiceMenu(MainWindow* win, NMTOOLBARW* nmtb) {
    if (!win || !nmtb || nmtb->iItem != CmdReadAloud) {
        return;
    }

    RECT rc{};
    SendMessageW(nmtb->hdr.hwndFrom, TB_GETRECT, CmdReadAloud, (LPARAM)&rc);
    MapWindowPoints(nmtb->hdr.hwndFrom, HWND_DESKTOP, (POINT*)&rc, 2);

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    BuildReadAloudMenuItems(menu, win, false, false);

    UINT selected = (UINT)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, rc.left, rc.bottom, 0,
                                         win->hwndFrame, nullptr);

    HandleReadAloudMenuSelection(win, selected);
    DestroyMenu(menu);
}

LRESULT CALLBACK WndProcSumatraFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);

    // DbgLogMsg("frame:", hwnd, msg, wp, lp);
    // detect when an external host (e.g. Total Commander's lister) embeds us
    // by reparenting our window as WS_CHILD
    bool isChildWindow = IsWindowStyleSet(hwnd, WS_CHILD);
    if (win && !gMyWindowWasEmbedded && isChildWindow) {
        logf("Detected window embedded in another window\n");
        gMyWindowWasEmbedded = true;
        str::ReplaceWithCopy(&gGlobalPrefs->scrollbars, "windows");
        SetTabsInTitlebar(win, false);
        DestroyMenuBarRebar(win);
        SetMenu(hwnd, nullptr);
        UpdateTabWidth(win);
        ScheduleUiUpdate(win);
    }
    if (win && win->tabsInTitlebar) {
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

    switch (msg) {
        case WM_CREATE:
            // do nothing
            TtsSetNotifyWindow(hwnd, WM_TTS_EVENT, 0, 0);
            goto InitMouseWheelInfo;

        case WM_SIZE:
            if (win && SIZE_MINIMIZED != wp) {
                RememberDefaultWindowPosition(win);
                // UIState.layout.rc remembers the last laid-out client size;
                // the scheduled update relayouts only when the size actually
                // changed, and a burst of WM_SIZE does the work once
                ScheduleUiUpdate(win);
            }
            break;

        case WM_UPDATE_UI:
            // deferred, coalesced UI update requested via ScheduleUiUpdate
            if (win) {
                FrameUpdateUi(win);
            }
            return 0;

        case WM_GETMINMAXINFO:
            return OnFrameGetMinMaxInfo((MINMAXINFO*)lp);

        case WM_DPICHANGED:
            if (win) {
                OnDpiChanged(win, (RECT*)lp);
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
            if (UseDarkModeLib() && DarkMode::isEnabled()) {
                HWND hMenuWnd = FindWindow(UNDOCUMENTED_MENU_CLASS_NAME, nullptr);
                if (hMenuWnd) {
                    DarkMode::setDarkTitleBarEx(hMenuWnd, false);
                }
            }
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

        case WM_NOTIFY: {
            NMHDR* hdr = (NMHDR*)lp;
            if (win && hdr && hdr->hwndFrom == win->hwndToolbar && hdr->code == TBN_DROPDOWN) {
                ShowTtsVoiceMenu(win, (NMTOOLBARW*)lp);
                return TBDDRET_DEFAULT;
            }
            break;
        }

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

        case WM_ACTIVATE:
            if (wp != WA_INACTIVE) {
                gLastActiveFrameHwnd = hwnd;
            } else if (win) {
                // hide the topmost citation-hover popup when switching to
                // another application (no WM_MOUSELEAVE is generated then)
                RefHoverHide(win->refHover, win->hwndCanvas);
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

        case WM_SETTINGCHANGE:
            // Windows switched between light and dark mode: re-resolve the
            // System theme (no-op unless Theme = System)
            if (lp && str::EqI(ToUtf8Temp((const WCHAR*)lp), StrL("ImmersiveColorSet"))) {
                UpdateThemeAfterSystemColorChange();
            }
        InitMouseWheelInfo:
            UpdateDeltaPerLine();

            if (win) {
                // in tablets it's possible to rotate the screen. if we're
                // in full screen, resize our window to match new screen size
                if (win->presentation) {
                    EnterFullScreen(win, true);
                } else if (win->isFullScreen) {
                    EnterFullScreen(win, false);
                }
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
                InvalidateRect(gReadAloudSourceTab->win->hwndCanvas, nullptr, FALSE);
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
                RECT rc;
                GetClientRect(hwnd, &rc);
                HBRUSH br = CreateSolidBrush(ThemeMainWindowBackgroundColor());
                FillRect(hdc, &rc, br);
                DeleteObject(br);
                if (!win->captionRect.IsEmpty()) {
                    RECT rcCaption = ToRECT(win->captionRect);
                    HBRUSH brCaption = CreateSolidBrush(ThemeControlBackgroundColor());
                    FillRect(hdc, &rcCaption, brCaption);
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
        TempStr dllPath = path::JoinTemp(dir, StrL("libmupdf.dll"));
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
    auto url = "https://www.sumatrapdfreader.org/docs/Submit-crash-report.html";
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
    if (gWindows.IsEmpty()) {
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

void ShutdownCleanup() {
    TtsRelease();
    FreeHomePageTips();
    DisconnectLastDragDataObject();

    gAllowedFileTypes.Reset();
    gAllowedLinkProtocols.Reset();
}
