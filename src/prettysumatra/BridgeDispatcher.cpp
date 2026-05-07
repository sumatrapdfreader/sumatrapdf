#include "utils/BaseUtil.h"

#include "Settings.h"
#include "SumatraPDF.h"

#include "prettysumatra/BridgeDispatcher.h"
#include "prettysumatra/CommandBridgeSpec.h"

#include "Commands.h"
#include "DisplayMode.h"
#include "wingui/UIModels.h"
#include "DocController.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"
#include "MainWindow.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SearchAndDDE.h"
#include "WindowTab.h"
#include "Theme.h"
#include "Translations.h"
#include "AppSettings.h"
#include "DarkModeSubclass.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"
#include "FileHistory.h"

#include "utils/JsonParser.h"
#include "utils/Log.h"
#include "utils/StrUtil.h"
#include "utils/WinUtil.h"

namespace prettysumatra {
namespace bridge {

static bool ParseBoolEnvWithDefault(const char* envName, bool defValue) {
    char buf[16] = {};
    DWORD n = GetEnvironmentVariableA(envName, buf, dimof(buf));
    if (n == 0 || n >= dimof(buf)) {
        return defValue;
    }
    if (str::EqI(buf, "1") || str::EqI(buf, "true") || str::EqI(buf, "yes") || str::EqI(buf, "on")) {
        return true;
    }
    if (str::EqI(buf, "0") || str::EqI(buf, "false") || str::EqI(buf, "no") || str::EqI(buf, "off")) {
        return false;
    }
    return defValue;
}

static TempStr ColorToCssHex(COLORREF c) {
    return str::FormatTemp("#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
}

static bool WindowsPrefersDarkModeForHybridToolbar() {
    DWORD val = 1;
    DWORD cbData = sizeof(val);
    constexpr const wchar_t* kThemeRegPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    LONG err = RegGetValueW(HKEY_CURRENT_USER, kThemeRegPath, L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &val,
                            &cbData);
    if (err != ERROR_SUCCESS) {
        return false;
    }
    return val == 0;
}

bool UseHybridToolbar() {
    return ParseBoolEnvWithDefault("PRETTYSUMATRA_WEBVIEW_TOOLBAR", true);
}

bool UseHybridSidebar() {
    return ParseBoolEnvWithDefault("PRETTYSUMATRA_WEBVIEW_SIDEBAR", false);
}

bool UseHybridShell() {
    if (ParseBoolEnvWithDefault("PRETTYSUMATRA_WEBVIEW_SHELL", false)) {
        return true;
    }
    return UseHybridToolbar() || UseHybridSidebar();
}

bool LogBridgeMessages() {
    return ParseBoolEnvWithDefault("PRETTYSUMATRA_LOG_BRIDGE", true);
}

static bool gHybridFollowWindowsTheme = true;
static bool gHybridFollowWindowsThemeInitialized = false;

static void EnsureHybridFollowWindowsThemeState() {
    if (gHybridFollowWindowsThemeInitialized) {
        return;
    }
    gHybridFollowWindowsTheme = IsCurrentThemeDefault();
    gHybridFollowWindowsThemeInitialized = true;
}

bool HybridThemeFollowsWindows() {
    EnsureHybridFollowWindowsThemeState();
    return gHybridFollowWindowsTheme;
}

static MainWindow* FindWindowForFrame(HWND hwndFrame) {
    if (!hwndFrame) {
        return nullptr;
    }
    return FindMainWindowByHwnd(hwndFrame);
}

static bool HomePageUsesDarkTheme() {
    return DarkMode::isColorDark(ThemeMainWindowBackgroundColor());
}

static TempStr HybridToolbarThemeJs(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return nullptr;
    }

    COLORREF canvas = ThemeMainWindowBackgroundColor();
    COLORREF panel = ThemeMainWindowBackgroundColor();
    COLORREF panel2 = PrettyStyleEnabled() ? PrettySurfaceColor() : ThemeWindowControlBackgroundColor();
    COLORREF stroke = PrettyBorderColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF muted = ThemeWindowTextDisabledColor();
    COLORREF btn = ThemeControlBackgroundColor();
    COLORREF accent = PrettyAccentColor();
    // Keep toolbar brand colors stable (gold) instead of following system accent (often blue).
    COLORREF brand1 = RGB(248, 204, 24);
    COLORREF brand2 = RGB(215, 167, 0);
    bool appDark = DarkMode::isColorDark(panel);
    bool docInverted = gGlobalPrefs->fixedPageUI.invertColors;
    bool windowsDark = WindowsPrefersDarkModeForHybridToolbar();
    bool followWindows = HybridThemeFollowsWindows();

    return str::FormatTemp(
        "window.__hybridToolbarThemePayload={canvas:'%s',panel:'%s',panel2:'%s',stroke:'%s',"
        "text:'%s',muted:'%s',btn:'%s',accent:'%s',brand1:'%s',brand2:'%s',appDark:%s,docInverted:%s,windowsDark:%s,followWindows:%s};"
        "if(window.hybridToolbarApplyTheme){window.hybridToolbarApplyTheme(window.__hybridToolbarThemePayload);}",
        ColorToCssHex(canvas), ColorToCssHex(panel), ColorToCssHex(panel2), ColorToCssHex(stroke), ColorToCssHex(text),
        ColorToCssHex(muted), ColorToCssHex(btn), ColorToCssHex(accent), ColorToCssHex(brand1), ColorToCssHex(brand2),
        appDark ? "true" : "false", docInverted ? "true" : "false", windowsDark ? "true" : "false",
        followWindows ? "true" : "false");
}

static const char* JsQuoted(const char* s) {
    if (!s) {
        static const char empty[] = {'\'', '\'', '\0'};
        return empty;
    }
    
    // Calculate needed size
    size_t len = 0;
    for (const char* p = s; *p; ++p) {
        switch (*p) {
            case '\\':
            case '\'':
            case '\n':
            case '\t':
                len += 2;
                break;
            default:
                len += 1;
                break;
        }
    }
    len += 2;  // for surrounding quotes
    len += 1;  // for null terminator
    
    char* out = (char*)malloc(len);
    if (!out) return nullptr;
    
    char* dst = out;
    *dst++ = '\'';
    for (const char* p = s; *p; ++p) {
        switch (*p) {
            case '\\':
                *dst++ = '\\';
                *dst++ = '\\';
                break;
            case '\'':
                *dst++ = '\\';
                *dst++ = '\'';
                break;
            case '\r':
                break;  // skip carriage returns
            case '\n':
                *dst++ = '\\';
                *dst++ = 'n';
                break;
            case '\t':
                *dst++ = '\\';
                *dst++ = 't';
                break;
            default:
                *dst++ = *p;
                break;
        }
    }
    *dst++ = '\'';
    *dst = '\0';
    
    return out;
}

static TempStr HybridToolbarTextJs(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return nullptr;
    }

