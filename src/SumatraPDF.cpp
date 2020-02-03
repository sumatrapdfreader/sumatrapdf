/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/CryptoUtil.h"
#include "utils/DirIter.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/FileWatcher.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HttpUtil.h"
#include "utils/SquareTreeParser.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"
#include "utils/GdiPlusUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"
#include "wingui/SplitterWnd.h"
#include "wingui/LabelWithCloseWnd.h"
#include "wingui/FrameRateWnd.h"
#include "wingui/TooltipCtrl.h"
#include "wingui/DropDownCtrl.h"

#include "EngineBase.h"
#include "EnginePs.h"
#include "EngineManager.h"
#include "EngineImages.h"
#include "Doc.h"
#include "FileModifications.h"
#include "PdfCreator.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "DisplayModel.h"
#include "EbookController.h"
#include "FileHistory.h"
#include "PdfSync.h"
#include "RenderCache.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "AppColors.h"
#include "SumatraPDF.h"
#include "Notifications.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "Flags.h"
#include "AppPrefs.h"
#include "AppTools.h"
#include "AppUtil.h"
#include "Canvas.h"
#include "Caption.h"
#include "CrashHandler.h"
#include "ExternalViewers.h"
#include "Favorites.h"
#include "FileThumbnails.h"
#include "Menu.h"
#include "Print.h"
#include "Search.h"
#include "Selection.h"
#include "StressTesting.h"
#ifdef ENABLE_ALTERNATIVE_ABOUT_DIALOG
#include "SumatraAbout2.h"
#else
#include "SumatraAbout.h"
#endif
#include "SumatraDialogs.h"
#include "SumatraProperties.h"
#include "TableOfContents.h"
#include "Tabs.h"
#include "Toolbar.h"
#include "Translations.h"
#include "uia/Provider.h"
#include "Version.h"
#include "SumatraConfig.h"

/* if true, we're in debug mode where we show links as blue rectangle on
   the screen. Makes debugging code related to links easier. */
#if defined(DEBUG)
bool gDebugShowLinks = true;
#else
bool gDebugShowLinks = false;
#endif

// used to show it in debug, but is not very useful,
// so always disable
bool gShowFrameRate = false;

// in plugin mode, the window's frame isn't drawn and closing and
// fullscreen are disabled, so that SumatraPDF can be displayed
// embedded (e.g. in a web browser)
const WCHAR* gPluginURL = nullptr; // owned by Flags in WinMain

static NotificationGroupId NG_PERSISTENT_WARNING = "persistentWarning";
static NotificationGroupId NG_PAGE_INFO_HELPER = "pageInfoHelper";

#define SPLITTER_DX 5
#define SIDEBAR_MIN_WIDTH 150

#define SPLITTER_DY 4
#define TOC_MIN_DY 100

// minimum size of the window
constexpr LONG MIN_WIN_DX = 480;
constexpr LONG MIN_WIN_DY = 320;

std::vector<WindowInfo*> gWindows;
FileHistory gFileHistory;
Favorites gFavorites;

HBITMAP gBitmapReloadingCue;
RenderCache gRenderCache;
HCURSOR gCursorDrag;

// set after mouse shortcuts involving the Alt key (so that the menu bar isn't activated)
bool gSuppressAltKey = false;

bool gCrashOnOpen = false;

// in restricted mode, some features can be disabled (such as
// opening files, printing, following URLs), so that SumatraPDF
// can be used as a PDF reader on locked down systems
static int gPolicyRestrictions = Perm_RestrictedUse;
// only the listed protocols will be passed to the OS for
// opening in e.g. a browser or an email client (ignored,
// if gPolicyRestrictions doesn't contain Perm_DiskAccess)
static WStrVec gAllowedLinkProtocols;
// only files of the listed perceived types will be opened
// externally by LinkHandler::LaunchFile (i.e. when clicking
// on an in-document link); examples: "audio", "video", ...
static WStrVec gAllowedFileTypes;

static void CloseDocumentInTab(WindowInfo*, bool keepUIEnabled = false, bool deleteModel = false);
static void UpdatePageInfoHelper(WindowInfo*, NotificationWnd* wnd = nullptr, int pageNo = -1);
static void OnSidebarSplitterMove(SplitterMoveArgs*);
static void OnFavSplitterMove(SplitterMoveArgs*);
static void DownloadDebugSymbols();

void SetCurrentLang(const char* langCode) {
    if (!langCode) {
        return;
    }
    if (langCode != gGlobalPrefs->uiLanguage) {
        str::ReplacePtr(&gGlobalPrefs->uiLanguage, langCode);
    }
    trans::SetCurrentLangByCode(langCode);
}

#ifndef SUMATRA_UPDATE_INFO_URL
#if 0 // defined(PRE_RELEASE_VER)
#define SUMATRA_UPDATE_INFO_URL \
    L"https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/sumpdf-prerelease-latest.txt"
#else
#define SUMATRA_UPDATE_INFO_URL \
    L"https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/sumpdf-prerelease-update.txt"
#endif
#endif

#ifndef SVN_UPDATE_LINK
#ifdef PRE_RELEASE_VER
#define SVN_UPDATE_LINK L"https://www.sumatrapdfreader.org/prerelease.html"
#else
#define SVN_UPDATE_LINK L"https://www.sumatrapdfreader.org/download-free-pdf-viewer.html"
#endif
#endif

#define SECS_IN_DAY 60 * 60 * 24

#define RESTRICTIONS_FILE_NAME L"sumatrapdfrestrict.ini"

#define DEFAULT_LINK_PROTOCOLS L"http,https,mailto"
#define DEFAULT_FILE_PERCEIVED_TYPES L"audio,video,webpage"

void InitializePolicies(bool restrict) {
    // default configuration should be to restrict everything
    CrashIf(gPolicyRestrictions != Perm_RestrictedUse);
    CrashIf(gAllowedLinkProtocols.size() != 0 || gAllowedFileTypes.size() != 0);

    // the -restrict command line flag overrides any sumatrapdfrestrict.ini configuration
    if (restrict)
        return;

    // allow to restrict SumatraPDF's functionality from an INI file in the
    // same directory as SumatraPDF.exe (cf. ../docs/sumatrapdfrestrict.ini)
    // (if the file isn't there, everything is allowed)
    AutoFreeWstr restrictPath(path::GetPathOfFileInAppDir(RESTRICTIONS_FILE_NAME));
    if (!file::Exists(restrictPath)) {
        gPolicyRestrictions = Perm_All;
        gAllowedLinkProtocols.Split(DEFAULT_LINK_PROTOCOLS, L",");
        gAllowedFileTypes.Split(DEFAULT_FILE_PERCEIVED_TYPES, L",");
        return;
    }

    AutoFree restrictData(file::ReadFile(restrictPath));
    SquareTree sqt(restrictData.data);
    SquareTreeNode* polsec = sqt.root ? sqt.root->GetChild("Policies") : nullptr;
    // if the restriction file is broken, err on the side of full restriction
    if (!polsec) {
        return;
    }

    static struct {
        const char* name;
        int perm;
    } policies[] = {
        {"InternetAccess", Perm_InternetAccess},     {"DiskAccess", Perm_DiskAccess},
        {"SavePreferences", Perm_SavePreferences},   {"RegistryAccess", Perm_RegistryAccess},
        {"PrinterAccess", Perm_PrinterAccess},       {"CopySelection", Perm_CopySelection},
        {"FullscreenAccess", Perm_FullscreenAccess},
    };

    // enable policies as indicated in sumatrapdfrestrict.ini
    for (size_t i = 0; i < dimof(policies); i++) {
        const char* value = polsec->GetValue(policies[i].name);
        if (value && atoi(value) != 0) {
            gPolicyRestrictions = gPolicyRestrictions | policies[i].perm;
        }
    }

    // determine the list of allowed link protocols and perceived file types
    if ((gPolicyRestrictions & Perm_DiskAccess)) {
        const char* value;
        if ((value = polsec->GetValue("LinkProtocols")) != nullptr) {
            AutoFreeWstr protocols = strconv::Utf8ToWstr(value);
            str::ToLowerInPlace(protocols);
            str::TransChars(protocols, L":; ", L",,,");
            gAllowedLinkProtocols.Split(protocols, L",", true);
        }
        if ((value = polsec->GetValue("SafeFileTypes")) != nullptr) {
            AutoFreeWstr protocols = strconv::Utf8ToWstr(value);
            str::ToLowerInPlace(protocols);
            str::TransChars(protocols, L":; ", L",,,");
            gAllowedFileTypes.Split(protocols, L",", true);
        }
    }
}

void RestrictPolicies(int revokePermission) {
    gPolicyRestrictions = (gPolicyRestrictions | Perm_RestrictedUse) & ~revokePermission;
}

bool HasPermission(int permission) {
    return (permission & gPolicyRestrictions) == permission;
}

// lets the shell open a URI for any supported scheme in
// the appropriate application (web browser, mail client, etc.)
bool LaunchBrowser(const WCHAR* url) {
    if (gPluginMode) {
        // pass the URI back to the browser
        CrashIf(gWindows.empty());
        if (gWindows.empty()) {
            return false;
        }
        HWND plugin = gWindows.at(0)->hwndFrame;
        HWND parent = GetAncestor(plugin, GA_PARENT);
        AutoFree urlUtf8(strconv::WstrToUtf8(url));
        if (!parent || !urlUtf8.Get() || (urlUtf8.size() > 4096)) {
            return false;
        }
        COPYDATASTRUCT cds = {0x4C5255 /* URL */, (DWORD)urlUtf8.size() + 1, urlUtf8.Get()};
        return SendMessage(parent, WM_COPYDATA, (WPARAM)plugin, (LPARAM)&cds);
    }

    if (!HasPermission(Perm_DiskAccess))
        return false;

    // check if this URL's protocol is allowed
    AutoFreeWstr protocol;
    if (!str::Parse(url, L"%S:", &protocol))
        return false;
    str::ToLowerInPlace(protocol);
    if (!gAllowedLinkProtocols.Contains(protocol))
        return false;

    return LaunchFile(url, nullptr, L"open");
}

// lets the shell open a file of any supported perceived type
// in the default application for opening such files
bool OpenFileExternally(const WCHAR* path) {
    if (!HasPermission(Perm_DiskAccess) || gPluginMode)
        return false;

    // check if this file's perceived type is allowed
    const WCHAR* ext = path::GetExtNoFree(path);
    AutoFreeWstr perceivedType(ReadRegStr(HKEY_CLASSES_ROOT, ext, L"PerceivedType"));
    // since we allow following hyperlinks, also allow opening local webpages
    if (str::EndsWithI(path, L".htm") || str::EndsWithI(path, L".html") || str::EndsWithI(path, L".xhtml"))
        perceivedType.SetCopy(L"webpage");
    str::ToLowerInPlace(perceivedType);
    if (gAllowedFileTypes.Contains(L"*"))
        /* allow all file types (not recommended) */;
    else if (!perceivedType || !gAllowedFileTypes.Contains(perceivedType))
        return false;

    // TODO: only do this for trusted files (cf. IsUntrustedFile)?
    return LaunchFile(path);
}

void SwitchToDisplayMode(WindowInfo* win, DisplayMode displayMode, bool keepContinuous) {
    if (!win->IsDocLoaded()) {
        return;
    }

    win->ctrl->SetDisplayMode(displayMode, keepContinuous);
    UpdateToolbarState(win);
}

WindowInfo* FindWindowInfoByHwnd(HWND hwnd) {
    HWND parent = GetParent(hwnd);
    for (size_t i = 0; i < gWindows.size(); i++) {
        WindowInfo* win = gWindows.at(i);
        if (hwnd == win->hwndFrame)
            return win;
        if (!parent)
            continue;
        if ( // canvas, toolbar, rebar, tocbox, splitters
            parent == win->hwndFrame ||
            // infotips, message windows
            parent == win->hwndCanvas ||
            // page and find labels and boxes
            parent == win->hwndToolbar ||
            // ToC tree, sidebar title and close button
            parent == win->hwndTocBox ||
            // Favorites tree, title, and close button
            parent == win->hwndFavBox ||
            // tab bar
            parent == win->hwndTabBar ||
            // caption buttons, tab bar
            parent == win->hwndCaption) {
            return win;
        }
    }
    return nullptr;
}

bool WindowInfoStillValid(WindowInfo* win) {
    return vectorContains(gWindows, win);
}

// Find the first window showing a given PDF file
WindowInfo* FindWindowInfoByFile(const WCHAR* file, bool focusTab) {
    AutoFreeWstr normFile(path::Normalize(file));

    for (WindowInfo* win : gWindows) {
        if (!win->IsAboutWindow() && path::IsSame(win->currentTab->filePath, normFile)) {
            return win;
        }
        if (focusTab && win->tabs.size() > 1) {
            // bring a background tab to the foreground
            for (TabInfo* tab : win->tabs) {
                if (tab != win->currentTab && path::IsSame(tab->filePath, normFile)) {
                    TabsSelect(win, win->tabs.Find(tab));
                    return win;
                }
            }
        }
    }
    return nullptr;
}

// Find the first window that has been produced from <file>
WindowInfo* FindWindowInfoBySyncFile(const WCHAR* file, bool focusTab) {
    for (WindowInfo* win : gWindows) {
        Vec<RectI> rects;
        UINT page;
        auto dm = win->AsFixed();
        if (dm && dm->pdfSync && dm->pdfSync->SourceToDoc(file, 0, 0, &page, rects) != PDFSYNCERR_UNKNOWN_SOURCEFILE) {
            return win;
        }
        if (focusTab && win->tabs.size() > 1) {
            // bring a background tab to the foreground
            for (TabInfo* tab : win->tabs) {
                if (tab != win->currentTab && tab->AsFixed() && tab->AsFixed()->pdfSync &&
                    tab->AsFixed()->pdfSync->SourceToDoc(file, 0, 0, &page, rects) != PDFSYNCERR_UNKNOWN_SOURCEFILE) {
                    TabsSelect(win, win->tabs.Find(tab));
                    return win;
                }
            }
        }
    }
    return nullptr;
}

WindowInfo* FindWindowInfoByTab(TabInfo* tab) {
    auto fn = [&](WindowInfo* win) -> bool { return win->tabs.Contains(tab); };
    // TODO: make it into a function
    auto b = std::begin(gWindows);
    auto e = std::end(gWindows);
    if (auto res = std::find_if(b, e, fn); res != e) {
        return *res;
    }
    return nullptr;
}

WindowInfo* FindWindowInfoByController(Controller* ctrl) {
    auto fn = [&](WindowInfo* win) {
        return win->tabs.FindEl([&](TabInfo* tab) { return tab->ctrl == ctrl; }) != NULL;
    };
    auto b = std::begin(gWindows);
    auto e = std::end(gWindows);
    if (auto res = std::find_if(b, e, fn); res != e) {
        return *res;
    }
    return nullptr;
}

class HwndPasswordUI : public PasswordUI {
    HWND hwnd;
    size_t pwdIdx;

  public:
    explicit HwndPasswordUI(HWND hwnd) : hwnd(hwnd), pwdIdx(0) {
    }

    WCHAR* GetPassword(const WCHAR* fileName, unsigned char* fileDigest, unsigned char decryptionKeyOut[32],
                       bool* saveKey) override;
};

/* Get password for a given 'fileName', can be nullptr if user cancelled the
   dialog box or if the encryption key has been filled in instead.
   Caller needs to free() the result. */
WCHAR* HwndPasswordUI::GetPassword(const WCHAR* fileName, unsigned char* fileDigest, unsigned char decryptionKeyOut[32],
                                   bool* saveKey) {
    DisplayState* fileFromHistory = gFileHistory.Find(fileName, nullptr);
    if (fileFromHistory && fileFromHistory->decryptionKey) {
        AutoFree fingerprint(str::MemToHex(fileDigest, 16));
        *saveKey = str::StartsWith(fileFromHistory->decryptionKey, fingerprint.Get());
        if (*saveKey && str::HexToMem(fileFromHistory->decryptionKey + 32, decryptionKeyOut, 32)) {
            return nullptr;
        }
    }

    *saveKey = false;

    // try the list of default passwords before asking the user
    if (pwdIdx < gGlobalPrefs->defaultPasswords->size())
        return str::Dup(gGlobalPrefs->defaultPasswords->at(pwdIdx++));

    if (IsStressTesting())
        return nullptr;

    // extract the filename from the URL in plugin mode instead
    // of using the more confusing temporary filename
    AutoFreeWstr urlName;
    if (gPluginMode) {
        urlName.Set(url::GetFileName(gPluginURL));
        if (urlName)
            fileName = urlName;
    }
    fileName = path::GetBaseNameNoFree(fileName);

    // check if the window is still valid as it might have been closed by now
    if (!IsWindow(hwnd)) {
        CrashIf(true);
        hwnd = GetForegroundWindow();
    }
    // make sure that the password dialog is visible
    win::ToForeground(hwnd);

    bool* rememberPwd = gGlobalPrefs->rememberOpenedFiles ? saveKey : nullptr;
    return Dialog_GetPassword(hwnd, fileName, rememberPwd);
}

