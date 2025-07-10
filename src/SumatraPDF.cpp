/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/StrFormat.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/CryptoUtil.h"
#include "utils/DirIter.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/FileWatcher.h"
#include "utils/GuessFileType.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HttpUtil.h"
#include "utils/SquareTreeParser.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/Archive.h"
#include "utils/Timer.h"
#include "utils/LzmaSimpleArchive.h"

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
#include "Annotation.h"
#include "PdfCreator.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
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
#include "AppColors.h"
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
#include "Caption.h"
#include "CrashHandler.h"
#include "ExternalViewers.h"
#include "Favorites.h"
#include "FileThumbnails.h"
#include "Menu.h"
#include "Print.h"
#include "SearchAndDDE.h"
#include "Selection.h"
#include "StressTesting.h"
#include "HomePage.h"
#include "SumatraDialogs.h"
#include "SumatraProperties.h"
#include "TableOfContents.h"
#include "Tabs.h"
#include "Toolbar.h"
#include "Translations.h"
#include "uia/Provider.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "EditAnnotations.h"
#include "CommandPalette.h"
#include "Theme.h"
#include "Caption.h"
#include "DarkModeSubclass.h"

#include "utils/Log.h"

constexpr const char* kRestrictionsFileName = "sumatrapdfrestrict.ini";

constexpr const char* kSumatraWindowTitle = "SumatraPDF";
constexpr const WCHAR* kSumatraWindowTitleW = L"SumatraPDF";

// used to show it in debug, but is not very useful,
// so always disable
bool gShowFrameRate = false;

// in plugin mode, the window's frame isn't drawn and closing and
// fullscreen are disabled, so that SumatraPDF can be displayed
// embedded (e.g. in a web browser)
const char* gPluginURL = nullptr; // owned by Flags in WinMain

static Kind kNotifPersistentWarning = "persistentWarning";
static Kind kNotifZoom = "zoom";

HBITMAP gBitmapReloadingCue;
RenderCache* gRenderCache;
HCURSOR gCursorDrag;

// set after mouse shortcuts involving the Alt key (so that the menu bar isn't activated)
bool gSupressNextAltMenuTrigger = false;

bool gCrashOnOpen = false;

// in restricted mode, some features can be disabled (such as
// opening files, printing, following URLs), so that SumatraPDF
// can be used as a PDF reader on locked down systems
static Perm gPolicyRestrictions = Perm::RestrictedUse;
// only the listed protocols will be passed to the OS for
// opening in e.g. a browser or an email client (ignored,
// if gPolicyRestrictions doesn't contain Perm::DiskAccess)
static StrVec gAllowedLinkProtocols;
// only files of the listed perceived types will be opened
// externally by LinkHandler::LaunchFile (i.e. when clicking
// on an in-document link); examples: "audio", "video", ...
static StrVec gAllowedFileTypes;

static const char* gNextPrevDir = nullptr;
static StrVec gNextPrevDirCache; // cached files in gNextPrevDir

static void CloseDocumentInCurrentTab(MainWindow*, bool keepUIEnabled, bool deleteModel);
static void OnSidebarSplitterMove(Splitter::MoveEvent*);
static void OnFavSplitterMove(Splitter::MoveEvent*);

EBookUI* GetEBookUI() {
    return &gGlobalPrefs->eBookUI;
}

LoadArgs::LoadArgs(const char* origPath, MainWindow* win) {
    this->fileArgs = ParseFileArgs(origPath);
    const char* cleanPath = origPath;
    if (fileArgs) {
        cleanPath = fileArgs->cleanPath;
        logf("LoadArgs: origPath='%s', cleanPath='%s'\n", origPath, cleanPath);
    }
    char* path = path::NormalizeTemp(cleanPath);
    if (!str::EqI(path, cleanPath)) {
        logf("LoadArgs: cleanPath='%s', path='%s'\n", cleanPath, path);
    }
    this->fileName.SetCopy(path);
    this->win = win;
}

LoadArgs::~LoadArgs() {
    delete fileArgs;
}

const char* LoadArgs::FilePath() const {
    return fileName.Get();
}

void LoadArgs::SetFilePath(const char* path) {
    fileName.SetCopy(path);
}

LoadArgs* LoadArgs::Clone() {
    LoadArgs* res = new LoadArgs(fileName, win);
    res->tabState = this->tabState;
    return res;
}

void SetCurrentLang(const char* langCode) {
    if (!langCode) {
        return;
    }
    str::ReplaceWithCopy(&gGlobalPrefs->uiLanguage, langCode);
    trans::SetCurrentLangByCode(langCode);
}

#define DEFAULT_FILE_PERCEIVED_TYPES "audio,video,webpage"
#define DEFAULT_LINK_PROTOCOLS "http,https,mailto,file"