    return str::FormatTemp(
        "window.__hybridToolbarTextPayload={lang:%s,subtitle:%s,openButton:%s,pagePrevTitle:%s,pageNextTitle:%s,"
        "pageTemplate:%s,zoomOutTitle:%s,zoomInTitle:%s,viewSinglePage:%s,viewFacing:%s,viewBookView:%s,"
        "continuousTitle:%s,searchPlaceholder:%s,bookmarksTitle:%s,favoritesTitle:%s,fullscreenTitle:%s,"
        "commandPaletteText:%s,rotateLeftTitle:%s,rotateRightTitle:%s,printTitle:%s,themeLabel:%s,"
            "followWindowsTitle:%s,followingWindowsTitle:%s,darkWord:%s,lightWord:%s,toggleThemeTitle:%s,documentLabel:%s,documentInvertTitle:%s};"
        "if(window.hybridToolbarApplyText){window.hybridToolbarApplyText(window.__hybridToolbarTextPayload);}",
        JsQuoted(trans::GetCurrentLangCode()), JsQuoted(_TRA("Focused reading")), JsQuoted(_TRA("Open")),
        JsQuoted(_TRA("Previous page")), JsQuoted(_TRA("Next page")), JsQuoted(_TRA("Page {current} / {total}")),
        JsQuoted(_TRA("Zoom out")), JsQuoted(_TRA("Zoom in")), JsQuoted(_TRA("Single Page")),
        JsQuoted(_TRA("Facing")), JsQuoted(_TRA("Book View")), JsQuoted(_TRA("Show pages continuously")),
        JsQuoted(_TRA("Search text")), JsQuoted(_TRA("Sidebar")), JsQuoted(_TRA("Favorites")),
        JsQuoted(_TRA("Fullscreen")), JsQuoted(_TRA("Cmd")), JsQuoted(_TRA("Rotate left")),
        JsQuoted(_TRA("Rotate right")), JsQuoted(_TRA("Print")), JsQuoted(_TRA("Theme")),
        JsQuoted(_TRA("Follow Windows")), JsQuoted(_TRA("Following Windows ({mode})")), JsQuoted(_TRA("dark")),
        JsQuoted(_TRA("light")), JsQuoted(_TRA("Toggle light/dark")), JsQuoted(_TRA("Doc")),
        JsQuoted(_TRA("Invert document colors")));
}

bool HasHybridToolbar(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    return win && win->hybridToolbar;
}

void SyncHybridToolbarTheme(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    TempStr js = HybridToolbarThemeJs(hwndFrame);
    if (!js) {
        return;
    }
    win->hybridToolbar->Eval(js);
}

void SyncHybridToolbarText(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    TempStr js = HybridToolbarTextJs(hwndFrame);
    if (!js) {
        return;
    }
    win->hybridToolbar->Eval(js);
}

void SyncHybridToolbarSearchText(HWND hwndFrame, const char* text) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    TempStr escaped = str::ReplaceTemp(text ? text : "", "\\", "\\\\");
    escaped = str::ReplaceTemp(escaped, "'", "\\'");
    escaped = str::ReplaceTemp(escaped, "\r", "");
    escaped = str::ReplaceTemp(escaped, "\n", "\\n");
    TempStr js = str::FormatTemp("window.hybridToolbarSetSearchText && window.hybridToolbarSetSearchText('%s');", escaped);
    win->hybridToolbar->Eval(js);
}