// update global windowState for next default launch when either
// no pdf is opened or a document without window dimension information
static void RememberDefaultWindowPosition(WindowInfo* win) {
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

static void UpdateDisplayStateWindowRect(WindowInfo* win, DisplayState& ds, bool updateGlobal = true) {
    if (updateGlobal) {
        RememberDefaultWindowPosition(win);
    }

    ds.windowState = gGlobalPrefs->windowState;
    ds.windowPos = gGlobalPrefs->windowPos;
    ds.sidebarDx = gGlobalPrefs->sidebarDx;
}

static void UpdateSidebarDisplayState(WindowInfo* win, TabInfo* tab, DisplayState* ds) {
    CrashIf(!tab);

    ds->showToc = tab->showToc;
    if (win->tocLoaded && tab == win->currentTab) {
        TocTree* tocTree = tab->ctrl->GetToc();
        UpdateTocExpansionState(tab->tocState, win->tocTreeCtrl, tocTree);
    }
    *ds->tocState = tab->tocState;
}

void UpdateTabFileDisplayStateForWin(WindowInfo* win, TabInfo* tab) {
    RememberDefaultWindowPosition(win);
    if (!tab || !tab->ctrl) {
        return;
    }
    DisplayState* ds = gFileHistory.Find(tab->filePath, nullptr);
    if (!ds) {
        return;
    }
    tab->ctrl->GetDisplayState(ds);
    UpdateDisplayStateWindowRect(win, *ds, false);
    UpdateSidebarDisplayState(win, tab, ds);
}

bool IsUIRightToLeft() {
    return trans::IsCurrLangRtl();
}

UINT MbRtlReadingMaybe() {
    if (IsUIRightToLeft())
        return MB_RTLREADING;
    return 0;
}

void MessageBoxWarning(HWND hwnd, const WCHAR* msg, const WCHAR* title) {
    UINT type = MB_OK | MB_ICONEXCLAMATION | MbRtlReadingMaybe();
    if (!title)
        title = _TR("Warning");
    MessageBox(hwnd, msg, title, type);
}

// updates the layout for a window to either left-to-right or right-to-left
// depending on the currently used language (cf. IsUIRightToLeft)
static void UpdateWindowRtlLayout(WindowInfo* win) {
    bool isRTL = IsUIRightToLeft();
    bool wasRTL = (GetWindowLong(win->hwndFrame, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
    if (wasRTL == isRTL)
        return;

    bool tocVisible = win->tocVisible;
    bool favVisible = gGlobalPrefs->showFavorites;
    if (tocVisible || favVisible)
        SetSidebarVisibility(win, false, false);

    // https://www.microsoft.com/middleeast/msdn/mirror.aspx
    SetRtl(win->hwndFrame, isRTL);

    SetRtl(win->hwndTocBox, isRTL);
    HWND tocBoxTitle = win->tocLabelWithClose->hwnd;
    SetRtl(tocBoxTitle, isRTL);
    if (win->altBookmarks) {
        SetRtl(win->altBookmarks->hwnd, isRTL);
    }

    SetRtl(win->hwndFavBox, isRTL);
    HWND favBoxTitle = win->favLabelWithClose->hwnd;
    SetRtl(favBoxTitle, isRTL);
    win->favTreeCtrl->SetRtl(isRTL);

    SetRtl(win->hwndReBar, isRTL);
    SetRtl(win->hwndToolbar, isRTL);
    SetRtl(win->hwndFindBox, isRTL);
    SetRtl(win->hwndFindText, isRTL);
    SetRtl(win->hwndPageText, isRTL);

    SetRtl(win->hwndCaption, isRTL);
    SetCaptionButtonsRtl(win->caption, isRTL);

    // TODO: why isn't SetWindowPos(..., SWP_FRAMECHANGED) enough?
    SendMessage(win->hwndFrame, WM_DWMCOMPOSITIONCHANGED, 0, 0);
    RelayoutCaption(win);
    // TODO: make tab bar RTL aware
    // SetRtl(win->hwndTabBar, isRTL);

    win->notifications->Relayout();

    // TODO: also update the canvas scrollbars (?)

    // ensure that the ToC sidebar is on the correct side and that its
    // title and close button are also correctly laid out
    if (tocVisible || favVisible) {
        SetSidebarVisibility(win, tocVisible, favVisible);
        if (tocVisible) {
            SendMessage(win->hwndTocBox, WM_SIZE, 0, 0);
        }
        if (favVisible) {
            SendMessage(win->hwndFavBox, WM_SIZE, 0, 0);
        }
    }
}

void RebuildMenuBarForWindow(WindowInfo* win) {
    HMENU oldMenu = win->menu;
    win->menu = BuildMenu(win);
    if (!win->presentation && !win->isFullScreen && !win->isMenuHidden) {
        SetMenu(win->hwndFrame, win->menu);
    }
    FreeMenuOwnerDrawInfoData(oldMenu);
    DestroyMenu(oldMenu);
}

static bool ShouldSaveThumbnail(DisplayState& ds) {
    // don't create thumbnails if we won't be needing them at all
    if (!HasPermission(Perm_SavePreferences))
        return false;

    // don't create thumbnails for files that won't need them anytime soon
    Vec<DisplayState*> list;
    gFileHistory.GetFrequencyOrder(list);
    int idx = list.Find(&ds);
    if (idx < 0 || FILE_HISTORY_MAX_FREQUENT * 2 <= idx)
        return false;

    if (HasThumbnail(ds))
        return false;
    return true;
}

// TODO: replace with std::function
class ThumbnailRenderingTask : public RenderingCallback {
    std::function<void(RenderedBitmap*)> saveThumbnail;

  public:
    explicit ThumbnailRenderingTask(const std::function<void(RenderedBitmap*)>& saveThumbnail)
        : saveThumbnail(saveThumbnail) {
    }
    ~ThumbnailRenderingTask() {
    }

    virtual void Callback(RenderedBitmap* bmp) {
        saveThumbnail(bmp);
        delete this;
    }
};

class ControllerCallbackHandler : public ControllerCallback {
    WindowInfo* win;

  public:
    ControllerCallbackHandler(WindowInfo* win) : win(win) {
    }
    ~ControllerCallbackHandler() override {
    }

    void Repaint() override {
        win->RepaintAsync();
    }
    void PageNoChanged(Controller* ctrl, int pageNo) override;
    void UpdateScrollbars(SizeI canvas) override;
    void RequestRendering(int pageNo) override;
    void CleanUp(DisplayModel* dm) override;
    void RenderThumbnail(DisplayModel* dm, SizeI size, const onBitmapRenderedCb&) override;
    void GotoLink(PageDestination* dest) override {
        win->linkHandler->GotoLink(dest);
    }
    void FocusFrame(bool always) override;
    void SaveDownload(const WCHAR* url, std::string_view data) override;
    void HandleLayoutedPages(EbookController* ctrl, EbookFormattingData* data) override;
    void RequestDelayedLayout(int delay) override;
};

void ControllerCallbackHandler::RenderThumbnail(DisplayModel* dm, SizeI size, const onBitmapRenderedCb& saveThumbnail) {
    RectD pageRect = dm->GetEngine()->PageMediabox(1);
    if (pageRect.IsEmpty()) {
        // saveThumbnail must always be called for clean-up code
        saveThumbnail(nullptr);
        return;
    }

    pageRect = dm->GetEngine()->Transform(pageRect, 1, 1.0f, 0);
    float zoom = size.dx / (float)pageRect.dx;
    if (pageRect.dy > (float)size.dy / zoom)
        pageRect.dy = (float)size.dy / zoom;
    pageRect = dm->GetEngine()->Transform(pageRect, 1, 1.0f, 0, true);

    RenderingCallback* callback = new ThumbnailRenderingTask(saveThumbnail);
    gRenderCache.Render(dm, 1, 0, zoom, pageRect, *callback);
}

static void CreateThumbnailForFile(WindowInfo* win, DisplayState& ds) {
    if (!ShouldSaveThumbnail(ds)) {
        return;
    }

    CrashIf(!win->IsDocLoaded());
    if (!win->IsDocLoaded()) {
        return;
    }

    // don't create thumbnails for password protected documents
    // (unless we're also remembering the decryption key anyway)
    auto* model = win->AsFixed();
    if (model) {
        auto* engine = model->GetEngine();
        bool withPwd = engine->IsPasswordProtected();
        AutoFree decrKey(engine->GetDecryptionKey());
        if (withPwd && !decrKey) {
            RemoveThumbnail(ds);
            return;
        }
    }

    WCHAR* filePath = str::Dup(win->ctrl->FilePath());
    win->ctrl->CreateThumbnail(SizeI(THUMBNAIL_DX, THUMBNAIL_DY), [=](RenderedBitmap* bmp) {
        uitask::Post([=] {
            if (bmp) {
                SetThumbnail(gFileHistory.Find(filePath, nullptr), bmp);
            }
            free(filePath);
        });
    });
}

/* Send the request to render a given page to a rendering thread */
void ControllerCallbackHandler::RequestRendering(int pageNo) {
    CrashIf(!win->AsFixed());
    if (!win->AsFixed()) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    // don't render any plain images on the rendering thread,
    // they'll be rendered directly in DrawDocument during
    // WM_PAINT on the UI thread
    if (dm->ShouldCacheRendering(pageNo)) {
        gRenderCache.RequestRendering(dm, pageNo);
    }
}

void ControllerCallbackHandler::CleanUp(DisplayModel* dm) {
    gRenderCache.CancelRendering(dm);
    gRenderCache.FreeForDisplayModel(dm);
}

void ControllerCallbackHandler::FocusFrame(bool always) {
    if (always || !FindWindowInfoByHwnd(GetFocus())) {
        SetFocus(win->hwndFrame);
    }
}

void ControllerCallbackHandler::SaveDownload(const WCHAR* url, std::string_view data) {
    AutoFreeWstr fileName(url::GetFileName(url));
    // LinkSaver linkSaver(win->currentTab, win->hwndFrame, fileName);
    SaveDataToFile(win->hwndFrame, fileName, data);
}

void ControllerCallbackHandler::HandleLayoutedPages(EbookController* ctrl, EbookFormattingData* data) {
    uitask::Post([=] {
        if (FindWindowInfoByController(ctrl)) {
            ctrl->HandlePagesFromEbookLayout(data);
        } else {
            // don't leak data if ctrl has already been deleted
            EbookController::DeleteEbookFormattingData(data);
        }
    });
}

void ControllerCallbackHandler::RequestDelayedLayout(int delay) {
    SetTimer(win->hwndCanvas, EBOOK_LAYOUT_TIMER_ID, delay, nullptr);
}

void ControllerCallbackHandler::UpdateScrollbars(SizeI canvas) {
    CrashIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();

    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;

    SizeI viewPort = dm->GetViewPort().Size();

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

        if (ZOOM_FIT_PAGE != dm->GetZoomVirtual()) {
            // keep the top/bottom 5% of the previous page visible after paging down/up
            si.nPage = (UINT)(si.nPage * 0.95);
            si.nMax -= viewPort.dy - si.nPage;
        }
    }
    ShowScrollBar(win->hwndCanvas, SB_VERT, viewPort.dy < canvas.dy);
    SetScrollInfo(win->hwndCanvas, SB_VERT, &si, TRUE);
}

// The current page edit box is updated with the current page number
void ControllerCallbackHandler::PageNoChanged(Controller* ctrl, int pageNo) {
    // discard page number change requests from documents
    // loaded asynchronously in a background tab
    if (win->ctrl != ctrl) {
        return;
    }

    CrashIf(!win->ctrl || win->ctrl->PageCount() <= 0);
    if (!win->ctrl || win->ctrl->PageCount() == 0) {
        return;
    }

    if (win->AsEbook())
        pageNo = win->AsEbook()->CurrentTocPageNo();
    else if (INVALID_PAGE_NO != pageNo) {
        AutoFreeWstr buf(win->ctrl->GetPageLabel(pageNo));
        win::SetText(win->hwndPageBox, buf);
        ToolbarUpdateStateForWindow(win, false);
        if (win->ctrl->HasPageLabels())
            UpdateToolbarPageText(win, win->ctrl->PageCount(), true);
    }
    if (pageNo == win->currPageNo)
        return;

    UpdateTocSelection(win, pageNo);
    win->currPageNo = pageNo;

    NotificationWnd* wnd = win->notifications->GetForGroup(NG_PAGE_INFO_HELPER);
    if (wnd) {
        CrashIf(!win->AsFixed());
        UpdatePageInfoHelper(win, wnd, pageNo);
    }
}

static Controller* CreateControllerForFile(const WCHAR* filePath, PasswordUI* pwdUI, WindowInfo* win) {
    logf(L"CreateControllerForFile: '%s'\n", filePath);
    if (!win->cbHandler) {
        win->cbHandler = new ControllerCallbackHandler(win);
    }

    Controller* ctrl = nullptr;

    bool enableChmAndEbook = gGlobalPrefs->chmUI.useFixedPageUI;
    // enableChmAndEbook = true;
    EngineBase* engine = EngineManager::CreateEngine(filePath, pwdUI, enableChmAndEbook, enableChmAndEbook);

    if (engine) {
    LoadEngineInFixedPageUI:
        ctrl = new DisplayModel(engine, win->cbHandler);
        CrashIf(!ctrl || !ctrl->AsFixed() || ctrl->AsChm() || ctrl->AsEbook());
    } else if (ChmModel::IsSupportedFile(filePath) && !gGlobalPrefs->chmUI.useFixedPageUI) {
        ChmModel* chmModel = ChmModel::Create(filePath, win->cbHandler);
        if (chmModel) {
            // make sure that MSHTML can't be used as a potential exploit
            // vector through another browser and our plugin (which doesn't
            // advertise itself for Chm documents but could be tricked into
            // loading one nonetheless); note: this crash should never happen,
            // since gGlobalPrefs->chmUI.useFixedPageUI is set in SetupPluginMode
            CrashAlwaysIf(gPluginMode);
            // if CLSID_WebBrowser isn't available, fall back on ChmEngine
            if (!chmModel->SetParentHwnd(win->hwndCanvas)) {
                delete chmModel;
                engine = EngineManager::CreateEngine(filePath, pwdUI, true);
                if (!engine) {
                    return nullptr;
                }
                CrashIf(engine->kind != kindEngineChm);
                goto LoadEngineInFixedPageUI;
            }
            // another ChmModel might still be active
            chmModel->RemoveParentHwnd();
            ctrl = chmModel;
        }
        CrashIf(ctrl && (!ctrl->AsChm() || ctrl->AsFixed() || ctrl->AsEbook()));
    } else if (Doc::IsSupportedFile(filePath) && !gGlobalPrefs->ebookUI.useFixedPageUI) {
        Doc doc = Doc::CreateFromFile(filePath);
        if (doc.IsDocLoaded()) {
            ctrl = EbookController::Create(doc, win->hwndCanvas, win->cbHandler, win->frameRateWnd);
        }
        CrashIf(ctrl && (!ctrl->AsEbook() || ctrl->AsFixed() || ctrl->AsChm()));
    }
    if (ctrl && !str::Eq(ctrl->FilePath(), filePath)) {
        // TODO: remove when we figure out why we crash
        auto ctrlFilePath = ctrl->FilePath();
        auto s1 = ctrlFilePath ? strconv::WstrToUtf8(ctrlFilePath).data() : str::Dup("<null>");
        auto s2 = filePath ? strconv::WstrToUtf8(filePath).data() : str::Dup("<null>");
        logf("CreateControllerForFile: ctrl->FilePath: '%s', filePath: '%s'\n", s1, s2);
        CrashIf(ctrl && !str::Eq(ctrl->FilePath(), filePath));
        str::Free(s1);
        str::Free(s2);
    }
    return ctrl;
}

static void SetFrameTitleForTab(TabInfo* tab, bool needRefresh) {
    const WCHAR* titlePath = tab->filePath;
    if (!gGlobalPrefs->fullPathInTitle) {
        titlePath = path::GetBaseNameNoFree(titlePath);
    }

    AutoFreeWstr docTitle(str::Dup(L""));
    if (tab->ctrl) {
        WCHAR* title = tab->ctrl->GetProperty(DocumentProperty::Title);
        if (title != nullptr) {
            str::NormalizeWS(title);
            docTitle.Set(title);
            if (!str::IsEmpty(title)) {
                docTitle.Set(str::Format(L"- [%s] ", title));
            }
        }
    }

    if (!IsUIRightToLeft()) {
        tab->frameTitle.Set(str::Format(L"%s %s- %s", titlePath, docTitle.get(), SUMATRA_WINDOW_TITLE));
    } else {
        // explicitly revert the title, so that filenames aren't garbled
        tab->frameTitle.Set(str::Format(L"%s %s- %s", SUMATRA_WINDOW_TITLE, docTitle.get(), titlePath));
    }
    if (needRefresh && tab->ctrl) {
        // TODO: this isn't visible when tabs are used
        tab->frameTitle.Set(str::Format(_TR("[Changes detected; refreshing] %s"), tab->frameTitle.get()));
    }
}

static void UpdateUiForCurrentTab(WindowInfo* win) {
    // hide the scrollbars before any other relayouting (for assertion in WindowInfo::GetViewPortSize)
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

    OnMenuFindMatchCase(win);
    UpdateFindbox(win);

    win::SetText(win->hwndFrame, win->currentTab->frameTitle);
    UpdateCurrentTabBgColor(win);

    bool onlyNumbers = !win->ctrl || !win->ctrl->HasPageLabels();
    ToggleWindowStyle(win->hwndPageBox, ES_NUMBER, onlyNumbers);
}

static bool showTocByDefault(const WCHAR* filePath) {
    if (!gGlobalPrefs->showToc) {
        return false;
    }
    // we don't want to show toc by default for comic book files
    bool showByDefault = !IsCbxEngineSupportedFile(filePath);
    return showByDefault;
}

// meaning of the internal values of LoadArgs:
// isNewWindow : if true then 'win' refers to a newly created window that needs
//   to be resized and placed
// placeWindow : if true then the Window will be moved/sized according
//   to the 'state' information even if the window was already placed
//   before (isNewWindow=false)
static void LoadDocIntoCurrentTab(const LoadArgs& args, Controller* ctrl, DisplayState* state) {
    WindowInfo* win = args.win;
    TabInfo* tab = win->currentTab;
    CrashIf(!tab);

    // Never load settings from a preexisting state if the user doesn't wish to
    // (unless we're just refreshing the document, i.e. only if state && !state->useDefaultState)
    if (!state && gGlobalPrefs->rememberStatePerDocument) {
        state = gFileHistory.Find(args.fileName, nullptr);
        if (state) {
            if (state->windowPos.IsEmpty()) {
                state->windowPos = gGlobalPrefs->windowPos;
            }
            EnsureAreaVisibility(state->windowPos);
        }
    }
    if (state && state->useDefaultState) {
        state = nullptr;
    }

    DisplayMode displayMode = gGlobalPrefs->defaultDisplayModeEnum;
    float zoomVirtual = gGlobalPrefs->defaultZoomFloat;
    ScrollState ss(1, -1, -1);
    int rotation = 0;
    bool showToc = showTocByDefault(args.fileName);
    bool showAsFullScreen = WIN_STATE_FULLSCREEN == gGlobalPrefs->windowState;
    int showType = SW_NORMAL;
    if (gGlobalPrefs->windowState == WIN_STATE_MAXIMIZED || showAsFullScreen) {
        showType = SW_MAXIMIZE;
    }

    if (state) {
        ss.page = state->pageNo;
        displayMode = prefs::conv::ToDisplayMode(state->displayMode, DM_AUTOMATIC);
        showAsFullScreen = WIN_STATE_FULLSCREEN == state->windowState;
        if (state->windowState == WIN_STATE_NORMAL) {
            showType = SW_NORMAL;
        } else if (state->windowState == WIN_STATE_MAXIMIZED || showAsFullScreen) {
            showType = SW_MAXIMIZE;
        } else if (state->windowState == WIN_STATE_MINIMIZED) {
            showType = SW_MINIMIZE;
        }
        showToc = state->showToc;
        if (win->ctrl && win->presentation) {
            showToc = tab->showTocPresentation;
        }
    }

    AbortFinding(args.win, false);

    Controller* prevCtrl = win->ctrl;
    tab->ctrl = ctrl;
    win->ctrl = tab->ctrl;

    // ToC items might hold a reference to an Engine, so make sure to
    // delete them before destroying the whole DisplayModel
    // (same for linkOnLastButtonDown)
    ClearTocBox(win);
    delete win->linkOnLastButtonDown;
    win->linkOnLastButtonDown = nullptr;

    CrashIf(win->IsAboutWindow() || win->IsDocLoaded() != (win->ctrl != nullptr));
    // TODO: https://code.google.com/p/sumatrapdf/issues/detail?id=1570
    if (win->ctrl) {
        if (win->AsFixed()) {
            DisplayModel* dm = win->AsFixed();
            int dpi = gGlobalPrefs->customScreenDPI;
            if (dpi == 0) {
                dpi = DpiGetForHwnd(win->hwndFrame);
            }
            dm->SetInitialViewSettings(displayMode, ss.page, win->GetViewPortSize(), dpi);
            // TODO: also expose Manga Mode for image folders?
            if (tab->GetEngineType() == kindEngineComicBooks || tab->GetEngineType() == kindEngineImageDir)
                dm->SetDisplayR2L(state ? state->displayR2L : gGlobalPrefs->comicBookUI.cbxMangaMode);
            if (prevCtrl && prevCtrl->AsFixed() && str::Eq(win->ctrl->FilePath(), prevCtrl->FilePath())) {
                gRenderCache.KeepForDisplayModel(prevCtrl->AsFixed(), dm);
                dm->CopyNavHistory(*prevCtrl->AsFixed());
            }
            // reload user annotations
            dm->userAnnots = LoadFileModifications(args.fileName);
            dm->userAnnotsModified = false;
            dm->GetEngine()->UpdateUserAnnotations(dm->userAnnots);
            // tell UI Automation about content change
            if (win->uia_provider)
                win->uia_provider->OnDocumentLoad(dm);
        } else if (win->AsChm()) {
            win->AsChm()->SetParentHwnd(win->hwndCanvas);
            win->ctrl->SetDisplayMode(displayMode);
            ss.page = limitValue(ss.page, 1, win->ctrl->PageCount());
            win->ctrl->GoToPage(ss.page, false);
        } else if (win->AsEbook()) {
            if (prevCtrl && prevCtrl->AsEbook() && str::Eq(win->ctrl->FilePath(), prevCtrl->FilePath()))
                win->ctrl->AsEbook()->CopyNavHistory(*prevCtrl->AsEbook());
        } else
            CrashIf(true);
    } else {
        state = nullptr;
    }
    delete prevCtrl;

    if (state) {
        CrashIf(!win->IsDocLoaded());
        zoomVirtual = prefs::conv::ToZoom(state->zoom, ZOOM_FIT_PAGE);
        if (win->ctrl->ValidPageNo(ss.page)) {
            if (ZOOM_FIT_CONTENT != zoomVirtual) {
                ss.x = state->scrollPos.x;
                ss.y = state->scrollPos.y;
            }
            // else let win->AsFixed()->Relayout() scroll to fit the page (again)
        } else if (win->ctrl->PageCount() > 0) {
            ss.page = limitValue(ss.page, 1, win->ctrl->PageCount());
        }
        // else let win->ctrl->GoToPage(ss.page, false) verify the page number
        rotation = state->rotation;
        tab->tocState = *state->tocState;
    }

    // DisplayModel needs a valid zoom value before any relayout
    // caused by showing/hiding UI elements happends
    if (win->AsFixed()) {
        win->AsFixed()->Relayout(zoomVirtual, rotation);
    } else if (win->IsDocLoaded()) {
        win->ctrl->SetZoomVirtual(zoomVirtual, nullptr);
    }

    // TODO: why is this needed?
    if (!args.isNewWindow && win->IsDocLoaded()) {
        win->RedrawAll();
    }

    SetFrameTitleForTab(tab, false);
    UpdateUiForCurrentTab(win);

    if (win->AsEbook()) {
        // start ebook UI layout after UpdateUiForCurrentTab
        // (prevents the need for an instant re-layout)
        win->AsEbook()->StartLayouting(state ? state->reparseIdx : 0, displayMode);
    }

    if (HasPermission(Perm_DiskAccess) && tab->GetEngineType() == kindEnginePdf) {
        CrashIf(!win->AsFixed() || win->AsFixed()->pdfSync);
        int res = Synchronizer::Create(args.fileName, win->AsFixed()->GetEngine(), &win->AsFixed()->pdfSync);
        // expose SyncTeX in the UI
        if (PDFSYNCERR_SUCCESS == res)
            gGlobalPrefs->enableTeXEnhancements = true;
    }

    bool shouldPlace = args.isNewWindow || args.placeWindow && state;
    if (args.noPlaceWindow) {
        shouldPlace = false;
    }
    if (shouldPlace) {
        if (args.isNewWindow && state && !state->windowPos.IsEmpty()) {
            // Make sure it doesn't have a position like outside of the screen etc.
            RectI rect = ShiftRectToWorkArea(state->windowPos);
            // This shouldn't happen until !win.IsAboutWindow(), so that we don't
            // accidentally update gGlobalState with this window's dimensions
            MoveWindow(win->hwndFrame, rect);
        }
        if (args.showWin) {
            ShowWindow(win->hwndFrame, showType);
        }
        UpdateWindow(win->hwndFrame);
    }

    // if the window isn't shown and win.canvasRc is still empty, zoom
    // has not been determined yet
    // cf. https://code.google.com/p/sumatrapdf/issues/detail?id=2541
    // CrashIf(win->IsDocLoaded() && args.showWin && win->canvasRc.IsEmpty() && !win->AsChm());

    SetSidebarVisibility(win, showToc, gGlobalPrefs->showFavorites);
    // restore scroll state after the canvas size has been restored
    if ((args.showWin || ss.page != 1) && win->AsFixed()) {
        win->AsFixed()->SetScrollState(ss);
    }

    win->RedrawAll(true);
    TabsOnChangedDoc(win);

    if (!win->IsDocLoaded()) {
        return;
    }

    AutoFreeWstr unsupported(win->ctrl->GetProperty(DocumentProperty::UnsupportedFeatures));
    if (unsupported) {
        unsupported.Set(str::Format(_TR("This document uses unsupported features (%s) and might not render properly"),
                                    unsupported.get()));
        win->ShowNotification(unsupported, NOS_WARNING, NG_PERSISTENT_WARNING);
    }

    // This should only happen after everything else is ready
    if ((args.isNewWindow || args.placeWindow) && args.showWin && showAsFullScreen) {
        EnterFullScreen(win);
    }
    if (!args.isNewWindow && win->presentation && win->ctrl) {
        win->ctrl->SetPresentationMode(true);
    }
}

void ReloadDocument(WindowInfo* win, bool autorefresh) {
    TabInfo* tab = win->currentTab;
    if (!win->IsDocLoaded()) {
        if (!autorefresh && tab) {
            LoadArgs args(tab->filePath, win);
            args.forceReuse = true;
            LoadDocument(args);
        }
        return;
    }

    HwndPasswordUI pwdUI(win->hwndFrame);
    Controller* ctrl = CreateControllerForFile(tab->filePath, &pwdUI, win);
    // We don't allow PDF-repair if it is an autorefresh because
    // a refresh event can occur before the file is finished being written,
    // in which case the repair could fail. Instead, if the file is broken,
    // we postpone the reload until the next autorefresh event
    if (!ctrl && autorefresh) {
        SetFrameTitleForTab(tab, true);
        win::SetText(win->hwndFrame, tab->frameTitle);
        return;
    }

    DisplayState* ds = NewDisplayState(tab->filePath);
    tab->ctrl->GetDisplayState(ds);
    UpdateDisplayStateWindowRect(win, *ds);
    UpdateSidebarDisplayState(win, tab, ds);
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
    ds->windowState = wstate;
    ds->useDefaultState = false;

    LoadArgs args(tab->filePath, win);
    args.showWin = true;
    args.placeWindow = false;
    LoadDocIntoCurrentTab(args, ctrl, ds);

    if (!ctrl) {
        DeleteDisplayState(ds);
        return;
    }

    tab->reloadOnFocus = false;

    if (gGlobalPrefs->showStartPage) {
        // refresh the thumbnail for this file
        DisplayState* state = gFileHistory.Find(ds->filePath, nullptr);
        if (state) {
            CreateThumbnailForFile(win, *state);
        }
    }

    if (tab->AsFixed()) {
        // save a newly remembered password into file history so that
        // we don't ask again at the next refresh
        AutoFree decryptionKey(tab->AsFixed()->GetEngine()->GetDecryptionKey());
        if (decryptionKey) {
            DisplayState* state = gFileHistory.Find(ds->filePath, nullptr);
            if (state && !str::Eq(state->decryptionKey, decryptionKey)) {
                free(state->decryptionKey);
                state->decryptionKey = decryptionKey.release();
            }
        }
    }

    DeleteDisplayState(ds);
}

static void CreateSidebar(WindowInfo* win) {
    win->sidebarSplitter = new SplitterCtrl(win->hwndFrame);
    win->sidebarSplitter->type = SplitterType::Vert;
    win->sidebarSplitter->onSplitterMove = OnSidebarSplitterMove;
    bool ok = win->sidebarSplitter->Create();
    CrashIf(!ok);

    CreateToc(win);

    win->favSplitter = new SplitterCtrl(win->hwndFrame);
    win->favSplitter->type = SplitterType::Horiz;
    win->favSplitter->onSplitterMove = OnFavSplitterMove;
    ok = win->favSplitter->Create();
    CrashIf(!ok);

    CreateFavorites(win);

    if (win->tocVisible) {
        RepaintNow(win->hwndTocBox);
    }

    if (gGlobalPrefs->showFavorites) {
        RepaintNow(win->hwndFavBox);
    }
}

static void UpdateToolbarSidebarText(WindowInfo* win) {
    UpdateToolbarPageText(win, -1);
    UpdateToolbarFindText(win);
    UpdateToolbarButtonsToolTipsForWindow(win);

    win->tocLabelWithClose->SetLabel(_TR("Bookmarks"));
    win->favLabelWithClose->SetLabel(_TR("Favorites"));
}

static WindowInfo* CreateWindowInfo() {
    RectI windowPos = gGlobalPrefs->windowPos;
    if (!windowPos.IsEmpty()) {
        EnsureAreaVisibility(windowPos);
    } else {
        windowPos = GetDefaultWindowPos();
    }
    // we don't want the windows to overlap so shift each window by a bit
    int nShift = (int)gWindows.size();
    windowPos.x += (nShift * 15); // TODO: DPI scale

    const WCHAR* clsName = FRAME_CLASS_NAME;
    const WCHAR* title = SUMATRA_WINDOW_TITLE;
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

    CrashIf(nullptr != FindWindowInfoByHwnd(hwndFrame));
    WindowInfo* win = new WindowInfo(hwndFrame);

    // don't add a WS_EX_STATICEDGE so that the scrollbars touch the
    // screen's edge when maximized (cf. Fitts' law) and there are
    // no additional adjustments needed when (un)maximizing
    clsName = CANVAS_CLASS_NAME;
    style = WS_CHILD | WS_HSCROLL | WS_VSCROLL;
    /* position and size determined in OnSize */
    win->hwndCanvas = CreateWindowExW(0, clsName, nullptr, style, 0, 0, 0, 0, hwndFrame, nullptr, h, nullptr);
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

    CrashIf(win->menu);
    win->menu = BuildMenu(win);
    win->isMenuHidden = !gGlobalPrefs->showMenubar;
    if (!win->isMenuHidden) {
        SetMenu(win->hwndFrame, win->menu);
    }

    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    win->infotip = new TooltipCtrl(win->hwndCanvas);
    win->infotip->Create();

    CreateCaption(win);
    CreateTabbar(win);
    CreateToolbar(win);
    CreateSidebar(win);
    UpdateFindbox(win);
    if (HasPermission(Perm_DiskAccess) && !gPluginMode) {
        DragAcceptFiles(win->hwndCanvas, TRUE);
    }

    gWindows.push_back(win);
    // needed for RTL languages
    UpdateWindowRtlLayout(win);
    UpdateToolbarSidebarText(win);

    if (touch::SupportsGestures()) {
        GESTURECONFIG gc = {0, GC_ALLGESTURES, 0};
        touch::SetGestureConfig(win->hwndCanvas, 0, 1, &gc, sizeof(GESTURECONFIG));
    }

    SetTabsInTitlebar(win, gGlobalPrefs->useTabs);

    return win;
}

WindowInfo* CreateAndShowWindowInfo(SessionData* data) {
    // CreateWindowInfo shouldn't change the windowState value
    int windowState = gGlobalPrefs->windowState;
    WindowInfo* win = CreateWindowInfo();
    if (!win) {
        return nullptr;
    }
    CrashIf(windowState != gGlobalPrefs->windowState);

    if (data) {
        windowState = data->windowState;
        RectI rect = ShiftRectToWorkArea(data->windowPos);
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

void DeleteWindowInfo(WindowInfo* win) {
    DeletePropertiesWindow(win->hwndFrame);

    vectorRemove(gWindows, win);

    ImageList_Destroy((HIMAGELIST)SendMessage(win->hwndToolbar, TB_GETIMAGELIST, 0, 0));
    DragAcceptFiles(win->hwndCanvas, FALSE);

    CrashIf(win->findThread && WaitForSingleObject(win->findThread, 0) == WAIT_TIMEOUT);
    CrashIf(win->printThread && WaitForSingleObject(win->printThread, 0) == WAIT_TIMEOUT);

    if (win->uia_provider) {
        // tell UIA to release all objects cached in its store
        uia::ReturnRawElementProvider(win->hwndCanvas, 0, 0, nullptr);
    }

    delete win;
}

static void RenameFileInHistory(const WCHAR* oldPath, const WCHAR* newPath) {
    DisplayState* ds = gFileHistory.Find(newPath, nullptr);
    bool oldIsPinned = false;
    int oldOpenCount = 0;
    if (ds) {
        oldIsPinned = ds->isPinned;
        oldOpenCount = ds->openCount;
        gFileHistory.Remove(ds);
        // TODO: merge favorites as well?
        if (ds->favorites->size() > 0) {
            UpdateFavoritesTreeForAllWindows();
        }
        DeleteDisplayState(ds);
    }
    ds = gFileHistory.Find(oldPath, nullptr);
    if (ds) {
        str::ReplacePtr(&ds->filePath, newPath);
        // merge Frequently Read data, so that a file
        // doesn't accidentally vanish from there
        ds->isPinned = ds->isPinned || oldIsPinned;
        ds->openCount += oldOpenCount;
        // the thumbnail is recreated by LoadDocument
        delete ds->thumbnail;
        ds->thumbnail = nullptr;
    }
}

// document path is either a file or a directory
// (when browsing images inside directory).
bool DocumentPathExists(const WCHAR* path) {
    if (file::Exists(path) || dir::Exists(path))
        return true;
    if (str::FindChar(path + 2, ':')) {
        // remove information needed for pointing at embedded documents
        // (e.g. "C:\path\file.pdf:3:0") to check at least whether the
        // container document exists
        AutoFreeWstr realPath(str::DupN(path, str::FindChar(path + 2, ':') - path));
        return file::Exists(realPath);
    }
    return false;
}

#if 0
// Load a file into a new or existing window, show error message
// if loading failed, set the right window position (based on history
// settings for this file or default position), update file history,
// update frequently read information, generate a thumbnail if necessary
// TODO: write me
static WindowInfo* LoadDocumentNew(LoadArgs& args)
{
    AutoFreeWstr fullPath(path::Normalize(args.fileName));
    // TODO: try to find file on other drives if doesn't exist

    CrashIf(true);
    return nullptr;
}
#endif

void scheduleReloadTab(TabInfo* tab) {
    // to prevent race conditions between file changes and closing tabs,
    // use the tab only on the main UI thread
    uitask::Post([=] {
        WindowInfo* win = FindWindowInfoByTab(tab);
        if (!win)
            return;
        tab->reloadOnFocus = true;
        if (tab == win->currentTab) {
            // delay the reload slightly, in case we get another request immediately after this one
            SetTimer(win->hwndCanvas, AUTO_RELOAD_TIMER_ID, AUTO_RELOAD_DELAY_IN_MS, nullptr);
        }
    });
}

// TODO: eventually I would like to move all loading to be async. To achieve that
// we need clear separatation of loading process into 2 phases: loading the
// file (and showing progress/load failures in topmost window) and placing
// the loaded document in the window (either by replacing document in existing
// window or creating a new window for the document)
WindowInfo* LoadDocument(LoadArgs& args) {
    CrashAlwaysIf(gCrashOnOpen);

    int threadID = (int)GetCurrentThreadId();
    AutoFreeWstr fullPath(path::Normalize(args.fileName));
    WindowInfo* win = args.win;
    {
        AutoFree path = strconv::WstrToUtf8(fullPath);
        logf("LoadDocument: '%s', tid=%d\n", path.get(), threadID);
    }

    bool failEarly = win && !args.forceReuse && !DocumentPathExists(fullPath);
    // try to find inexistent files with history data
    // on a different removable drive before failing
    if (failEarly && gFileHistory.Find(fullPath, nullptr)) {
        AutoFreeWstr adjPath(str::Dup(fullPath));
        if (AdjustVariableDriveLetter(adjPath)) {
            RenameFileInHistory(fullPath, adjPath);
            fullPath.Set(adjPath.StealData());
            failEarly = false;
        }
    }

    // fail with a notification if the file doesn't exist and
    // there is a window the user has just been interacting with
    if (failEarly) {
        AutoFreeWstr msg(str::Format(_TR("File %s not found"), fullPath.get()));
        win->ShowNotification(msg, NOS_HIGHLIGHT);
        // display the notification ASAP (prefs::Save() can introduce a notable delay)
        win->RedrawAll(true);

        if (gFileHistory.MarkFileInexistent(fullPath)) {
            prefs::Save();
            // update the Frequently Read list
            if (1 == gWindows.size() && gWindows.at(0)->IsAboutWindow()) {
                gWindows.at(0)->RedrawAll(true);
            }
        }
        return nullptr;
    }

    bool openNewTab = gGlobalPrefs->useTabs && !args.forceReuse;
    if (openNewTab && !args.win) {
        // modify the args so that we always reuse the same window
        // TODO: enable the tab bar if tabs haven't been initialized
        if (!gWindows.empty()) {
            win = args.win = gWindows.back();
            args.isNewWindow = false;
        }
    }

    if (!win && 1 == gWindows.size() && gWindows.at(0)->IsAboutWindow()) {
        win = gWindows.at(0);
        args.win = win;
        args.isNewWindow = false;
    } else if (!win || !openNewTab && !args.forceReuse && win->IsDocLoaded()) {
        WindowInfo* currWin = win;
        win = CreateWindowInfo();
        if (!win) {
            return nullptr;
        }
        args.win = win;
        args.isNewWindow = true;
        if (currWin) {
            RememberFavTreeExpansionState(currWin);
            win->expandedFavorites = currWin->expandedFavorites;
        }
    }

    HwndPasswordUI pwdUI(win->hwndFrame);
    Controller* ctrl = CreateControllerForFile(fullPath, &pwdUI, win);
    // don't fail if a user tries to load an SMX file instead
    if (!ctrl && IsModificationsFile(fullPath)) {
        *(WCHAR*)path::GetExtNoFree(fullPath) = '\0';
        ctrl = CreateControllerForFile(fullPath, &pwdUI, win);
    }

    if (!ctrl) {
        // TODO: same message as in Canvas.cpp to not introduce
        // new translation. Find a better message e.g. why failed.
        WCHAR* msg = str::Format(_TR("Error loading %s"), fullPath.get());
        win->ShowNotification(msg, NOS_HIGHLIGHT);
        str::Free(msg);
        ShowWindow(win->hwndFrame, SW_SHOW);

        // display the notification ASAP (prefs::Save() can introduce a notable delay)
        win->RedrawAll(true);

        if (gFileHistory.MarkFileInexistent(fullPath)) {
            prefs::Save();
            // update the Frequently Read list
            if (1 == gWindows.size() && gWindows.at(0)->IsAboutWindow()) {
                gWindows.at(0)->RedrawAll(true);
            }
        }
        return win;
    }
    CrashIf(openNewTab && args.forceReuse);
    if (win->IsAboutWindow()) {
        // invalidate the links on the Frequently Read page
        win->staticLinks.Reset();
        // there's no tab to reuse at this point
        args.forceReuse = false;
    } else {
        // TODO: figure out why happens. seen in 2019/12/11/3e06348ed000006.txt
        if (!args.forceReuse && !openNewTab) {
            logf("LoadDocument: got !args.forceReuse && !openNewTab\n");
        }
        SubmitCrashIf(!args.forceReuse && !openNewTab);
        if (openNewTab) {
            SaveCurrentTabInfo(args.win);
        }
        CloseDocumentInTab(win, true, args.forceReuse);
    }
    if (!args.forceReuse) {
        // insert a new tab for the loaded document
        win->currentTab = CreateNewTab(win, fullPath);
        logf("LoadDocument: !forceReuse, created win->currentTab at 0x%p\n", win->currentTab);
    } else {
        win->currentTab->filePath.SetCopy(fullPath);
        AutoFree path = strconv::WstrToUtf8(fullPath);
        logf("LoadDocument: forceReuse, set win->currentTab (0x%p) filePath to '%s'\n", win->currentTab, path.get());
    }

    args.fileName = fullPath;
    // TODO: stop remembering/restoring window positions when using tabs?
    args.placeWindow = !gGlobalPrefs->useTabs;
    LoadDocIntoCurrentTab(args, ctrl, nullptr);

    if (gPluginMode) {
        // hide the menu for embedded documents opened from the plugin
        SetMenu(win->hwndFrame, nullptr);
        return win;
    }

    if (!ctrl) {
        if (gFileHistory.MarkFileInexistent(fullPath)) {
            prefs::Save();
        }
        return win;
    }

    auto currTab = win->currentTab;
    AutoFree path = strconv::WstrToUtf8(currTab->filePath);
    logf("LoadDocument: after LoadDocIntoCurrentTab win->currentTab is 0x%p, path: '%s'\n", currTab, path.get());

    // TODO: figure why we hit this.
    // happens when opening 3 files via "Open With"
    // the first file is loaded via cmd-line arg, the rest
    // via DDE Open command.
    CrashIf(currTab->watcher);

    if (gGlobalPrefs->reloadModifiedDocuments) {
        currTab->watcher = FileWatcherSubscribe(win->currentTab->filePath, [currTab] { scheduleReloadTab(currTab); });
    }

    if (gGlobalPrefs->rememberOpenedFiles) {
        CrashIf(!str::Eq(fullPath, win->currentTab->filePath));
        DisplayState* ds = gFileHistory.MarkFileLoaded(fullPath);
        if (gGlobalPrefs->showStartPage) {
            CreateThumbnailForFile(win, *ds);
        }
        // TODO: this seems to save the state of file that we just opened
        // add a way to skip saving currTab?
        prefs::Save();
    }

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    if (HasPermission(Perm_DiskAccess) && !gPluginMode && !IsStressTesting()) {
        SHAddToRecentDocs(SHARD_PATH, fullPath);
    }

    return win;
}

// Loads document data into the WindowInfo.
void LoadModelIntoTab(WindowInfo* win, TabInfo* tdata) {
    if (!win || !tdata)
        return;

    CloseDocumentInTab(win, true);

    win->currentTab = tdata;
    win->ctrl = tdata->ctrl;

    if (win->AsChm())
        win->AsChm()->SetParentHwnd(win->hwndCanvas);
    // prevent the ebook UI from redrawing before win->RedrawAll at the bottom
    else if (win->AsEbook())
        win->AsEbook()->EnableMessageHandling(false);
    // tell UI Automation about content change
    else if (win->AsFixed() && win->uia_provider)
        win->uia_provider->OnDocumentLoad(win->AsFixed());

    UpdateUiForCurrentTab(win);

    if (win->presentation != PM_DISABLED)
        SetSidebarVisibility(win, tdata->showTocPresentation, gGlobalPrefs->showFavorites);
    else
        SetSidebarVisibility(win, tdata->showToc, gGlobalPrefs->showFavorites);

    if (win->AsFixed()) {
        if (tdata->canvasRc != win->canvasRc)
            win->ctrl->SetViewPortSize(win->GetViewPortSize());
        DisplayModel* dm = win->AsFixed();
        dm->SetScrollState(dm->GetScrollState());
        if (dm->GetPresentationMode() != (win->presentation != PM_DISABLED))
            dm->SetPresentationMode(!dm->GetPresentationMode());
    } else if (win->AsChm()) {
        win->ctrl->GoToPage(win->ctrl->CurrentPageNo(), false);
    } else if (win->AsEbook()) {
        win->AsEbook()->EnableMessageHandling(true);
        if (tdata->canvasRc != win->canvasRc)
            win->ctrl->SetViewPortSize(win->GetViewPortSize());
    }
    tdata->canvasRc = win->canvasRc;

    win->showSelection = tdata->selectionOnPage != nullptr;
    if (win->uia_provider)
        win->uia_provider->OnSelectionChanged();

    SetFocus(win->hwndFrame);
    win->RedrawAll(true);

    if (tdata->reloadOnFocus) {
        tdata->reloadOnFocus = false;
        ReloadDocument(win, true);
    }
}

static void UpdatePageInfoHelper(WindowInfo* win, NotificationWnd* wnd, int pageNo) {
    if (!win->ctrl->ValidPageNo(pageNo))
        pageNo = win->ctrl->CurrentPageNo();
    AutoFreeWstr pageInfo(str::Format(L"%s %d / %d", _TR("Page:"), pageNo, win->ctrl->PageCount()));
    if (win->ctrl->HasPageLabels()) {
        AutoFreeWstr label(win->ctrl->GetPageLabel(pageNo));
        pageInfo.Set(str::Format(L"%s %s (%d / %d)", _TR("Page:"), label.get(), pageNo, win->ctrl->PageCount()));
    }
    if (!wnd) {
        int options = IsShiftPressed() ? NOS_PERSIST : NOS_DEFAULT;
        win->ShowNotification(pageInfo, options, NG_PAGE_INFO_HELPER);
    } else {
        wnd->UpdateMessage(pageInfo);
    }
}

enum class MeasurementUnit { pt, mm, in };

static WCHAR* FormatCursorPosition(EngineBase* engine, PointD pt, MeasurementUnit unit) {
    if (pt.x < 0)
        pt.x = 0;
    if (pt.y < 0)
        pt.y = 0;
    pt.x /= engine->GetFileDPI();
    pt.y /= engine->GetFileDPI();

    // for MeasurementUnit::in
    float factor = 1;
    const WCHAR* unitName = L"in";
    switch (unit) {
        case MeasurementUnit::pt:
            factor = 72;
            unitName = L"pt";
            break;

        case MeasurementUnit::mm:
            factor = 25.4f;
            unitName = L"mm";
            break;
    }

    AutoFreeWstr xPos(str::FormatFloatWithThousandSep(pt.x * factor));
    AutoFreeWstr yPos(str::FormatFloatWithThousandSep(pt.y * factor));
    if (unit != MeasurementUnit::in) {
        // use similar precision for all units
        if (str::IsDigit(xPos[str::Len(xPos) - 2]))
            xPos[str::Len(xPos) - 1] = '\0';
        if (str::IsDigit(yPos[str::Len(yPos) - 2]))
            yPos[str::Len(yPos) - 1] = '\0';
    }
    return str::Format(L"%s x %s %s", xPos.get(), yPos.get(), unitName);
}

void UpdateCursorPositionHelper(WindowInfo* win, PointI pos, NotificationWnd* wnd) {
    static auto unit = MeasurementUnit::pt;
    // toggle measurement unit by repeatedly invoking the helper
    if (!wnd && win->notifications->GetForGroup(NG_CURSOR_POS_HELPER)) {
        switch (unit) {
            case MeasurementUnit::pt:
                unit = MeasurementUnit::mm;
                break;
            case MeasurementUnit::mm:
                unit = MeasurementUnit::in;
                break;
            case MeasurementUnit::in:
                unit = MeasurementUnit::pt;
                break;
            default:
                CrashAlwaysIf(true);
        }
        wnd = win->notifications->GetForGroup(NG_CURSOR_POS_HELPER);
    }

    CrashIf(!win->AsFixed());
    EngineBase* engine = win->AsFixed()->GetEngine();
    PointD pt = win->AsFixed()->CvtFromScreen(pos);
    AutoFreeWstr posStr(FormatCursorPosition(engine, pt, unit)), selStr;
    if (!win->selectionMeasure.IsEmpty()) {
        pt = PointD(win->selectionMeasure.dx, win->selectionMeasure.dy);
        selStr.Set(FormatCursorPosition(engine, pt, unit));
    }

    AutoFreeWstr posInfo(str::Format(L"%s %s", _TR("Cursor position:"), posStr.get()));
    if (selStr) {
        posInfo.Set(str::Format(L"%s - %s %s", posInfo.get(), _TR("Selection:"), selStr.get()));
    }
    if (!wnd) {
        win->ShowNotification(posInfo, NOS_PERSIST, NG_CURSOR_POS_HELPER);
    } else {
        wnd->UpdateMessage(posInfo);
    }
}

void AssociateExeWithPdfExtension() {
    if (!HasPermission(Perm_RegistryAccess)) {
        return;
    }

    DoAssociateExeWithPdfExtension(HKEY_CURRENT_USER);
    DoAssociateExeWithPdfExtension(HKEY_LOCAL_MACHINE);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, 0, 0);

    // Remind the user, when a different application takes over
    str::ReplacePtr(&gGlobalPrefs->associatedExtensions, L".pdf");
    gGlobalPrefs->associateSilently = false;
}

// TODO: actually restore the session on startup depending on
//       gGlobalPrefs->restoreSession
static void RememberSessionState() {
    if (!gGlobalPrefs->rememberOpenedFiles) {
        return;
    }

    ResetSessionState(gGlobalPrefs->sessionData);
    for (auto* win : gWindows) {
        if (win->tabs.size() == 0) {
            continue;
        }
        SessionData* data = NewSessionData();
        for (TabInfo* tab : win->tabs) {
            DisplayState* ds = NewDisplayState(tab->filePath);
            if (tab->ctrl) {
                tab->ctrl->GetDisplayState(ds);
            }
            // TODO: pageNo should be good enough, as canvas size is restored as well
            if (tab->AsEbook() && tab->ctrl) {
                ds->pageNo = tab->ctrl->CurrentPageNo();
            }
            ds->showToc = tab->showToc;
            *ds->tocState = tab->tocState;
            data->tabStates->Append(NewTabState(ds));
            DeleteDisplayState(ds);
        }
        data->tabIndex = win->tabs.Find(win->currentTab) + 1;
        // TODO: allow recording this state without changing gGlobalPrefs
        RememberDefaultWindowPosition(win);
        data->windowState = gGlobalPrefs->windowState;
        data->windowPos = gGlobalPrefs->windowPos;
        data->sidebarDx = gGlobalPrefs->sidebarDx;
        gGlobalPrefs->sessionData->Append(data);
    }
}

#if defined(SUPPORTS_AUTO_UPDATE) && !defined(HAS_PUBLIC_APP_KEY)
#error Auto-update without authentication of the downloaded data is not recommended
#endif

#if defined(SUPPORTS_AUTO_UPDATE) || defined(DEBUG)
static void OnMenuExit();

bool AutoUpdateInitiate(const char* updateData) {
    SquareTree tree(updateData);
    SquareTreeNode* node = tree.root ? tree.root->GetChild("SumatraPDF") : nullptr;
    CrashIf(!node);

    bool installer = HasBeenInstalled();
    SquareTreeNode* data = node->GetChild(installer ? "Installer" : "Portable");
    if (!data)
        return false;
    const char* url = data->GetValue("URL");
    const char* hash = data->GetValue("Hash");
    if (!url || !hash || !str::EndsWithI(url, ".exe"))
        return false;

    AutoFreeWstr exeUrl = strconv::Utf8ToWstr(url);
    HttpRsp rsp;
    if (!HttpGet(exeUrl, &rsp))
        return false;

    unsigned char digest[32];
    CalcSHA2Digest((const unsigned char*)rsp.data.Get(), rsp.data.size(), digest);
    AutoFree fingerPrint(_MemToHex(&digest));
    if (!str::EqI(fingerPrint, hash))
        return false;

    AutoFreeWstr updateExe, updateArgs;
    if (installer) {
        AutoFreeWstr tmpDir(path::GetTempPath());
        updateExe.Set(path::Join(tmpDir, L"SumatraPDF-install-update.exe"));
        // TODO: make the installer delete itself after the update?
        updateArgs.SetCopy(L"-autoupdate");
    } else {
        AutoFreeWstr thisExe(GetExePath());
        updateExe.Set(str::Join(thisExe, L"-update.exe"));
        updateArgs.Set(str::Format(L"-autoupdate replace:\"%s\"", thisExe.get()));
    }

    bool ok = file::WriteFile(updateExe, rsp.data.as_view());
    if (!ok)
        return false;

    // remember currently opened files for reloading after the update
    CrashIf(gGlobalPrefs->reopenOnce->size() > 0);
#if 0
    RememberSessionState();
    gGlobalPrefs->reopenOnce->Append(str::Dup(L"SessionData"));
#else
    for (auto* win : gWindows) {
        for (TabInfo* tab : win->tabs) {
            gGlobalPrefs->reopenOnce->Append(str::Dup(tab->filePath));
        }
    }
#endif
    // save session before launching the installer (which force-quits SumatraPDF)
    if (installer)
        prefs::Save();

    ok = LaunchFile(updateExe, updateArgs);
    if (ok)
        OnMenuExit();
    else {
        gGlobalPrefs->reopenOnce->FreeMembers();
#if 0
        ResetSessionState(gGlobalPrefs->sessionData);
#endif
    }
    return ok;
}
#endif

/* The format used for SUMATRA_UPDATE_INFO_URL looks as follows:

[SumatraPDF]
# the first line must start with SumatraPDF (optionally as INI header)
Latest 2.6
# Latest must be the version number of the version currently offered for download
Stable 2.5.3
# Stable is optional and indicates the oldest version for which automated update
# checks don't yet report the available update

# further information can be added, e.g. the following experimental subkey for
# auto-updating (requires SUPPORTS_AUTO_UPDATE)
Portable [
    URL: <download URL for the uncompressed portable .exe>
    Hash <SHA-256 hash of that file>
]

# to allow safe transmission over http, the file may also be signed:
# Signature sha1:<SHA-1 signature to be verified using IDD_PUBLIC_APP_KEY>
*/

static DWORD ShowAutoUpdateDialog(HWND hParent, HttpRsp* rsp, bool silent) {
    if (rsp->error != 0) {
        return rsp->error;
    }
    if (rsp->httpStatusCode != 200) {
        return ERROR_INTERNET_INVALID_URL;
    }
    if (!str::StartsWith(rsp->url.Get(), SUMATRA_UPDATE_INFO_URL)) {
        return ERROR_INTERNET_INVALID_URL;
    }
    str::Str* data = &rsp->data;
    if (0 == data->size()) {
        return ERROR_INTERNET_CONNECTION_ABORTED;
    }

    // See https://code.google.com/p/sumatrapdf/issues/detail?id=725
    // If a user configures os-wide proxy that is not regular ie proxy
    // (which we pick up) we might get complete garbage in response to
    // our query. Make sure to check whether the returned data is sane.
    if (!str::StartsWith(data->Get(), '[' == data->at(0) ? "[SumatraPDF]" : "SumatraPDF")) {
        return ERROR_INTERNET_LOGIN_FAILURE;
    }

#ifdef HAS_PUBLIC_APP_KEY
    std::string_view pubkey = LoadDataResource(IDD_PUBLIC_APP_KEY);
    CrashIf(pubkey.empty());
    bool ok = VerifySHA1Signature(data->Get(), data->Size(), nullptr, pubkey.data(), pubkey.size());
    if (!ok) {
        return ERROR_INTERNET_SEC_CERT_ERRORS;
    }
#endif

    SquareTree tree(data->Get());
    SquareTreeNode* node = tree.root ? tree.root->GetChild("SumatraPDF") : nullptr;
    const char* latest = node ? node->GetValue("Latest") : nullptr;
    if (!latest || !IsValidProgramVersion(latest)) {
        return ERROR_INTERNET_INCORRECT_FORMAT;
    }

    AutoFreeWstr verTxt = strconv::Utf8ToWstr(latest);
    if (CompareVersion(verTxt, UPDATE_CHECK_VER) <= 0) {
        /* if automated => don't notify that there is no new version */
        if (!silent) {
            MessageBoxWarning(hParent, _TR("You have the latest version."), _TR("SumatraPDF Update"));
        }
        return 0;
    }

    if (silent) {
        const char* stable = node->GetValue("Stable");
        if (stable && IsValidProgramVersion(stable) &&
            CompareVersion(AutoFreeWstr(strconv::Utf8ToWstr(stable)), UPDATE_CHECK_VER) <= 0) {
            // don't update just yet if the older version is still marked as stable
            return 0;
        }
    }

    // if automated, respect gGlobalPrefs->versionToSkip
    if (silent && str::EqI(gGlobalPrefs->versionToSkip, verTxt)) {
        return 0;
    }

    // ask whether to download the new version and allow the user to
    // either open the browser, do nothing or don't be reminded of
    // this update ever again
    bool skipThisVersion = false;
    INT_PTR res = Dialog_NewVersionAvailable(hParent, UPDATE_CHECK_VER, verTxt, &skipThisVersion);
    if (skipThisVersion) {
        free(gGlobalPrefs->versionToSkip);
        gGlobalPrefs->versionToSkip = verTxt.StealData();
    }
    if (IDYES == res) {
#ifdef SUPPORTS_AUTO_UPDATE
        if (AutoUpdateInitiate(data->Get()))
            return 0;
#endif
        LaunchBrowser(SVN_UPDATE_LINK);
    }
    prefs::Save();

    return 0;
}

// prevent multiple update tasks from happening simultaneously
// (this might e.g. happen if a user checks manually very quickly after startup)
bool gUpdateTaskInProgress = false;

static void ProcessAutoUpdateCheckResult(HWND hwnd, HttpRsp* rsp, bool autoCheck) {
    DWORD error = ShowAutoUpdateDialog(hwnd, rsp, autoCheck);
    if (error != 0 && !autoCheck) {
        // notify the user about network error during a manual update check
        AutoFreeWstr msg(str::Format(_TR("Can't connect to the Internet (error %#x)."), error));
        MessageBoxWarning(hwnd, msg, _TR("SumatraPDF Update"));
    }
}

// start auto-update check by downloading auto-update information from url
// on a background thread and processing the retrieved data on ui thread
// if autoCheck is true, this is a check *not* triggered by explicit action
// of the user and therefore will show less UI
void UpdateCheckAsync(WindowInfo* win, bool autoCheck) {
    if (!HasPermission(Perm_InternetAccess)) {
        return;
    }

    // For auto-check, only check if at least a day passed since last check
    if (autoCheck) {
        // don't check if the timestamp or version to skip can't be updated
        // (mainly in plugin mode, stress testing and restricted settings)
        if (!HasPermission(Perm_SavePreferences)) {
            return;
        }

        // don't check for updates at the first start, so that privacy
        // sensitive users can disable the update check in time
        FILETIME never = {0};
        if (FileTimeEq(gGlobalPrefs->timeOfLastUpdateCheck, never)) {
            return;
        }

        FILETIME currentTimeFt;
        GetSystemTimeAsFileTime(&currentTimeFt);
        int secs = FileTimeDiffInSecs(currentTimeFt, gGlobalPrefs->timeOfLastUpdateCheck);
        // if secs < 0 => somethings wrong, so ignore that case
        if ((secs >= 0) && (secs < SECS_IN_DAY)) {
            return;
        }
    }

    GetSystemTimeAsFileTime(&gGlobalPrefs->timeOfLastUpdateCheck);
    if (gUpdateTaskInProgress) {
        return;
    }
    gUpdateTaskInProgress = true;
    HWND hwnd = win->hwndFrame;
    const WCHAR* url = SUMATRA_UPDATE_INFO_URL L"?v=" UPDATE_CHECK_VER;
    HttpGetAsync(url, [=](HttpRsp* rsp) {
        gUpdateTaskInProgress = false;
        uitask::Post([=] {
            ProcessAutoUpdateCheckResult(hwnd, rsp, autoCheck);
            delete rsp;
        });
    });
}

static void RerenderEverything() {
    for (auto* win : gWindows) {
        if (!win->AsFixed()) {
            continue;
        }
        DisplayModel* dm = win->AsFixed();
        gRenderCache.CancelRendering(dm);
        gRenderCache.KeepForDisplayModel(dm, dm);
        win->RedrawAll(true);
    }
}

void UpdateDocumentColors() {
    // TODO: only do this if colors have actually changed?
    for (auto* win : gWindows) {
        if (win->AsEbook()) {
            win->AsEbook()->UpdateDocumentColors();
            UpdateTreeCtrlColors(win);
        }
    }

    COLORREF text, bg;
    GetFixedPageUiColors(text, bg);

    if ((text == gRenderCache.textColor) && (bg == gRenderCache.backgroundColor)) {
        return; // colors didn't change
    }

    gRenderCache.textColor = text;
    gRenderCache.backgroundColor = bg;
    RerenderEverything();
}

static void OnMenuExit() {
    if (gPluginMode) {
        return;
    }

    for (WindowInfo* win : gWindows) {
        if (!MayCloseWindow(win)) {
            return;
        }
    }

    RememberSessionState();

    // CloseWindow removes the WindowInfo from gWindows,
    // so use a stable copy for iteration
    std::vector<WindowInfo*> toClose = gWindows;
    for (WindowInfo* win : toClose) {
        CloseWindow(win, true);
    }
}

// closes a document inside a WindowInfo and optionally turns it into
// about window (set keepUIEnabled if a new document will be loaded
// into the tab right afterwards and LoadDocIntoCurrentTab would revert
// the UI disabling afterwards anyway)
static void CloseDocumentInTab(WindowInfo* win, bool keepUIEnabled, bool deleteModel) {
    bool wasntFixed = !win->AsFixed();
    if (win->AsChm())
        win->AsChm()->RemoveParentHwnd();
    ClearTocBox(win);
    AbortFinding(win, true);
    delete win->linkOnLastButtonDown;
    win->linkOnLastButtonDown = nullptr;
    win->fwdSearchMark.show = false;
    if (win->uia_provider) {
        win->uia_provider->OnDocumentUnload();
    }
    win->ctrl = nullptr;
    auto currentTab = win->currentTab;
    if (deleteModel) {
        delete currentTab->ctrl;
        currentTab->ctrl = nullptr;
        FileWatcherUnsubscribe(win->currentTab->watcher);
        win->currentTab->watcher = nullptr;
    } else {
        win->currentTab = nullptr;
    }
    win->notifications->RemoveForGroup(NG_RESPONSE_TO_ACTION);
    win->notifications->RemoveForGroup(NG_PAGE_INFO_HELPER);
    win->notifications->RemoveForGroup(NG_CURSOR_POS_HELPER);
    // TODO: this can cause a mouse capture to stick around when called from LoadModelIntoTab (cf. OnSelectionStop)
    win->mouseAction = MouseAction::Idle;

    DeletePropertiesWindow(win->hwndFrame);
    DeleteOldSelectionInfo(win, true);

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
        win::SetText(win->hwndFrame, SUMATRA_WINDOW_TITLE);
        CrashIf(win->tabs.size() != 0 || win->currentTab);
    }

    // Note: this causes https://code.google.com/p/sumatrapdf/issues/detail?id=2702. For whatever reason
    // edit ctrl doesn't receive WM_KILLFOCUS if we do SetFocus() here, even if we call SetFocus() later on
    // SetFocus(win->hwndFrame);
}

// closes the current tab, selecting the next one
// if there's only a single tab left, the window is closed if there
// are other windows, else the Frequently Read page is displayed
void CloseTab(WindowInfo* win, bool quitIfLast) {
    CrashIf(!win);
    if (!win)
        return;

    size_t tabCount = win->tabs.size();
    if (tabCount == 1 || (tabCount == 0 && quitIfLast)) {
        if (MayCloseWindow(win))
            CloseWindow(win, quitIfLast);
    } else {
        CrashIf(gPluginMode && !vectorContains(gWindows, win));
        AbortFinding(win, true);
        TabsOnCloseDoc(win);
    }
}

bool MayCloseWindow(WindowInfo* win) {
    CrashIf(!win);
    if (!win)
        return false;
    // a plugin window should only be closed when its parent is destroyed
    if (gPluginMode && !vectorContains(gWindows, win))
        return false;

    if (win->printThread && !win->printCanceled && WaitForSingleObject(win->printThread, 0) == WAIT_TIMEOUT) {
        int res = MessageBox(win->hwndFrame, _TR("Printing is still in progress. Abort and quit?"),
                             _TR("Printing in progress."), MB_ICONEXCLAMATION | MB_YESNO | MbRtlReadingMaybe());
        if (IDNO == res)
            return false;
    }

    return true;
}

/* Close the document associated with window 'hwnd'.
   Closes the window unless this is the last window in which
   case it switches to empty window and disables the "File\Close"
   menu item. */
void CloseWindow(WindowInfo* win, bool quitIfLast, bool forceClose) {
    CrashIf(!win);
    if (!win) {
        return;
    }

    CrashIf(forceClose && !quitIfLast);
    if (forceClose) {
        quitIfLast = true;
    }

    // when used as an embedded plugin, closing should happen automatically
    // when the parent window is destroyed (cf. WM_DESTROY)
    if (gPluginMode && !vectorContains(gWindows, win) && !forceClose) {
        return;
    }

    AbortFinding(win, true);
    AbortPrinting(win);

    if (win->AsFixed()) {
        win->AsFixed()->dontRenderFlag = true;
    } else if (win->AsEbook()) {
        win->AsEbook()->EnableMessageHandling(false);
    }
    if (win->presentation) {
        ExitFullScreen(win);
    }

    bool lastWindow = (1 == gWindows.size());
    // RememberDefaultWindowPosition becomes a no-op once the window is hidden
    RememberDefaultWindowPosition(win);
    // hide the window before saving prefs (closing seems slightly faster that way)
    if (!lastWindow || quitIfLast) {
        ShowWindow(win->hwndFrame, SW_HIDE);
    }
    if (lastWindow) {
        // don't call RememberSessionState if OnMenuExit already has
        // also don't remember a single document (unless quitting through Menu -> Exit)
        if (quitIfLast && gGlobalPrefs->sessionData->size() == 0 && win->tabs.size() > 1) {
            RememberSessionState();
        }
        prefs::Save();
    } else {
        // this happens otherwise in prefs::Save
        for (TabInfo* tab : win->tabs) {
            UpdateTabFileDisplayStateForWin(win, tab);
        }
    }
    TabsOnCloseWindow(win);

    if (forceClose) {
        // WM_DESTROY has already been sent, so don't destroy win->hwndFrame again
        DeleteWindowInfo(win);
    } else if (lastWindow && !quitIfLast) {
        /* last window - don't delete it */
        CloseDocumentInTab(win);
        SetFocus(win->hwndFrame);
        CrashIf(!vectorContains(gWindows, win));
    } else {
        FreeMenuOwnerDrawInfoData(win->menu);
        HWND hwndToDestroy = win->hwndFrame;
        DeleteWindowInfo(win);
        DestroyWindow(hwndToDestroy);
    }

    if (lastWindow && quitIfLast) {
        logf("Calling PostQuitMessage() in CloseWindow() because closing lastWindow\n");
        CrashIf(gWindows.size() != 0);
        PostQuitMessage(0);
    }
}

// returns false if no filter has been appended
static bool AppendFileFilterForDoc(Controller* ctrl, str::WStr& fileFilter) {
    // TODO: add a way to get Engine from Controller and use engine->kind
    Kind type = nullptr;
    if (ctrl->AsFixed())
        type = ctrl->AsFixed()->engineType;
    else if (ctrl->AsChm())
        type = kindEngineChm;
    else if (ctrl->AsEbook()) {
        switch (ctrl->AsEbook()->GetDocType()) {
            case DocType::Epub:
                type = kindEngineEpub;
                break;
            case DocType::Fb2:
                type = kindEngineFb2;
                break;
            case DocType::Mobi:
                type = kindEngineMobi;
                break;
            case DocType::Pdb:
                type = kindEnginePdb;
                break;
            default:
                type = nullptr;
                break;
        }
    }
    if (type == kindEngineXps) {
        fileFilter.Append(_TR("XPS documents"));
    } else if (type == kindEngineDjVu) {
        fileFilter.Append(_TR("DjVu documents"));
    } else if (type == kindEngineComicBooks) {
        fileFilter.Append(_TR("Comic books"));
    } else if (type == kindEngineImage) {
        fileFilter.AppendFmt(_TR("Image files (*.%s)"), ctrl->DefaultFileExt() + 1);
    } else if (type == kindEngineImageDir) {
        return false; // only show "All files"
    } else if (type == kindEnginePostScript) {
        fileFilter.Append(_TR("Postscript documents"));
    } else if (type == kindEngineChm) {
        fileFilter.Append(_TR("CHM documents"));
    } else if (type == kindEngineEpub) {
        fileFilter.Append(_TR("EPUB ebooks"));
    } else if (type == kindEngineMobi) {
        fileFilter.Append(_TR("Mobi documents"));
    } else if (type == kindEngineFb2) {
        fileFilter.Append(_TR("FictionBook documents"));
    } else if (type == kindEnginePdb) {
        fileFilter.Append(_TR("PalmDoc documents"));
    } else if (type == kindEngineTxt) {
        fileFilter.Append(_TR("Text documents"));
    } else {
        fileFilter.Append(_TR("PDF documents"));
    }
    return true;
}

static void OnMenuSaveAnnotationsToSmx(WindowInfo* win) {
    // this could be invoked due to external apps sending the message
    // so need to validate. see https://github.com/sumatrapdfreader/sumatrapdf/issues/1442
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine->supportsAnnotations) {
        return;
    }
    if (!dm->userAnnotsModified) {
        return;
    }

    const WCHAR* path = engine->FileName();
    bool ok = SaveFileModifications(path, dm->userAnnots);
    if (ok) {
        dm->userAnnotsModified = false;
    }
}

static void OnMenuSaveAs(WindowInfo* win) {
    if (!HasPermission(Perm_DiskAccess)) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }

    auto* ctrl = win->ctrl;
    const WCHAR* srcFileName = ctrl->FilePath();
    AutoFreeWstr urlName;
    if (gPluginMode) {
        urlName.Set(url::GetFileName(gPluginURL));
        // fall back to a generic "filename" instead of the more confusing temporary filename
        srcFileName = urlName ? urlName.get() : L"filename";
    }

    CrashIf(!srcFileName);
    if (!srcFileName) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    bool canConvertToTXT = engine && !engine->IsImageCollection() && win->currentTab->GetEngineType() != kindEngineTxt;
    bool canConvertToPDF = engine && win->currentTab->GetEngineType() != kindEnginePdf;
#ifndef DEBUG
    // not ready for document types other than PS and image collections
    if (canConvertToPDF && win->currentTab->GetEngineType() != kindEnginePostScript && !engine->IsImageCollection()) {
        canConvertToPDF = false;
    }
#endif
#ifndef DISABLE_DOCUMENT_RESTRICTIONS
    // Can't save a document's content as plain text if text copying isn't allowed
    if (engine && !engine->AllowsCopyingText()) {
        canConvertToTXT = false;
    }
    // don't allow converting to PDF when printing isn't allowed
    if (engine && !engine->AllowsPrinting()) {
        canConvertToPDF = false;
    }
#endif
    CrashIf(canConvertToTXT &&
            (!engine || engine->IsImageCollection() || kindEngineTxt == win->currentTab->GetEngineType()));
    CrashIf(canConvertToPDF && (!engine || kindEnginePdf == win->currentTab->GetEngineType()));

    const WCHAR* defExt = ctrl->DefaultFileExt();
    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::WStr fileFilter(256);
    if (AppendFileFilterForDoc(ctrl, fileFilter)) {
        fileFilter.AppendFmt(L"\1*%s\1", defExt);
    }
    if (canConvertToTXT) {
        fileFilter.Append(_TR("Text documents"));
        fileFilter.Append(L"\1*.txt\1");
    }
    if (canConvertToPDF) {
        fileFilter.Append(_TR("PDF documents"));
        fileFilter.Append(L"\1*.pdf\1");
    }
    fileFilter.Append(_TR("All files"));
    fileFilter.Append(L"\1*.*\1");
    str::TransChars(fileFilter.Get(), L"\1", L"\0");

    WCHAR dstFileName[MAX_PATH];
    str::BufSet(dstFileName, dimof(dstFileName), path::GetBaseNameNoFree(srcFileName));
    if (str::FindChar(dstFileName, ':')) {
        // handle embed-marks (for embedded PDF documents):
        // remove the container document's extension and include
        // the embedding reference in the suggested filename
        WCHAR* colon = (WCHAR*)str::FindChar(dstFileName, ':');
        str::TransChars(colon, L":", L"_");
        WCHAR* ext;
        for (ext = colon; ext > dstFileName && *ext != '.'; ext--) {
            // no-op
        }
        if (ext == dstFileName) {
            ext = colon;
        }
        memmove(ext, colon, (str::Len(colon) + 1) * sizeof(WCHAR));
    } else if (str::EndsWithI(dstFileName, defExt)) {
        // Remove the extension so that it can be re-added depending on the chosen filter
        dstFileName[str::Len(dstFileName) - str::Len(defExt)] = '\0';
    }

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter.Get();
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = defExt + 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    // note: explicitly not setting lpstrInitialDir so that the OS
    // picks a reasonable default (in particular, we don't want this
    // in plugin mode, which is likely the main reason for saving as...)

    bool ok = GetSaveFileName(&ofn);
    if (!ok) {
        return;
    }

    WCHAR* realDstFileName = dstFileName;
    bool convertToTXT = canConvertToTXT && str::EndsWithI(dstFileName, L".txt");
    bool convertToPDF = canConvertToPDF && str::EndsWithI(dstFileName, L".pdf");

    // Make sure that the file has a valid ending
    if (!str::EndsWithI(dstFileName, defExt) && !convertToTXT && !convertToPDF) {
        if (canConvertToTXT && 2 == ofn.nFilterIndex) {
            defExt = L".txt";
            convertToTXT = true;
        } else if (canConvertToPDF && (canConvertToTXT ? 3 : 2) == (int)ofn.nFilterIndex) {
            defExt = L".pdf";
            convertToPDF = true;
        }
        realDstFileName = str::Format(L"%s%s", dstFileName, defExt);
    }

    bool saveAnnotsInDoc = gGlobalPrefs->annotationDefaults.saveIntoDocument;
    AutoFree pathUtf8(strconv::WstrToUtf8(realDstFileName));
    AutoFreeWstr errorMsg;
    // Extract all text when saving as a plain text file
    if (convertToTXT) {
        str::WStr text(1024);
        for (int pageNo = 1; pageNo <= ctrl->PageCount(); pageNo++) {
            AutoFreeWstr tmp = engine->ExtractPageText(pageNo, nullptr);
            WCHAR* tmp2 = str::Replace(tmp, L"\n", L"\r\n");
            text.AppendAndFree(tmp2);
        }

        AutoFree textUTF8 = strconv::WstrToUtf8(text.LendData());
        AutoFree textUTF8BOM = str::Join(UTF8_BOM, textUTF8.Get());
        ok = file::WriteFile(realDstFileName, textUTF8BOM.as_view());
    } else if (convertToPDF) {
        // Convert the file into a PDF one
        AutoFreeWstr producerName = str::Join(getAppName(), L" ", CURR_VERSION_STR);
        PdfCreator::SetProducerName(producerName);
        ok = engine->SaveFileAsPDF(pathUtf8.Get(), saveAnnotsInDoc);
        if (!ok && gIsDebugBuild) {
            // rendering includes all page annotations
            ok = PdfCreator::RenderToFile(pathUtf8.Get(), engine);
        } else if (!saveAnnotsInDoc) {
            SaveFileModifications(realDstFileName, win->AsFixed()->userAnnots);
        }
    } else if (!file::Exists(srcFileName) && engine) {
        // Recreate inexistant files from memory...
        ok = engine->SaveFileAs(pathUtf8.Get(), saveAnnotsInDoc);
    } else if (saveAnnotsInDoc && engine && engine->supportsAnnotationsForSaving) {
        // ... as well as files containing annotations ...
        ok = engine->SaveFileAs(pathUtf8.Get(), true);
    } else if (!path::IsSame(srcFileName, realDstFileName)) {
        // ... else just copy the file
        WCHAR* msgBuf;
        ok = CopyFile(srcFileName, realDstFileName, FALSE);
        if (ok) {
            // Make sure that the copy isn't write-locked or hidden
            const DWORD attributesToDrop = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
            DWORD attributes = GetFileAttributes(realDstFileName);
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & attributesToDrop)) {
                SetFileAttributes(realDstFileName, attributes & ~attributesToDrop);
            }
        } else if (FormatMessage(
                       FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, GetLastError(), 0, (LPWSTR)&msgBuf, 0, nullptr)) {
            errorMsg.Set(str::Format(L"%s\n\n%s", _TR("Failed to save a file"), msgBuf));
            LocalFree(msgBuf);
        }
    }
    if (ok && dm && dm->userAnnots && dm->userAnnotsModified && !convertToTXT && !convertToPDF) {
        if (!saveAnnotsInDoc || !engine || !engine->supportsAnnotationsForSaving) {
            ok = SaveFileModifications(realDstFileName, dm->userAnnots);
        }
        if (ok && path::IsSame(srcFileName, realDstFileName)) {
            dm->userAnnotsModified = false;
        }
    }
    if (!ok) {
        const WCHAR* msg = _TR("Failed to save a file");
        if (errorMsg) {
            msg = errorMsg.get();
        }
        MessageBoxWarning(win->hwndFrame, msg);
    }

    if (ok && IsUntrustedFile(win->ctrl->FilePath(), gPluginURL) && !convertToTXT) {
        file::SetZoneIdentifier(realDstFileName);
    }

    if (realDstFileName != dstFileName) {
        free(realDstFileName);
    }
}

