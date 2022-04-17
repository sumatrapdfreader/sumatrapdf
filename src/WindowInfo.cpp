/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeCtrl.h"
#include "wingui/TooltipCtrl.h"
#include "wingui/TabsCtrl.h"
#include "wingui/LabelWithCloseWnd.h"
#include "wingui/FrameRateWnd.h"

#include "wingui/wingui2.h"
using namespace wg;

#include "Annotation.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "AppColors.h"
#include "ChmModel.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "TableOfContents.h"
#include "resource.h"
#include "Commands.h"
#include "Caption.h"
#include "Selection.h"
#include "Flags.h"
#include "StressTesting.h"
#include "Translations.h"
#include "uia/Provider.h"

#include "utils/Log.h"

struct LinkHandler : ILinkHandler {
    WindowInfo* win = nullptr;

    explicit LinkHandler(WindowInfo* w) {
        CrashIf(!w);
        win = w;
    }
    ~LinkHandler() override;

    Controller* GetController() override {
        return win->ctrl;
    }
    void GotoLink(IPageDestination*) override;
    void GotoNamedDest(const WCHAR*) override;
    void ScrollTo(IPageDestination*) override;
    void LaunchURL(const char*) override;
    void LaunchFile(const WCHAR* path, IPageDestination*) override;
    IPageDestination* FindTocItem(TocItem* item, const WCHAR* name, bool partially) override;
};

LinkHandler::~LinkHandler() {
    // do nothing
}

Vec<WindowInfo*> gWindows;

StaticLinkInfo::StaticLinkInfo(Rect rect, const WCHAR* target, const WCHAR* infotip) {
    this->rect = rect;
    this->target = str::Dup(target);
    this->infotip = str::Dup(infotip);
}

StaticLinkInfo::StaticLinkInfo(const StaticLinkInfo& other) {
    rect = other.rect;
    str::ReplaceWithCopy(&target, other.target);
    str::ReplaceWithCopy(&infotip, other.infotip);
}

StaticLinkInfo& StaticLinkInfo::operator=(const StaticLinkInfo& other) {
    if (this == &other) {
        return *this;
    }
    rect = other.rect;
    str::ReplaceWithCopy(&target, other.target);
    str::ReplaceWithCopy(&infotip, other.infotip);
    return *this;
}

StaticLinkInfo::~StaticLinkInfo() {
    str::Free(target);
    str::Free(infotip);
}

WindowInfo::WindowInfo(HWND hwnd) {
    hwndFrame = hwnd;
    linkHandler = new LinkHandler(this);
    notifications = new Notifications();
}

static WORD dotPatternBmp[8] = {0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055};

void CreateMovePatternLazy(WindowInfo* win) {
    if (win->bmpMovePattern) {
        return;
    }
    win->bmpMovePattern = CreateBitmap(8, 8, 1, 1, dotPatternBmp);
    CrashIf(!win->bmpMovePattern);
    win->brMovePattern = CreatePatternBrush(win->bmpMovePattern);
    CrashIf(!win->brMovePattern);
}

WindowInfo::~WindowInfo() {
    FinishStressTest(this);

    CrashIf(tabs.size() > 0);
    // CrashIf(ctrl); // TODO: seen in crash report
    CrashIf(linkOnLastButtonDown);
    CrashIf(annotationOnLastButtonDown);

    UnsubclassToc(this);

    DeleteObject(brMovePattern);
    DeleteObject(bmpMovePattern);

    // release our copy of UIA provider
    // the UI automation still might have a copy somewhere
    if (uiaProvider) {
        if (AsFixed()) {
            uiaProvider->OnDocumentUnload();
        }
        uiaProvider->Release();
    }

    delete linkHandler;
    delete buffer;
    delete notifications;
    delete tabSelectionHistory;
    DeleteCaption(caption);
    DeleteVecMembers(tabs);
    DeleteVecMembers(staticLinks);
    delete tabsCtrl;
    // cbHandler is passed into Controller and must be deleted afterwards
    // (all controllers should have been deleted prior to WindowInfo, though)
    delete cbHandler;

    delete frameRateWnd;
    delete infotip;
    delete tocTreeCtrl;
    if (favTreeCtrl) {
        delete favTreeCtrl->treeModel;
        delete favTreeCtrl;
    }

    delete sidebarSplitter;
    delete favSplitter;
    delete tocLabelWithClose;
    delete favLabelWithClose;
}