void FocusHybridToolbarSearch(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }

    win->hybridToolbar->Focus();

    // Then focus the search input element inside the webview
    win->hybridToolbar->Eval("window.hybridToolbarFocusSearch && window.hybridToolbarFocusSearch();");
}

void InitHybridToolbarTheme(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    TempStr js = HybridToolbarThemeJs(hwndFrame);
    if (!js) {
        return;
    }
    win->hybridToolbar->Init(js);
}

void InitHybridToolbarText(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    TempStr js = HybridToolbarTextJs(hwndFrame);
    if (!js) {
        return;
    }
    win->hybridToolbar->Init(js);
}

void SyncHybridToolbarPageState(HWND hwndFrame, int currentPage, int totalPages) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    if (currentPage <= 0) {
        currentPage = 1;
    }
    if (totalPages <= 0) {
        totalPages = 1;
    }
    TempStr js = str::FormatTemp("window.hybridToolbarSetPageState && window.hybridToolbarSetPageState(%d,%d);",
                                 currentPage, totalPages);
    win->hybridToolbar->Eval(js);
}

void SyncHybridToolbarZoomState(HWND hwndFrame, float zoomPercent) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    if (zoomPercent < 1.0f) {
        zoomPercent = 1.0f;
    }
    if (zoomPercent > 6400.0f) {
        zoomPercent = 6400.0f;
    }
    TempStr js = str::FormatTemp("window.hybridToolbarSetZoomState && window.hybridToolbarSetZoomState(%.4f);", zoomPercent);
    win->hybridToolbar->Eval(js);
}

struct BridgeMessage {
    const char* name = nullptr;
    const char* path = nullptr;
    const char* query = nullptr;
    const char* command = nullptr;
    const char* direction = nullptr;
    const char* mode = nullptr;
    int page = 0;
    int cmdId = 0;
    float value = 0.0f;
    bool hasPage = false;
    bool hasCmdId = false;
    bool hasValue = false;
};