static void OnMenuShowInFolder(WindowInfo* win) {
    if (!HasPermission(Perm_DiskAccess)) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }
    if (gPluginMode) {
        return;
    }
    auto* ctrl = win->ctrl;
    auto srcFileName = ctrl->FilePath();
    if (!srcFileName) {
        return;
    }

    WCHAR* process = L"explorer.exe";
    AutoFreeWstr args = str::Format(L"/select,%s", srcFileName);
    CreateProcessHelper(process, args);
}

static void OnMenuRenameFile(WindowInfo* win) {
    if (!HasPermission(Perm_DiskAccess)) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }
    if (gPluginMode) {
        return;
    }

    auto* ctrl = win->ctrl;
    AutoFreeWstr srcFileName = str::Dup(ctrl->FilePath());
    // this happens e.g. for embedded documents and directories
    if (!file::Exists(srcFileName)) {
        return;
    }

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    const WCHAR* defExt = ctrl->DefaultFileExt();
    str::WStr fileFilter(256);
    bool ok = AppendFileFilterForDoc(ctrl, fileFilter);
    CrashIf(!ok);
    fileFilter.AppendFmt(L"\1*%s\1", defExt);
    str::TransChars(fileFilter.Get(), L"\1", L"\0");

    WCHAR dstFileName[MAX_PATH];
    str::BufSet(dstFileName, dimof(dstFileName), path::GetBaseNameNoFree(srcFileName));
    // Remove the extension so that it can be re-added depending on the chosen filter
    if (str::EndsWithI(dstFileName, defExt)) {
        dstFileName[str::Len(dstFileName) - str::Len(defExt)] = '\0';
    }

    AutoFreeWstr initDir(path::GetDir(srcFileName));

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter.Get();
    ofn.nFilterIndex = 1;
    // note: the other two dialogs are named "Open" and "Save As"
    ofn.lpstrTitle = _TR("Rename To");
    ofn.lpstrInitialDir = initDir;
    ofn.lpstrDefExt = defExt + 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    ok = GetSaveFileName(&ofn);
    if (!ok) {
        return;
    }

    UpdateTabFileDisplayStateForWin(win, win->currentTab);
    CloseDocumentInTab(win, true, true);
    SetFocus(win->hwndFrame);

    DWORD flags = MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING;
    BOOL moveOk = MoveFileEx(srcFileName.Get(), dstFileName, flags);
    if (!moveOk) {
        LogLastError();
        LoadArgs args(srcFileName, win);
        args.forceReuse = true;
        LoadDocument(args);
        win->ShowNotification(_TR("Failed to rename the file!"), NOS_WARNING);
        return;
    }

    AutoFreeWstr newPath(path::Normalize(dstFileName));
    RenameFileInHistory(srcFileName, newPath);

    LoadArgs args(dstFileName, win);
    args.forceReuse = true;
    LoadDocument(args);
}