void ClearMouseState(WindowInfo* win) {
    win->linkOnLastButtonDown = nullptr;
    delete win->annotationOnLastButtonDown;
    win->annotationOnLastButtonDown = nullptr;
}

bool WindowInfo::IsAboutWindow() const {
    return nullptr == currentTab;
}

bool WindowInfo::IsDocLoaded() const {
    CrashIf(!this->ctrl != !(currentTab && currentTab->ctrl));
    return this->ctrl != nullptr;
}

DisplayModel* WindowInfo::AsFixed() const {
    return ctrl ? ctrl->AsFixed() : nullptr;
}

ChmModel* WindowInfo::AsChm() const {
    return ctrl ? ctrl->AsChm() : nullptr;
}

// Notify both display model and double-buffer (if they exist)
// about a potential change of available canvas size
void WindowInfo::UpdateCanvasSize() {
    Rect rc = ClientRect(hwndCanvas);
    if (buffer && canvasRc == rc) {
        return;
    }
    canvasRc = rc;

    // create a new output buffer and notify the model
    // about the change of the canvas size
    delete buffer;
    buffer = new DoubleBuffer(hwndCanvas, canvasRc);

    if (IsDocLoaded()) {
        // the display model needs to know the full size (including scroll bars)
        ctrl->SetViewPortSize(GetViewPortSize());
    }
    if (currentTab) {
        currentTab->canvasRc = canvasRc;
    }

    // keep the notifications visible (only needed for right-to-left layouts)
    if (IsUIRightToLeft()) {
        notifications->Relayout();
    }
}

Size WindowInfo::GetViewPortSize() const {
    Size size = canvasRc.Size();

    DWORD style = GetWindowLong(hwndCanvas, GWL_STYLE);
    if ((style & WS_VSCROLL)) {
        size.dx += GetSystemMetrics(SM_CXVSCROLL);
    }
    if ((style & WS_HSCROLL)) {
        size.dy += GetSystemMetrics(SM_CYHSCROLL);
    }
    CrashIf((style & (WS_VSCROLL | WS_HSCROLL)) && !AsFixed());

    return size;
}

void WindowInfo::RedrawAll(bool update) const {
    InvalidateRect(this->hwndCanvas, nullptr, false);
    if (update) {
        UpdateWindow(this->hwndCanvas);
    }
}