class BridgeMessageVisitor : public json::ValueVisitor {
  public:
    explicit BridgeMessageVisitor(BridgeMessage* out) : msg(out) {}

    bool Visit(const char* path, const char* value, json::Type type) override {
        if (!msg || !path || !value) {
            return false;
        }
        if (str::Eq(path, "/name") && type == json::Type::String) {
            msg->name = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/path") && type == json::Type::String) {
            msg->path = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/query") && type == json::Type::String) {
            msg->query = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/command") && type == json::Type::String) {
            msg->command = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/direction") && type == json::Type::String) {
            msg->direction = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/mode") && type == json::Type::String) {
            msg->mode = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/page") && type == json::Type::Number) {
            msg->page = atoi(value);
            msg->hasPage = true;
            return true;
        }
        if (str::Eq(path, "/payload/cmdId") && type == json::Type::Number) {
            msg->cmdId = atoi(value);
            msg->hasCmdId = true;
            return true;
        }
        if (str::Eq(path, "/payload/value") && type == json::Type::Number) {
            msg->value = (float)atof(value);
            msg->hasValue = true;
            return true;
        }
        return true;
    }

  private:
    BridgeMessage* msg = nullptr;
};

static bool ParseBridgeMessage(const char* rawMsg, BridgeMessage& out) {
    if (str::IsEmptyOrWhiteSpace(rawMsg)) {
        return false;
    }
    BridgeMessageVisitor visitor(&out);
    bool ok = json::Parse(rawMsg, &visitor);
    return ok && !str::IsEmpty(out.name);
}

static MainWindow* GetTargetWindow() {
    HWND hwnd = GetForegroundWindow();
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win && gLastActiveFrameHwnd) {
        win = FindMainWindowByHwnd(gLastActiveFrameHwnd);
    }
    if (!win && !gWindows.IsEmpty()) {
        win = gWindows.at(0);
    }
    return win;
}

static bool DispatchOpenFile(const BridgeMessage& msg) {
    if (!CanAccessDisk()) {
        return false;
    }
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    if (str::IsEmpty(msg.path)) {
        PostMessageW(win->hwndFrame, WM_COMMAND, CmdOpenFile, 0);
        return true;
    }
    LoadArgs args(msg.path, win);
    LoadDocument(&args);
    return true;
}

static bool DispatchGoToPage(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win || !win->ctrl || !msg.hasPage) {
        return false;
    }
    if (!win->ctrl->ValidPageNo(msg.page)) {
        return false;
    }
    win->ctrl->GoToPage(msg.page, true);
    return true;
}

static bool DispatchZoom(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win || !win->ctrl || !msg.hasValue) {
        return false;
    }

    float zoom = msg.value;
    if (zoom > 0 && zoom <= 8.0f) {
        zoom *= 100.0f;
    }
    if (zoom < kZoomMin) {
        zoom = kZoomMin;
    }
    if (zoom > kZoomMax) {
        zoom = kZoomMax;
    }
    win->ctrl->SetZoomVirtual(zoom, nullptr);
    SyncHybridToolbarZoomState(win->hwndFrame, win->ctrl->GetZoomVirtual(true));
    return true;
}

static bool DispatchSetFitMode(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win || !win->ctrl || str::IsEmpty(msg.mode)) {
        return false;
    }

    float zoom = 0;
    if (str::EqI(msg.mode, "page-width")) {
        zoom = kZoomFitWidth;
    } else if (str::EqI(msg.mode, "page")) {
        zoom = kZoomFitPage;
    } else if (str::EqI(msg.mode, "actual-size")) {
        zoom = kZoomActualSize;
    } else {
        return false;
    }
    win->ctrl->SetZoomVirtual(zoom, nullptr);
    SyncHybridToolbarZoomState(win->hwndFrame, win->ctrl->GetZoomVirtual(true));
    return true;
}