static void OnMenuSaveBookmark(WindowInfo* win) {
    if (!HasPermission(Perm_DiskAccess) || gPluginMode) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }

    auto* ctrl = win->ctrl;
    const WCHAR* defExt = ctrl->DefaultFileExt();

    WCHAR dstFileName[MAX_PATH];
    // Remove the extension so that it can be replaced with .lnk
    str::BufSet(dstFileName, dimof(dstFileName), path::GetBaseNameNoFree(ctrl->FilePath()));
    str::TransChars(dstFileName, L":", L"_");
    if (str::EndsWithI(dstFileName, defExt)) {
        dstFileName[str::Len(dstFileName) - str::Len(defExt)] = '\0';
    }

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    AutoFreeWstr fileFilter(str::Format(L"%s\1*.lnk\1", _TR("Bookmark Shortcuts")));
    str::TransChars(fileFilter, L"\1", L"\0");

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"lnk";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetSaveFileName(&ofn)) {
        return;
    }

    AutoFreeWstr fileName(str::Dup(dstFileName));
    if (!str::EndsWithI(dstFileName, L".lnk")) {
        fileName.Set(str::Join(dstFileName, L".lnk"));
    }

    ScrollState ss(win->ctrl->CurrentPageNo(), 0, 0);
    if (win->AsFixed()) {
        ss = win->AsFixed()->GetScrollState();
    }
    const WCHAR* viewMode = prefs::conv::FromDisplayMode(ctrl->GetDisplayMode());
    AutoFreeWstr ZoomVirtual(str::Format(L"%.2f", ctrl->GetZoomVirtual()));
    if (ZOOM_FIT_PAGE == ctrl->GetZoomVirtual()) {
        ZoomVirtual.SetCopy(L"fitpage");
    } else if (ZOOM_FIT_WIDTH == ctrl->GetZoomVirtual()) {
        ZoomVirtual.SetCopy(L"fitwidth");
    } else if (ZOOM_FIT_CONTENT == ctrl->GetZoomVirtual()) {
        ZoomVirtual.SetCopy(L"fitcontent");
    }

    AutoFreeWstr exePath = GetExePath();
    AutoFreeWstr args = str::Format(L"\"%s\" -page %d -view \"%s\" -zoom %s -scroll %d,%d", ctrl->FilePath(), ss.page,
                                    viewMode, ZoomVirtual.get(), (int)ss.x, (int)ss.y);
    AutoFreeWstr label = ctrl->GetPageLabel(ss.page);
    const WCHAR* srcFileName = path::GetBaseNameNoFree(ctrl->FilePath());
    AutoFreeWstr desc = str::Format(_TR("Bookmark shortcut to page %s of %s"), label.get(), srcFileName);
    CreateShortcut(fileName, exePath, args, desc, 1);
}