void InitializePolicies(bool restrict) {
    // default configuration should be to restrict everything
    ReportIf(gPolicyRestrictions != Perm::RestrictedUse);
    ReportIf(gAllowedLinkProtocols.Size() != 0 || gAllowedFileTypes.Size() != 0);

    // the -restrict command line flag overrides any sumatrapdfrestrict.ini configuration
    if (restrict) {
        return;
    }

    // allow to restrict SumatraPDF's functionality from an INI file in the
    // same directory as SumatraPDF.exe (see ../docs/sumatrapdfrestrict.ini)
    // (if the file isn't there, everything is allowed)
    TempStr restrictPath = GetPathInExeDirTemp(kRestrictionsFileName);
    if (!file::Exists(restrictPath)) {
        gPolicyRestrictions = Perm::All;
        Split(&gAllowedLinkProtocols, DEFAULT_LINK_PROTOCOLS, ",");
        Split(&gAllowedFileTypes, DEFAULT_FILE_PERCEIVED_TYPES, ",");
        return;
    }

    ByteSlice restrictData = file::ReadFile(restrictPath);
    SquareTreeNode* root = ParseSquareTree(restrictData);
    AutoDelete delRoot(root);
    SquareTreeNode* polsec = root ? root->GetChild("Policies") : nullptr;
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
        const char* name = seqstrings::IdxToStr(permNames, i);
        const char* val = polsec->GetValue(name);
        if (val && atoi(val) != 0) {
            gPolicyRestrictions = gPolicyRestrictions | perms[i];
        }
    }

    // determine the list of allowed link protocols and perceived file types
    if ((gPolicyRestrictions & Perm::DiskAccess) != (Perm)0) {
        const char* value = polsec->GetValue("LinkProtocols");
        if (value != nullptr) {
            char* protocols = str::DupTemp(value);
            str::ToLowerInPlace(protocols);
            str::TransCharsInPlace(protocols, " :;", ",,,");
            Split(&gAllowedLinkProtocols, protocols, ",", true);
        }
        value = polsec->GetValue("SafeFileTypes");
        if (value != nullptr) {
            char* protocols = str::DupTemp(value);
            str::ToLowerInPlace(protocols);
            str::TransCharsInPlace(protocols, " :;", ",,,");
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
bool SumatraLaunchBrowser(const char* url) {
    if (gPluginMode) {
        // pass the URI back to the browser
        ReportIf(gWindows.empty());
        if (gWindows.empty()) {
            return false;
        }
        HWND plugin = gWindows.at(0)->hwndFrame;
        HWND parent = GetAncestor(plugin, GA_PARENT);
        int urlLen = str::Leni(url);
        if (!parent || !url || (urlLen > 4096)) {
            return false;
        }
        COPYDATASTRUCT cds = {0x4C5255 /* URL */, (DWORD)urlLen + 1, (char*)url};
        return SendMessageW(parent, WM_COPYDATA, (WPARAM)plugin, (LPARAM)&cds);
    }

    if (!CanAccessDisk()) {
        return false;
    }

    // check if this URL's protocol is allowed
    AutoFreeStr protocol;
    if (!str::Parse(url, "%S:", &protocol)) {
        return false;
    }
    str::ToLowerInPlace(protocol);
    if (!gAllowedLinkProtocols.Contains(protocol)) {
        return false;
    }

    return LaunchFileShell(url, nullptr, "open");
}

bool DocIsSupportedFileType(Kind kind) {
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
bool OpenFileExternally(const char* path) {
    if (!CanAccessDisk() || gPluginMode) {
        return false;
    }

    // check if this file's perceived type is allowed
    char* ext = path::GetExtTemp(path);
    char* perceivedType = ReadRegStrTemp(HKEY_CLASSES_ROOT, ext, "PerceivedType");
    // since we allow following hyperlinks, also allow opening local webpages
    if (str::EndsWithI(path, ".htm") || str::EndsWithI(path, ".html") || str::EndsWithI(path, ".xhtml")) {
        perceivedType = str::DupTemp("webpage");
    }
    str::ToLowerInPlace(perceivedType);
    if (gAllowedFileTypes.Contains("*")) {
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

WindowTab* FindTabByFile(const char* file) {
    char* normFile = path::NormalizeTemp(file);

    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            const char* fp = tab->filePath;
            if (!fp || !path::IsSame(fp, normFile)) {
                continue;
            }
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
MainWindow* FindMainWindowByFile(const char* file, bool focusTab) {
    WindowTab* tab = nullptr;
    if (!file) {
        return nullptr;
    }
    if (gMostRecentlyOpenedDoc != nullptr) {
        auto lastPath = gMostRecentlyOpenedDoc->GetFilePath();
        if (path::IsSame(lastPath, file)) {
            tab = FindTabByController(gMostRecentlyOpenedDoc);
        }
    }
    if (!tab) {
        tab = FindTabByFile(file);
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
MainWindow* FindMainWindowBySyncFile(const char* path, bool focusTab) {
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

class HwndPasswordUI : public PasswordUI {
    HWND hwnd;
    size_t pwdIdx;

  public:
    explicit HwndPasswordUI(HWND hwnd) : hwnd(hwnd), pwdIdx(0) {
    }

    char* GetPassword(const char* fileName, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) override;
};

/* Get password for a given 'fileName', can be nullptr if user cancelled the
   dialog box or if the encryption key has been filled in instead.
   Caller needs to free() the result. */
char* HwndPasswordUI::GetPassword(const char* path, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) {
    FileState* fileFromHistory = gFileHistory.FindByName(path, nullptr);
    if (fileFromHistory && fileFromHistory->decryptionKey) {
        AutoFreeStr fingerprint = str::MemToHex(fileDigest, 16);
        *saveKey = str::StartsWith(fileFromHistory->decryptionKey, fingerprint.Get());
        if (*saveKey && str::HexToMem(fileFromHistory->decryptionKey + 32, decryptionKeyOut, 32)) {
            return nullptr;
        }
    }

    *saveKey = false;

    // try the list of default passwords before asking the user
    if (pwdIdx < gGlobalPrefs->defaultPasswords->size()) {
        char* pwd = gGlobalPrefs->defaultPasswords->at(pwdIdx++);
        return str::Dup(pwd);
    }

    if (IsStressTesting()) {
        return nullptr;
    }

    // extract the filename from the URL in plugin mode instead
    // of using the more confusing temporary filename
    if (gPluginMode) {
        TempStr urlName = url::GetFileNameTemp(gPluginURL);
        if (urlName) {
            path = urlName;
        }
    }
    path = path::GetBaseNameTemp(path);

    // check if the window is still valid as it might have been closed by now
    if (!IsWindow(hwnd)) {
        ReportIf(true);
        hwnd = GetForegroundWindow();
    }
    // make sure that the password dialog is visible
    HwndToForeground(hwnd);

    bool* rememberPwd = gGlobalPrefs->rememberOpenedFiles ? saveKey : nullptr;
    return Dialog_GetPassword(hwnd, path, rememberPwd);
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

    gGlobalPrefs->sidebarDx = WindowRect(win->hwndTocBox).dx;

    /* don't update the window's dimensions if it is maximized, mimimized or fullscreened */
    if (WIN_STATE_NORMAL == gGlobalPrefs->windowState && !IsIconic(win->hwndFrame) && !win->presentation) {
        // TODO: Use Get/SetWindowPlacement (otherwise we'd have to separately track
        //       the non-maximized dimensions for proper restoration)
        gGlobalPrefs->windowPos = WindowRect(win->hwndFrame);
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
    const char* fp = tab->filePath;
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

void MessageBoxWarning(HWND hwnd, const char* msg, const char* title) {
    uint type = MB_OK | MB_ICONEXCLAMATION | MbRtlReadingMaybe();
    if (!title) {
        title = _TRA("Warning");
    }
    MsgBox(hwnd, msg, title, type);
}

// updates the layout for a window to either left-to-right or right-to-left
// depending on the currently used language (see IsUIRtl)
static void UpdateWindowRtlLayout(MainWindow* win) {
    bool isRTL = IsUIRtl();
    bool wasRTL = (GetWindowLongW(win->hwndFrame, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
    if (wasRTL == isRTL) {
        return;
    }

    bool tocVisible = win->tocVisible;
    bool favVisible = gGlobalPrefs->showFavorites;
    if (tocVisible || favVisible) {
        SetSidebarVisibility(win, false, false);
    }

    // https://www.microsoft.com/middleeast/msdn/mirror.aspx
    HwndSetRtl(win->hwndFrame, isRTL);
    HwndSetRtl(win->hwndTocBox, isRTL);
    HwndSetRtl(win->tocLabelWithClose->hwnd, isRTL);
    HwndSetRtl(win->hwndFavBox, isRTL);
    HwndSetRtl(win->favLabelWithClose->hwnd, isRTL);
    HwndSetRtl(win->favTreeView->hwnd, isRTL);
    HwndSetRtl(win->hwndReBar, isRTL);
    HwndSetRtl(win->hwndToolbar, isRTL);
    HwndSetRtl(win->hwndFindEdit, isRTL);
    HwndSetRtl(win->hwndFindLabel, isRTL);
    HwndSetRtl(win->hwndTbInfoText, isRTL);
    HwndSetRtl(win->hwndPageLabel, isRTL);
    HwndSetRtl(win->hwndCaption, isRTL);
    SetCaptionButtonsRtl(win->caption, isRTL);

    // TODO: why isn't SetWindowPos(..., SWP_FRAMECHANGED) enough?
    SendMessageW(win->hwndFrame, WM_DWMCOMPOSITIONCHANGED, 0, 0);
    RelayoutCaption(win);
    // TODO: make tab bar RTL aware
    // HwndSetRtl(win->tabsCtrl->hwnd, isRTL);

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
}

void RebuildMenuBarForWindow(MainWindow* win) {
    HMENU oldMenu = win->menu;
    win->menu = BuildMenu(win);
    if (!win->presentation && !win->isFullScreen && !win->isMenuHidden) {
        SetMenu(win->hwndFrame, win->menu);
    }
    FreeMenuOwnerDrawInfoData(oldMenu);
    DestroyMenu(oldMenu);
}

static bool ShouldSaveThumbnail(FileState* ds) {
    // don't create thumbnails if we won't be needing them at all
    if (!HasPermission(Perm::SavePreferences)) {
        return false;
    }

    // don't create thumbnails for files that won't need them anytime soon
    Vec<FileState*> list;
    gFileHistory.GetFrequencyOrder(list);
    int idx = list.Find(ds);
    if (idx < 0 || kFileHistoryMaxFrequent * 2 <= idx) {
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
    explicit ControllerCallbackHandler(MainWindow* win) : win(win) {
    }
    ~ControllerCallbackHandler() override = default;

    void Repaint() override {
        ScheduleRepaint(win, 0);
    }
    void PageNoChanged(DocController* ctrl, int pageNo) override;
    void ZoomChanged(DocController* ctrl, float zoomVirtual) override;
    void UpdateScrollbars(Size canvas) override;
    void RequestRendering(int pageNo) override;
    void CleanUp(DisplayModel* dm) override;
    void RenderThumbnail(DisplayModel* dm, Size size, const OnBitmapRendered*) override;
    void GotoLink(IPageDestination* dest) override {
        win->linkHandler->GotoLink(dest);
    }
    void FocusFrame(bool always) override;
    void SaveDownload(const char* url, const ByteSlice&) override;
};

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

    gRenderCache->Render(dm, 1, 0, zoom, pageRect, *saveThumbnail);
}

struct CreateThumbnailData {
    char* filePath = nullptr;
    RenderedBitmap* bmp = nullptr;

    ~CreateThumbnailData() {
        str::Free(filePath);
    }
};

static void CreateThumbnailFinish(CreateThumbnailData* d) {
    char* path = d->filePath;
    if (d->bmp) {
        SetThumbnail(gFileHistory.FindByPath(path), d->bmp);
    }
    delete d;
}

static void CreateThumbnailOnBitmapRendered(CreateThumbnailData* d, RenderedBitmap* bmp) {
    d->bmp = bmp;
    auto fn = MkFunc0<CreateThumbnailData>(CreateThumbnailFinish, d);
    uitask::PostOptimized(fn, "TaskSetThumbnail");
}

static void CreateThumbnailForFile(MainWindow* win, FileState* ds) {
    if (!ShouldSaveThumbnail(ds)) {
        return;
    }

    ReportIf(!win->IsDocLoaded());
    if (!win->IsDocLoaded()) {
        return;
    }

    // don't create thumbnails for password protected documents
    // (unless we're also remembering the decryption key anyway)
    auto* model = win->AsFixed();
    if (model) {
        auto* engine = model->GetEngine();
        bool withPwd = engine->IsPasswordProtected();
        AutoFreeStr decrKey = engine->GetDecryptionKey();
        if (withPwd && !decrKey) {
            RemoveThumbnail(ds);
            return;
        }
    }

    auto size = Size(kThumbnailDx, kThumbnailDy);
    auto d = new CreateThumbnailData{};
    d->filePath = str::Dup(win->ctrl->GetFilePath());
    auto fn = NewFunc1(CreateThumbnailOnBitmapRendered, d);
    win->ctrl->CreateThumbnail(size, fn);
}

/* Send the request to render a given page to a rendering thread */
void ControllerCallbackHandler::RequestRendering(int pageNo) {
    ReportIf(!win->AsFixed());
    if (!win->AsFixed()) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    // don't render any plain images on the rendering thread,
    // they'll be rendered directly in DrawDocument during
    // WM_PAINT on the UI thread
    if (dm->ShouldCacheRendering(pageNo)) {
        gRenderCache->RequestRendering(dm, pageNo);
    }
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

void ControllerCallbackHandler::SaveDownload(const char* url, const ByteSlice& data) {
    TempStr path = url::GetFileNameTemp(url);
    // LinkSaver linkSaver(win->CurrentTab(), win->hwndFrame, fileName);
    SaveDataToFile(win->hwndFrame, path, data);
}

void ControllerCallbackHandler::UpdateScrollbars(Size canvas) {
    ReportIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;

    Size viewPort = dm->GetViewPort().Size();

    if (viewPort.dx >= canvas.dx) {
        si.nPos = 0;
        si.nMin = 0;
        si.nMax = 99;
        si.nPage = 100;
    } else {
        si.nPos = dm->GetViewPort().x;
        si.nMin = 0;
        si.nMax = canvas.dx - 1;
        si.nPage = viewPort.dx;
    }
    ShowScrollBar(win->hwndCanvas, SB_HORZ, viewPort.dx < canvas.dx);
    SetScrollInfo(win->hwndCanvas, SB_HORZ, &si, TRUE);

    if (viewPort.dy >= canvas.dy) {
        si.nPos = 0;
        si.nMin = 0;
        si.nMax = 99;
        si.nPage = 100;
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
    ShowScrollBar(win->hwndCanvas, SB_VERT, viewPort.dy < canvas.dy);
    SetScrollInfo(win->hwndCanvas, SB_VERT, &si, TRUE);
}

static TempStr BuildZoomString(float zoomLevel) {
    TempStr zoomLevelStr = ZoomLevelStr(zoomLevel);
    const char* zoomStr = _TRA("Zoom");
    return str::FormatTemp("%s: %s", zoomStr, zoomLevelStr);
}

static void UpdatePageInfoHelper(DocController* ctrl, NotificationWnd* wnd, int pageNo) {
    if (!ctrl->ValidPageNo(pageNo)) {
        pageNo = ctrl->CurrentPageNo();
    }
    int nPages = ctrl->PageCount();
    TempStr pageInfo = str::FormatTemp("%s %d / %d", _TRA("Page:"), pageNo, nPages);
    if (ctrl->HasPageLabels()) {
        TempStr label = ctrl->GetPageLabeTemp(pageNo);
        pageInfo = str::FormatTemp("%s %s (%d / %d)", _TRA("Page:"), label, pageNo, nPages);
    }
    float zoomLevel = ctrl->GetZoomVirtual();
    auto zoomStr = BuildZoomString(zoomLevel);
    pageInfo = str::JoinTemp(pageInfo, " ", zoomStr);
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
        HwndSetText(win->hwndPageEdit, label);
        ToolbarUpdateStateForWindow(win, false);
        if (win->ctrl->HasPageLabels()) {
            UpdateToolbarPageText(win, win->ctrl->PageCount(), true);
        }
    }
    if (pageNo == win->currPageNo) {
        return;
    }

    UpdateTocSelection(win, pageNo);
    win->currPageNo = pageNo;

    NotificationWnd* wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
    if (!wnd) {
        return;
    }
    UpdatePageInfoHelper(win->ctrl, wnd, pageNo);
}

// TODO: remove when we figure out why this ctrl->GetFilePath() is not always same as path
static NO_INLINE void VerifyController(DocController* ctrl, const char* path) {
    if (!ctrl) {
        return;
    }
    const char* ctrlFilePath = ctrl->GetFilePath();
    if (str::Eq(ctrlFilePath, path)) {
        return;
    }
    const char* s1 = ctrlFilePath ? ctrlFilePath : "<null>";
    const char* s2 = path ? path : "<null>";
    logf("VerifyController: ctrl->FilePath: '%s', filePath: '%s'\n", s1, s2);
    ReportIf(true);
}

static DocController* CreateControllerForChm(const char* path, PasswordUI* pwdUI, MainWindow* win) {
    Kind kind = GuessFileType(path, true);

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
    // if CLSID_WebBrowser isn't available, fall back on ChmEngine
    DocController* ctrl = nullptr;
    if (!chmModel->SetParentHwnd(win->hwndCanvas)) {
        delete chmModel;
        EngineBase* engine = CreateEngineFromFile(path, pwdUI, true);
        if (!engine) {
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

DocController* CreateControllerForEngineOrFile(EngineBase* engine, const char* path, PasswordUI* pwdUI,
                                               MainWindow* win) {
    // TODO: move this to MainWindow constructor
    if (!win->cbHandler) {
        win->cbHandler = new ControllerCallbackHandler(win);
    }

    auto timeStart = TimeGet();
    bool chmInFixedUI = gGlobalPrefs->chmUI.useFixedPageUI;
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
    const char* titlePath = tab->filePath;
    if (!gGlobalPrefs->fullPathInTitle) {
        titlePath = path::GetBaseNameTemp(titlePath);
    }

    TempStr docTitle = (TempStr) "";
    if (tab->ctrl) {
        TempStr title = tab->ctrl->GetPropertyTemp(kPropTitle);
        if (title != nullptr) {
            str::NormalizeWSInPlace(title);
            docTitle = str::DupTemp(title);
            if (!str::IsEmpty(title)) {
                docTitle = str::FormatTemp("- [%s] ", title);
            }
        }
    }

    TempStr s = nullptr;
    if (!IsUIRtl()) {
        s = str::FormatTemp("%s %s- %s", titlePath, docTitle, kSumatraWindowTitle);
    } else {
        // explicitly revert the title, so that filenames aren't garbled
        s = str::FormatTemp("%s %s- %s", kSumatraWindowTitle, docTitle, titlePath);
    }
    if (needRefresh && tab->ctrl) {
        // TODO: this isn't visible when tabs are used
        s = str::FormatTemp(_TRA("[Changes detected; refreshing] %s"), tab->frameTitle);
    }
    str::ReplaceWithCopy(&tab->frameTitle, s);
}

static void UpdateUiForCurrentTab(MainWindow* win) {
    // hide the scrollbars before any other relayouting (for assertion in MainWindow::GetViewPortSize)
    if (!win->AsFixed()) {
        ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);
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

    FindToggleMatchCase(win);
    UpdateFindbox(win);

    HwndSetText(win->hwndFrame, win->CurrentTab()->frameTitle);

    // TODO: match either the toolbar (if shown) or background
    HwndScheduleRepaint(win->tabsCtrl->hwnd); // TODO: was RepaintNow() ?

    bool onlyNumbers = !win->ctrl || !win->ctrl->HasPageLabels();
    SetWindowStyle(win->hwndPageEdit, ES_NUMBER, onlyNumbers);
}

static bool showTocByDefault(const char* path) {
    if (!gGlobalPrefs->showToc) {
        return false;
    }
    // we don't want to show toc by default for comic book files
    Kind kind = GuessFileTypeFromName(path);
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
    WindowTab* tab = win->CurrentTab();
    ReportIf(!tab);

    // Never load settings from a preexisting state if the user doesn't wish to
    // (unless we're just refreshing the document, i.e. only if state && !state->useDefaultState)
    if (!fs && gGlobalPrefs->rememberStatePerDocument) {
        const char* fn = args->FilePath();
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
    const char* path = args->FilePath();
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
    }

    AbortFinding(args->win, true);

    DocController* prevCtrl = win->ctrl;
    tab->ctrl = ctrl;
    win->ctrl = tab->ctrl;

    // ToC items might hold a reference to an Engine, so make sure to
    // delete them before destroying the whole DisplayModel
    // (same for linkOnLastButtonDown)
    ClearTocBox(win);
    ClearMouseState(win);

    // TODO: this crashes with new tabs
    // ReportIf(win->IsAboutWindow() || win->IsDocLoaded() != (win->ctrl != nullptr));
    // TODO: https://code.google.com/p/sumatrapdf/issues/detail?id=1570
    if (win->ctrl) {
        DisplayModel* dm = win->AsFixed();
        if (dm) {
            int dpi = gGlobalPrefs->customScreenDPI;
            if (dpi == 0) {
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
        } else if (win->AsChm()) {
            win->AsChm()->SetParentHwnd(win->hwndCanvas);
            win->ctrl->SetDisplayMode(displayMode);
            ss.page = limitValue(ss.page, 1, win->ctrl->PageCount());
            win->ctrl->GoToPage(ss.page, false);
        } else {
            ReportIf(true);
        }
    } else {
        fs = nullptr;
    }
    delete prevCtrl;

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
    // caused by showing/hiding UI elements happends
    if (win->AsFixed()) {
        win->AsFixed()->Relayout(zoomVirtual, rotation);
    } else if (win && win->ctrl && win->IsDocLoaded()) {
        win->ctrl->SetZoomVirtual(zoomVirtual, nullptr);
    }

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
        int res = Synchronizer::Create(path, win->AsFixed()->GetEngine(), &win->AsFixed()->pdfSync);
        // expose SyncTeX in the UI
        if (PDFSYNCERR_SUCCESS == res) {
            gGlobalPrefs->enableTeXEnhancements = true;
        }
    }

    bool shouldPlace = args->isNewWindow || args->placeWindow && fs;
    if (args->noPlaceWindow) {
        shouldPlace = false;
    }
    if (shouldPlace) {
        if (args->isNewWindow && fs && !fs->windowPos.IsEmpty()) {
            // Make sure it doesn't have a position like outside of the screen etc.
            Rect rect = ShiftRectToWorkArea(fs->windowPos);
            // This shouldn't happen until !win.IsAboutWindow(), so that we don't
            // accidentally update gGlobalState with this window's dimensions
            MoveWindow(win->hwndFrame, rect);
        }
        if (args->showWin) {
            ShowWindow(win->hwndFrame, showType);
        }
        if (win) {
            UpdateWindow(win->hwndFrame);
        }
    }

    // if the window isn't shown and win.canvasRc is still empty, zoom
    // has not been determined yet
    // cf. https://code.google.com/p/sumatrapdf/issues/detail?id=2541
    // ReportIf(win->IsDocLoaded() && args->showWin && win->canvasRc.IsEmpty() && !win->AsChm());

    SetSidebarVisibility(win, showToc, gGlobalPrefs->showFavorites);
    // restore scroll state after the canvas size has been restored
    if ((args->showWin || ss.page != 1) && win->AsFixed()) {
        win->AsFixed()->SetScrollState(ss);
    }

    win->RedrawAll(true);
    TabsOnChangedDoc(win);

    if (!win->IsDocLoaded()) {
        return;
    }

    TempStr unsupported = win->ctrl->GetPropertyTemp(kPropUnsupportedFeatures);
    if (unsupported) {
        const char* s = _TRA("This document uses unsupported features (%s) and might not render properly");
        TempStr msg = str::FormatTemp(s, unsupported);
        NotificationCreateArgs nargs;
        nargs.hwndParent = win->hwndCanvas;
        nargs.warning = true;
        nargs.timeoutMs = 0;
        nargs.groupId = kNotifPersistentWarning;
        nargs.msg = msg;
        ShowNotification(nargs);
    }

    // This should only happen after everything else is ready
    if ((args->isNewWindow || args->placeWindow) && args->showWin && showAsFullScreen) {
        EnterFullScreen(win);
    }
    if (!args->isNewWindow && win->presentation && win->ctrl) {
        win->ctrl->SetInPresentation(true);
    }
}

void ReloadDocument(MainWindow* win, bool autoRefresh) {
    WindowTab* tab = win->CurrentTab();

    // we can't reload while having annotations window open because
    // that invalidates the mupdf objects that we hold in editAnnotsWindow
    // TODO: a better approach would be to have a callback that editAnnotsWindow
    // would register for and re-do its state
    if (!tab || tab->editAnnotsWindow) {
        return;
    }
    // TODO: maybe should ensure it never is called for IsAboutTab() ?
    // This only happens if gLazyLoading is true
    if (tab->IsAboutTab()) {
        return;
    }

    tab->selectedAnnotation = nullptr;

    if (!tab->IsDocLoaded()) {
        if (!autoRefresh) {
            // TODO: seen a crash
            if (str::IsEmpty(tab->filePath)) {
                logf("tab->filePath is empty\n");
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
    const char* path = tab->filePath;
    logfa("ReloadDocument: %s, auto refresh: %d\n", path, (int)autoRefresh);
    DocController* ctrl = CreateControllerForEngineOrFile(nullptr, path, &pwdUI, win);
    // We don't allow PDF-repair if it is an autorefresh because
    // a refresh event can occur before the file is finished being written,
    // in which case the repair could fail. Instead, if the file is broken,
    // we postpone the reload until the next autorefresh event
    if (!ctrl && autoRefresh) {
        SetFrameTitleForTab(tab, true);
        HwndSetText(win->hwndFrame, tab->frameTitle);
        return;
    }

    FileState* fs = NewDisplayState(path);
    tab->ctrl->GetDisplayState(fs);
    UpdateDisplayStateWindowRect(win, fs);
    UpdateSidebarDisplayState(tab, fs);
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
        DeleteDisplayState(fs);
        return;
    }

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
        AutoFreeStr decryptionKey = tab->AsFixed()->GetEngine()->GetDecryptionKey();
        if (decryptionKey) {
            FileState* fs2 = gFileHistory.FindByName(fs->filePath, nullptr);
            if (fs2 && !str::Eq(fs2->decryptionKey, decryptionKey)) {
                free(fs2->decryptionKey);
                fs2->decryptionKey = decryptionKey.Release();
            }
        }
    }

    DeleteDisplayState(fs);
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

    if (win->tocVisible) {
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

static MainWindow* CreateMainWindow() {
    Rect windowPos = gGlobalPrefs->windowPos;
    if (!windowPos.IsEmpty()) {
        EnsureAreaVisibility(windowPos);
    } else {
        windowPos = GetDefaultWindowPos();
    }
    // we don't want the windows to overlap so shift each window by a bit
    int nShift = (int)gWindows.size();
    windowPos.x += (nShift * 15); // TODO: DPI scale

    const WCHAR* clsName = FRAME_CLASS_NAME;
    const WCHAR* title = kSumatraWindowTitleW;
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    int x = windowPos.x;
    int y = windowPos.y;
    int dx = windowPos.dx;
    int dy = windowPos.dy;
    HINSTANCE h = GetModuleHandle(nullptr);
    HWND hwndFrame = CreateWindowExW(0, clsName, title, style, x, y, dx, dy, nullptr, nullptr, h, nullptr);
    if (!hwndFrame) {
        return nullptr;
    }

    ReportIf(nullptr != FindMainWindowByHwnd(hwndFrame));
    MainWindow* win = new MainWindow(hwndFrame);

    // don't add a WS_EX_STATICEDGE so that the scrollbars touch the
    // screen's edge when maximized (cf. Fitts' law) and there are
    // no additional adjustments needed when (un)maximizing
    clsName = CANVAS_CLASS_NAME;
    style = WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_CLIPCHILDREN;
    /* position and size determined in OnSize */
    Rect rcFrame = ClientRect(hwndFrame);
    win->hwndCanvas =
        CreateWindowExW(0, clsName, nullptr, style, 0, 0, rcFrame.dx, rcFrame.dy, hwndFrame, nullptr, h, nullptr);
    if (!win->hwndCanvas) {
        delete win;
        return nullptr;
    }

    if (gShowFrameRate) {
        win->frameRateWnd = new FrameRateWnd();
        win->frameRateWnd->Create(win->hwndCanvas);
    }

    // hide scrollbars to avoid showing/hiding on empty window
    ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);

    ReportIf(win->menu);
    win->menu = BuildMenu(win);
    win->isMenuHidden = !gGlobalPrefs->showMenubar;
    if (!win->isMenuHidden) {
        SetMenu(win->hwndFrame, win->menu);
    }
    win->brControlBgColor = CreateSolidBrush(ThemeControlBackgroundColor());

    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    win->infotip = new Tooltip();
    Tooltip::CreateArgs args;
    args.parent = win->hwndCanvas;
    win->infotip->Create(args);

    CreateCaption(win);
    CreateTabbar(win);
    CreateToolbar(win);
    CreateSidebar(win);
    UpdateFindbox(win);
    if (CanAccessDisk() && !gPluginMode) {
        DragAcceptFiles(win->hwndCanvas, TRUE);
    }

    gWindows.Append(win);
    // needed for RTL languages
    UpdateWindowRtlLayout(win);
    UpdateToolbarSidebarText(win);

    if (touch::SupportsGestures()) {
        GESTURECONFIG gc = {0, GC_ALLGESTURES, 0};
        touch::SetGestureConfig(win->hwndCanvas, 0, 1, &gc, sizeof(GESTURECONFIG));
    }

    SetTabsInTitlebar(win, gGlobalPrefs->useTabs);
    // TODO: this is hackish. in general we should divorce
    // layout re-calculations from MainWindow and creation of windows
    win->UpdateCanvasSize();
    if (gUseDarkModeLib) {
        DarkMode::setDarkTitleBar(win->hwndFrame);
        DarkMode::setChildCtrlsSubclassAndTheme(win->hwndFrame);
        DarkMode::removeTabCtrlSubclass(win->tabsCtrl->hwnd);
        DarkMode::setDarkScrollBar(win->hwndCanvas);
        DarkMode::setDarkTooltips(win->infotip->hwnd);
    }
    return win;
}

MainWindow* CreateAndShowMainWindow(SessionData* data) {
    int windowState = gGlobalPrefs->windowState;
    MainWindow* win = CreateMainWindow();
    if (!win) {
        return nullptr;
    }
    // CreateMainWindow shouldn't change the windowState value
    ReportIf(windowState != gGlobalPrefs->windowState);

    if (data) {
        windowState = data->windowState;
        Rect rect = ShiftRectToWorkArea(data->windowPos);
        MoveWindow(win->hwndFrame, rect);
        // TODO: also restore data->sidebarDx
    }

    if (WIN_STATE_FULLSCREEN == windowState || WIN_STATE_MAXIMIZED == windowState) {
        ShowWindow(win->hwndFrame, SW_MAXIMIZE);
    } else {
        ShowWindow(win->hwndFrame, SW_SHOW);
    }
    UpdateWindow(win->hwndFrame);

    SetSidebarVisibility(win, false, gGlobalPrefs->showFavorites);
    ToolbarUpdateStateForWindow(win, true);

    if (WIN_STATE_FULLSCREEN == windowState) {
        EnterFullScreen(win);
    }
    return win;
}

void DeleteMainWindow(MainWindow* win) {
    int winIdx = gWindows.Remove(win);

    int nWindowsLeft = gWindows.Size();
    logf("DeleteMainWindow: win: 0x%p, hwndFrame: 0x%p, hwndCanvas: 0x%p, winIdx : %d, nWindowsLeft: %d\n", win,
         win->hwndFrame, win->hwndCanvas, winIdx, nWindowsLeft);
    if (winIdx < 0) {
        logf("  not deleting because not in gWindows, probably already deleted\n");
        return;
    }

    DeletePropertiesWindow(win->hwndFrame);
    ImageList_Destroy((HIMAGELIST)SendMessageW(win->hwndToolbar, TB_GETIMAGELIST, 0, 0));
    DragAcceptFiles(win->hwndCanvas, FALSE);

    ReportIf(win->findThread && WaitForSingleObject(win->findThread, 0) == WAIT_TIMEOUT);
    ReportIf(win->printThread && WaitForSingleObject(win->printThread, 0) == WAIT_TIMEOUT);

    if (win->uiaProvider) {
        // tell UIA to release all objects cached in its store
        UiaReturnRawElementProvider(win->hwndCanvas, 0, 0, nullptr);
    }

    delete win;
}

void UpdateAfterThemeChange() {
    for (auto win : gWindows) {
        DeleteObject(win->brControlBgColor);
        win->brControlBgColor = CreateSolidBrush(ThemeControlBackgroundColor());

        UpdateControlsColors(win);
        RebuildMenuBarForWindow(win);
        CaptionUpdateUI(win, win->caption);
        // TODO: probably leaking toolbar image list
        UpdateToolbarAfterThemeChange(win);
        if (gUseDarkModeLib) {
            DarkMode::setDarkTitleBar(win->hwndFrame);
            DarkMode::setChildCtrlsTheme(win->hwndFrame);
            DarkMode::setDarkScrollBar(win->hwndCanvas);
            DarkMode::setDarkTooltips(win->infotip->hwnd);
        }
        // TODO: this only rerenders canvas, not frame, even with
        // includingNonClientArea == true.
        MainWindowRerender(win, true);
        uint flags = RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN;
        RedrawWindow(win->hwndCaption, nullptr, nullptr, flags);
        RedrawWindow(win->hwndFrame, nullptr, nullptr, flags);
    }
    UpdateDocumentColors();
}

static void RenameFileInHistory(const char* oldPath, const char* newPath) {
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
        if (fs->favorites->size() > 0) {
            UpdateFavoritesTreeForAllWindows();
        }
        DeleteDisplayState(fs);
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
    const char* path = args->FilePath();
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
    char* adjPath = str::DupTemp(path);
    if (AdjustVariableDriveLetter(adjPath)) {
        RenameFileInHistory(path, adjPath);
        args->SetFilePath(adjPath);
    }
    return false;
}

static void LoadDocumentMarkNotExist(MainWindow* win, const char* path, bool noSavePrefs) {
    ShowWindow(win->hwndFrame, SW_SHOW);

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
    if (1 == gWindows.size() && gWindows.at(0)->IsCurrentTabAbout()) {
        gWindows.at(0)->RedrawAll(true);
    }
}

static void ShowFileNotFound(MainWindow* win, const char* path, bool noSavePrefs) {
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.warning = true;
    nargs.msg = str::FormatTemp(_TRA("File %s not found"), path);
    ShowNotification(nargs);
    LoadDocumentMarkNotExist(win, path, noSavePrefs);
}

void ShowErrorLoadingNotification(MainWindow* win, const char* path, bool noSavePrefs) {
    // TODO: same message as in Canvas.cpp to not introduce
    // new translation. Find a better message e.g. why failed.
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.msg = str::FormatTemp(_TRA("Error loading %s"), path);
    nargs.warning = true;
    nargs.timeoutMs = 1000 * 5;
    ShowNotification(nargs);
    LoadDocumentMarkNotExist(win, path, noSavePrefs);
}

extern void SetTabState(WindowTab* tab, TabState* state);

MainWindow* LoadDocumentFinish(LoadArgs* args) {
    MainWindow* win = args->win;
    const char* fullPath = args->FilePath();

    bool openNewTab = gGlobalPrefs->useTabs && !args->forceReuse;
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
        win->currentTabTemp = AddTabToWindow(win, tab);

        // logf("LoadDocument: !forceReuse, created win->CurrentTab() at 0x%p\n", win->CurrentTab());
    } else {
        win->CurrentTab()->SetFilePath(fullPath);
#if 0
        auto path = ToUtf8Temp(fullPath);
        logf("LoadDocument: forceReuse, set win->CurrentTab() (0x%p) filePath to '%s'\n", win->CurrentTab(), path.Get());
#endif
    }

    // TODO: stop remembering/restoring window positions when using tabs?
    args->placeWindow = !gGlobalPrefs->useTabs;
    bool lazyLoad = args->lazyLoad;
    if (!lazyLoad) {
        ReplaceDocumentInCurrentTab(args, args->ctrl, nullptr);
    }

    if (gPluginMode) {
        // hide the menu for embedded documents opened from the plugin
        SetMenu(win->hwndFrame, nullptr);
        return win;
    }

    auto currTab = win->CurrentTab();
    const char* path = currTab->filePath;
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
        currTab->watcher = FileWatcherSubscribe(path, fn);
    }

    if (gGlobalPrefs->rememberOpenedFiles) {
        ReportIf(!str::Eq(fullPath, path));
        FileState* ds = gFileHistory.MarkFileLoaded(fullPath);
        if (!lazyLoad && gGlobalPrefs->showStartPage) {
            CreateThumbnailForFile(win, ds);
        }
        // TODO: this seems to save the state of file that we just opened
        // add a way to skip saving currTab?
        if (!args->noSavePrefs) {
            SaveSettings();
        }
    }

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    if (CanAccessDisk() && !gPluginMode && !IsStressTesting()) {
        AddPathToRecentDocs(fullPath);
    }

    return win;
}

static NotificationWnd* ShowLoadingNotif(MainWindow* win, const char* path) {
    NotificationCreateArgs nargs;
    nargs.hwndParent = win->hwndCanvas;
    nargs.groupId = path;
    nargs.msg = str::FormatTemp(_TRA("Loading %s ..."), path);
    return ShowNotification(nargs);
}

static MainWindow* MaybeCreateWindowForFileLoad(LoadArgs* args) {
    MainWindow* win = args->win;
    bool openNewTab = gGlobalPrefs->useTabs && !args->forceReuse;
    if (openNewTab && !args->win) {
        // modify the args so that we always reuse the same window
        // TODO: enable the tab bar if tabs haven't been initialized
        if (!gWindows.empty()) {
            win = gWindows.Last();
            args->win = win;
            args->isNewWindow = false;
        }
    }

    if (!win && 1 == gWindows.size() && gWindows.at(0)->IsCurrentTabAbout()) {
        win = gWindows.at(0);
        args->win = win;
        args->isNewWindow = false;
    } else if (!win || !openNewTab && !args->forceReuse && win->IsDocLoaded()) {
        MainWindow* currWin = win;
        win = CreateAndShowMainWindow(nullptr);
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
    ~LoadDocumentAsyncData() {
        delete args;
    }
};

static void LoadDocumentAsyncFinish(LoadDocumentAsyncData* d) {
    AutoDelete delData(d);

    auto args = d->args;
    RemoveNotification(d->wndNotif);
    MainWindow* win = args->win;
    const char* path = args->FilePath();
    if (!args->ctrl) {
        ShowErrorLoadingNotification(win, path, args->noSavePrefs);
        return;
    }
    args->activateExisting = false;
    LoadDocumentFinish(args);
}

static void LoadDocumentAsync(LoadDocumentAsyncData* d) {
    auto args = d->args;
    gDangerousThreadCount.Inc();
    DocController* ctrl = nullptr;
    MainWindow* win = args->win;
    HwndPasswordUI pwdUI(win->hwndFrame ? win->hwndFrame : nullptr);
    const char* path = args->FilePath();
    EngineBase* engine = args->engine;
    args->ctrl = CreateControllerForEngineOrFile(engine, path, &pwdUI, win);
    if (args->ctrl && gIsDebugBuild) {
        //::Sleep(5000);
    }

    auto fn = MkFunc0<LoadDocumentAsyncData>(LoadDocumentAsyncFinish, d);
    uitask::Post(fn, "TaskLoadDocumentAsyncFinish");
    gDangerousThreadCount.Dec();
}

void StartLoadDocument(LoadArgs* argsIn) {
    if (gCrashOnOpen) {
        log("LoadDocumentAsync: about to call CrashMe()\n");
        CrashMe();
    }

    MainWindow* win = argsIn->win;
    bool failEarly = AdjustPathForMaybeMovedFile(argsIn);
    const char* path = argsIn->FilePath();
    if (failEarly) {
        ShowFileNotFound(win, path, argsIn->noSavePrefs);
        return;
    }

    if (argsIn->activateExisting) {
        MainWindow* existing = FindMainWindowByFile(path, true);
        if (existing) {
            existing->Focus();
            return;
        }
    }

    win = MaybeCreateWindowForFileLoad(argsIn);
    if (!win) {
        return;
    }

    auto wndNotif = ShowLoadingNotif(win, path);
    LoadArgs* args = argsIn->Clone();

    // when using mshtml to display CHM files, we can't load in a thread
    // TODO: that's because we create web control on a thread which
    // violates threading rules and that happens as part of CreateControllerForEngineOrFile()
    // we could probably delay creating web control but that's more complicated
    if (!gGlobalPrefs->chmUI.useFixedPageUI) {
        Kind kind = GuessFileTypeFromName(path);
        bool isChm = ChmModel::IsSupportedFileType(kind);
        if (isChm) {
            // TODO: repeating the code below
            DocController* ctrl = nullptr;
            HwndPasswordUI pwdUI(win->hwndFrame ? win->hwndFrame : nullptr);
            EngineBase* engine = args->engine;
            args->ctrl = CreateControllerForEngineOrFile(engine, path, &pwdUI, win);
            RemoveNotification(wndNotif);
            if (!args->ctrl) {
                ShowErrorLoadingNotification(win, path, args->noSavePrefs);
                delete args;
                return;
            }
            args->activateExisting = false;
            LoadDocumentFinish(args);
            delete args;
            return;
        }
    }

    auto data = new LoadDocumentAsyncData;
    data->wndNotif = wndNotif;
    data->args = args;
    auto fn = MkFunc0<LoadDocumentAsyncData>(LoadDocumentAsync, data);
    RunAsync(fn, "LoadDocumentThread");
}

// remember which files failed to open so that a failure to
// open a file doesn't block next/prev file in
static StrVec gFilesFailedToOpen;

MainWindow* LoadDocument(LoadArgs* args) {
    if (gCrashOnOpen) {
        log("LoadDocument: about to call CrashMe()\n");
        CrashMe();
    }

    const char* path = args->FilePath();
    if (args->activateExisting) {
        MainWindow* existing = FindMainWindowByFile(path, true);
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
        ShowFileNotFound(win, path, args->noSavePrefs);
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
            ShowErrorLoadingNotification(win, path, args->noSavePrefs);
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
        args.msg = str::FormatTemp(_TRA("Please wait - loading..."));
        args.warning = true;
        ShowNotification(args);
        ShowWindow(win->hwndFrame, SW_SHOW);
        // display the notification ASAP
        win->RedrawAll(true);
    }
    CloseDocumentInCurrentTab(win, true, false);

    win->currentTabTemp = tab;
    win->ctrl = tab->ctrl;

    if (win->AsChm()) {
        win->AsChm()->SetParentHwnd(win->hwndCanvas);
    } else if (win->AsFixed() && win->uiaProvider) {
        // tell UI Automation about content change
        win->uiaProvider->OnDocumentLoad(win->AsFixed());
    }

    UpdateUiForCurrentTab(win);

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
    } else if (win->AsChm()) {
        win->ctrl->GoToPage(win->ctrl->CurrentPageNo(), false);
    }
    tab->canvasRc = win->canvasRc;

    win->showSelection = tab->selectionOnPage != nullptr;
    if (win->uiaProvider) {
        win->uiaProvider->OnSelectionChanged();
    }

    HwndSetFocus(win->hwndFrame);
    if (!tab->IsAboutTab()) {
        if (gGlobalPrefs->lazyLoading && !tab->ctrl) {
            ReloadDocument(win, false);
        } else {
            if (tab->reloadOnFocus) {
                tab->reloadOnFocus = false;
                ReloadDocument(win, true);
            }
        }
    }
    win->RedrawAll(true);
}

enum class MeasurementUnit { pt, mm, in };

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
    const char* unitName = "in";
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

    char* xPos = str::FormatFloatWithThousandSepTemp((double)pt.x * (double)factor);
    char* yPos = str::FormatFloatWithThousandSepTemp((double)pt.y * (double)factor);
    if (unit != MeasurementUnit::in) {
        // use similar precision for all units
        int xLen = str::Leni(xPos);
        if (str::IsDigit(xPos[xLen - 2])) {
            xPos[xLen - 1] = '\0';
        }
        int yLen = str::Leni(yPos);
        if (str::IsDigit(yPos[yLen - 2])) {
            yPos[yLen - 1] = '\0';
        }
    }
    return fmt::FormatTemp("%s x %s %s", xPos, yPos, unitName);
}

static auto cursorPosUnit = MeasurementUnit::pt;
void UpdateCursorPositionHelper(MainWindow* win, Point pos, NotificationWnd* wnd) {
    ReportIf(!win->AsFixed());
    EngineBase* engine = win->AsFixed()->GetEngine();
    PointF pt = win->AsFixed()->CvtFromScreen(pos);
    char* posStr = FormatCursorPositionTemp(engine, pt, cursorPosUnit);
    char* selStr = nullptr;
    if (!win->selectionMeasure.IsEmpty()) {
        pt = PointF(win->selectionMeasure.dx, win->selectionMeasure.dy);
        selStr = FormatCursorPositionTemp(engine, pt, cursorPosUnit);
    }

    char* posInfo = fmt::FormatTemp("%s %s", _TRA("Cursor position:"), posStr);
    if (selStr) {
        posInfo = fmt::FormatTemp("%s - %s %s", posInfo, _TRA("Selection:"), selStr);
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
        MainWindowRerender(win);
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
    COLORREF text = ThemeDocumentColors(bg);
    // logfa("retrieved doc colors in UpdateDocumentColors: 0x%x 0x%x\n", text, bg);

    if ((text == gRenderCache->textColor) && (bg == gRenderCache->backgroundColor)) {
        return; // colors didn't change
    }

    gRenderCache->textColor = text;
    gRenderCache->backgroundColor = bg;
    RerenderEverything();
}

void UpdateFixedPageScrollbarsVisibility() {
    bool hideScrollbars = gGlobalPrefs->fixedPageUI.hideScrollbars;
    bool scrollbarsVisible = false; // assume no scrollbars by default

    // iterate through each fixed page window to check whether scrollbars are shown
    for (auto* win : gWindows) {
        if (auto* pdfWin = win->AsFixed()) {
            scrollbarsVisible = pdfWin->IsVScrollbarVisible() || pdfWin->IsHScrollbarVisible();
            if (scrollbarsVisible) {
                break;
            }
        }
    }

    bool rerenderRequired = (hideScrollbars && scrollbarsVisible) || (!hideScrollbars && !scrollbarsVisible);
    if (!rerenderRequired) {
        return;
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

// closes a document inside a MainWindow and optionally turns it into
// about window (set keepUIEnabled if a new document will be loaded
// into the tab right afterwards and ReplaceDocumentInCurrentTab would revert
// the UI disabling afterwards anyway)
static void CloseDocumentInCurrentTab(MainWindow* win, bool keepUIEnabled, bool deleteModel) {
    bool wasntFixed = !win->AsFixed();
    if (win->AsChm()) {
        win->AsChm()->RemoveParentHwnd();
    }
    ClearTocBox(win);
    AbortFinding(win, true);

    win->linkOnLastButtonDown = nullptr;
    win->annotationUnderCursor = nullptr;

    win->fwdSearchMark.show = false;
    if (win->uiaProvider) {
        win->uiaProvider->OnDocumentUnload();
    }
    win->ctrl = nullptr;
    WindowTab* currentTab = win->CurrentTab();
    if (currentTab) {
        currentTab->selectedAnnotation = nullptr;
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
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifZoom);

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
        ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);
        win->RedrawAll();
        HwndSetText(win->hwndFrame, kSumatraWindowTitle);
        ReportIf(win->TabCount() != 0 || win->CurrentTab());
    }

    // Note: this causes https://code.google.com/p/sumatrapdf/issues/detail?id=2702. For whatever reason
    // edit ctrl doesn't receive WM_KILLFOCUS if we do SetFocus() here, even if we call SetFocus() later on
    // HwndSetFocus(win->hwndFrame);
}

void ShowSavedAnnotationsNotification(HWND hwndParent, const char* path) {
    str::Str msg;
    msg.AppendFmt(_TRA("Saved annotations to '%s'"), path);
    NotificationCreateArgs nargs;
    nargs.hwndParent = hwndParent;
    nargs.font = GetDefaultGuiFont();
    nargs.timeoutMs = 5000;
    nargs.msg = msg.Get();
    ShowNotification(nargs);
}

void ShowSavedAnnotationsFailedNotification(HWND hwndParent, const char* path, const char* mupdfErr) {
    str::Str msg;
    msg.AppendFmt(_TRA("Saving of '%s' failed with: '%s'"), path, mupdfErr);
    ShowWarningNotification(hwndParent, msg.Get(), 0);
}

struct ShowErrorData {
    WindowTab* tab;
    const char* path;
};

static void ShowSaveAnnotationError(ShowErrorData* d, const char* err) {
    auto tab = d->tab;
    auto path = d->path;
    ShowSavedAnnotationsFailedNotification(tab->win->hwndCanvas, path, err);
}

bool SaveAnnotationsToExistingFile(WindowTab* tab) {
    if (!tab) {
        return false;
    }
    EngineBase* engine = tab->AsFixed()->GetEngine();
    const char* path = engine->FilePath();
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
        ShowEditAnnotationsWindow(tab);
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
    str::Str fileFilter(256);
    fileFilter.Append(_TRA("PDF documents"));
    fileFilter.Append("\1*.pdf\1");
    fileFilter.Append("\1*.*\1");
    str::TransCharsInPlace(fileFilter.CStr(), "\1", "\0");
    TempWStr fileFilterW = ToWStrTemp(fileFilter);

    // TODO: automatically construct "foo.pdf" => "foo Copy.pdf"
    EngineBase* engine = tab->AsFixed()->GetEngine();
    const char* srcFileName = engine->FilePath();
    str::BufSet(dstFileName, dimof(dstFileName), srcFileName);

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
        return false;
    }
    char* dstFilePath = ToUtf8Temp(dstFileName);
    bool savingToExisting = str::Eq(dstFilePath, srcFileName);
    if (savingToExisting) {
        return SaveAnnotationsToExistingFile(tab);
    }

    ShowErrorData data{tab, dstFilePath};
    auto fn = MkFunc1(ShowSaveAnnotationError, &data);
    ok = EngineMupdfSaveUpdated(engine, dstFilePath, fn);
    if (!ok) {
        return false;
    }

    // have to re-open edit annotations window because the current has
    // a reference to deleted Engine
    bool hadEditAnnotations = CloseAndDeleteEditAnnotationsWindow(tab);

    auto win = tab->win;
    UpdateTabFileDisplayStateForTab(tab);
    CloseDocumentInCurrentTab(win, true, true);
    HwndSetFocus(win->hwndFrame);

    char* newPath = path::NormalizeTemp(dstFilePath);
    // TODO: this should be 'duplicate FileInHistory"
    RenameFileInHistory(srcFileName, newPath);

    LoadArgs args(newPath, win);
    args.forceReuse = true;
    LoadDocument(&args);

    ShowSavedAnnotationsNotification(win->hwndCanvas, newPath);
    if (hadEditAnnotations) {
        ShowEditAnnotationsWindow(tab);
    }
    return true;
}

enum class SaveChoice {
    Discard,
    SaveNew,
    SaveExisting,
    Cancel,
};

SaveChoice ShouldSaveAnnotationsDialog(HWND hwndParent, const char* filePath) {
    TempStr fileName = (TempStr)path::GetBaseNameTemp(filePath);
    TempStr mainInstrA = str::FormatTemp(_TRA("Unsaved annotations in '%s'"), fileName);
    TempWStr mainInstr = ToWStrTemp(mainInstrA);
    auto content = _TRA("Save annotations?");

    constexpr int kBtnIdDiscard = 100;
    constexpr int kBtnIdSaveToExisting = 101;
    constexpr int kBtnIdSaveToNew = 102;
    // constexpr int kBtnIdCancel = 103;
    TASKDIALOGCONFIG dialogConfig{};
    TASKDIALOG_BUTTON buttons[4];

    buttons[0].nButtonID = kBtnIdSaveToExisting;
    auto s = _TRA("&Save to existing PDF");
    buttons[0].pszButtonText = ToWStrTemp(s);
    buttons[1].nButtonID = kBtnIdSaveToNew;
    s = _TRA("Save to &new PDF");
    buttons[1].pszButtonText = ToWStrTemp(s);
    buttons[2].nButtonID = kBtnIdDiscard;
    s = _TRA("&Discard changes");
    buttons[2].pszButtonText = ToWStrTemp(s);
    buttons[3].nButtonID = IDCANCEL;
    s = _TRA("&Cancel");
    buttons[3].pszButtonText = ToWStrTemp(s);

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    s = _TRA("Unsaved annotations");
    dialogConfig.pszWindowTitle = ToWStrTemp(s);
    dialogConfig.pszMainInstruction = mainInstr;
    dialogConfig.pszContent = ToWStrTemp(content);
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
static bool MaybeSaveAnnotations(WindowTab* tab) {
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
    bool shouldConfirm = EngineHasUnsavedAnnotations(engine);
    if (!shouldConfirm) {
        return true;
    }
    tab->askedToSaveAnnotations = true;
    auto path = dm->GetFilePath();
    auto choice = ShouldSaveAnnotationsDialog(tab->win->hwndFrame, path);
    switch (choice) {
        case SaveChoice::Discard:
            return true;
        case SaveChoice::SaveNew: {
            bool didSave = SaveAnnotationsToMaybeNewPdfFile(tab);
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
    AbortFinding(win, true);
    ClearFindBox(win);
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifPageInfo);
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifAnnotation);
    RemoveNotificationsForGroup(win->hwndCanvas, kNotifZoom);

    RememberRecentlyClosedDocument(tab->filePath);

    // TODO: maybe should have a way to over-ride this for unconditional close?
    bool canClose = MaybeSaveAnnotations(tab);
    if (!canClose) {
        return;
    }

    int tabCount = win->TabCount();
    if (tabCount == 1 || (tabCount == 0 && quitIfLast)) {
        if (CanCloseWindow(win)) {
            CloseWindow(win, quitIfLast, false);
            return;
        }
    } else {
        ReportIf(gPluginMode && !gWindows.Contains(win));
        RemoveTab(tab);
        delete tab;
    }

    tabCount = win->TabCount();
    if (tabCount == 1 && win->GetTab(0)->IsAboutTab()) {
        // showing only home page tab so remove it
        // if there are other windows, close this one
        if (gWindows.size() > 1) {
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
    if (!tab) {
        return;
    }
    CloseTab(tab, quitIfLast);
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

    ReportIf(forceClose && !quitIfLast);
    if (forceClose) {
        quitIfLast = true;
    }

    // when used as an embedded plugin, closing should happen automatically
    // when the parent window is destroyed (cf. WM_DESTROY)
    if (gPluginMode && !gWindows.Contains(win) && !forceClose) {
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
    }

    // TODO: should be more intelligent i.e. close the tabs we can and only
    // leave those where user cancelled closing
    // would have to remember a list of tabs to not close above
    // if list not empty, only close the tabs not on the list
    if (!canCloseWindow) {
        return;
    }

    bool lastWindow = (1 == gWindows.size());
    // RememberDefaultWindowPosition becomes a no-op once the window is hidden
    RememberDefaultWindowPosition(win);
    // hide the window before saving prefs (closing seems slightly faster that way)
    if (!lastWindow || quitIfLast) {
        ShowWindow(win->hwndFrame, SW_HIDE);
    }
    SaveSettings();
    TabsOnCloseWindow(win);

    if (forceClose) {
        // WM_DESTROY has already been sent, so don't destroy win->hwndFrame again
        DeleteMainWindow(win);
    } else if (lastWindow && !quitIfLast) {
        /* last window - don't delete it */
        CloseDocumentInCurrentTab(win, false, false);
        HwndSetFocus(win->hwndFrame);
        ReportIf(!gWindows.Contains(win));
    } else {
        FreeMenuOwnerDrawInfoData(win->menu);
        HWND hwndToDestroy = win->hwndFrame;
        DeleteMainWindow(win);
        DestroyWindow(hwndToDestroy);
    }

    if (lastWindow && quitIfLast) {
        int nWindows = gWindows.size();
        logf("Calling PostQuitMessage() in CloseWindow() because closing lastWindow, nWindows: %d\n", nWindows);
        ReportDebugIf(nWindows != 0);
        PostQuitMessage(0);
    }
}

// returns false if no filter has been appended
static bool AppendFileFilterForDoc(DocController* ctrl, str::Str& fileFilter) {
    // TODO: use ctrl->GetDefaultFileExt()
    Kind type = nullptr;
    if (ctrl->AsFixed()) {
        type = ctrl->AsFixed()->engineType;
    } else if (ctrl->AsChm()) {
        type = kindEngineChm;
    }

    auto ext = ctrl->GetDefaultFileExt();
    if (str::EqI(ext, ".xps")) {
        fileFilter.Append(_TRA("XPS documents"));
    } else if (type == kindEngineDjVu) {
        fileFilter.Append(_TRA("DjVu documents"));
    } else if (type == kindEngineComicBooks) {
        fileFilter.Append(_TRA("Comic books"));
    } else if (type == kindEngineImage) {
        WCHAR* extW = ToWStrTemp(ctrl->GetDefaultFileExt() + 1);
        fileFilter.AppendFmt(_TRA("Image files (*.%s)"), extW);
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
    TempStr srcFileName = (TempStr)ctrl->GetFilePath();
    if (gPluginMode) {
        // fall back to a generic "filename" instead of the more confusing temporary filename
        srcFileName = (TempStr) "filename";
        TempStr urlName = url::GetFileNameTemp(gPluginURL);
        if (urlName) {
            srcFileName = urlName;
        }
    }

    ReportIf(!srcFileName);
    if (!srcFileName) {
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
    str::Str fileFilter(256);
    if (AppendFileFilterForDoc(ctrl, fileFilter)) {
        fileFilter.AppendFmt("\1*%s\1", defExt);
    }
    fileFilter.Append(_TRA("All files"));
    fileFilter.Append("\1*.*\1");
    str::TransCharsInPlace(fileFilter.CStr(), "\1", "\0");

    WCHAR dstFileName[MAX_PATH];
    TempStr baseName = path::GetBaseNameTemp(srcFileName);
    str::BufSet(dstFileName, dimof(dstFileName), baseName);
    if (str::FindChar(dstFileName, ':')) {
        // handle embed-marks (for embedded PDF documents):
        // remove the container document's extension and include
        // the embedding reference in the suggested filename
        WCHAR* colon = (WCHAR*)str::FindChar(dstFileName, ':');
        str::TransCharsInPlace(colon, L":", L"_");
        WCHAR* ext;
        for (ext = colon; ext > dstFileName && *ext != '.'; ext--) {
            // no-op
        }
        if (ext == dstFileName) {
            ext = colon;
        }
        memmove(ext, colon, (str::Len(colon) + 1) * sizeof(WCHAR));
    } else if (str::EndsWithI(dstFileName, ToWStrTemp(defExt))) {
        // Remove the extension so that it can be re-added depending on the chosen filter
        int idx = str::Leni(dstFileName) - str::Leni(defExt);
        dstFileName[idx] = '\0';
    }

    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = ToWStrTemp(fileFilter);
    ofn.nFilterIndex = 1;
    // defExt can be null, we want to skip '.'
    if (str::Leni(defExt) > 0 && defExt[0] == L'.') {
        defExt++;
    }
    ofn.lpstrDefExt = ToWStrTemp(defExt);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    // note: explicitly not setting lpstrInitialDir so that the OS
    // picks a reasonable default (in particular, we don't want this
    // in plugin mode, which is likely the main reason for saving as...)

    bool ok = GetSaveFileNameW(&ofn);
    if (!ok) {
        return;
    }

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
        // Recreate inexistant files from memory...
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
            TempStr s = GetLastErrorStrTemp();
            if (str::Leni(s) > 0) {
                errorMsg = str::FormatTemp("%s\n\n%s", _TRA("Failed to save a file"), s);
            }
        }
    }
    if (!ok) {
        TempStr msg = (errorMsg != nullptr) ? errorMsg : (TempStr)_TRA("Failed to save a file");
        logf("SaveCurrentFileAs() failed with '%s'\n", msg);
        MessageBoxWarning(win->hwndFrame, msg);
    }

    auto path = ctrl->GetFilePath();
    if (ok && IsUntrustedFile(path, gPluginURL)) {
        file::SetZoneIdentifier(realDstFileName);
    }
}

void SumatraOpenPathInExplorer(const char* path) {
    if (gPluginMode || !CanAccessDisk()) {
        return;
    }
    OpenPathInExplorer(path);
}

static void ShowCurrentFileInFolder(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    auto* ctrl = win->ctrl;
    SumatraOpenPathInExplorer(ctrl->GetFilePath());
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
    const char* path = str::DupTemp(ctrl->GetFilePath());
    // this happens e.g. for embedded documents and directories
    if (!file::Exists(path)) {
        return;
    }
    CloseCurrentTab(win, false);
    file::DeleteFileToTrash(path);
    gFileHistory.MarkFileInexistent(path, true);
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
    const char* srcPath = str::DupTemp(ctrl->GetFilePath());
    // this happens e.g. for embedded documents and directories
    if (!file::Exists(srcPath)) {
        return;
    }

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    const char* defExt = ctrl->GetDefaultFileExt();
    TempWStr defExtW = ToWStrTemp(defExt);
    str::Str fileFilter(256);
    bool ok = AppendFileFilterForDoc(ctrl, fileFilter);
    ReportIf(!ok);
    fileFilter.AppendFmt("\1*%s\1", defExt);
    str::TransCharsInPlace(fileFilter.Get(), "\1", "\0");

    WCHAR dstFilePathW[MAX_PATH];
    auto baseName = path::GetBaseNameTemp(srcPath);
    str::BufSet(dstFilePathW, dimof(dstFilePathW), baseName);
    // Remove the extension so that it can be re-added depending on the chosen filter
    if (str::EndsWithI(dstFilePathW, defExtW)) {
        int idx = str::Leni(dstFilePathW) - str::Leni(defExtW);
        dstFilePathW[idx] = '\0';
    }

    WCHAR* srcPathW = ToWStrTemp(srcPath);
    WCHAR* initDir = path::GetDirTemp(srcPathW);

    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFilePathW;
    ofn.nMaxFile = dimof(dstFilePathW);
    ofn.lpstrFilter = ToWStrTemp(fileFilter);
    ofn.nFilterIndex = 1;
    // note: the other two dialogs are named "Open" and "Save As"
    auto s = _TRA("Rename To");
    ofn.lpstrTitle = ToWStrTemp(s);
    ofn.lpstrInitialDir = initDir;
    ofn.lpstrDefExt = defExtW + 1;
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
    BOOL moveOk = MoveFileExW(srcPathW, dstFilePathW, flags);
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
    const char* path = ctrl->GetFilePath();

    const WCHAR* defExt = ToWStrTemp(ctrl->GetDefaultFileExt());

    WCHAR dstFileName[MAX_PATH] = {0};
    // Remove the extension so that it can be replaced with .lnk
    auto name = path::GetBaseNameTemp(path);
    str::BufSet(dstFileName, dimof(dstFileName), name);
    str::TransCharsInPlace(dstFileName, L":", L"_");
    if (str::EndsWithI(dstFileName, defExt)) {
        int idx = str::Leni(dstFileName) - str::Leni(defExt);
        dstFileName[idx] = '\0';
    }

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::Str fileFilter;
    fileFilter.AppendFmt("%s\1*.lnk\1", _TRA("Bookmark Shortcuts"));
    str::TransCharsInPlace(fileFilter.CStr(), "\1", "\0");
    TempWStr fileFilterW = ToWStrTemp(fileFilter);

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

    char* fileName = ToUtf8Temp(dstFileName);
    if (!str::EndsWithI(fileName, ".lnk")) {
        fileName = str::JoinTemp(fileName, ".lnk");
    }

    ScrollState ss(win->ctrl->CurrentPageNo(), 0, 0);
    if (win->AsFixed()) {
        ss = win->AsFixed()->GetScrollState();
    }
    const char* viewMode = DisplayModeToString(ctrl->GetDisplayMode());
    TempStr zoomVirtual = str::FormatTemp("%.2f", ctrl->GetZoomVirtual());
    if (kZoomFitPage == ctrl->GetZoomVirtual()) {
        zoomVirtual = (TempStr) "fitpage";
    } else if (kZoomFitWidth == ctrl->GetZoomVirtual()) {
        zoomVirtual = (TempStr) "fitwidth";
    } else if (kZoomFitContent == ctrl->GetZoomVirtual()) {
        zoomVirtual = (TempStr) "fitcontent";
    }

    TempStr args = str::FormatTemp("\"%s\" -page %d -view \"%s\" -zoom %s -scroll %d,%d", path, ss.page, viewMode,
                                   zoomVirtual, (int)ss.x, (int)ss.y);
    TempStr label = ctrl->GetPageLabeTemp(ss.page);
    TempStr desc = str::FormatTemp(_TRA("Bookmark shortcut to page %s of %s"), label, path);
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
                WCHAR *oldBuffer = lpofn->lpstrFile;
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

void DuplicateTabInNewWindow(WindowTab* tab) {
    if (!tab || tab->IsAboutTab()) {
        return;
    }
    // so that the file is opened in the same state
    SaveSettings();

    const char* path = tab->filePath;
    ReportIf(!path);
    if (!path) {
        return;
    }
    MainWindow* newWin = CreateAndShowMainWindow(nullptr);
    if (!newWin) {
        return;
    }

    // TODO: should copy the display state from current file
    LoadArgs args(path, newWin);
    args.showWin = true;
    args.noPlaceWindow = true;
    LoadDocument(&args);
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

static void GetFilesFromGetOpenFileName(OPENFILENAMEW* ofn, StrVec& filesOut) {
    WCHAR* dir = ofn->lpstrFile;
    WCHAR* file = ofn->lpstrFile + ofn->nFileOffset;
    // only a single file, full path
    char* path;
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
        file += str::Leni(file) + 1;
    }
}

static TempWStr GetFileFilterTemp() {
    const struct {
        const char* name; /* nullptr if only to include in "All supported documents" */
        const char* filter;
        bool available;
    } fileFormats[] = {
        {_TRA("PDF documents"), "*.pdf", true},
        {_TRA("XPS documents"), "*.xps;*.oxps", true},
        {_TRA("DjVu documents"), "*.djvu", true},
        {_TRA("Postscript documents"), "*.ps;*.eps", IsEnginePsAvailable()},
        {_TRA("Comic books"), "*.cbz;*.cbr;*.cb7;*.cbt", true},
        {_TRA("CHM documents"), "*.chm", true},
        {_TRA("SVG documents"), "*.svg", true},
        {_TRA("EPUB ebooks"), "*.epub", true},
        {_TRA("Mobi documents"), "*.mobi", true},
        {_TRA("FictionBook documents"), "*.fb2;*.fb2z;*.zfb2;*.fb2.zip", true},
        {_TRA("PalmDoc documents"), "*.pdb;*.prc", true},
        {_TRA("Images"), "*.bmp;*.dib;*.gif;*.jpg;*.jpeg;*.jxr;*.png;*.tga;*.tif;*.tiff;*.webp;*.heic;*.avif", true},
        {_TRA("Text documents"), "*.txt;*.log;*.nfo;file_id.diz;read.me;*.tcr", true},
    };
    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::Str fileFilter;
    fileFilter.Append(_TRA("All supported documents"));
    fileFilter.AppendChar('\1');
    for (int i = 0; i < dimof(fileFormats); i++) {
        if (fileFormats[i].available) {
            fileFilter.Append(fileFormats[i].filter);
            fileFilter.AppendChar(';');
        }
    }
    ReportIf(fileFilter.Last() != ';');
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
    str::TransCharsInPlace(fileFilter.CStr(), "\1", "\0");
    return ToWStrTemp(fileFilter);
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

    ofn.lpstrFilter = GetFileFilterTemp();
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    // OFN_ENABLEHOOK disables the new Open File dialog under Windows Vista
    // and later, so don't use it and just allocate enough memory to contain
    // several dozen file paths and hope that this is enough
    // TODO: Use IFileOpenDialog instead (requires a Vista SDK, though)
    ofn.nMaxFile = MAX_PATH * 100;
    if (false && !IsWindowsVistaOrGreater()) {
#if 0
        ofn.lpfnHook = FileOpenHook;
        ofn.Flags |= OFN_ENABLEHOOK;
        ofn.nMaxFile = MAX_PATH / 2;
#endif
    }
    // note: ofn.lpstrFile can be reallocated by GetOpenFileName -> FileOpenHook

    AutoFreeWStr file = AllocArray<WCHAR>(ofn.nMaxFile);
    ofn.lpstrFile = file;

    if (!GetOpenFileNameW(&ofn)) {
        return;
    }

    StrVec files;
    GetFilesFromGetOpenFileName(&ofn, files);
    for (char* path : files) {
        LoadArgs args(path, win);
        LoadDocument(&args);
    }
}

static void RemoveFailedFiles(StrVec& files) {
    for (char* path : gFilesFailedToOpen) {
        int idx = files.Find(path);
        if (idx >= 0) {
            files.RemoveAt(idx);
        }
    }
}

static StrVec& CollectNextPrevFilesIfChanged(const char* path) {
    StrVec& files = gNextPrevDirCache;

    char* dir = path::GetDirTemp(path);
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
    int nFiles = files.Size();
    // remove unsupported files
    // traverse from the end so that removing doesn't change iterator
    for (int i = nFiles - 1; i >= 0; i--) {
        char* path2 = files[i];
        Kind kind = GuessFileTypeFromName(path2);
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

static void OpenNextPrevFileInFolder(MainWindow* win, bool forward) {
    ReportIf(win->IsCurrentTabAbout());
    if (win->IsCurrentTabAbout()) {
        return;
    }
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }

    WindowTab* tab = win->CurrentTab();
    bool didRetry = false;
again:
    const char* path = tab->filePath;
    StrVec files = CollectNextPrevFilesIfChanged(path);
    if (files.Size() < 2) {
        return;
    }

    int nFiles = files.Size();
    int idx = files.Find(path);
    if (forward) {
        idx = (idx + 1) % nFiles;
    } else {
        idx = (idx + nFiles - 1) % nFiles;
    }
    path = files[idx];
    if (!file::Exists(path)) {
        if (didRetry) {
            // TODO: can I do something better?
            return;
        }
        didRetry = true;
        str::FreePtr(&gNextPrevDir); // trigger re-reading the directory
        goto again;
    }

    // TODO: check for unsaved modifications
    UpdateTabFileDisplayStateForTab(tab);
    // TODO: should take onFinish() callback so that if failed
    // we could automatically go to next file
    LoadArgs args(path, win);
    args.forceReuse = true;
    LoadDocument(&args);
}

constexpr int kSplitterDx = 5;
constexpr int kSplitterDy = 4;
constexpr int kSidebarMinDx = 150;
constexpr int kTocMinDy = 100;

static void RelayoutFrame(MainWindow* win, bool updateToolbars = true, int sidebarDx = -1) {
    Rect rc = ClientRect(win->hwndFrame);
    // don't relayout while the window is minimized
    if (rc.IsEmpty()) {
        return;
    }

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        // make the black/white canvas cover the entire window
        MoveWindow(win->hwndCanvas, rc);
        return;
    }

    DeferWinPosHelper dh;

    // Tabbar and toolbar at the top
    if (!win->presentation && !win->isFullScreen) {
        if (win->tabsInTitlebar) {
            if (dwm::IsCompositionEnabled()) {
                int frameThickness = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                rc.y += frameThickness;
                rc.dy -= frameThickness;
            }
            float scale = IsZoomed(win->hwndFrame) ? 1.f : kCaptionTabBarDyFactor;
            int captionHeight = GetTabbarHeight(win->hwndFrame, scale);
            if (updateToolbars) {
                int captionWidth;
                RECT capButtons;
                if (dwm::IsCompositionEnabled() &&
                    SUCCEEDED(dwm::GetWindowAttribute(win->hwndFrame, DWMWA_CAPTION_BUTTON_BOUNDS, &capButtons,
                                                      sizeof(RECT)))) {
                    Rect wr = WindowRect(win->hwndFrame);
                    POINT pt = {wr.x + capButtons.left, wr.y + capButtons.top};
                    ScreenToClient(win->hwndFrame, &pt);
                    if (IsUIRtl()) {
                        captionWidth = rc.x + rc.dx - pt.x;
                    } else {
                        captionWidth = pt.x - rc.x;
                    }
                } else {
                    captionWidth = rc.dx;
                }
                dh.SetWindowPos(win->hwndCaption, nullptr, rc.x, rc.y, captionWidth, captionHeight, SWP_NOZORDER);
            }
            rc.y += captionHeight;
            rc.dy -= captionHeight;
        } else if (win->tabsVisible) {
            int tabHeight = GetTabbarHeight(win->hwndFrame);
            if (updateToolbars) {
                dh.SetWindowPos(win->tabsCtrl->hwnd, nullptr, rc.x, rc.y, rc.dx, tabHeight, SWP_NOZORDER);
            }
            // TODO: show tab bar also for About window (or hide the toolbar so that it doesn't jump around)
            if (!win->IsCurrentTabAbout()) {
                rc.y += tabHeight;
                rc.dy -= tabHeight;
            }
        }
    }
    if (gGlobalPrefs->showToolbar && !win->presentation && !win->isFullScreen) {
        if (updateToolbars) {
            Rect rcRebar = WindowRect(win->hwndReBar);
            dh.SetWindowPos(win->hwndReBar, nullptr, rc.x, rc.y, rc.dx, rcRebar.dy, SWP_NOZORDER);
        }
        Rect rcRebar = WindowRect(win->hwndReBar);
        rc.y += rcRebar.dy;
        rc.dy -= rcRebar.dy;
    }

    // ToC and Favorites sidebars at the left
    bool showFavorites = gGlobalPrefs->showFavorites && !gPluginMode && CanAccessDisk();
    bool tocVisible = win->tocVisible;
    if (tocVisible || showFavorites) {
        Size toc = ClientRect(win->hwndTocBox).Size();
        if (sidebarDx > 0) {
            toc = Size(sidebarDx, rc.y);
        }
        if (0 == toc.dx) {
            // TODO: use saved sidebarDx from saved preferences?
            toc.dx = rc.dx / 4;
        }
        // make sure that the sidebar is never too wide or too narrow
        // note: requires that the main frame is at least 2 * kSidebarMinDx
        //       wide (cf. OnFrameGetMinMaxInfo)
        toc.dx = limitValue(toc.dx, kSidebarMinDx, rc.dx / 2);

        toc.dy = 0;
        if (tocVisible) {
            if (!showFavorites) {
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

        if (tocVisible && showFavorites) {
            toc.dy = limitValue(toc.dy, kTocMinDy, rc.dy - kTocMinDy);
        }

        if (tocVisible) {
            Rect rToc(rc.TL(), toc);
            dh.MoveWindow(win->hwndTocBox, rToc);
            if (showFavorites) {
                Rect rSplitV(rc.x, rc.y + toc.dy, toc.dx, kSplitterDy);
                dh.MoveWindow(win->favSplitter->hwnd, rSplitV);
                toc.dy += kSplitterDy;
            }
        }
        if (showFavorites) {
            Rect rFav(rc.x, rc.y + toc.dy, toc.dx, rc.dy - toc.dy);
            dh.MoveWindow(win->hwndFavBox, rFav);
        }
        Rect rSplitH(rc.x + toc.dx, rc.y, kSplitterDx, rc.dy);
        dh.MoveWindow(win->sidebarSplitter->hwnd, rSplitH);

        rc.x += toc.dx + kSplitterDx;
        rc.dx -= toc.dx + kSplitterDx;
    }

    dh.MoveWindow(win->hwndCanvas, rc);

    dh.End();

    // TODO: if a document with ToC and a broken document are loaded
    //       and the first document is closed with the ToC still visible,
    //       we have tocVisible but !win->ctrl
    if (tocVisible && win->ctrl) {
        // the ToC selection may change due to resizing
        // (and SetSidebarVisibility relies on this for initialization)
        UpdateTocSelection(win, win->ctrl->CurrentPageNo());
    }
}

static void FrameOnSize(MainWindow* win, int, int) {
    RelayoutFrame(win);

    if (win->presentation || win->isFullScreen) {
        Rect fullscreen = GetFullscreenRect(win->hwndFrame);
        Rect rect = WindowRect(win->hwndFrame);
        // Windows XP sometimes seems to change the window size on it's own
        if (rect != fullscreen && rect != GetVirtualScreenRect()) {
            MoveWindow(win->hwndFrame, fullscreen);
        }
    }
}

void RelayoutWindow(MainWindow* win) {
    RelayoutFrame(win);
}

void SetCurrentLanguageAndRefreshUI(const char* langCode) {
    if (!langCode || str::Eq(langCode, trans::GetCurrentLangCode())) {
        return;
    }
    SetCurrentLang(langCode);

    for (MainWindow* win : gWindows) {
        UpdateWindowRtlLayout(win);
        RebuildMenuBarForWindow(win);
        UpdateToolbarSidebarText(win);
        // About page text is translated during (re)drawing
        if (win->IsCurrentTabAbout()) {
            win->RedrawAll(true);
        }
    }

    SaveSettings();
}

static void OnMenuChangeLanguage(HWND hwnd) {
    const char* newLangCode = Dialog_ChangeLanguge(hwnd, trans::GetCurrentLangCode());
    SetCurrentLanguageAndRefreshUI(newLangCode);
}

static void OnMenuViewShowHideToolbar() {
    gGlobalPrefs->showToolbar = !gGlobalPrefs->showToolbar;
    for (MainWindow* win : gWindows) {
        ShowOrHideToolbar(win);
    }
}

static void OnMenuViewShowHideScrollbars() {
    gGlobalPrefs->fixedPageUI.hideScrollbars = !gGlobalPrefs->fixedPageUI.hideScrollbars;
    UpdateFixedPageScrollbarsVisibility();
}

#if 0 // note: was used in OpenAdvancedOptions()
static void OpenFileWithTextEditor(const char* path) {
    Vec<TextEditor*> editors;
    DetectTextEditors(editors);
    const char* cmd = editors[0]->openFileCmd;

    char* cmdLine = BuildOpenFileCmd(cmd, path, 1, 1);
    logf("OpenFileWithTextEditor: '%s'\n", cmdLine);
    char* appDir = GetSelfExeDirTemp();
    AutoCloseHandle process(LaunchProcess(cmdLine, appDir));
    str::Free(cmdLine);
}
#endif

static void OpenAdvancedOptions() {
    if (!CanAccessDisk() || !HasPermission(Perm::SavePreferences)) {
        return;
    }

    // TODO: disable/hide the menu item when there's no prefs file
    //       (happens e.g. when run in portable mode from a CD)?
    TempStr path = GetSettingsPathTemp();
    LaunchFileIfExists(path);
}

static void ShowOptionsDialog(HWND hwnd) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }

    if (IDOK != Dialog_Settings(hwnd, gGlobalPrefs)) {
        return;
    }

    if (!gGlobalPrefs->rememberOpenedFiles) {
        gFileHistory.Clear(true);
        DeleteThumbnailCacheDirectory();
    }
    UpdateDocumentColors();

    // note: ideally we would also update state for useTabs changes but that's complicated since
    // to do it right we would have to convert tabs to windows. When moving no tabs -> tabs,
    // there's no problem. When moving tabs -> no tabs, a half solution would be to only
    // call SetTabsInTitlebar() for windows that have only one tab, but that's somewhat inconsistent
    SaveSettings();
}

// TODO: should use currently active window, but most of the time
// there's only one window
void MaybeRedrawHomePage() {
    if (!gWindows.empty() && gWindows.at(0)->IsCurrentTabAbout()) {
        gWindows.at(0)->RedrawAll(true);
    }
}

static void ShowOptionsDialog(MainWindow* win) {
    ShowOptionsDialog(win->hwndFrame);
    MaybeRedrawHomePage();
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
    args.groupId = kNotifZoom;
    args.timeoutMs = 2000;
    args.hwndParent = win->hwndCanvas;
    args.msg = BuildZoomString(zoomLevel);
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
    AutoFreeStr newPageLabel = Dialog_GoToPage(win->hwndFrame, label, ctrl->PageCount(), !ctrl->HasPageLabels());
    if (!newPageLabel) {
        return;
    }

    int newPageNo = ctrl->GetPageByLabel(newPageLabel.Get());
    if (ctrl->ValidPageNo(newPageNo)) {
        ctrl->GoToPage(newPageNo, true);
    }
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

    // ToC and Favorites sidebars are hidden when entering presentation mode
    // TODO: make showFavorites a per-window pref
    bool showFavoritesTmp = gGlobalPrefs->showFavorites;
    if (presentation && (win->tocVisible || gGlobalPrefs->showFavorites)) {
        SetSidebarVisibility(win, false, false);
    }

    long ws = GetWindowLong(win->hwndFrame, GWL_STYLE);
    if (!presentation || !win->isFullScreen) {
        win->nonFullScreenWindowStyle = ws;
    }
    // remove window styles that add to non-client area
    ws &= ~(WS_CAPTION | WS_THICKFRAME);
    ws |= WS_MAXIMIZE;

    win->nonFullScreenFrameRect = WindowRect(win->hwndFrame);
    Rect rect = GetFullscreenRect(win->hwndFrame);

    SetMenu(win->hwndFrame, nullptr);
    ShowWindow(win->hwndReBar, SW_HIDE);
    win->tabsCtrl->SetIsVisible(false);
    ShowWindow(win->hwndCaption, SW_HIDE);

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
}

void ExitFullScreen(MainWindow* win) {
    if (!win->isFullScreen && !win->presentation) {
        return;
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

    bool tocVisible = win->CurrentTab() && win->CurrentTab()->showToc;
    SetSidebarVisibility(win, tocVisible, gGlobalPrefs->showFavorites);

    if (win->tabsInTitlebar) {
        ShowWindow(win->hwndCaption, SW_SHOW);
    }
    if (win->tabsVisible) {
        win->tabsCtrl->SetIsVisible(true);
    }
    if (gGlobalPrefs->showToolbar) {
        ShowWindow(win->hwndReBar, SW_SHOW);
    }
    if (!win->isMenuHidden) {
        SetMenu(win->hwndFrame, win->menu);
    }

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
    if (hasToolbar && NeedsFindUI(win)) {
        tabOrder[nWindows++] = win->hwndFindEdit;
    }
    if (win->tocLoaded && win->tocVisible) {
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

static Annotation* GetAnnotionUnderCursor(WindowTab* tab, Annotation* annot) {
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return nullptr;
    }
    Point pt = HwndGetCursorPos(tab->win->hwndCanvas);
    if (pt.IsEmpty()) {
        return nullptr;
    }
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
#if 0
    if (win->tabsVisible && isCtrl && VK_TAB == key) {
        TabsOnCtrlTab(win, isShift);
        return true;
    }
#endif
    if (!win->IsDocLoaded()) {
        return false;
    }

    DisplayModel* dm = win->AsFixed();

    // some of the chm key bindings are different than the rest and we
    // need to make sure we don't break them
    bool isChm = win->AsChm();
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
            win->AsChm()->PassUIMsg(WM_KEYDOWN, key, lp);
            return true;
        }
    }
    // lf("key=%d,%c,shift=%d\n", key, (char)key, (int)WasKeyDown(VK_SHIFT));

    // the parts that are commented out should now be handled
    // in OnCommand via accelerators
    if (VK_UP == key) {
        logf("VK_UP\n");
    } else if (VK_DOWN == key) {
        logf("VK_DOWN\n");
    } else if (VK_PRIOR == key && isCtrl) {
        // win->ctrl->GoToPrevPage();
        logf("CTRL + VK_PRIOR\n");
    } else if (VK_NEXT == key && isCtrl) {
        // win->ctrl->GoToNextPage();
        logf("CTRL + VK_NEXTds\n");
    } else if (VK_HOME == key && isCtrl) {
        logf("CTRL + VK_HOME\n");
        // win->ctrl->GoToFirstPage();
    } else if (VK_END == key && isCtrl) {
        logf("CTRL + VK_END\n");
        if (!win->ctrl->GoToLastPage()) {
            //    SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_BOTTOM, 0);
        }
    } else if (VK_LEFT == key) {
        logf("VK_LEFT\n");
    } else if (VK_RIGHT == key) {
        logf("VK_RIGHT\n");
    } else if (VK_HOME == key) {
        logf("VK_HOME\n");
        // win->ctrl->GoToFirstPage();
    } else if (VK_END == key) {
        logf("VK_END\n");
        if (!win->ctrl->GoToLastPage()) {
            // SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_BOTTOM, 0);
        }
    } else if (VK_MULTIPLY == key && dm) {
        logf("VK_MULTIPLY\n");
        dm->RotateBy(90);
    } else if (VK_DIVIDE == key && dm) {
        logf("VK_DIVIDE\n");
        dm->RotateBy(-90);
        gIsDivideKeyDown = true;
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
    if (RemoveNotificationsForGroup(win->hwndCanvas, kNotifZoom)) {
        return;
    }
    if (win->showSelection) {
        ClearSearchResult(win);
        ToolbarUpdateStateForWindow(win, false);
        return;
    }
    if (win->presentation || win->isFullScreen) {
        ToggleFullScreen(win, win->presentation != PM_DISABLED);
        return;
    }
    if (gGlobalPrefs->escToExit && CanCloseWindow(win)) {
        CloseWindow(win, true, false);
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
Annotation* MakeAnnotationsFromSelection(WindowTab* tab, AnnotCreateArgs* args) {
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

    if (args->setContent) {
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
        TabsSelect(win, key < '9' ? (int)(key - '1') : (int)win->TabCount() - 1);
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
    // note: without the min/max(..., rToc.dx), the sidebar will be
    //       stuck at its width if it accidentally got too wide or too narrow
    Rect rFrame = ClientRect(win->hwndFrame);
    Rect rToc = ClientRect(win->hwndTocBox);
    int minDx = std::min(kSidebarMinDx, rToc.dx);
    int maxDx = std::max(rFrame.dx / 2, rToc.dx);
    if (sidebarDx < minDx || sidebarDx > maxDx) {
        ev->resizeAllowed = false;
        return;
    }

    RelayoutFrame(win, false, sidebarDx);
}

static void OnFavSplitterMove(Splitter::MoveEvent* ev) {
    Splitter* splitter = ev->w;
    HWND hwnd = splitter->hwnd;
    MainWindow* win = FindMainWindowByHwnd(hwnd);

    Point pcur = HwndGetCursorPos(win->hwndCanvas);
    int tocDy = pcur.y; // without splitter

    // make sure to keep this in sync with the calculations in RelayoutFrame
    Rect rFrame = ClientRect(win->hwndFrame);
    Rect rToc = ClientRect(win->hwndTocBox);
    ReportIf(rToc.dx != ClientRect(win->hwndFavBox).dx);
    int minDy = std::min(kTocMinDy, rToc.dy);
    int maxDy = std::max(rFrame.dy - kTocMinDy, rToc.dy);
    if (tocDy < minDy || tocDy > maxDy) {
        ev->resizeAllowed = false;
        return;
    }
    gGlobalPrefs->tocDy = tocDy;
    RelayoutFrame(win, false, rToc.dx);
}

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
    win->tocVisible = tocVisible;

    // TODO: make this a per-window setting as well?
    gGlobalPrefs->showFavorites = showFavorites;

    if ((!tocVisible && HwndIsFocused(win->tocTreeView->hwnd)) ||
        (!showFavorites && HwndIsFocused(win->favTreeView->hwnd))) {
        HwndSetFocus(win->hwndFrame);
    }

    HwndSetVisibility(win->sidebarSplitter->hwnd, tocVisible || showFavorites);
    HwndSetVisibility(win->hwndTocBox, tocVisible);
    win->sidebarSplitter->isLive = true;

    HwndSetVisibility(win->favSplitter->hwnd, tocVisible && showFavorites);
    HwndSetVisibility(win->hwndFavBox, showFavorites);
    win->favSplitter->isLive = true;

    RelayoutFrame(win, false);
}

constexpr int kMaxURLLen = 1500;

// if url-encoded s is bigger than a reasonable URL path,
// we don't want to fail but truncate and encode less
static TempStr URLEncodeMayTruncateTemp(const char* s) {
    HRESULT hr;
    DWORD diff;
    WCHAR buf[kMaxURLLen + 1]{};
    DWORD flags = URL_ESCAPE_AS_UTF8;
    TempWStr ws = ToWStrTemp(s);
    // we can't predict the length of encoded string so we try
    // with increasingly smaller input strings, from 1500 down to 1000
    int maxLen = kMaxURLLen;
    for (int i = 0; i < 10; i++) {
        if (str::Leni(ws) >= maxLen) {
            ws[maxLen - 1] = 0;
        }
        DWORD cchSizeInOut = kMaxURLLen;
        hr = UrlEscapeW(ws, buf, &cchSizeInOut, flags);
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
        maxLen -= diff;
    }
    return nullptr;
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
static const char* GetISO639LangCodeFromLang(const char* lang) {
    int idx = seqstrings::StrToIdx(gLangsMap, lang);
    if (idx < 0 || idx % 2 != 0) {
        return lang;
    }
    lang = seqstrings::IdxToStr(gLangsMap, idx + 1);
    return lang;
}

static void LaunchBrowserWithSelection(WindowTab* tab, const char* urlPattern) {
    if (!tab || !HasPermission(Perm::InternetAccess) || !HasPermission(Perm::CopySelection)) {
        return;
    }

#if 0 // TODO: get selection from Chm
    if (tab->AsChm()) {
        tab->AsChm()->CopySelection();
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
    const char* lang = trans::GetCurrentLangCode();
    auto contryCode = GetISO639LangCodeFromLang(lang);
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
    if (!Dialog_CustomZoom(win->hwndFrame, win->AsChm(), &virtZoom)) {
        return;
    }
    SmartZoom(win, virtZoom, nullptr, true);
}

// this is a directory for not important data, like downloaded symbols
// this directory is the same for installed / portable etc. versions
TempStr GetNotImportantDataDirTemp() {
    TempStr dir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (!dir) {
        return nullptr;
    }
    return path::JoinTemp(dir, "SumatraPDF-data");
}

TempStr GetLogFilePathTemp() {
    TempStr dir = GetNotImportantDataDirTemp();
    if (!dir) {
        return nullptr;
    }
    // TODO: maybe use unique name
    return path::JoinTemp(dir, "sumatra-log.txt");
}

// separate directory for each build number / type
TempStr GetVerDirNameTemp(const char* prefix) {
    auto variant = gIsPreReleaseBuild ? "prerel" : "rel";
    if (gIsDebugBuild) {
        variant = "dbg";
    }
    auto bits = IsProcess64() ? "64" : "32";
    if (IsArmBuild()) {
        bits = "arm64";
    }
    auto ver = CURR_VERSION_STRA; // 3.6.16105 for pre-release,  3.5.2 for release
    // TODO: something different for store build?
    return str::FormatTemp("%s%s-%s-%s", prefix, variant, ver, bits);
}

TempStr GetCrashInfoDirTemp() {
    TempStr dataDir = GetNotImportantDataDirTemp();
    TempStr dirName = GetVerDirNameTemp("crashinfo-");
    return path::JoinTemp(dataDir, dirName);
}

void ShowLogFileSmart() {
    TempStr path = gLogFilePath;
    if (path == nullptr) {
        path = GetLogFilePathTemp();
    }
    WriteCurrentLogToFile(path);
    LaunchFileIfExists(path);
}

void ReopenLastClosedFile(MainWindow* win) {
    char* path = PopRecentlyClosedDocument();
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
    const char* path = tab->filePath;
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
    TempStr msg2 = str::FormatTemp(_TRA("Cleared history of %d files, deleted thumbnails."), d->nFiles);
    ShowTemporaryNotification(win->hwndCanvas, msg2, kNotif5SecsTimeOut);
}

static void ClearHistoryAsync(ClearHistoryData* d) {
    DeleteThumbnailCacheDirectory();
    TempStr symDir = GetCrashInfoDirTemp();
    dir::RemoveAll(symDir);
    auto fn = MkFunc0<ClearHistoryData>(ClearHistoryFinish, d);
    uitask::Post(fn, "TaksClearHistoryAsyncPart");
    DestroyTempAllocator();
}

void ClearHistory(MainWindow* win) {
    if (!win) {
        // TODO: find current active MainWindow ?
        return;
    }

    // TODO: what is relation between gFileHistory and gGlobalPrefs->fileStates?
    int nFiles = 0;
    if (gFileHistory.states) {
        nFiles = gFileHistory.states->Size();
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

    const char* msg = _TRA("Clearing history...");
    NotificationCreateArgs args;
    args.groupId = kNotifClearHistory;
    args.hwndParent = win->hwndCanvas;
    args.timeoutMs = kNotif5SecsTimeOut;
    ShowNotification(args);
    auto data = new ClearHistoryData;
    data->win = win;
    data->nFiles = nFiles;
    auto fn = MkFunc0<ClearHistoryData>(ClearHistoryAsync, data);
    RunAsync(fn, "ClearHistoryAsync");
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
    TempStr msg = (TempStr) "Symbols were already downloaded";

    bool ok = AreSymbolsDownloaded(gSymbolsDir);
    if (ok) {
        goto ShowMessage;
    }
    ok = CrashHandlerDownloadSymbols();
    if (!ok) {
        msg = (TempStr) "Failed to download symbols";
        goto ShowMessage;
    }
    msg = str::FormatTemp("Downloaded symbols to %s", gSymbolsDir);
    {
        bool didInitializeDbgHelp = InitializeDbgHelp(false);
        ReportIfQuick(!didInitializeDbgHelp);
    }
ShowMessage:
    MessageBoxWarning(nullptr, msg, "Downloading symbols");
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
    free(s);
}

static bool ExtractFiles(lzma::SimpleArchive* archive, const char* destDir) {
    logf("ExtractFiles(): dir '%s'\n", destDir);
    lzma::FileInfo* fi;
    u8* uncompressed;
    bool ok;

    int nFiles = archive->filesCount;

    for (int i = 0; i < nFiles; i++) {
        fi = &archive->files[i];
        uncompressed = lzma::GetFileDataByIdx(archive, i, nullptr);

        if (!uncompressed) {
            logf("ExtractFiles: lzma::GetFileDataByIdx() failed\n");
            return false;
        }
        TempStr filePath = path::JoinTemp(destDir, fi->name);
        ok = dir::CreateForFile(filePath);
        if (!ok) {
            logf("ExtractFiles: dir::CreateForFile(%s) failed\n", filePath);
            free(uncompressed);
            return false;
        }

        ByteSlice d = {uncompressed, fi->uncompressedSize};
        ok = file::WriteFile(filePath, d);
        free(uncompressed);
        if (!ok) {
            logf("ExtractFiles: lzma::Write(%s) failed\n", filePath);
            return false;
        }
        logf("  extracted '%s'\n", filePath);
    }

    return true;
}

constexpr const char* kManualIndex = "SumatraPDF-documentation.html";
constexpr const char* kManualKeyboard = "Keyboard-shortcuts.html";

static LoadedDataResource gManualArchiveData;
static lzma::SimpleArchive gManualArchive{};
static SimpleBrowserWindow* gManualBrowserWindow = nullptr;
static bool gUseOurWindowForManual = false;

static void OnDestroyManualBrowserWindow(Wnd::DestroyEvent*) {
    gManualBrowserWindow = nullptr;
}

void DeleteManualBrowserWindow() {
    delete gManualBrowserWindow;
}

static void OpenManualAtFile(const char* htmlFileName) {
    TempStr dataDir = GetNotImportantDataDirTemp();
    TempStr dirName = GetVerDirNameTemp("crashinfo-");
    TempStr dir = path::JoinTemp(dataDir, dirName);
    TempStr htmlFilePath = path::JoinTemp(dir, htmlFileName);
    // in debug build we force extraction because those could be stale files
    bool ok = !gIsDebugBuild && file::Exists(htmlFilePath);
    if (ok) {
        logf("OpenManualAtFile: '%s' already exists\n", htmlFilePath);
        goto OpenFileInBrowser;
    }

    ok = gManualArchive.filesCount > 0;
    if (!ok) {
        ok = LockDataResource(IDR_MANUAL_PAK, &gManualArchiveData);
        if (!ok) {
            logf("OpenManualAtFile(): LockDataResource() failed\n");
            return;
        }
        auto data = gManualArchiveData.data;
        auto size = gManualArchiveData.dataSize;
        ok = lzma::ParseSimpleArchive(data, (size_t)size, &gManualArchive);
        if (!ok) {
            logf("OpenManualAtFile: lzma:ParseSimpleArchive() failed\n");
            return;
        }
        logf("OpenManualAtFile(): opened manual.dat, %d files\n", gManualArchive.filesCount);
        ok = gManualArchive.filesCount > 0;
        if (!ok) {
            return;
        }
    }
    dir::CreateAll(dir);
    // on error, ExtractFiles() shows error message itself
    ok = ExtractFiles(&gManualArchive, dir);
    if (!ok) {
        return;
    }
OpenFileInBrowser:
    TempStr url = str::JoinTemp("file://", htmlFilePath);

    if (gUseOurWindowForManual) {
        if (gManualBrowserWindow) {
            // re-use existing manual window
            gManualBrowserWindow->webView->Navigate(url);
            return;
        }
        if (!gManualBrowserWindow) {
            // try to launch in our window
            SimpleBrowserCreateArgs args;
            args.title = "SumatraPDF Documentation";
            args.url = url;
            // TODO: dataDir
            gManualBrowserWindow = SimpleBrowserWindowCreate(args);
            if (gManualBrowserWindow != nullptr) {
                auto fn = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroyManualBrowserWindow);
                gManualBrowserWindow->onDestroy = fn;
                return;
            }
        }
    }
    // couldn't create WebView2 window so fallback to default web browser
    SumatraLaunchBrowser(url);
}

static void SetAnnotCreateArgs(AnnotCreateArgs& args, CustomCommand* cmd) {
    if (cmd) {
        args.copyToClipboard = GetCommandBoolArg(cmd, kCmdArgCopyToClipboard, false);
        args.setContent = GetCommandBoolArg(cmd, kCmdArgSetContent, false);
        auto col = GetCommandArg(cmd, kCmdArgColor);
        ReportIf(col && !col->colorVal.parsedOk);
        if (col && col->colorVal.parsedOk) {
            args.col = col->colorVal;
            return;
        }
    }
    auto& a = gGlobalPrefs->annotations;
    ParsedColor* col = nullptr;
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
    } else {
        logf("SetAnnotCreateArgs: unexpected type %d for default prefs color\n", (int)typ);
        // ReportIf(true);
    }
    if (col && col->parsedOk) {
        args.col = *col;
    }
}

static void CloseWindowDelayedAsync(MainWindow* win) {
    if (IsGUIThread(FALSE)) {
        CloseWindow(win, false, false);
        return;
    }
    Sleep(4 * 1000);
    auto fn = MkFunc0(CloseWindowDelayedAsync, win);
    uitask::Post(fn, nullptr);
}

static LRESULT FrameOnCommand(MainWindow* win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    int cmdId = LOWORD(wp);

    if (cmdId >= 0xF000) {
        // handle system menu messages for the Window menu (needed for Tabs in Titlebar)
        return SendMessageW(hwnd, WM_SYSCOMMAND, wp, lp);
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

    if (!win) {
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

    AnnotationType annotType = (AnnotationType)(cmdId - CmdCreateAnnotText);
    switch (cmdId) {
        case CmdCreateAnnotHighlight:
            annotType = AnnotationType::Highlight;
            break;
        case CmdCreateAnnotSquiggly:
            annotType = AnnotationType::Squiggly;
            break;
        case CmdCreateAnnotStrikeOut:
            annotType = AnnotationType::StrikeOut;
            break;
        case CmdCreateAnnotUnderline:
            annotType = AnnotationType::Underline;
            break;
    }

    // most of them require a win, the few exceptions are no-ops
    switch (cmdId) {
        case CmdViewWithExternalViewer: {
            const char* cmdLine = GetCommandStringArg(cmd, kCmdArgCommandLine, nullptr);
            if (!cmdLine || !CanAccessDisk() || !tab || !file::Exists(tab->filePath)) {
                return 0;
            }
            const char* filter = GetCommandStringArg(cmd, kCmdArgFilter, nullptr);
            RunWithExe(tab, cmdLine, filter);
            return 0;
        }

        case CmdSetTheme: {
            auto theme = GetCommandStringArg(cmd, kCmdArgTheme, nullptr);
            if (theme) {
                SetTheme(theme);
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
            bool isValidURL = (str::Find(url, "://") != nullptr);
            if (!isValidURL) {
                url = str::JoinTemp("https://", url);
            }
            LaunchBrowserWithSelection(tab, url);
            return 0;
        }

        case CmdExec: {
            auto filter = GetCommandStringArg(cmd, kCmdArgFilter, nullptr);
            auto cmdLine = GetCommandStringArg(cmd, kCmdArgExe, nullptr);
            if (cmdLine == nullptr) {
                return 0;
            }
            RunWithExe(tab, cmdLine, filter);
            return 0;
        }

        case CmdNewWindow:
            CreateAndShowMainWindow(nullptr);
            break;

        case CmdDuplicateInNewWindow:
            DuplicateInNewWindow(win);
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
            const char* mode = nullptr;
            if (cmd) {
                mode = GetCommandStringArg(cmd, kCmdArgMode, nullptr);
            }
            RunCommandPallette(win, mode, 0);
        } break;

        case CmdClearHistory:
            ClearHistory(win);
            break;

        case CmdReopenLastClosedFile:
            ReopenLastClosedFile(win);
            break;

        case CmdShowLog:
            ShowLogFileSmart();
            break;

        case CmdNextTab:
        case CmdPrevTab: {
            bool reverse = cmdId == CmdPrevTab;
            TabsOnCtrlTab(win, reverse);
        } break;

        case CmdNextTabSmart:
        case CmdPrevTabSmart: {
            if (win && win->TabCount() > 1) {
                int advance = cmdId == CmdNextTabSmart ? 1 : -1;
                RunCommandPallette(win, kPalettePrefixTabs, advance);
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

        case CmdDebugDelayCloseWindow: {
            auto fn = MkFunc0(CloseWindowDelayedAsync, win);
            RunAsync(fn, "DelayedCloseWindow");
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
        case CmdZoomFitContent:
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
            break;

        case CmdFacingView:
            SwitchToDisplayMode(win, DisplayMode::Facing, true);
            break;

        case CmdBookView:
            SwitchToDisplayMode(win, DisplayMode::BookView, true);
            break;

        case CmdToggleContinuousView:
            ToggleContinuousView(win);
            break;

        case CmdToggleMangaMode:
            ToggleMangaMode(win);
            break;

        case CmdToggleToolbar:
            OnMenuViewShowHideToolbar();
            break;

        case CmdToggleScrollbars:
            OnMenuViewShowHideScrollbars();
            break;

        case CmdSaveAnnotations: {
            SaveAnnotationsToExistingFile(tab);
            break;
        }

        case CmdToggleFrequentlyRead: {
            gGlobalPrefs->showStartPage = !gGlobalPrefs->showStartPage;
            win->RedrawAll(true);
            break;
        }

        case CmdInvokeInverseSearch: {
            InvokeInverseSearch(tab);
            break;
        }

        case CmdSaveAnnotationsNewFile: {
            SaveAnnotationsToMaybeNewPdfFile(tab);
            break;
        }

        case CmdToggleMenuBar: {
            if (!win->tabsInTitlebar) {
                ToggleMenuBar(win, false);
            }
            break;
        }

        case CmdChangeLanguage:
            OnMenuChangeLanguage(win->hwndFrame);
            break;

        case CmdToggleBookmarks:
        case CmdToggleTableOfContents:
            ToggleTocBox(win);
            break;

        case CmdScrollUpHalfPage: {
            if (!win->IsDocLoaded()) {
                return 0;
            }
            if (dm && dm->NeedVScroll()) {
                SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_HALF_PAGEUP, 0);
            } else {
                // in single page view, scrolls by page
                win->ctrl->GoToPrevPage();
            }
        } break;

        // TODO: do I need both CmdScrollUpPage and CmdGoToPrevPage
        case CmdScrollUpPage: {
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
        } break;

        case CmdScrollDown:
        case CmdScrollUp: {
            if (!win->IsDocLoaded()) {
                return 0;
            }
            if (dm && dm->NeedVScroll()) {
                int n = GetCommandIntArg(cmd, kCmdArgN, 1);
                WPARAM dir = (cmdId == CmdScrollUp) ? SB_LINEUP : SB_LINEDOWN;
                for (int i = 0; i < n; i++) {
                    SendMessageW(win->hwndCanvas, WM_VSCROLL, dir, 0);
                }
            } else {
                // in single page view, scrolls by page
                if (cmdId == CmdScrollUp) {
                    win->ctrl->GoToPrevPage(true);
                } else {
                    win->ctrl->GoToNextPage();
                }
            }
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
            break;
        }

        case CmdScrollDownHalfPage: {
            if (!win->IsDocLoaded()) {
                return 0;
            }
            if (dm && dm->NeedVScroll()) {
                SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_HALF_PAGEDOWN, 0);
            } else {
                // in single page view, scrolls by page
                win->ctrl->GoToNextPage();
            }
        } break;

        case CmdScrollDownPage: {
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
        } break;

        case CmdScrollRightPage: {
            SendMessageW(win->hwndCanvas, WM_HSCROLL, SB_PAGERIGHT, 0);
        } break;

        case CmdGoToFirstPage:
            if (!win->IsDocLoaded()) {
                return 0;
            }
            ctrl->GoToFirstPage();
            break;

        case CmdGoToLastPage:
            if (!win->IsDocLoaded()) {
                return 0;
            }
            if (!ctrl->GoToLastPage()) {
                SendMessageW(win->hwndCanvas, WM_VSCROLL, SB_BOTTOM, 0);
            }
            break;

        case CmdGoToPage:
            OnMenuGoToPage(win);
            break;

        case CmdTogglePresentationMode:
            TogglePresentationMode(win);
            break;

        case CmdToggleFullscreen:
            ToggleFullScreen(win);
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
            FindFirst(win);
            break;

        case CmdFindNext:
            FindNext(win);
            break;

        case CmdFindPrev:
            FindPrev(win);
            break;

        case CmdFindMatch:
            FindToggleMatchCase(win);
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
            OpenManualAtFile(kManualIndex);
            break;

        case CmdHelpOpenKeyboardShortcuts:
            OpenManualAtFile(kManualKeyboard);
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

        case CmdOptions:
            ShowOptionsDialog(win);
            break;

        case CmdAdvancedOptions:
        case CmdAdvancedSettings:
            OpenAdvancedOptions();
            break;

        case CmdSendByEmail:
            SendAsEmailAttachment(tab, win->hwndFrame);
            break;

        case CmdProperties: {
            bool extended = false;
            ShowProperties(win->hwndFrame, win->ctrl, extended);
            break;
        }

        case CmdMoveFrameFocus:
            if (!HwndIsFocused(win->hwndFrame)) {
                HwndSetFocus(win->hwndFrame);
            } else if (win->tocVisible) {
                HwndSetFocus(win->tocTreeView->hwnd);
            }
            break;

        case CmdTranslateSelectionWithGoogle:
            LaunchBrowserWithSelection(
                tab, "https://translate.google.com/?op=translate&sl=auto&tl=${userlang}&text=${selection}");
            break;

        case CmdTranslateSelectionWithDeepL: {
            // Note: we don't know if selected string is English but I don't know
            // how to get deepl.com to auto-detect language
            const char* lang = trans::GetCurrentLangCode();
            const char* uri = "https://www.deepl.com/translator#en/${userlang}/${selection}";
            if (str::Eq(lang, "en")) {
                // it's pointless to translate from English to English
                // this format will hopefully trigger auto-detection of user languge by deepl.com
                uri = "https://www.deepl.com/translator#en/${selection}";
            }
            LaunchBrowserWithSelection(tab, uri);
        } break;

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
            RelayoutWindow(win);
            ScheduleRepaint(win, 0);
            break;

        case CmdDebugDownloadSymbols:
            DownloadDebugSymbols();
            break;

        case CmdDebugTogglePredictiveRender:
            TogglePredictiveRender(win);
            break;

        case CmdToggleLinks:
            gGlobalPrefs->showLinks = !gGlobalPrefs->showLinks;
            for (auto& w : gWindows) {
                w->RedrawAll(true);
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
            f.stressTestPath = str::Dup("D:\\sumstress");
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

        case CmdTogglePageInfo: {
            // "page info" tip: make figuring out current page and
            // total pages count a one-key action (unless they're already visible)
            TogglePageInfoHelper(win);
        } break;

        case CmdInvertColors: {
            gGlobalPrefs->fixedPageUI.invertColors ^= true;
            UpdateDocumentColors();
            UpdateControlsColors(win);
            // UpdateUiForCurrentTab(win);
            SaveSettings();
            break;
        }

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
                win->ChangePresentationMode(PM_BLACK_SCREEN);
            }
            break;
        case CmdPresentationWhiteBackground:
            if (win->presentation) {
                win->ChangePresentationMode(PM_WHITE_SCREEN);
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

#if 0
        case CmdSelectAnnotation: {
            if (tab) {
                Annotation* annot = GetAnnotionUnderCursor(tab, nullptr);
                if (annot) {
                    SetSelectedAnnotation(tab, annot);
                }
            }
            break;
        }
#endif

        case CmdEditAnnotations: {
            if (tab) {
                Annotation* annot = GetAnnotionUnderCursor(tab, nullptr);
                ShowEditAnnotationsWindow(tab);
                if (annot) {
                    SetSelectedAnnotation(tab, annot);
                }
            }
            break;
        }

        case CmdDeleteAnnotation: {
            if (tab) {
                Annotation* annot = tab->selectedAnnotation;
                if (!annot) {
                    Point pt = HwndGetCursorPos(tab->win->hwndCanvas);
                    if (!pt.IsEmpty()) {
                        annot = dm->GetAnnotationAtPos(pt, nullptr);
                    }
                }
                if (annot) {
                    DeleteAnnotationAndUpdateUI(tab, annot);
                }
            }
        } break;

        // TODO: make it closer to handling in OnWindowContextMenu()
        case CmdCreateAnnotHighlight:
            [[fallthrough]];
        case CmdCreateAnnotSquiggly:
            [[fallthrough]];
        case CmdCreateAnnotStrikeOut:
            [[fallthrough]];
        case CmdCreateAnnotUnderline:
            if (win && tab) {
                AnnotCreateArgs args{annotType};
                SetAnnotCreateArgs(args, cmd);
                auto annot = MakeAnnotationsFromSelection(tab, &args);
                if (annot) {
                    bool openEdit = GetCommandBoolArg(cmd, kCmdArgOpenEdit, IsShiftPressed());
                    if (openEdit) {
                        ShowEditAnnotationsWindow(tab);
                        SetSelectedAnnotation(tab, annot);
                    }
                }
            }
            break;

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
            if (!dm) {
                return 0;
            }
            EngineBase* engine = dm->GetEngine();
            if (!engine) {
                return 0;
            }
            bool handle = !win->isFullScreen && EngineSupportsAnnotations(engine);
            if (!handle) {
                return 0;
            }
            Point pt = HwndGetCursorPos(hwnd);
            int pageNoUnderCursor = dm->GetPageNoByPoint(pt);
            if (pageNoUnderCursor < 0) {
                return 0;
            }
            PointF ptOnPage = dm->CvtFromScreen(pt, pageNoUnderCursor);
            MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
            AnnotCreateArgs args{annotType};
            SetAnnotCreateArgs(args, cmd);
            lastCreatedAnnot = EngineMupdfCreateAnnotation(engine, pageNoUnderCursor, ptOnPage, &args);
        } break;

        case CmdSelectNextTheme:
            SelectNextTheme();
            break;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    if (lastCreatedAnnot) {
        UpdateAnnotationsList(tab->editAnnotsWindow);
        ShowEditAnnotationsWindow(tab);
        SetSelectedAnnotation(tab, lastCreatedAnnot);
    }
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

HWND gLastActiveFrameHwnd = nullptr;

LRESULT CALLBACK WndProcSumatraFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);

    // DbgLogMsg("frame:", hwnd, msg, wp, lp);
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
            goto InitMouseWheelInfo;

        case WM_SIZE:
            if (win && SIZE_MINIMIZED != wp) {
                RememberDefaultWindowPosition(win);
                int dx = LOWORD(lp);
                int dy = HIWORD(lp);
                // dbglog::LogF("dx: %d, dy: %d", dx, dy);
                FrameOnSize(win, dx, dy);
            }
            break;

        case WM_GETMINMAXINFO:
            return OnFrameGetMinMaxInfo((MINMAXINFO*)lp);

        case WM_MOVE:
            if (win) {
                RememberDefaultWindowPosition(win);
            }
            break;

        case WM_INITMENUPOPUP:
            // TODO: should I just build the menu from scratch every time?
            UpdateAppMenu(win, (HMENU)wp);
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

        case WM_ACTIVATE:
            if (wp != WA_INACTIVE) {
                gLastActiveFrameHwnd = hwnd;
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
            if (win) {
                FrameOnChar(win, wp, lp);
            }
            break;

        case WM_KEYDOWN:
            if (win) {
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

        case WM_SYSCOMMAND:
            // temporarily show the menu bar if it has been hidden
            if (wp == SC_KEYMENU && win && win->isMenuHidden) {
                ToggleMenuBar(win, true);
            }
            return DefWindowProc(hwnd, msg, wp, lp);

        case WM_EXITMENULOOP:
            // hide the menu bar again if it was shown only temporarily
            if (!wp && win && win->isMenuHidden) {
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
            ReportIf(!win->AsFixed());
            // Pass the message to the canvas' window procedure
            // (required since the canvas itself never has the focus and thus
            // never receives WM_MOUSEWHEEL messages)
            return SendMessageW(win->hwndCanvas, msg, wp, lp);

        case WM_CLOSE: {
            if (!win) {
                logf("WM_CLOSE to 0x%p, but didn't find MainWindow for it\n", hwnd);
                ReportIf(true);
            }
            if (CanCloseWindow(win)) {
                CloseWindow(win, true, false);
            }
            break;
        }

        case WM_DESTROY: {
            /* WM_DESTROY is generated by windows when close button is pressed
               or if we explicitly call DestroyWindow()
               It might be sent as a result of File\Close, in which
               case CloseWindow() has already been called. */
            FreeMenuOwnerDrawInfoData(GetMenu(hwnd));
            CloseWindow(win, true, true);
        } break;

        case WM_ENDSESSION:
            // TODO: check for unfinished print jobs in WM_QUERYENDSESSION?
            SaveSettings();
            gDontSaveSettings = true;
            if (wp == TRUE) {
                // we must quit so that we restore opened files on start.
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_DDE_INITIATE:
            if (gPluginMode) {
                break;
            }
            return OnDDEInitiate(hwnd, wp, lp);
        case WM_DDE_EXECUTE:
            return OnDDExecute(hwnd, wp, lp);
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

        case WM_ERASEBKGND:
            return TRUE;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

static TempStr GetFileSizeAsStrTemp(const char* path) {
    i64 fileSize = file::GetSize(path);
    return str::FormatFileSizeTemp(fileSize);
}

void GetProgramInfo(str::Str& s) {
    s.AppendFmt("Crash file: %s\r\n", gCrashFilePath);

    TempStr exePath = GetSelfExePathTemp();
    auto fileSizeExe = GetFileSizeAsStrTemp(exePath);
    s.AppendFmt("Exe: %s %s\r\n", exePath, fileSizeExe);
    if (IsDllBuild()) {
        // show the size of the dll so that we can verify it's the
        // correct size for the given version
        char* dir = path::GetDirTemp(exePath);
        char* dllPath = path::JoinTemp(dir, "libmupdf.dll");
        auto fileSizeDll = GetFileSizeAsStrTemp(dllPath);
        s.AppendFmt("Dll: %s %s\r\n", dllPath, fileSizeDll);
    }

    const char* exeType = IsDllBuild() ? "dll" : "static";
    if (builtOn != nullptr) {
        s.AppendFmt("BuiltOn: %s\n", builtOn);
    }
    s.AppendFmt("Type: %s\r\n", exeType);
    s.AppendFmt("Ver: %s", currentVersion);
    if (gIsPreReleaseBuild) {
        s.AppendFmt(" pre-release");
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
        if (!str::Find(s.Get(), " (dbg)")) {
            s.Append(" (dbg)");
        }
    }
    if (gPluginMode) {
        s.Append(" [plugin]");
    }
    s.Append("\r\n");

    if (gitCommidId != nullptr) {
        s.AppendFmt("Git: %s (https://github.com/sumatrapdfreader/sumatrapdf/commit/%s)\r\n", gitCommidId, gitCommidId);
    }
}

bool CrashHandlerCanUseNet() {
    return HasPermission(Perm::InternetAccess);
}

void ShowCrashHandlerMessage() {
    log("ShowCrashHandlerMessage()\n");
    // don't show a message box in restricted use, as the user most likely won't be
    // able to do anything about it anyway and it's up to the application provider
    // to fix the unexpected behavior (of which for a restricted set of documents
    // there should be much less, anyway)
    if (!CanAccessDisk()) {
        log("ShowCrashHandlerMessage: skipping beacuse !CanAccessDisk()\n");
        return;
    }

#if 0
    int res = MsgBox(nullptr, _TRA("Sorry, that shouldn't have happened!\n\nPlease press 'Cancel', if you want to help us fix the cause of this crash."), _TRA("SumatraPDF crashed"), MB_ICONERROR | MB_OKCANCEL | MbRtlReadingMaybe());
    if (IDCANCEL == res) {
        LaunchBrowser(CRASH_REPORT_URL);
    }
#endif

    const char* msg = "We're sorry, SumatraPDF crashed.\n\nPress 'Cancel' to see crash report.";
    uint flags = MB_ICONERROR | MB_OK | MB_OKCANCEL | MbRtlReadingMaybe();
    flags |= MB_SETFOREGROUND | MB_TOPMOST;

    int res = MsgBox(nullptr, msg, "SumatraPDF crashed", flags);
    if (IDCANCEL != res) {
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

void ShutdownCleanup() {
    gAllowedFileTypes.Reset();
    gAllowedLinkProtocols.Reset();
}