static bool DispatchSearch(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win || !win->IsDocLoaded() || str::IsEmptyOrWhiteSpace(msg.query)) {
        return false;
    }

    TextSearch::Direction dir = TextSearch::Direction::Forward;
    if (str::EqI(msg.direction, "backward") || str::EqI(msg.direction, "prev") || str::EqI(msg.direction, "previous")) {
        dir = TextSearch::Direction::Backward;
    }

    bool wasModified = true;
    TempStr currentFind = HwndGetTextTemp(win->hwndFindEdit);
    if (str::Eq(msg.query, currentFind)) {
        wasModified = false;
    }
    FindTextOnThread(win, dir, msg.query, wasModified, true);
    return true;
}

static bool DispatchToggleSidebar() {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    bool nextTocVisible = !win->tocVisible;
    SetSidebarVisibility(win, nextTocVisible, gGlobalPrefs->showFavorites);
    return true;
}

static bool DispatchSetViewMode(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win || str::IsEmpty(msg.mode)) {
        return false;
    }

    DisplayMode mode = DisplayMode::Automatic;
    if (str::EqI(msg.mode, "continuous")) {
        mode = DisplayMode::Continuous;
    } else if (str::EqI(msg.mode, "single-page")) {
        mode = DisplayMode::SinglePage;
    } else {
        return false;
    }
    SwitchToDisplayMode(win, mode, false);
    return true;
}

static bool DispatchPrint() {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    PostMessageW(win->hwndFrame, WM_COMMAND, CmdPrint, 0);
    return true;
}

static bool WindowsPrefersDarkModeForHybrid() {
    DWORD val = 1;
    DWORD cbData = sizeof(val);
    constexpr const wchar_t* kThemeRegPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    LONG err = RegGetValueW(HKEY_CURRENT_USER, kThemeRegPath, L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &val,
                            &cbData);
    if (err != ERROR_SUCCESS) {
        return false;
    }
    return val == 0;
}

static void ApplyThemeState(bool targetDark) {
    SetTheme(targetDark ? "Dark" : "Light");
    SaveSettings();
}

static bool DispatchToggleThemeFollowWindows() {
    EnsureHybridFollowWindowsThemeState();
    gHybridFollowWindowsTheme = !gHybridFollowWindowsTheme;

    if (gHybridFollowWindowsTheme) {
        bool windowsDark = WindowsPrefersDarkModeForHybrid();
        ApplyThemeState(windowsDark);
    } else {
        SaveSettings();
    }
    MainWindow* win = GetTargetWindow();
    if (win) {
        SyncHybridToolbarTheme(win->hwndFrame);
    }
    return true;
}

static bool DispatchToggleThemeLightDark() {
    EnsureHybridFollowWindowsThemeState();
    // Theme button always toggles only Light/Dark and exits follow-Windows mode.
    gHybridFollowWindowsTheme = false;

    bool appIsDark = DarkMode::isColorDark(ThemeWindowControlBackgroundColor());
    bool targetDark = !appIsDark;
    ApplyThemeState(targetDark);
    MainWindow* win = GetTargetWindow();
    if (win) {
        SyncHybridToolbarTheme(win->hwndFrame);
    }
    return true;
}

static bool DispatchToggleDocumentInvert() {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }

    gGlobalPrefs->fixedPageUI.invertColors ^= true;
    UpdateDocumentColors();
    UpdateControlsColors(win);
    SaveSettings();
    SyncHybridToolbarTheme(win->hwndFrame);
    return true;
}