#if 0
// code adapted from https://support.microsoft.com/kb/131462/en-us
static UINT_PTR CALLBACK FileOpenHook(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uiMsg) {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)lParam);
        break;
    case WM_NOTIFY:
        if (((LPOFNOTIFY)lParam)->hdr.code == CDN_SELCHANGE) {
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

static void OnMenuNewWindow() {
    CreateAndShowWindowInfo(nullptr);
}

// create a new window and load currently shown document into it
// meant to make it easy to compare 2 documents
static void OnDuplicateInNewWindow(WindowInfo* win) {
    if (win->IsAboutWindow()) {
        return;
    }
    if (!win->IsDocLoaded()) {
        return;
    }
    TabInfo* tab = win->currentTab;
    WCHAR* path = tab->filePath;
    CrashIf(!path);
    if (!path) {
        return;
    }
    WindowInfo* newWin = CreateAndShowWindowInfo(nullptr);
    if (!newWin) {
        return;
    }

    // TODO: should copy the display state from current file
    LoadArgs args(path, newWin);
    args.fileName = tab->filePath;
    args.showWin = true;
    args.noPlaceWindow = true;
    LoadDocument(args);
}

static void OnMenuOpen(WindowInfo* win) {
    if (!HasPermission(Perm_DiskAccess)) {
        return;
    }

    // don't allow opening different files in plugin mode
    if (gPluginMode) {
        return;
    }

    const struct {
        const WCHAR* name; /* nullptr if only to include in "All supported documents" */
        const WCHAR* filter;
        bool available;
    } fileFormats[] = {
        {_TR("PDF documents"), L"*.pdf;*.vbkm", true},
        {_TR("XPS documents"), L"*.xps;*.oxps", true},
        {_TR("DjVu documents"), L"*.djvu", true},
        {_TR("Postscript documents"), L"*.ps;*.eps", IsPsEngineAvailable()},
        {_TR("Comic books"), L"*.cbz;*.cbr;*.cb7;*.cbt", true},
        {_TR("CHM documents"), L"*.chm", true},
        {_TR("EPUB ebooks"), L"*.epub", true},
        {_TR("Mobi documents"), L"*.mobi", true},
        {_TR("FictionBook documents"), L"*.fb2;*.fb2z;*.zfb2;*.fb2.zip", true},
        {_TR("PalmDoc documents"), L"*.pdb;*.prc", true},
        {nullptr, /* multi-page images */ L"*.tif;*.tiff", true},
        {_TR("Text documents"), L"*.txt;*.log;*.nfo;rfc*.txt;file_id.diz;read.me;*.tcr;*.vbkm", true},
    };
    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::WStr fileFilter;
    fileFilter.Append(_TR("All supported documents"));
    fileFilter.Append(L'\1');
    for (int i = 0; i < dimof(fileFormats); i++) {
        if (fileFormats[i].available) {
            fileFilter.Append(fileFormats[i].filter);
            fileFilter.Append(';');
        }
    }
    CrashIf(fileFilter.Last() != L';');
    fileFilter.Last() = L'\1';
    for (int i = 0; i < dimof(fileFormats); i++) {
        if (fileFormats[i].available && fileFormats[i].name) {
            fileFilter.Append(fileFormats[i].name);
            fileFilter.Append(L'\1');
            fileFilter.Append(fileFormats[i].filter);
            fileFilter.Append(L'\1');
        }
    }
    fileFilter.Append(_TR("All files"));
    fileFilter.Append(L"\1*.*\1");
    str::TransChars(fileFilter.Get(), L"\1", L"\0");

    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;

    ofn.lpstrFilter = fileFilter.Get();
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    // OFN_ENABLEHOOK disables the new Open File dialog under Windows Vista
    // and later, so don't use it and just allocate enough memory to contain
    // several dozen file paths and hope that this is enough
    // TODO: Use IFileOpenDialog instead (requires a Vista SDK, though)
    ofn.nMaxFile = MAX_PATH * 100;
#if 0
    if (!IsVistaOrGreater())
    {
        ofn.lpfnHook = FileOpenHook;
        ofn.Flags |= OFN_ENABLEHOOK;
        ofn.nMaxFile = MAX_PATH / 2;
    }
    // note: ofn.lpstrFile can be reallocated by GetOpenFileName -> FileOpenHook
#endif
    AutoFreeWstr file = AllocArray<WCHAR>(ofn.nMaxFile);
    ofn.lpstrFile = file;

    if (!GetOpenFileNameW(&ofn)) {
        return;
    }

    WCHAR* fileName = ofn.lpstrFile + ofn.nFileOffset;
    if (*(fileName - 1)) {
        // special case: single filename without nullptr separator
        LoadArgs args(ofn.lpstrFile, win);
        LoadDocument(args);
        return;
    }

    while (*fileName) {
        AutoFreeWstr filePath = path::Join(ofn.lpstrFile, fileName);
        if (filePath) {
            LoadArgs args(filePath, win);
            LoadDocument(args);
        }
        fileName += str::Len(fileName) + 1;
    }
}

static void BrowseFolder(WindowInfo* win, bool forward) {
    CrashIf(win->IsAboutWindow());
    if (win->IsAboutWindow()) {
        return;
    }
    if (!HasPermission(Perm_DiskAccess) || gPluginMode) {
        return;
    }

    TabInfo* tab = win->currentTab;
    WStrVec files;
    AutoFreeWstr pattern(path::GetDir(tab->filePath));
    // TODO: make pattern configurable (for users who e.g. want to skip single images)?
    pattern.Set(path::Join(pattern, L"*"));
    if (!CollectPathsFromDirectory(pattern, files)) {
        return;
    }

    // remove unsupported files that have never been successfully loaded
    for (size_t i = files.size(); i > 0; i--) {
        if (!EngineManager::IsSupportedFile(files.at(i - 1), false, gGlobalPrefs->ebookUI.useFixedPageUI) &&
            !Doc::IsSupportedFile(files.at(i - 1)) && !gFileHistory.Find(files.at(i - 1), nullptr)) {
            free(files.PopAt(i - 1));
        }
    }

    if (!files.Contains(tab->filePath)) {
        files.Append(str::Dup(tab->filePath));
    }
    files.SortNatural();

    int index = files.Find(tab->filePath);
    if (forward) {
        index = (index + 1) % files.size();
    } else {
        index = (int)(index + files.size() - 1) % files.size();
    }

    // TODO: check for unsaved modifications
    UpdateTabFileDisplayStateForWin(win, tab);
    LoadArgs args(files.at(index), win);
    args.forceReuse = true;
    LoadDocument(args);
}

static void RelayoutFrame(WindowInfo* win, bool updateToolbars = true, int sidebarDx = -1) {
    ClientRect rc(win->hwndFrame);
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
            float scale = IsZoomed(win->hwndFrame) ? 1.f : CAPTION_TABBAR_HEIGHT_FACTOR;
            int captionHeight = GetTabbarHeight(win->hwndFrame, scale);
            if (updateToolbars) {
                int captionWidth;
                RECT capButtons;
                if (dwm::IsCompositionEnabled() &&
                    SUCCEEDED(dwm::GetWindowAttribute(win->hwndFrame, DWMWA_CAPTION_BUTTON_BOUNDS, &capButtons,
                                                      sizeof(RECT)))) {
                    WindowRect wr(win->hwndFrame);
                    POINT pt = {wr.x + capButtons.left, wr.y + capButtons.top};
                    ScreenToClient(win->hwndFrame, &pt);
                    if (IsUIRightToLeft()) {
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
                dh.SetWindowPos(win->hwndTabBar, nullptr, rc.x, rc.y, rc.dx, tabHeight, SWP_NOZORDER);
            }
            // TODO: show tab bar also for About window (or hide the toolbar so that it doesn't jump around)
            if (!win->IsAboutWindow()) {
                rc.y += tabHeight;
                rc.dy -= tabHeight;
            }
        }
    }
    if (gGlobalPrefs->showToolbar && !win->presentation && !win->isFullScreen && !win->AsEbook()) {
        if (updateToolbars) {
            WindowRect rcRebar(win->hwndReBar);
            dh.SetWindowPos(win->hwndReBar, nullptr, rc.x, rc.y, rc.dx, rcRebar.dy, SWP_NOZORDER);
        }
        WindowRect rcRebar(win->hwndReBar);
        rc.y += rcRebar.dy;
        rc.dy -= rcRebar.dy;
    }

    // ToC and Favorites sidebars at the left
    bool showFavorites = gGlobalPrefs->showFavorites && !gPluginMode && HasPermission(Perm_DiskAccess);
    bool tocVisible = win->tocVisible;
    if (tocVisible || showFavorites) {
        SizeI toc = ClientRect(win->hwndTocBox).Size();
        if (sidebarDx > 0) {
            toc = SizeI(sidebarDx, rc.y);
        }
        if (0 == toc.dx) {
            // TODO: use saved sidebarDx from saved preferences?
            toc.dx = rc.dx / 4;
        }
        // make sure that the sidebar is never too wide or too narrow
        // note: requires that the main frame is at least 2 * SIDEBAR_MIN_WIDTH
        //       wide (cf. OnFrameGetMinMaxInfo)
        toc.dx = limitValue(toc.dx, SIDEBAR_MIN_WIDTH, rc.dx / 2);

        toc.dy = 0;
        if (tocVisible) {
            if (!showFavorites) {
                toc.dy = rc.dy;
            } else {
                toc.dy = gGlobalPrefs->tocDy;
                if (toc.dy > 0) {
                    toc.dy = limitValue<int>(gGlobalPrefs->tocDy, 0, rc.dy);
                } else {
                    toc.dy = rc.dy / 2; // default value
                }
            }
        }

        if (tocVisible && showFavorites) {
            toc.dy = limitValue(toc.dy, TOC_MIN_DY, rc.dy - TOC_MIN_DY);
        }

        if (tocVisible) {
            RectI rToc(rc.TL(), toc);
            dh.MoveWindow(win->hwndTocBox, rToc);
            if (showFavorites) {
                RectI rSplitV(rc.x, rc.y + toc.dy, toc.dx, SPLITTER_DY);
                dh.MoveWindow(win->favSplitter->hwnd, rSplitV);
                toc.dy += SPLITTER_DY;
            }
        }
        if (showFavorites) {
            RectI rFav(rc.x, rc.y + toc.dy, toc.dx, rc.dy - toc.dy);
            dh.MoveWindow(win->hwndFavBox, rFav);
        }
        RectI rSplitH(rc.x + toc.dx, rc.y, SPLITTER_DX, rc.dy);
        dh.MoveWindow(win->sidebarSplitter->hwnd, rSplitH);

        rc.x += toc.dx + SPLITTER_DX;
        rc.dx -= toc.dx + SPLITTER_DX;
    }

    dh.MoveWindow(win->hwndCanvas, rc);

    dh.End();

    // TODO: if a document with ToC and a broken document are loaded
    //       and the first document is closed with the ToC still visible,
    //       we have tocVisible but !win->ctrl
    if (tocVisible && win->ctrl) {
        // the ToC selection may change due to resizing
        // (and SetSidebarVisibility relies on this for initialization)
        if (win->ctrl->AsEbook()) {
            UpdateTocSelection(win, win->ctrl->AsEbook()->CurrentTocPageNo());
        } else {
            UpdateTocSelection(win, win->ctrl->CurrentPageNo());
        }
    }
}

static void FrameOnSize(WindowInfo* win, int dx, int dy) {
    UNUSED(dx);
    UNUSED(dy);
    RelayoutFrame(win);

    if (win->presentation || win->isFullScreen) {
        RectI fullscreen = GetFullscreenRect(win->hwndFrame);
        WindowRect rect(win->hwndFrame);
        // Windows XP sometimes seems to change the window size on it's own
        if (rect != fullscreen && rect != GetVirtualScreenRect()) {
            MoveWindow(win->hwndFrame, fullscreen);
        }
    }
}

void RelayoutWindow(WindowInfo* win) {
    RelayoutFrame(win);
}

void SetCurrentLanguageAndRefreshUI(const char* langCode) {
    if (!langCode || str::Eq(langCode, trans::GetCurrentLangCode())) {
        return;
    }
    SetCurrentLang(langCode);

    for (WindowInfo* win : gWindows) {
        UpdateWindowRtlLayout(win);
        RebuildMenuBarForWindow(win);
        UpdateToolbarSidebarText(win);
        // About page text is translated during (re)drawing
        if (win->IsAboutWindow()) {
            win->RedrawAll(true);
        }
    }

    prefs::Save();
}

static void OnMenuChangeLanguage(HWND hwnd) {
    const char* newLangCode = Dialog_ChangeLanguge(hwnd, trans::GetCurrentLangCode());
    SetCurrentLanguageAndRefreshUI(newLangCode);
}

static void OnMenuViewShowHideToolbar() {
    gGlobalPrefs->showToolbar = !gGlobalPrefs->showToolbar;
    for (WindowInfo* win : gWindows) {
        ShowOrHideToolbar(win);
    }
}

static void OnMenuAdvancedOptions() {
    if (!HasPermission(Perm_DiskAccess) || !HasPermission(Perm_SavePreferences)) {
        return;
    }

    AutoFreeWstr path = prefs::GetSettingsPath();
    // TODO: disable/hide the menu item when there's no prefs file
    //       (happens e.g. when run in portable mode from a CD)?
    LaunchFile(path.get(), nullptr, L"open");
}

static void OnMenuOptions(HWND hwnd) {
    if (!HasPermission(Perm_SavePreferences)) {
        return;
    }

    if (IDOK != Dialog_Settings(hwnd, gGlobalPrefs)) {
        return;
    }

    if (!gGlobalPrefs->rememberOpenedFiles) {
        gFileHistory.Clear(true);
        CleanUpThumbnailCache(gFileHistory);
    }
    UpdateDocumentColors();

    // note: ideally we would also update state for useTabs changes but that's complicated since
    // to do it right we would have to convert tabs to windows. When moving no tabs -> tabs,
    // there's no problem. When moving tabs -> no tabs, a half solution would be to only
    // call SetTabsInTitlebar() for windows that have only one tab, but that's somewhat inconsistent
    prefs::Save();
}

static void OnMenuOptions(WindowInfo* win) {
    OnMenuOptions(win->hwndFrame);
    if (!gWindows.empty() && gWindows.at(0)->IsAboutWindow()) {
        gWindows.at(0)->RedrawAll(true);
    }
}

// toggles 'show pages continuously' state
static void OnMenuViewContinuous(WindowInfo* win) {
    if (!win->IsDocLoaded()) {
        return;
    }

    DisplayMode newMode = win->ctrl->GetDisplayMode();
    switch (newMode) {
        case DM_SINGLE_PAGE:
        case DM_CONTINUOUS:
            newMode = IsContinuous(newMode) ? DM_SINGLE_PAGE : DM_CONTINUOUS;
            break;
        case DM_FACING:
        case DM_CONTINUOUS_FACING:
            newMode = IsContinuous(newMode) ? DM_FACING : DM_CONTINUOUS_FACING;
            break;
        case DM_BOOK_VIEW:
        case DM_CONTINUOUS_BOOK_VIEW:
            newMode = IsContinuous(newMode) ? DM_BOOK_VIEW : DM_CONTINUOUS_BOOK_VIEW;
            break;
    }
    SwitchToDisplayMode(win, newMode);
}

static void OnMenuViewMangaMode(WindowInfo* win) {
    CrashIf(!win->currentTab || win->currentTab->GetEngineType() != kindEngineComicBooks);
    if (!win->currentTab || win->currentTab->GetEngineType() != kindEngineComicBooks) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    dm->SetDisplayR2L(!dm->GetDisplayR2L());
    ScrollState state = dm->GetScrollState();
    dm->Relayout(dm->GetZoomVirtual(), dm->GetRotation());
    dm->SetScrollState(state);
}

static void ChangeZoomLevel(WindowInfo* win, float newZoom, bool pagesContinuously) {
    if (!win->IsDocLoaded()) {
        return;
    }

    float zoom = win->ctrl->GetZoomVirtual();
    DisplayMode mode = win->ctrl->GetDisplayMode();
    DisplayMode newMode = pagesContinuously ? DM_CONTINUOUS : DM_SINGLE_PAGE;

    if (mode != newMode || zoom != newZoom) {
        float prevZoom = win->currentTab->prevZoomVirtual;
        DisplayMode prevMode = win->currentTab->prevDisplayMode;

        if (mode != newMode) {
            SwitchToDisplayMode(win, newMode);
        }
        OnMenuZoom(win, MenuIdFromVirtualZoom(newZoom));

        // remember the previous values for when the toolbar button is unchecked
        if (INVALID_ZOOM == prevZoom) {
            win->currentTab->prevZoomVirtual = zoom;
            win->currentTab->prevDisplayMode = mode;
        } else {
            // keep the rememberd values when toggling between the two toolbar buttons
            win->currentTab->prevZoomVirtual = prevZoom;
            win->currentTab->prevDisplayMode = prevMode;
        }
    } else if (win->currentTab->prevZoomVirtual != INVALID_ZOOM) {
        float prevZoom = win->currentTab->prevZoomVirtual;
        SwitchToDisplayMode(win, win->currentTab->prevDisplayMode);
        ZoomToSelection(win, prevZoom);
    }
}

static void FocusPageNoEdit(HWND hwndPageBox) {
    if (IsFocused(hwndPageBox)) {
        SendMessage(hwndPageBox, WM_SETFOCUS, 0, 0);
    } else {
        SetFocus(hwndPageBox);
    }
}

static void OnMenuGoToPage(WindowInfo* win) {
    if (!win->IsDocLoaded()) {
        return;
    }

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs->showToolbar && !win->isFullScreen && !win->presentation && !win->AsEbook()) {
        FocusPageNoEdit(win->hwndPageBox);
        return;
    }

    auto* ctrl = win->ctrl;
    AutoFreeWstr label = ctrl->GetPageLabel(ctrl->CurrentPageNo());
    AutoFreeWstr newPageLabel(Dialog_GoToPage(win->hwndFrame, label, ctrl->PageCount(), !ctrl->HasPageLabels()));
    if (!newPageLabel) {
        return;
    }

    int newPageNo = ctrl->GetPageByLabel(newPageLabel);
    if (ctrl->ValidPageNo(newPageNo)) {
        ctrl->GoToPage(newPageNo, true);
    }
}