void WindowInfo::RedrawAllIncludingNonClient(bool update) const {
    InvalidateRect(this->hwndCanvas, nullptr, false);
    if (update) {
        RedrawWindow(this->hwndCanvas, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
    }
}

void WindowInfo::ChangePresentationMode(PresentationMode mode) {
    presentation = mode;
    if (PM_BLACK_SCREEN == mode || PM_WHITE_SCREEN == mode) {
        HideToolTip();
    }
    RedrawAll();
}

static HWND FindModalOwnedBy(HWND hwndParent) {
    HWND hwnd = nullptr;
    while (true) {
        hwnd = FindWindowExW(HWND_DESKTOP, hwnd, nullptr, nullptr);
        if (hwnd == nullptr) {
            break;
        }
        bool isDlg = (GetWindowStyle(hwnd) & WS_DLGFRAME) != 0;
        if (!isDlg) {
            continue;
        }
        if (GetWindow(hwnd, GW_OWNER) != hwndParent) {
            continue;
        }
        return hwnd;
    }
    return nullptr;
}

void WindowInfo::Focus() const {
    win::ToForeground(hwndFrame);
    // set focus to an owned modal dialog if there is one
    HWND hwnd = FindModalOwnedBy(hwndFrame);
    if (hwnd != nullptr) {
        SetFocus(hwnd);
        return;
    }
    SetFocus(hwndFrame);
}

void WindowInfo::ToggleZoom() const {
    if (currentTab) {
        currentTab->ToggleZoom();
    }
}

void WindowInfo::MoveDocBy(int dx, int dy) const {
    CrashIf(!currentTab);
    currentTab->MoveDocBy(dx, dy);
}

void WindowInfo::ShowToolTip(const WCHAR* text, Rect& rc, bool multiline) const {
    if (str::IsEmpty(text)) {
        HideToolTip();
        return;
    }
    infotip->ShowOrUpdate(text, rc, multiline);
}

void WindowInfo::HideToolTip() const {
    infotip->Hide();
}

bool WindowInfo::CreateUIAProvider() {
    if (uiaProvider) {
        return true;
    }
    uiaProvider = new SumatraUIAutomationProvider(this->hwndCanvas);
    if (!uiaProvider) {
        return false;
    }
    // load data to provider
    if (AsFixed()) {
        uiaProvider->OnDocumentLoad(AsFixed());
    }
    return true;
}

void LinkHandler::GotoLink(IPageDestination* dest) {
    CrashIf(!win || win->linkHandler != this);
    if (!dest || !win || !win->IsDocLoaded()) {
        return;
    }

    HWND hwndFrame = win->hwndFrame;
    Kind kind = dest->GetKind();

    if (kindDestinationScrollTo == kind) {
        // TODO: respect link->ld.gotor.new_window for PDF documents ?
        ScrollTo(dest);
        return;
    }
    if (kindDestinationLaunchURL == kind) {
        auto d = (PageDestinationURL*)dest;
        char* urlA = ToUtf8Temp(d->url);
        LaunchURL(urlA);
        return;
    }
    if (kindDestinationLaunchFile == kind) {
        PageDestinationFile* pdf = (PageDestinationFile*)dest;
        // LaunchFile only opens files inside SumatraPDF
        // (except for allowed perceived file types)
        WCHAR* tmpPath = CleanupFileURL(pdf->path);
        // heuristic: replace %20 with ' '
        if (!file::Exists(tmpPath) && (str::Find(tmpPath, L"%20") != nullptr)) {
            WCHAR* tmp = str::Replace(tmpPath, L"%20", L" ");
            str::Free(tmpPath);
            tmpPath = tmp;
        }
        LaunchFile(tmpPath, dest);
        str::Free(tmpPath);
        return;
    }
    if (kindDestinationLaunchEmbedded == kind) {
        // Not handled here. Must use context menu to trigger launching
        // embedded files
        return;
    }
    if (kindDestinationLaunchURL == kind) {
        return;
    }

    logf("LinkHandler::GotoLink: unhandled kind %s\n", kind);
    ReportIf(true);
}

void LinkHandler::ScrollTo(IPageDestination* dest) {
    ReportIf(!win || !win->ctrl || win->linkHandler != this);
    if (!dest || !win || !win->ctrl || !win->IsDocLoaded()) {
        return;
    }
    int pageNo = dest->GetPageNo();
    if (!win->ctrl->ValidPageNo(pageNo)) {
        return;
    }
    RectF rect = dest->GetRect();
    float zoom = dest->GetZoom();
    win->ctrl->ScrollTo(pageNo, rect, zoom);
}

void LinkHandler::LaunchURL(const char* uri) {
    if (!uri) {
        /* ignore missing URLs */;
        return;
    }

    WCHAR* path = ToWstrTemp(uri);
    WCHAR* colon = str::FindChar(path, ':');
    WCHAR* hash = str::FindChar(path, '#');
    if (!colon || (hash && colon > hash)) {
        // treat relative URIs as file paths (without fragment identifier)
        if (hash) {
            *hash = '\0';
        }
        str::TransCharsInPlace(path, L"/", L"\\");
        url::DecodeInPlace(path);
        // LaunchFile will reject unsupported file types
        LaunchFile(path, nullptr);
    } else {
        // LaunchBrowser will reject unsupported URI schemes
        // TODO: support file URIs?
        SumatraLaunchBrowser(path);
    }
}

void LinkHandler::LaunchFile(const WCHAR* pathOrig, IPageDestination* link) {
    // for safety, only handle relative paths and only open them in SumatraPDF
    // (unless they're of an allowed perceived type) and never launch any external
    // file in plugin mode (where documents are supposed to be self-contained)
    // TDOO: maybe should enable this in plugin mode
    if (gPluginMode) {
        return;
    }

    // TODO: make it a function
    AutoFreeWstr path = str::Replace(pathOrig, L"/", L"\\");
    if (str::StartsWith(path, L".\\")) {
        path.Set(str::Dup(path + 2));
    }

    WCHAR drive;
    bool isAbsPath = str::StartsWith(path, L"\\") || str::Parse(path, L"%c:\\", &drive);
    if (isAbsPath) {
        return;
    }

    IPageDestination* remoteLink = link;
    AutoFreeWstr fullPath(path::GetDir(win->ctrl->GetFilePath()));
    fullPath.Set(path::Join(fullPath, path));

    // TODO: respect link->ld.gotor.new_window for PDF documents ?
    WindowInfo* newWin = FindWindowInfoByFile(fullPath, true);
    // TODO: don't show window until it's certain that there was no error
    if (!newWin) {
        LoadArgs args(fullPath, win);
        newWin = LoadDocument(args);
        if (!newWin) {
            return;
        }
    }

    if (!newWin->IsDocLoaded()) {
        CloseCurrentTab(newWin);
        // OpenFileExternally rejects files we'd otherwise
        // have to show a notification to be sure (which we
        // consider bad UI and thus simply don't)
        bool ok = OpenFileExternally(fullPath);
        if (!ok) {
            AutoFreeWstr msg(str::Format(_TR("Error loading %s"), fullPath.Get()));
            win->notifications->Show(win->hwndCanvas, msg, NotificationOptions::Highlight);
        }
        return;
    }

    newWin->Focus();
    if (!remoteLink) {
        return;
    }

    WCHAR* destName = remoteLink->GetName();
    if (destName) {
        IPageDestination* dest = newWin->ctrl->GetNamedDest(destName);
        if (dest) {
            newWin->linkHandler->ScrollTo(dest);
            delete dest;
        }
    } else {
        newWin->linkHandler->ScrollTo(remoteLink);
    }
}

// normalizes case and whitespace in the string
// caller needs to free() the result
static WCHAR* NormalizeFuzzy(const WCHAR* str) {
    WCHAR* dup = str::Dup(str);
    CharLowerW(dup);
    str::NormalizeWSInPlace(dup);
    // cf. AddTocItemToView
    return dup;
}

static bool MatchFuzzy(const WCHAR* s1, const WCHAR* s2, bool partially) {
    if (!partially) {
        return str::Eq(s1, s2);
    }

    // only match at the start of a word (at the beginning and after a space)
    for (const WCHAR* last = s1; (last = str::Find(last, s2)) != nullptr; last++) {
        if (last == s1 || *(last - 1) == ' ') {
            return true;
        }
    }
    return false;
}

// finds the first ToC entry that (partially) matches a given normalized name
// (ignoring case and whitespace differences)
IPageDestination* LinkHandler::FindTocItem(TocItem* item, const WCHAR* name, bool partially) {
    for (; item; item = item->next) {
        if (item->title) {
            AutoFreeWstr fuzTitle(NormalizeFuzzy(item->title));
            if (MatchFuzzy(fuzTitle, name, partially)) {
                return item->GetPageDestination();
            }
        }
        IPageDestination* dest = FindTocItem(item->child, name, partially);
        if (dest) {
            return dest;
        }
    }
    return nullptr;
}

void LinkHandler::GotoNamedDest(const WCHAR* name) {
    CrashIf(!win || win->linkHandler != this);
    Controller* ctrl = win->ctrl;
    if (!ctrl) {
        return;
    }

    // Match order:
    // 1. Exact match on internal destination name
    // 2. Fuzzy match on full ToC item title
    // 3. Fuzzy match on a part of a ToC item title
    // 4. Exact match on page label
    IPageDestination* dest = ctrl->GetNamedDest(name);
    bool hasDest = dest != nullptr;
    if (dest) {
        ScrollTo(dest);
        delete dest;
    } else if (ctrl->HacToc()) {
        auto* docTree = ctrl->GetToc();
        TocItem* root = docTree->root;
        AutoFreeWstr fuzName(NormalizeFuzzy(name));
        dest = FindTocItem(root, fuzName, false);
        if (!dest) {
            dest = FindTocItem(root, fuzName, true);
        }
        if (dest) {
            ScrollTo(dest);
            hasDest = true;
        }
    }
    if (!hasDest && ctrl->HasPageLabels()) {
        int pageNo = ctrl->GetPageByLabel(name);
        if (ctrl->ValidPageNo(pageNo)) {
            ctrl->GoToPage(pageNo, true);
        }
    }
}

void UpdateTreeCtrlColors(WindowInfo* win) {
    COLORREF labelBgCol = GetSysColor(COLOR_BTNFACE);
    COLORREF labelTxtCol = GetSysColor(COLOR_BTNTEXT);
    COLORREF treeBgCol = GetAppColor(AppColor::DocumentBg);
    COLORREF treeTxtCol = GetAppColor(AppColor::DocumentText);
    COLORREF splitterCol = GetSysColor(COLOR_BTNFACE);
    bool flatTreeWnd = false;

    {
        auto tocTreeCtrl = win->tocTreeCtrl;
        tocTreeCtrl->SetBackgroundColor(treeBgCol);
        tocTreeCtrl->SetTextColor(treeTxtCol);

        win->tocLabelWithClose->SetBgCol(labelBgCol);
        win->tocLabelWithClose->SetTextCol(labelTxtCol);
        win->sidebarSplitter->SetBackgroundColor(splitterCol);
        SetWindowExStyle(tocTreeCtrl->hwnd, WS_EX_STATICEDGE, !flatTreeWnd);
        uint flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED;
        SetWindowPos(tocTreeCtrl->hwnd, nullptr, 0, 0, 0, 0, flags);
    }

    auto favTreeCtrl = win->favTreeCtrl;
    if (favTreeCtrl) {
        favTreeCtrl->SetBackgroundColor(treeBgCol);
        favTreeCtrl->SetTextColor(treeTxtCol);

        win->favLabelWithClose->SetBgCol(labelBgCol);
        win->favLabelWithClose->SetTextCol(labelTxtCol);

        win->favSplitter->SetBackgroundColor(splitterCol);

        SetWindowExStyle(favTreeCtrl->hwnd, WS_EX_STATICEDGE, !flatTreeWnd);
        uint flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED;
        SetWindowPos(favTreeCtrl->hwnd, nullptr, 0, 0, 0, 0, flags);
    }

    // TODO: more work needed to to ensure consistent look of the ebook window:
    // - tab bar should match the colort
    // - change the tree item text color
    // - change the tree item background color when selected (for both focused and non-focused cases)
    // - ultimately implement owner-drawn scrollbars in a simpler style (like Chrome or VS 2013)
    //   and match their colors as well
}

void ClearFindBox(WindowInfo* win) {
    HWND hwndFocused = GetFocus();
    if (hwndFocused == win->hwndFindBox) {
        SetFocus(win->hwndFrame);
    }
    HwndSetText(win->hwndFindBox, "");
}

bool IsRightDragging(WindowInfo* win) {
    if (win->mouseAction != MouseAction::Dragging) {
        return false;
    }
    return win->dragRightClick;
}

bool WindowInfoStillValid(WindowInfo* win) {
    return gWindows.Contains(win);
}

WindowInfo* FindWindowInfoByHwnd(HWND hwnd) {
    for (WindowInfo* win : gWindows) {
        if ((win->hwndFrame == hwnd) || ::IsChild(win->hwndFrame, hwnd)) {
            return win;
        }
    }
    return nullptr;
}

// Find WindowInfo using TabInfo. Diffrent than TabInfo->win in that
// it validates that TabInfo is still valid
WindowInfo* FindWindowInfoByTabInfo(TabInfo* tabToFind) {
    for (WindowInfo* win : gWindows) {
        for (TabInfo* tab : win->tabs) {
            if (tab == tabToFind) {
                return win;
            }
        }
    }
    return nullptr;
}

WindowInfo* FindWindowInfoByController(Controller* ctrl) {
    for (auto& win : gWindows) {
        for (auto& tab : win->tabs) {
            if (tab->ctrl == ctrl) {
                return win;
            }
        }
    }
    return nullptr;
}