static int ResolveBridgeCommandId(const char* commandName) {
    if (str::IsEmptyOrWhiteSpace(commandName)) {
        return 0;
    }
    if (str::EqI(commandName, "commandPalette")) return CmdCommandPalette;
    if (str::EqI(commandName, "properties")) return CmdProperties;
    if (str::EqI(commandName, "find")) return CmdFindFirst;
    if (str::EqI(commandName, "newWindow")) return CmdNewWindow;
    if (str::EqI(commandName, "saveAs")) return CmdSaveAs;
    if (str::EqI(commandName, "reload")) return CmdReloadDocument;
    if (str::EqI(commandName, "reopenLastClosed")) return CmdReopenLastClosedFile;

    if (str::EqI(commandName, "navigateBack")) return CmdNavigateBack;
    if (str::EqI(commandName, "navigateForward")) return CmdNavigateForward;
    if (str::EqI(commandName, "prevPage")) return CmdGoToPrevPage;
    if (str::EqI(commandName, "nextPage")) return CmdGoToNextPage;
    if (str::EqI(commandName, "firstPage")) return CmdGoToFirstPage;
    if (str::EqI(commandName, "lastPage")) return CmdGoToLastPage;

    if (str::EqI(commandName, "zoomIn")) return CmdZoomIn;
    if (str::EqI(commandName, "zoomOut")) return CmdZoomOut;
    if (str::EqI(commandName, "fitWidth")) return CmdZoomFitWidth;
    if (str::EqI(commandName, "fitPage")) return CmdZoomFitPage;
    if (str::EqI(commandName, "actualSize")) return CmdZoomActualSize;
    if (str::EqI(commandName, "singlePage")) return CmdSinglePageView;
    if (str::EqI(commandName, "facing")) return CmdFacingView;
    if (str::EqI(commandName, "bookView")) return CmdBookView;
    if (str::EqI(commandName, "showPagesContinuously")) return CmdToggleContinuousView;

    if (str::EqI(commandName, "toggleBookmarks")) return CmdToggleBookmarks;
    if (str::EqI(commandName, "toggleFavorites")) return CmdFavoriteToggle;
    if (str::EqI(commandName, "toggleFullscreen")) return CmdToggleFullscreen;
    if (str::EqI(commandName, "rotateLeft")) return CmdRotateLeft;
    if (str::EqI(commandName, "rotateRight")) return CmdRotateRight;
    if (str::EqI(commandName, "print")) return CmdPrint;
    return 0;
}

static bool DispatchExecCommand(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }

    if (str::EqI(msg.command, "toggleTheme")) {
        return DispatchToggleThemeLightDark();
    }
    if (str::EqI(msg.command, "toggleThemeFollowWindows")) {
        return DispatchToggleThemeFollowWindows();
    }
    if (str::EqI(msg.command, "toggleDocumentInvert")) {
        return DispatchToggleDocumentInvert();
    }

    int cmdId = 0;
    if (msg.hasCmdId) {
        cmdId = msg.cmdId;
    } else {
        cmdId = ResolveBridgeCommandId(msg.command);
    }
    if (cmdId <= CmdFirst || cmdId >= CmdLast) {
        return false;
    }

    PostMessageW(win->hwndFrame, WM_COMMAND, cmdId, 0);
    return true;
}

static bool DispatchToolbarReady() {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    SyncHybridToolbarTheme(win->hwndFrame);
    if (win->ctrl) {
        SyncHybridToolbarPageState(win->hwndFrame, win->ctrl->CurrentPageNo(), win->ctrl->PageCount());
        SyncHybridToolbarZoomState(win->hwndFrame, win->ctrl->GetZoomVirtual(true));
    } else {
        SyncHybridToolbarPageState(win->hwndFrame, 1, 1);
        SyncHybridToolbarZoomState(win->hwndFrame, 100.0f);
    }
    return true;
}

// Helper to escape string for JSON
static void AppendJsonString(char*& dst, size_t& remaining, const char* src) {
    *dst++ = '"';
    remaining--;
    while (*src && remaining > 1) {
        if (*src == '"' || *src == '\\') {
            if (remaining < 2) break;
            *dst++ = '\\';
            *dst++ = *src++;
            remaining -= 2;
        } else if (*src == '\n') {
            if (remaining < 2) break;
            *dst++ = '\\';
            *dst++ = 'n';
            src++;
            remaining -= 2;
        } else if (*src == '\r') {
            if (remaining < 2) break;
            *dst++ = '\\';
            *dst++ = 'r';
            src++;
            remaining -= 2;
        } else if (*src == '\t') {
            if (remaining < 2) break;
            *dst++ = '\\';
            *dst++ = 't';
            src++;
            remaining -= 2;
        } else {
            *dst++ = *src++;
            remaining--;
        }
    }
    if (remaining > 0) {
        *dst++ = '"';
        remaining--;
    }
}