void EnterFullScreen(WindowInfo* win, bool presentation) {
    if (!HasPermission(Perm_FullscreenAccess) || gPluginMode) {
        return;
    }

    if ((presentation ? win->presentation : win->isFullScreen) || !IsWindowVisible(win->hwndFrame)) {
        return;
    }

    CrashIf(presentation ? win->isFullScreen : win->presentation);
    if (presentation) {
        CrashIf(!win->ctrl);
        if (!win->IsDocLoaded()) {
            return;
        }

        if (IsZoomed(win->hwndFrame)) {
            win->windowStateBeforePresentation = WIN_STATE_MAXIMIZED;
        } else {
            win->windowStateBeforePresentation = WIN_STATE_NORMAL;
        }
        win->presentation = PM_ENABLED;

        SetTimer(win->hwndCanvas, HIDE_CURSOR_TIMER_ID, HIDE_CURSOR_DELAY_IN_MS, nullptr);
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
    if (!presentation || !win->isFullScreen)
        win->nonFullScreenWindowStyle = ws;
    // remove window styles that add to non-client area
    ws &= ~(WS_CAPTION | WS_THICKFRAME);
    ws |= WS_MAXIMIZE;

    win->nonFullScreenFrameRect = WindowRect(win->hwndFrame);
    RectI rect = GetFullscreenRect(win->hwndFrame);

    SetMenu(win->hwndFrame, nullptr);
    ShowWindow(win->hwndReBar, SW_HIDE);
    ShowWindow(win->hwndTabBar, SW_HIDE);
    ShowWindow(win->hwndCaption, SW_HIDE);

    SetWindowLong(win->hwndFrame, GWL_STYLE, ws);
    UINT flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER;
    SetWindowPos(win->hwndFrame, nullptr, rect.x, rect.y, rect.dx, rect.dy, flags);

    if (presentation) {
        win->ctrl->SetPresentationMode(true);
    }

    // Make sure that no toolbar/sidebar keeps the focus
    SetFocus(win->hwndFrame);
    // restore gGlobalPrefs->showFavorites changed by SetSidebarVisibility()
    gGlobalPrefs->showFavorites = showFavoritesTmp;
}

void ExitFullScreen(WindowInfo* win) {
    if (!win->isFullScreen && !win->presentation) {
        return;
    }

    bool wasPresentation = PM_DISABLED != win->presentation;
    if (wasPresentation) {
        win->presentation = PM_DISABLED;
        if (win->IsDocLoaded()) {
            win->ctrl->SetPresentationMode(false);
        }
        // re-enable the auto-hidden cursor
        KillTimer(win->hwndCanvas, HIDE_CURSOR_TIMER_ID);
        SetCursor(IDC_ARROW);
        // ensure that no ToC is shown when entering presentation mode the next time
        for (TabInfo* tab : win->tabs) {
            tab->showTocPresentation = false;
        }
    } else {
        win->isFullScreen = false;
    }

    bool tocVisible = win->currentTab && win->currentTab->showToc;
    SetSidebarVisibility(win, tocVisible, gGlobalPrefs->showFavorites);

    if (win->tabsInTitlebar) {
        ShowWindow(win->hwndCaption, SW_SHOW);
    }
    if (win->tabsVisible) {
        ShowWindow(win->hwndTabBar, SW_SHOW);
    }
    if (gGlobalPrefs->showToolbar && !win->AsEbook()) {
        ShowWindow(win->hwndReBar, SW_SHOW);
    }
    if (!win->isMenuHidden) {
        SetMenu(win->hwndFrame, win->menu);
    }

    ClientRect cr(win->hwndFrame);
    SetWindowLong(win->hwndFrame, GWL_STYLE, win->nonFullScreenWindowStyle);
    UINT flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);
    MoveWindow(win->hwndFrame, win->nonFullScreenFrameRect);
    // TODO: this CrashIf() fires in pre-release e.g. 64011
    // CrashIf(WindowRect(win.hwndFrame) != win.nonFullScreenFrameRect);
    // We have to relayout here, because it isn't done in the SetWindowPos nor MoveWindow,
    // if the client rectangle hasn't changed.
    if (ClientRect(win->hwndFrame) == cr) {
        RelayoutFrame(win);
    }
}

void OnMenuViewFullscreen(WindowInfo* win, bool presentation) {
    bool enterFullScreen = presentation ? !win->presentation : !win->isFullScreen;

    if (!win->presentation && !win->isFullScreen) {
        RememberDefaultWindowPosition(win);
    } else {
        ExitFullScreen(win);
    }

    if (enterFullScreen && (!presentation || win->IsDocLoaded())) {
        EnterFullScreen(win, presentation);
    }
}

static void OnMenuViewPresentation(WindowInfo* win) {
    // only DisplayModel currently supports an actual presentation mode
    OnMenuViewFullscreen(win, win->AsFixed() != nullptr);
}

// make sure that idx falls within <0, max-1> inclusive range
// negative numbers wrap from the end
static int wrapIdx(int idx, int max) {
    for (; idx < 0; idx += max) {
        idx += max;
    }
    return idx % max;
}

void AdvanceFocus(WindowInfo* win) {
    // Tab order: Frame -> Page -> Find -> ToC -> Favorites -> Frame -> ...

    bool hasToolbar =
        !win->isFullScreen && !win->presentation && !win->AsEbook() && gGlobalPrefs->showToolbar && win->IsDocLoaded();
    int direction = IsShiftPressed() ? -1 : 1;

    const int MAX_WINDOWS = 5;
    HWND tabOrder[MAX_WINDOWS] = {win->hwndFrame};
    int nWindows = 1;
    if (hasToolbar) {
        tabOrder[nWindows++] = win->hwndPageBox;
    }
    if (hasToolbar && NeedsFindUI(win)) {
        tabOrder[nWindows++] = win->hwndFindBox;
    }
    if (win->tocLoaded && win->tocVisible) {
        tabOrder[nWindows++] = win->tocTreeCtrl->hwnd;
    }
    if (gGlobalPrefs->showFavorites) {
        tabOrder[nWindows++] = win->favTreeCtrl->hwnd;
    }
    CrashIf(nWindows > MAX_WINDOWS);

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
    SetFocus(tabOrder[i]);
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

bool FrameOnKeydown(WindowInfo* win, WPARAM key, LPARAM lparam, bool inTextfield) {
    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        // black/white screen is disabled on any unmodified key press in FrameOnChar
        return true;
    }

    bool isCtrl = IsCtrlPressed();
    bool isShift = IsShiftPressed();

    if (win->tabsVisible && isCtrl && VK_TAB == key) {
        TabsOnCtrlTab(win, isShift);
        return true;
    }

    if ((VK_LEFT == key || VK_RIGHT == key) && isShift && isCtrl && !win->IsAboutWindow() && !inTextfield) {
        // folder browsing should also work when an error page is displayed,
        // so special-case it before the win->IsDocLoaded() check
        BrowseFolder(win, VK_RIGHT == key);
        return true;
    }

    if (!win->IsDocLoaded()) {
        return false;
    }

    DisplayModel* dm = win->AsFixed();
    // some of the chm key bindings are different than the rest and we
    // need to make sure we don't break them
    bool isChm = win->AsChm();

    bool isPageUp = (isCtrl && (VK_UP == key));
    if (!isChm) {
        isPageUp |= (VK_PRIOR == key) && !isCtrl;
    }

    if (isPageUp) {
        int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        if (win->ctrl->GetZoomVirtual() != ZOOM_FIT_CONTENT) {
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_PAGEUP, 0);
        }
        if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos) {
            win->ctrl->GoToPrevPage(true);
        }
        return true;
    }

    bool isPageDown = (isCtrl && (VK_DOWN == key));
    if (!isChm) {
        isPageDown |= (VK_NEXT == key) && !isCtrl;
    }

    if (isPageDown) {
        int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        if (win->ctrl->GetZoomVirtual() != ZOOM_FIT_CONTENT) {
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_PAGEDOWN, 0);
        }
        if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos) {
            win->ctrl->GoToNextPage();
        }
        return true;
    }

    if (isChm) {
        if (ChmForwardKey(key)) {
            win->AsChm()->PassUIMsg(WM_KEYDOWN, key, lparam);
            return true;
        }
    }
    // lf("key=%d,%c,shift=%d\n", key, (char)key, (int)WasKeyDown(VK_SHIFT));

    if (VK_UP == key) {
        if (dm && dm->NeedVScroll()) {
            SendMessage(win->hwndCanvas, WM_VSCROLL, isShift ? SB_HPAGEUP : SB_LINEUP, 0);
        } else {
            win->ctrl->GoToPrevPage(true);
        }
    } else if (VK_DOWN == key) {
        if (dm && dm->NeedVScroll()) {
            SendMessage(win->hwndCanvas, WM_VSCROLL, isShift ? SB_HPAGEDOWN : SB_LINEDOWN, 0);
        } else {
            win->ctrl->GoToNextPage();
        }
    } else if (VK_PRIOR == key && isCtrl) {
        win->ctrl->GoToPrevPage();
    } else if (VK_NEXT == key && isCtrl) {
        win->ctrl->GoToNextPage();
    } else if (VK_HOME == key && isCtrl) {
        win->ctrl->GoToFirstPage();
    } else if (VK_END == key && isCtrl) {
        if (!win->ctrl->GoToLastPage()) {
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_BOTTOM, 0);
        }
    } else if (inTextfield) {
        // The remaining keys have a different meaning
        return false;
    } else if (VK_LEFT == key) {
        if (dm && dm->NeedHScroll() && !isCtrl) {
            SendMessage(win->hwndCanvas, WM_HSCROLL, isShift ? SB_PAGELEFT : SB_LINELEFT, 0);
        } else {
            win->ctrl->GoToPrevPage();
        }
    } else if (VK_RIGHT == key) {
        if (dm && dm->NeedHScroll() && !isCtrl) {
            SendMessage(win->hwndCanvas, WM_HSCROLL, isShift ? SB_PAGERIGHT : SB_LINERIGHT, 0);
        } else {
            win->ctrl->GoToNextPage();
        }
    } else if (VK_HOME == key) {
        win->ctrl->GoToFirstPage();
    } else if (VK_END == key) {
        if (!win->ctrl->GoToLastPage()) {
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_BOTTOM, 0);
        }
    } else if (VK_MULTIPLY == key && dm) {
        dm->RotateBy(90);
    } else if (VK_DIVIDE == key && dm) {
        dm->RotateBy(-90);
        gIsDivideKeyDown = true;
#ifdef DEBUG
    } else if (VK_F1 == key && win->AsEbook()) {
        // TODO: this was in EbookWindow - is it still needed?
        SendMessage(win->hwndFrame, WM_COMMAND, IDM_DEBUG_MUI, 0);
#endif
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

static void OnFrameKeyEsc(WindowInfo* win) {
    if (win->findThread) {
        AbortFinding(win, false);
        return;
    }
    if (win->notifications->GetForGroup(NG_PERSISTENT_WARNING)) {
        win->notifications->RemoveForGroup(NG_PERSISTENT_WARNING);
        return;
    }
    if (win->notifications->GetForGroup(NG_PAGE_INFO_HELPER)) {
        win->notifications->RemoveForGroup(NG_PAGE_INFO_HELPER);
        return;
    }
    if (win->notifications->GetForGroup(NG_CURSOR_POS_HELPER)) {
        win->notifications->RemoveForGroup(NG_CURSOR_POS_HELPER);
        return;
    }
    if (win->showSelection) {
        ClearSearchResult(win);
        return;
    }
    if (gGlobalPrefs->escToExit && MayCloseWindow(win)) {
        CloseWindow(win, true);
        return;
    }
    if (win->presentation || win->isFullScreen) {
        OnMenuViewFullscreen(win, win->presentation != PM_DISABLED);
        return;
    }
}

static void OnFrameKeyB(WindowInfo* win) {
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

        DisplayMode newMode = DM_BOOK_VIEW;
        if (IsBookView(ctrl->GetDisplayMode())) {
            newMode = DM_FACING;
        }
        SwitchToDisplayMode(win, newMode, true);

        if (forward && currPage >= ctrl->CurrentPageNo() && (currPage > 1 || newMode == DM_BOOK_VIEW)) {
            ctrl->GoToNextPage();
        } else if (!forward && currPage <= ctrl->CurrentPageNo()) {
            win->ctrl->GoToPrevPage();
        }
    } else if (win->AsEbook() && !isSinglePage) {
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

static void MakeAnnotationFromSelection(WindowInfo* win) {
    bool annotsEnabled = gIsDebugBuild || gIsPreReleaseBuild;
    if (!annotsEnabled) {
        return;
    }

    // converts current selection to annotation (or back to regular text
    // if it's already an annotation)
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    auto engine = dm->GetEngine();
    if (!engine) {
        return;
    }

    bool ok = engine->supportsAnnotations && win->showSelection && win->currentTab->selectionOnPage;
    if (!ok) {
        return;
    }
    // TODO: we probably should construct list of new annotations and replace
    // existing list with a new one at the end
    // TODO: we need to support overlapping selections better (merge them into existing
    // annotation?
    if (!dm->userAnnots) {
        dm->userAnnots = new Vec<PageAnnotation>();
    }
    Vec<PageAnnotation>* annots = dm->userAnnots;
    for (SelectionOnPage& sel : *win->currentTab->selectionOnPage) {
        COLORREF c = gGlobalPrefs->annotationDefaults.highlightColor;
        c = ColorSetAlpha(c, 0xcc);
        auto addedAnnotation = PageAnnotation(PageAnnotType::Highlight, sel.pageNo, sel.rect, c);
        size_t oldLen = annots->size();
        for (size_t i = 0; i < oldLen && i < annots->size(); ++i) {
            if (annots->at(i) == addedAnnotation) {
                annots->RemoveAtFast(i);
            }
        }
        if (oldLen == annots->size()) {
            annots->Append(PageAnnotation(PageAnnotType::Highlight, sel.pageNo, sel.rect, c));
        }
        gRenderCache.Invalidate(dm, sel.pageNo, sel.rect);
    }
    dm->userAnnotsModified = true;
    dm->GetEngine()->UpdateUserAnnotations(dm->userAnnots);
    ClearSearchResult(win); // causes invalidated tiles to be rerendered
}

static void OnFrameKeyM(WindowInfo* win) {
    // "cursor position" tip: make figuring out the current
    // cursor position in cm/in/pt possible (for exact layouting)
    if (!win->AsFixed()) {
        return;
    }
    PointI pt;
    if (GetCursorPosInHwnd(win->hwndCanvas, pt)) {
        UpdateCursorPositionHelper(win, pt, nullptr);
    }
}

static void FrameOnChar(WindowInfo* win, WPARAM key, LPARAM info = 0) {
    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        win->ChangePresentationMode(PM_ENABLED);
        return;
    }

    if (key >= 0x100 && info && !IsCtrlPressed() && !IsAltPressed()) {
        // determine the intended keypress by scan code for non-Latin keyboard layouts
        UINT vk = MapVirtualKeyW((info >> 16) & 0xFF, MAPVK_VSC_TO_VK);
        if ('A' <= vk && vk <= 'Z') {
            key = vk;
        }
    }

    if (IsCharUpperW((WCHAR)key)) {
        key = (WPARAM)SingleCharLowerW((WCHAR)key);
    }

    switch (key) {
        case VK_ESCAPE:
            OnFrameKeyEsc(win);
            return;
        case 'q':
            // close the current document (it's too easy to press for discarding multiple tabs)
            // quit if this is the last window
            CloseTab(win, true);
            return;
        case 'r':
            ReloadDocument(win);
            return;
        case VK_TAB:
            AdvanceFocus(win);
            break;
    }

    if (!win->IsDocLoaded()) {
        return;
    }

    auto* ctrl = win->ctrl;

    switch (key) {
        case VK_SPACE:
        case VK_RETURN:
            FrameOnKeydown(win, IsShiftPressed() ? VK_PRIOR : VK_NEXT, 0);
            break;
        case VK_BACK: {
            bool forward = IsShiftPressed();
            ctrl->Navigate(forward ? 1 : -1);
        } break;
        case 'g':
            OnMenuGoToPage(win);
            break;
        case 'h':
            FrameOnKeydown(win, VK_LEFT, 0);
            break;
        case 'j':
            FrameOnKeydown(win, VK_DOWN, 0);
            break;
        case 'k':
            FrameOnKeydown(win, VK_UP, 0);
            break;
        case 'l':
            FrameOnKeydown(win, VK_RIGHT, 0);
            break;
        case 'n':
            ctrl->GoToNextPage();
            break;
        case 'p':
            ctrl->GoToPrevPage();
            break;
        case 'z':
            win->ToggleZoom();
            break;
        case '[':
            win->AsFixed()->RotateBy(-90);
            break;
        case ']':
            win->AsFixed()->RotateBy(90);
            break;
        case 'f':
            if (win->isFullScreen == false) {
                EnterFullScreen(win);
            } else {
                ExitFullScreen(win);
            }
            break;
        // per https://en.wikipedia.org/wiki/Keyboard_layout
        // almost all keyboard layouts allow to press either
        // '+' or '=' unshifted (and one of them is also often
        // close to '-'); the other two alternatives are for
        // the major exception: the two Swiss layouts
        case '+':
        case '=':
        case 0xE0:
        case 0xE4:
            ZoomToSelection(win, win->ctrl->GetNextZoomStep(ZOOM_MAX), false);
            break;
        case '-':
            ZoomToSelection(win, win->ctrl->GetNextZoomStep(ZOOM_MIN), false);
            break;
        case '/':
            if (!gIsDivideKeyDown)
                OnMenuFind(win);
            gIsDivideKeyDown = false;
            break;
        case 'c':
            OnMenuViewContinuous(win);
            break;
        case 'b':
            OnFrameKeyB(win);
            break;
        case '.':
            // for Logitech's wireless presenters which target PowerPoint's shortcuts
            if (win->presentation) {
                win->ChangePresentationMode(PM_BLACK_SCREEN);
            }
            break;
        case 'w':
            if (win->presentation) {
                win->ChangePresentationMode(PM_WHITE_SCREEN);
            }
            break;
        case 'i':
            // experimental "page info" tip: make figuring out current page and
            // total pages count a one-key action (unless they're already visible)
            if (win->AsFixed() &&
                (!gGlobalPrefs->showToolbar || win->isFullScreen || PM_ENABLED == win->presentation)) {
                UpdatePageInfoHelper(win);
            }
            break;
        case 'm':
            OnFrameKeyM(win);
            break;
        case 'a':
            MakeAnnotationFromSelection(win);
            break;
    }
}

static bool FrameOnSysChar(WindowInfo* win, WPARAM key) {
    // use Alt+1 to Alt+8 for selecting the first 8 tabs and Alt+9 for the last tab
    if (win->tabsVisible && ('1' <= key && key <= '9')) {
        TabsSelect(win, key < '9' ? (int)(key - '1') : (int)win->tabs.size() - 1);
        return true;
    }

    return false;
}

static void OnSidebarSplitterMove(SplitterMoveArgs* args) {
    SplitterCtrl* splitter = args->w;
    HWND hwnd = splitter->hwnd;
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    CrashIf(!win);
    bool done = args->done;

    PointI pcur;
    GetCursorPosInHwnd(win->hwndFrame, pcur);
    int sidebarDx = pcur.x; // without splitter

    // make sure to keep this in sync with the calculations in RelayoutFrame
    // note: without the min/max(..., rToc.dx), the sidebar will be
    //       stuck at its width if it accidentally got too wide or too narrow
    ClientRect rFrame(win->hwndFrame);
    ClientRect rToc(win->hwndTocBox);
    int minDx = std::min(SIDEBAR_MIN_WIDTH, rToc.dx);
    int maxDx = std::max(rFrame.dx / 2, rToc.dx);
    if (sidebarDx < minDx || sidebarDx > maxDx) {
        args->resizeAllowed = false;
        return;
    }

    if (done || !win->AsEbook()) {
        RelayoutFrame(win, false, sidebarDx);
    }
}

static void OnFavSplitterMove(SplitterMoveArgs* args) {
    SplitterCtrl* splitter = args->w;
    HWND hwnd = splitter->hwnd;
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    CrashIf(!win);
    bool done = args->done;

    PointI pcur;
    GetCursorPosInHwnd(win->hwndTocBox, pcur);
    int tocDy = pcur.y; // without splitter

    // make sure to keep this in sync with the calculations in RelayoutFrame
    ClientRect rFrame(win->hwndFrame);
    ClientRect rToc(win->hwndTocBox);
    CrashIf(rToc.dx != ClientRect(win->hwndFavBox).dx);
    int minDy = std::min(TOC_MIN_DY, rToc.dy);
    int maxDy = std::max(rFrame.dy - TOC_MIN_DY, rToc.dy);
    if (tocDy < minDy || tocDy > maxDy) {
        args->resizeAllowed = false;
        return;
    }
    gGlobalPrefs->tocDy = tocDy;
    if (done || !win->AsEbook()) {
        RelayoutFrame(win, false, rToc.dx);
    }
}

void SetSidebarVisibility(WindowInfo* win, bool tocVisible, bool showFavorites) {
    if (gPluginMode || !HasPermission(Perm_DiskAccess)) {
        showFavorites = false;
    }

    if (!win->IsDocLoaded() || !win->ctrl->HacToc()) {
        tocVisible = false;
    }

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        tocVisible = false;
        showFavorites = false;
    }

    if (tocVisible) {
        LoadTocTree(win);
        CrashIf(!win->tocLoaded);
    }

    if (showFavorites) {
        PopulateFavTreeIfNeeded(win);
    }

    if (!win->currentTab) {
        CrashIf(tocVisible);
    } else if (!win->presentation) {
        win->currentTab->showToc = tocVisible;
    } else if (PM_ENABLED == win->presentation) {
        win->currentTab->showTocPresentation = tocVisible;
    }
    win->tocVisible = tocVisible;

    // TODO: make this a per-window setting as well?
    gGlobalPrefs->showFavorites = showFavorites;

    if ((!tocVisible && IsFocused(win->tocTreeCtrl->hwnd)) || (!showFavorites && IsFocused(win->favTreeCtrl->hwnd))) {
        SetFocus(win->hwndFrame);
    }

    win::SetVisibility(win->sidebarSplitter->hwnd, tocVisible || showFavorites);
    win::SetVisibility(win->hwndTocBox, tocVisible);
    win->sidebarSplitter->isLive = !win->AsEbook();

    win::SetVisibility(win->favSplitter->hwnd, tocVisible && showFavorites);
    win::SetVisibility(win->hwndFavBox, showFavorites);
    win->favSplitter->isLive = !win->AsEbook();

    RelayoutFrame(win, false);
}

// Tests that various ways to crash will generate crash report.
// Commented-out because they are ad-hoc. Left in code because
// I don't want to write them again if I ever need to test crash reporting
#if 0
#include <signal.h>
static void TestCrashAbort()
{
    raise(SIGABRT);
}

struct Base;
void foo(Base* b);

struct Base {
    Base() {
        foo(this);
    }
    virtual ~Base() = 0;
    virtual void pure() = 0;
};
struct Derived : public Base {
    void pure() { }
};

void foo(Base* b) {
    b->pure();
}

static void TestCrashPureCall()
{
    Derived d; // should crash
}

// tests that making a big allocation with new raises an exception
static int TestBigNew()
{
    size_t size = 1024*1024*1024*1;  // 1 GB should be out of reach
    char *mem = (char*)1;
    while (mem) {
        mem = new char[size];
    }
    // just some code so that compiler doesn't optimize this code to null
    for (size_t i = 0; i < 1024; i++) {
        mem[i] = i & 0xff;
    }
    int res = 0;
    for (size_t i = 0; i < 1024; i++) {
        res += mem[i];
    }
    return res;
}
#endif

// To avoid including mui/Mui.h, which conflicts with wingui/Layout.h
namespace mui {
extern void SetDebugPaint(bool);
extern bool IsDebugPaint();
} // namespace mui