static constexpr int kHomePageMaxRecentItems = 40;

static u32 CalcRecentFilesSignature() {
    u32 sig = 0;
    for (int i = 0; i < kHomePageMaxRecentItems; i++) {
        FileState* fs = gFileHistory.Get(i);
        if (!fs || fs->isMissing) {
            break;
        }
        if (!fs->filePath) {
            continue;
        }
        sig ^= MurmurHash2(fs->filePath, str::Len(fs->filePath));
    }
    return sig;
}

// Helper function to serialize recent files to JSON for HomePage
static ::TempStr SerializeRecentFilesToJson() {
    // Allocate a large enough buffer for the JSON
    const size_t bufSize = 8192;
    char* buf = (char*)malloc(bufSize);
    if (!buf) return nullptr;
    
    char* dst = buf;
    size_t remaining = bufSize - 1;  // Leave room for null terminator
    
    *dst++ = '[';
    remaining--;
    
    bool first = true;
    for (int i = 0; i < kHomePageMaxRecentItems; i++) {
        FileState* fs = gFileHistory.Get(i);
        if (!fs || fs->isMissing) {
            break;
        }
        if (!fs->filePath) {
            continue;
        }
        
        if (!first && remaining > 0) {
            *dst++ = ',';
            remaining--;
        }
        first = false;
        
        // Extract filename from path
        const char* filePath = fs->filePath;
        const char* fileName = filePath;
        for (const char* p = filePath; *p; p++) {
            if (*p == '\\' || *p == '/') {
                fileName = p + 1;
            }
        }
        
        // Build the JSON entry
        const char* entryStart = "{\"path\":";
        if (remaining > str::Len(entryStart)) {
            memcpy(dst, entryStart, str::Len(entryStart));
            dst += str::Len(entryStart);
            remaining -= str::Len(entryStart);
        }
        
        AppendJsonString(dst, remaining, filePath);
        
        const char* separator = ",\"name\":";
        if (remaining > str::Len(separator)) {
            memcpy(dst, separator, str::Len(separator));
            dst += str::Len(separator);
            remaining -= str::Len(separator);
        }
        
        AppendJsonString(dst, remaining, fileName);
        
        if (remaining > 0) {
            *dst++ = '}';
            remaining--;
        }
    }
    
    if (remaining > 0) {
        *dst++ = ']';
        remaining--;
    }
    
    if (remaining > 0) {
        *dst++ = '\0';
    } else {
        buf[bufSize - 1] = '\0';
    }
    
    return buf;
}

// HomePage UI message handlers
static bool DispatchHomePageReady() {
    MainWindow* win = GetTargetWindow();
    if (!win || !win->homePageWebView) {
        return false;
    }

    static u32 sCachedRecentSignature = (u32)-1;
    static char* sCachedRecentFilesJson = nullptr;
    u32 recentSignature = CalcRecentFilesSignature();
    if (recentSignature != sCachedRecentSignature || !sCachedRecentFilesJson) {
        free(sCachedRecentFilesJson);
        sCachedRecentFilesJson = (char*)SerializeRecentFilesToJson();
        sCachedRecentSignature = recentSignature;
    }
    
    // Send recent files list to HomePage
    const char* recentFilesJson = sCachedRecentFilesJson ? sCachedRecentFilesJson : "[]";
    char* js = str::FormatTemp(
        "window.setRecentFiles && window.setRecentFiles(%s);",
        recentFilesJson);
    win->homePageWebView->Eval(js);
    
    // Apply current theme
    bool isDarkMode = HomePageUsesDarkTheme();
    char* themeJs = str::FormatTemp(
        "window.applyTheme && window.applyTheme(%s);",
        isDarkMode ? "true" : "false");
    win->homePageWebView->Eval(themeJs);
    
    return true;
}

static bool DispatchOpenRecent(const BridgeMessage& msg) {
    if (!CanAccessDisk()) {
        return false;
    }
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    if (str::IsEmpty(msg.path)) {
        return false;
    }
    LoadArgs args(msg.path, win);
    LoadDocument(&args);
    return true;
}

static bool DispatchApplyTheme(const BridgeMessage& msg) {
    if (LogBridgeMessages()) {
        logf("[PrettySumatraBridge] theme changed by user\n");
    }
    return true;
}

static bool DispatchReopenLast() {
    char* path = PopRecentlyClosedDocument();
    if (!path) {
        return false;
    }
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    LoadArgs args(path, win);
    LoadDocument(&args);
    return true;
}

static bool DispatchKnownCommand(const BridgeMessage& msg) {
    if (str::Eq(msg.name, kOpenFile)) {
        return DispatchOpenFile(msg);
    }
    if (str::Eq(msg.name, kGoToPage)) {
        return DispatchGoToPage(msg);
    }
    if (str::Eq(msg.name, kZoom)) {
        return DispatchZoom(msg);
    }
    if (str::Eq(msg.name, kSetFitMode)) {
        return DispatchSetFitMode(msg);
    }
    if (str::Eq(msg.name, kSearch)) {
        return DispatchSearch(msg);
    }
    if (str::Eq(msg.name, kToggleSidebar)) {
        return DispatchToggleSidebar();
    }
    if (str::Eq(msg.name, kSetViewMode)) {
        return DispatchSetViewMode(msg);
    }
    if (str::Eq(msg.name, kExecCommand)) {
        return DispatchExecCommand(msg);
    }
    if (str::Eq(msg.name, kToolbarReady)) {
        return DispatchToolbarReady();
    }
    if (str::EqI(msg.name, "print")) {
        return DispatchPrint();
    }
    if (str::Eq(msg.name, kHomePageReady)) {
        return DispatchHomePageReady();
    }
    if (str::Eq(msg.name, kOpenRecent)) {
        return DispatchOpenRecent(msg);
    }
    if (str::Eq(msg.name, kApplyTheme)) {
        return DispatchApplyTheme(msg);
    }
    if (str::Eq(msg.name, kReopenLast)) {
        return DispatchReopenLast();
    }

    if (str::Eq(msg.name, kAddAnnotation) || str::Eq(msg.name, kEditAnnotation) || str::Eq(msg.name, kDeleteAnnotation) ||
        str::Eq(msg.name, kExportAnnotations) || str::Eq(msg.name, kImportAnnotations)) {
        if (LogBridgeMessages()) {
            logf("[PrettySumatraBridge] command '%s' not implemented yet\n", msg.name);
        }
        return true;
    }
    return false;
}

void SyncHomePageTheme(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->homePageWebView) {
        return;
    }
    // Apply the actual app theme so the home page stays in sync with manual theme changes.
    bool isDarkMode = HomePageUsesDarkTheme();
    char* themeJs = str::FormatTemp(
        "window.applyTheme && window.applyTheme(%s);",
        isDarkMode ? "true" : "false");
    win->homePageWebView->Eval(themeJs);
}

DispatchResult DispatchShellMessage(const char* msg) {
    if (!UseHybridShell()) {
        return DispatchResult::Disabled;
    }
    if (str::IsEmptyOrWhiteSpace(msg)) {
        if (LogBridgeMessages()) {
            log("[PrettySumatraBridge] invalid bridge message: empty payload\n");
        }
        return DispatchResult::InvalidMessage;
    }

    BridgeMessage bridgeMsg;
    if (!ParseBridgeMessage(msg, bridgeMsg)) {
        if (LogBridgeMessages()) {
            logf("[PrettySumatraBridge] invalid command envelope: %s\n", msg);
        }
        return DispatchResult::InvalidMessage;
    }

    bool ok = DispatchKnownCommand(bridgeMsg);
    if (!ok) {
        if (LogBridgeMessages()) {
            logf("[PrettySumatraBridge] unknown command envelope: %s\n", msg);
        }
        return DispatchResult::UnknownCommand;
    }

    if (LogBridgeMessages()) {
        logf("[PrettySumatraBridge] accepted command '%s'\n", bridgeMsg.name);
    }
    return DispatchResult::Accepted;
}

} // namespace bridge
} // namespace prettysumatra