static LRESULT FrameOnCommand(WindowInfo* win, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int wmId = LOWORD(wParam);

    if (wmId >= 0xF000) {
        // handle system menu messages for the Window menu (needed for Tabs in Titlebar)
        return SendMessage(hwnd, WM_SYSCOMMAND, wParam, lParam);
    }

    // check if the menuId belongs to an entry in the list of
    // recently opened files and load the referenced file if it does
    if ((wmId >= IDM_FILE_HISTORY_FIRST) && (wmId <= IDM_FILE_HISTORY_LAST)) {
        DisplayState* state = gFileHistory.Get(wmId - IDM_FILE_HISTORY_FIRST);
        if (state && HasPermission(Perm_DiskAccess)) {
            LoadArgs args(state->filePath, win);
            LoadDocument(args);
        }
        return 0;
    }

    // 10 submenus max with 10 items each max (=100) plus generous buffer => 200
    static_assert(IDM_FAV_LAST - IDM_FAV_FIRST == 200, "wrong number of favorite menu ids");
    if ((wmId >= IDM_FAV_FIRST) && (wmId <= IDM_FAV_LAST)) {
        GoToFavoriteByMenuId(win, wmId);
    }

#if defined(ENABLE_THEME)
    // check if the menuId belongs to a theme
    if ((wmId >= IDM_CHANGE_THEME_FIRST) && (wmId <= IDM_CHANGE_THEME_LAST)) {
        auto newThemeName = GetThemeByIndex(wmId - IDM_CHANGE_THEME_FIRST)->name;
        str::ReplacePtr(&gGlobalPrefs->themeName, newThemeName);
        RelayoutWindow(win);    // fix tabbar height
        UpdateDocumentColors(); // update document colors
        RedrawWindow(win->hwndFrame, nullptr, nullptr,
                     RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN); // paint new theme
        UpdateDocumentColors();         // doing this a second time ensures the frequently read documents are updated
        UpdateMenu(win, (HMENU)wParam); // update the radio buttons
        prefs::Save();                  // save new preferences
    }
#endif

    if (!win) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    if (!win->IsAboutWindow() && IDM_OPEN_WITH_EXTERNAL_FIRST <= wmId && wmId <= IDM_OPEN_WITH_EXTERNAL_LAST) {
        ViewWithExternalViewer(win->currentTab, wmId - IDM_OPEN_WITH_EXTERNAL_FIRST);
        return 0;
    }

    auto* ctrl = win->ctrl;
    // most of them require a win, the few exceptions are no-ops
    switch (wmId) {
        case IDM_NEW_WINDOW:
            OnMenuNewWindow();
            break;
        case IDM_DUPLICATE_IN_NEW_WINDOW:
            OnDuplicateInNewWindow(win);
            break;
        case IDM_OPEN:
        case IDT_FILE_OPEN:
            OnMenuOpen(win);
            break;

        case IDM_SAVEAS:
            OnMenuSaveAs(win);
            break;

        case IDM_RENAME_FILE:
            OnMenuRenameFile(win);
            break;

        case IDM_SHOW_IN_FOLDER:
            OnMenuShowInFolder(win);
            break;

        case IDT_FILE_PRINT:
        case IDM_PRINT:
            OnMenuPrint(win);
            break;

        case IDM_CLOSE:
        case IDT_FILE_EXIT:
            CloseTab(win);
            break;

        case IDM_EXIT:
            OnMenuExit();
            break;

        case IDM_REFRESH:
            ReloadDocument(win);
            break;

        case IDM_SAVEAS_BOOKMARK:
            OnMenuSaveBookmark(win);
            break;

        case IDT_VIEW_FIT_WIDTH:
            ChangeZoomLevel(win, ZOOM_FIT_WIDTH, true);
            break;

        case IDT_VIEW_FIT_PAGE:
            ChangeZoomLevel(win, ZOOM_FIT_PAGE, false);
            break;

        case IDT_VIEW_ZOOMIN:
            ZoomToSelection(win, ctrl->GetNextZoomStep(ZOOM_MAX), false);
            break;

        case IDT_VIEW_ZOOMOUT:
            ZoomToSelection(win, ctrl->GetNextZoomStep(ZOOM_MIN), false);
            break;

        case IDM_ZOOM_6400:
        case IDM_ZOOM_3200:
        case IDM_ZOOM_1600:
        case IDM_ZOOM_800:
        case IDM_ZOOM_400:
        case IDM_ZOOM_200:
        case IDM_ZOOM_150:
        case IDM_ZOOM_125:
        case IDM_ZOOM_100:
        case IDM_ZOOM_50:
        case IDM_ZOOM_25:
        case IDM_ZOOM_12_5:
        case IDM_ZOOM_8_33:
        case IDM_ZOOM_FIT_PAGE:
        case IDM_ZOOM_FIT_WIDTH:
        case IDM_ZOOM_FIT_CONTENT:
        case IDM_ZOOM_ACTUAL_SIZE:
            OnMenuZoom(win, wmId);
            break;

        case IDM_ZOOM_CUSTOM:
            OnMenuCustomZoom(win);
            break;

        case IDM_VIEW_SINGLE_PAGE:
            SwitchToDisplayMode(win, DM_SINGLE_PAGE, true);
            break;

        case IDM_VIEW_FACING:
            SwitchToDisplayMode(win, DM_FACING, true);
            break;

        case IDM_VIEW_BOOK:
            SwitchToDisplayMode(win, DM_BOOK_VIEW, true);
            break;

        case IDM_VIEW_CONTINUOUS:
            OnMenuViewContinuous(win);
            break;

        case IDM_VIEW_MANGA_MODE:
            OnMenuViewMangaMode(win);
            break;

        case IDM_VIEW_SHOW_HIDE_TOOLBAR:
            OnMenuViewShowHideToolbar();
            break;

        case IDM_SAVE_ANNOTATIONS_SMX:
            OnMenuSaveAnnotationsToSmx(win);
            break;

        case IDM_VIEW_SHOW_HIDE_MENUBAR:
            if (!win->tabsInTitlebar) {
                ShowHideMenuBar(win);
            }
            break;

        case IDM_CHANGE_LANGUAGE:
            OnMenuChangeLanguage(win->hwndFrame);
            break;

        case IDM_VIEW_BOOKMARKS:
            ToggleTocBox(win);
            break;

        case IDM_GOTO_NEXT_PAGE:
            if (win->IsDocLoaded())
                ctrl->GoToNextPage();
            break;

        case IDM_GOTO_PREV_PAGE:
            if (win->IsDocLoaded())
                ctrl->GoToPrevPage();
            break;

        case IDM_GOTO_FIRST_PAGE:
            if (win->IsDocLoaded())
                ctrl->GoToFirstPage();
            break;

        case IDM_GOTO_LAST_PAGE:
            if (win->IsDocLoaded())
                ctrl->GoToLastPage();
            break;

        case IDM_GOTO_PAGE:
            OnMenuGoToPage(win);
            break;

        case IDM_VIEW_PRESENTATION_MODE:
            OnMenuViewPresentation(win);
            break;

        case IDM_VIEW_FULLSCREEN:
            OnMenuViewFullscreen(win);
            break;

        case IDM_VIEW_ROTATE_LEFT:
            if (win->AsFixed())
                win->AsFixed()->RotateBy(-90);
            break;

        case IDM_VIEW_ROTATE_RIGHT:
            if (win->AsFixed()) {
                win->AsFixed()->RotateBy(90);
            }
            break;

        case IDM_FIND_FIRST:
            OnMenuFind(win);
            break;

        case IDM_FIND_NEXT:
            OnMenuFindNext(win);
            break;

        case IDM_FIND_PREV:
            OnMenuFindPrev(win);
            break;

        case IDM_FIND_MATCH:
            OnMenuFindMatchCase(win);
            break;

        case IDM_FIND_NEXT_SEL:
            OnMenuFindSel(win, TextSearchDirection::Forward);
            break;

        case IDM_FIND_PREV_SEL:
            OnMenuFindSel(win, TextSearchDirection::Backward);
            break;

        case IDM_VISIT_WEBSITE:
            LaunchBrowser(WEBSITE_MAIN_URL);
            break;

        case IDM_MANUAL:
            LaunchBrowser(WEBSITE_MANUAL_URL);
            break;

        case IDM_CONTRIBUTE_TRANSLATION:
            LaunchBrowser(WEBSITE_TRANSLATIONS_URL);
            break;

        case IDM_ABOUT:
#ifdef ENABLE_ALTERNATIVE_ABOUT_DIALOG
            OnMenuAbout2();
#else
            OnMenuAbout();
#endif
            break;

        case IDM_CHECK_UPDATE:
            UpdateCheckAsync(win, false);
            break;

        case IDM_OPTIONS:
            OnMenuOptions(win);
            break;

        case IDM_ADVANCED_OPTIONS:
            OnMenuAdvancedOptions();
            break;

        case IDM_VIEW_WITH_ACROBAT:
            ViewWithAcrobat(win->currentTab);
            break;

        case IDM_VIEW_WITH_FOXIT:
            ViewWithFoxit(win->currentTab);
            break;

        case IDM_VIEW_WITH_PDF_XCHANGE:
            ViewWithPDFXChange(win->currentTab);
            break;

        case IDM_VIEW_WITH_XPS_VIEWER:
            ViewWithXPSViewer(win->currentTab);
            break;

        case IDM_VIEW_WITH_HTML_HELP:
            ViewWithHtmlHelp(win->currentTab);
            break;

        case IDM_SEND_BY_EMAIL:
            SendAsEmailAttachment(win->currentTab, win->hwndFrame);
            break;

        case IDM_PROPERTIES:
            OnMenuProperties(win);
            break;

        case IDM_MOVE_FRAME_FOCUS:
            if (!IsFocused(win->hwndFrame)) {
                SetFocus(win->hwndFrame);
            } else if (win->tocVisible) {
                SetFocus(win->tocTreeCtrl->hwnd);
            }
            break;

        case IDM_GOTO_NAV_BACK:
            if (win->IsDocLoaded()) {
                ctrl->Navigate(-1);
            }
            break;

        case IDM_GOTO_NAV_FORWARD:
            if (win->IsDocLoaded()) {
                ctrl->Navigate(1);
            }
            break;

        case IDM_COPY_SELECTION:
            // Don't break the shortcut for text boxes
            if (IsFocused(win->hwndFindBox) || IsFocused(win->hwndPageBox))
                SendMessage(GetFocus(), WM_COPY, 0, 0);
            else if (!HasPermission(Perm_CopySelection))
                break;
            else if (win->AsChm())
                win->AsChm()->CopySelection();
            else if (win->currentTab && win->currentTab->selectionOnPage)
                CopySelectionToClipboard(win);
            else if (win->AsFixed())
                win->ShowNotification(_TR("Select content with Ctrl+left mouse button"));
            break;

        case IDM_SELECT_ALL:
            OnSelectAll(win);
            break;

        case IDM_DEBUG_SHOW_LINKS:
            gDebugShowLinks = !gDebugShowLinks;
            for (auto& w : gWindows) {
                w->RedrawAll(true);
            }
            break;

        case IDM_DEBUG_EBOOK_UI:
            gGlobalPrefs->ebookUI.useFixedPageUI = !gGlobalPrefs->ebookUI.useFixedPageUI;
            // use the same setting to also toggle the CHM UI
            gGlobalPrefs->chmUI.useFixedPageUI = !gGlobalPrefs->chmUI.useFixedPageUI;
            break;

        case IDM_DEBUG_DOWNLOAD_SYMBOLS:
            DownloadDebugSymbols();
            break;

        case IDM_DEBUG_MUI:
            mui::SetDebugPaint(!mui::IsDebugPaint());
            win::menu::SetChecked(GetMenu(win->hwndFrame), IDM_DEBUG_MUI, !mui::IsDebugPaint());
            for (size_t i = 0; i < gWindows.size(); i++) {
                gWindows.at(i)->RedrawAll(true);
            }
            break;

        case IDM_DEBUG_ANNOTATION:
            FrameOnChar(win, 'h');
            break;

        case IDM_DEBUG_CRASH_ME:
            CrashMe();
            break;

        case IDM_FAV_ADD:
            AddFavoriteForCurrentPage(win);
            break;

        case IDM_FAV_DEL:
            DelFavorite(win->ctrl->FilePath(), win->currPageNo);
            break;

        case IDM_FAV_TOGGLE:
            ToggleFavorites(win);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

static LRESULT OnFrameGetMinMaxInfo(MINMAXINFO* info) {
    // limit windows min width to prevent render loop when siderbar is too big
    info->ptMinTrackSize.x = MIN_WIN_DX - SIDEBAR_MIN_WIDTH + gGlobalPrefs->sidebarDx;
    info->ptMinTrackSize.y = MIN_WIN_DY;
    return 0;
}

LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);

    if (win && win->tabsInTitlebar) {
        bool callDefault = true;
        LRESULT res = CustomCaptionFrameProc(hwnd, msg, wParam, lParam, &callDefault, win);
        if (!callDefault) {
            return res;
        }
    }

    switch (msg) {
        case WM_CREATE:
            // do nothing
            goto InitMouseWheelInfo;

        case WM_SIZE:
            if (win && SIZE_MINIMIZED != wParam) {
                RememberDefaultWindowPosition(win);
                int dx = LOWORD(lParam);
                int dy = HIWORD(lParam);
                // dbglog::LogF("dx: %d, dy: %d", dx, dy);
                FrameOnSize(win, dx, dy);
            }
            break;

        case WM_GETMINMAXINFO:
            return OnFrameGetMinMaxInfo((MINMAXINFO*)lParam);

        case WM_MOVE:
            if (win) {
                RememberDefaultWindowPosition(win);
            }
            break;

        case WM_INITMENUPOPUP:
            UpdateMenu(win, (HMENU)wParam);
            break;

        case WM_COMMAND:
            return FrameOnCommand(win, hwnd, msg, wParam, lParam);

#if defined(EXP_MENU_OWNER_DRAW)
        case WM_MEASUREITEM:
            MenuOwnerDrawnMesureItem(hwnd, (MEASUREITEMSTRUCT*)lParam);
            return TRUE;

        case WM_DRAWITEM:
            MenuOwnerDrawnDrawItem(hwnd, (DRAWITEMSTRUCT*)lParam);
            return TRUE;
#endif

        case WM_APPCOMMAND:
            // both keyboard and mouse drivers should produce WM_APPCOMMAND
            // messages for their special keys, so handle these here and return
            // TRUE so as to not make them bubble up further
            switch (GET_APPCOMMAND_LPARAM(lParam)) {
                case APPCOMMAND_BROWSER_BACKWARD:
                    SendMessage(hwnd, WM_COMMAND, IDM_GOTO_NAV_BACK, 0);
                    return TRUE;
                case APPCOMMAND_BROWSER_FORWARD:
                    SendMessage(hwnd, WM_COMMAND, IDM_GOTO_NAV_FORWARD, 0);
                    return TRUE;
                case APPCOMMAND_BROWSER_REFRESH:
                    SendMessage(hwnd, WM_COMMAND, IDM_REFRESH, 0);
                    return TRUE;
                case APPCOMMAND_BROWSER_SEARCH:
                    SendMessage(hwnd, WM_COMMAND, IDM_FIND_FIRST, 0);
                    return TRUE;
                case APPCOMMAND_BROWSER_FAVORITES:
                    SendMessage(hwnd, WM_COMMAND, IDM_VIEW_BOOKMARKS, 0);
                    return TRUE;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_CHAR:
            if (win) {
                FrameOnChar(win, wParam, lParam);
            }
            break;

        case WM_KEYDOWN:
            if (win) {
                FrameOnKeydown(win, wParam, lParam);
            }
            break;

        case WM_SYSKEYUP:
            // pressing and releasing the Alt key focuses the menu even if
            // the wheel has been used for scrolling horizontally, so we
            // have to suppress that effect explicitly in this situation
            if (VK_MENU == wParam && gSuppressAltKey) {
                gSuppressAltKey = false;
                return 0;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_SYSCHAR:
            if (win && FrameOnSysChar(win, wParam))
                return 0;
            return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_SYSCOMMAND:
            // temporarily show the menu bar if it has been hidden
            if (wParam == SC_KEYMENU && win && win->isMenuHidden) {
                ShowHideMenuBar(win, true);
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_EXITMENULOOP:
            // hide the menu bar again if it was shown only temporarily
            if (!wParam && win && win->isMenuHidden) {
                SetMenu(hwnd, nullptr);
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_CONTEXTMENU: {
            // opening the context menu with a keyboard doesn't call the canvas'
            // WM_CONTEXTMENU, as it never has the focus (mouse right-clicks are
            // handled as expected)
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (win && (x == -1) && (y == -1) && !IsFocused(win->tocTreeCtrl->hwnd))
                return SendMessage(win->hwndCanvas, WM_CONTEXTMENU, wParam, lParam);
            return DefWindowProc(hwnd, msg, wParam, lParam);
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
                return win->AsChm()->PassUIMsg(msg, wParam, lParam);
            }
            CrashIf(!win->AsFixed() && !win->AsEbook());
            // Pass the message to the canvas' window procedure
            // (required since the canvas itself never has the focus and thus
            // never receives WM_MOUSEWHEEL messages)
            return SendMessage(win->hwndCanvas, msg, wParam, lParam);

        case WM_CLOSE:
            if (MayCloseWindow(win)) {
                CloseWindow(win, true);
            }
            break;

        case WM_DESTROY: {
            /* WM_DESTROY is generated by windows when close button is pressed
               or if we explicitly call DestroyWindow()
               It might be sent as a result of File\Close, in which
               case CloseWindow() has already been called. */
            FreeMenuOwnerDrawInfoData(GetMenu(hwnd));
            if (win) {
                CloseWindow(win, true, true);
            }
        } break;

        case WM_ENDSESSION:
            // TODO: check for unfinished print jobs in WM_QUERYENDSESSION?
            if (wParam == TRUE) {
                prefs::Save();
                // we must quit so that we restore opened files on start.
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_DDE_INITIATE:
            if (gPluginMode)
                break;
            return OnDDEInitiate(hwnd, wParam, lParam);
        case WM_DDE_EXECUTE:
            return OnDDExecute(hwnd, wParam, lParam);
        case WM_DDE_TERMINATE:
            return OnDDETerminate(hwnd, wParam, lParam);

        case WM_COPYDATA:
            return OnCopyData(hwnd, wParam, lParam);

        case WM_TIMER:
            if (win && win->stressTest) {
                OnStressTestTimer(win, (int)wParam);
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
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

bool IsDllBuild() {
    HRSRC resSrc = FindResourceW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(1), RT_RCDATA);
    return resSrc != nullptr;
}

void GetProgramInfo(str::Str& s) {
    AutoFree d = strconv::WstrToUtf8(gCrashFilePath);
    s.AppendFmt("Crash file: %s\r\n", d.data);
    AutoFree exePath = GetExePathA();
    s.AppendFmt("Exe: %s\r\n", exePath.get());
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

    if (gitSha1 != nullptr) {
        s.AppendFmt("Git: %s (https://github.com/sumatrapdfreader/sumatrapdf/commit/%s)\r\n", gitSha1, gitSha1);
    }
}

bool CrashHandlerCanUseNet() {
    return HasPermission(Perm_InternetAccess);
}

void ShowCrashHandlerMessage() {
    // don't show a message box in restricted use, as the user most likely won't be
    // able to do anything about it anyway and it's up to the application provider
    // to fix the unexpected behavior (of which for a restricted set of documents
    // there should be much less, anyway)
    if (!HasPermission(Perm_DiskAccess)) {
        return;
    }

#if 0
    int res = MessageBox(nullptr, _TR("Sorry, that shouldn't have happened!\n\nPlease press 'Cancel', if you want to help us fix the cause of this crash."), _TR("SumatraPDF crashed"), MB_ICONERROR | MB_OKCANCEL | MbRtlReadingMaybe());
    if (IDCANCEL == res) {
        LaunchBrowser(CRASH_REPORT_URL);
    }
#endif

    char* msg = "We're sorry, SumatraPDF crashed.\n\nPress 'Cancel' to see crash report.";
    UINT flags = MB_ICONERROR | MB_OK | MB_OKCANCEL | MbRtlReadingMaybe();
    flags |= MB_SETFOREGROUND | MB_TOPMOST;
    int res = MessageBoxA(nullptr, msg, "SumatraPDF crashed", flags);
    if (IDCANCEL == res) {
        LaunchFile(gCrashFilePath, nullptr, L"open");
    }
}

static WCHAR* GetSymbolsDir() {
    if (IsRunningInPortableMode()) {
        /* Use the same path as the binary */
        return GetExeDir();
    }
    AutoFreeWstr dir = GetSpecialFolder(CSIDL_LOCAL_APPDATA, true);
    return path::Join(dir, getAppName(), L"crashinfo");
}

static void DownloadDebugSymbols() {
    // over-ride the default symbols directory to be more useful
    WCHAR* symDir = GetSymbolsDir();
    SetSymbolsDir(symDir);

    bool ok = CrashHandlerDownloadSymbols();
    char* msg = nullptr;
    if (ok) {
        AutoFree symDirA = strconv::WstrToUtf8(symDir);
        msg = str::Format("Downloaded symbols! to %s", symDirA.data);
    } else {
        msg = str::Dup("Failed to download symbols.");
    }
    UINT flags = MB_ICONINFORMATION | MB_OK | MbRtlReadingMaybe();
    MessageBoxA(nullptr, msg, "Downloading symbols", flags);

    free(msg);
    free(symDir);
}
