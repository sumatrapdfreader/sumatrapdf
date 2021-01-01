/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"
#include "wingui/DropDownCtrl.h"
#include "wingui/TooltipCtrl.h"
#include "wingui/TabsCtrl.h"
#include "wingui/LabelWithCloseWnd.h"
#include "wingui/SplitterWnd.h"
#include "wingui/FrameRateWnd.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineCreate.h"
#include "Doc.h"
#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "GlobalPrefs.h"
#include "AppColors.h"
#include "ChmModel.h"
#include "DisplayModel.h"
#include "EbookController.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "TableOfContents.h"
#include "resource.h"
#include "Commands.h"
#include "Caption.h"
#include "Selection.h"
#include "StressTesting.h"
#include "Translations.h"
#include "uia/Provider.h"

NotificationGroupId NG_CURSOR_POS_HELPER = "cursorPosHelper";
NotificationGroupId NG_RESPONSE_TO_ACTION = "responseToAction";

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
    CrashIf(ctrl);
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
    delete tabsCtrl;
    // cbHandler is passed into Controller and must be deleted afterwards
    // (all controllers should have been deleted prior to WindowInfo, though)
    delete cbHandler;

    delete frameRateWnd;
    delete infotip;
    delete altBookmarks;
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
    delete win->linkOnLastButtonDown;
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
EbookController* WindowInfo::AsEbook() const {
    return ctrl ? ctrl->AsEbook() : nullptr;
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

Size WindowInfo::GetViewPortSize() {
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

void WindowInfo::RedrawAll(bool update) {
    InvalidateRect(this->hwndCanvas, nullptr, false);
    if (this->AsEbook()) {
        this->AsEbook()->RequestRepaint();
    }
    if (update) {
        UpdateWindow(this->hwndCanvas);
    }
}

void WindowInfo::RedrawAllIncludingNonClient(bool update) {
    InvalidateRect(this->hwndCanvas, nullptr, false);
    if (this->AsEbook()) {
        this->AsEbook()->RequestRepaint();
    }
    if (update) {
        RedrawWindow(this->hwndCanvas, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
    }
}

void WindowInfo::ChangePresentationMode(PresentationMode mode) {
    presentation = mode;
    if (PM_BLACK_SCREEN == mode || PM_WHITE_SCREEN == mode) {
        HideToolTip();
    }
    RedrawAll();
}

void WindowInfo::Focus() {
    win::ToForeground(hwndFrame);
    // set focus to an owned modal dialog if there is one
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, nullptr, nullptr)) != nullptr) {
        if (GetWindow(hwnd, GW_OWNER) == hwndFrame && (GetWindowStyle(hwnd) & WS_DLGFRAME)) {
            SetFocus(hwnd);
            return;
        }
    }
    SetFocus(hwndFrame);
}

void WindowInfo::ToggleZoom() {
    if (currentTab) {
        currentTab->ToggleZoom();
    }
}

void WindowInfo::MoveDocBy(int dx, int dy) {
    CrashIf(!currentTab);
    currentTab->MoveDocBy(dx, dy);
}

void WindowInfo::ShowToolTip(const WCHAR* text, Rect& rc, bool multiline) {
    if (str::IsEmpty(text)) {
        HideToolTip();
        return;
    }
    infotip->Show(text, rc, multiline);
}

void WindowInfo::HideToolTip() {
    infotip->Hide();
}

NotificationWnd* WindowInfo::ShowNotification(const WCHAR* msg, int options, NotificationGroupId groupId) {
    int timeoutMS = (options & NOS_PERSIST) ? 0 : 3000;
    bool highlight = (options & NOS_HIGHLIGHT);

    NotificationWnd* wnd = new NotificationWnd(hwndCanvas, timeoutMS);
    wnd->highlight = highlight;
    wnd->wndRemovedCb = [this](NotificationWnd* wnd) { this->notifications->RemoveNotification(wnd); };
    if (NG_CURSOR_POS_HELPER == groupId) {
        wnd->shrinkLimit = 0.7f;
    }
    wnd->Create(msg, nullptr);
    notifications->Add(wnd, groupId);
    return wnd;
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

void LinkHandler::GotoLink(PageDestination* dest) {
    CrashIf(!owner || owner->linkHandler != this);
    if (!dest || !owner || !owner->IsDocLoaded()) {
        return;
    }

    HWND hwndFrame = owner->hwndFrame;
    TabInfo* tab = owner->currentTab;
    WCHAR* path = dest->GetValue();
    Kind kind = dest->Kind();
    if (kindDestinationNone == kind) {
        return;
    }

    if (kindDestinationScrollTo == kind) {
        // TODO: respect link->ld.gotor.new_window for PDF documents ?
        ScrollTo(dest);
        return;
    }

    if (kindDestinationLaunchURL == kind) {
        if (!path) {
            /* ignore missing URLs */;
            return;
        }

        WCHAR* colon = str::FindChar(path, ':');
        WCHAR* hash = str::FindChar(path, '#');
        if (!colon || (hash && colon > hash)) {
            // treat relative URIs as file paths (without fragment identifier)
            if (hash) {
                *hash = '\0';
            }
            str::TransChars(path, L"/", L"\\");
            url::DecodeInPlace(path);
            // LaunchFile will reject unsupported file types
            LaunchFile(path, nullptr);
        } else {
            // LaunchBrowser will reject unsupported URI schemes
            // TODO: support file URIs?
            SumatraLaunchBrowser(path);
        }
        return;
    }

    if (kindDestinationLaunchEmbedded == kind) {
        // Not handled here. Must use context menu to trigger launching
        // embedded files
        return;
    }

    if (kindDestinationLaunchFile == kind) {
        if (!path) {
            return;
        }
        // LaunchFile only opens files inside SumatraPDF
        // (except for allowed perceived file types)
        const WCHAR* tmpPath = SkipFileProtocol(path);
        LaunchFile(tmpPath, dest);
        return;
    }

    if (kindDestinationNextPage == kind) {
        // predefined named actions
        tab->ctrl->GoToNextPage();
        return;
    }

    if (kindDestinationPrevPage == kind) {
        tab->ctrl->GoToPrevPage();
        return;
    }

    if (kindDestinationFirstPage == kind) {
        tab->ctrl->GoToFirstPage();
        return;
    }

    if (kindDestinationLastPage == kind) {
        tab->ctrl->GoToLastPage();
        // Adobe Reader extensions to the spec, see http://www.tug.org/applications/hyperref/manual.html
        return;
    }

    if (kindDestinationFindDialog == kind) {
        PostMessageW(hwndFrame, WM_COMMAND, CmdFindFirst, 0);
        return;
    }

    if (kindDestinationFullScreen == kind) {
        PostMessageW(hwndFrame, WM_COMMAND, CmdViewPresentationMode, 0);
        return;
    }

    if (kindDestinationGoBack == kind) {
        tab->ctrl->Navigate(-1);
        return;
    }

    if (kindDestinationGoForward == kind) {
        tab->ctrl->Navigate(1);
        return;
    }

    if (kindDestinationGoToPageDialog == kind) {
        PostMessageW(hwndFrame, WM_COMMAND, CmdGoToPage, 0);
        return;
    }

    if (kindDestinationPrintDialog == kind) {
        PostMessageW(hwndFrame, WM_COMMAND, CmdPrint, 0);
        return;
    }

    if (kindDestinationSaveAsDialog == kind) {
        PostMessageW(hwndFrame, WM_COMMAND, CmdSaveAs, 0);
        return;
    }

    if (kindDestinationZoomToDialog == kind) {
        PostMessageW(hwndFrame, WM_COMMAND, CmdZoomCustom, 0);
        return;
    }

    CrashIf(nullptr != kind);
}

void LinkHandler::ScrollTo(PageDestination* dest) {
    CrashIf(!owner || owner->linkHandler != this);
    if (!dest || !owner || !owner->IsDocLoaded()) {
        return;
    }

    int pageNo = dest->GetPageNo();
    if (pageNo > 0) {
        owner->ctrl->ScrollToLink(dest);
    }
}

void LinkHandler::LaunchFile(const WCHAR* path, PageDestination* link) {
    // for safety, only handle relative paths and only open them in SumatraPDF
    // (unless they're of an allowed perceived type) and never launch any external
    // file in plugin mode (where documents are supposed to be self-contained)
    WCHAR drive;
    if (str::StartsWith(path, L"\\") || str::Parse(path, L"%c:\\", &drive) || gPluginMode) {
        return;
    }

    // TODO: link is deleted when opening the document in a new tab
    PageDestination* remoteLink = nullptr;
    if (link) {
        remoteLink = clonePageDestination(link);
    }
    AutoDelete deleteRemoteLink(remoteLink);

    AutoFreeWstr fullPath(path::GetDir(owner->ctrl->FilePath()));
    fullPath.Set(path::Join(fullPath, path));
    fullPath.Set(path::Normalize(fullPath));
    // TODO: respect link->ld.gotor.new_window for PDF documents ?
    WindowInfo* newWin = FindWindowInfoByFile(fullPath, true);
    // TODO: don't show window until it's certain that there was no error
    if (!newWin) {
        LoadArgs args(fullPath, owner);
        newWin = LoadDocument(args);
        if (!newWin) {
            return;
        }
    }

    if (!newWin->IsDocLoaded()) {
        CloseTab(newWin);
        // OpenFileExternally rejects files we'd otherwise
        // have to show a notification to be sure (which we
        // consider bad UI and thus simply don't)
        bool ok = OpenFileExternally(fullPath);
        if (!ok) {
            AutoFreeWstr msg(str::Format(_TR("Error loading %s"), fullPath.Get()));
            owner->ShowNotification(msg, NOS_HIGHLIGHT);
        }
        return;
    }

    newWin->Focus();
    if (!remoteLink) {
        return;
    }

    WCHAR* destName = remoteLink->GetName();
    if (destName) {
        PageDestination* dest = newWin->ctrl->GetNamedDest(destName);
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
    CharLower(dup);
    str::NormalizeWS(dup);
    // cf. AddTocItemToView
    return dup;
}

static bool MatchFuzzy(const WCHAR* s1, const WCHAR* s2, bool partially = false) {
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
PageDestination* LinkHandler::FindTocItem(TocItem* item, const WCHAR* name, bool partially) {
    for (; item; item = item->next) {
        AutoFreeWstr fuzTitle(NormalizeFuzzy(item->title));
        if (MatchFuzzy(fuzTitle, name, partially)) {
            return item->GetPageDestination();
        }
        PageDestination* dest = FindTocItem(item->child, name, partially);
        if (dest) {
            return dest;
        }
    }
    return nullptr;
}

void LinkHandler::GotoNamedDest(const WCHAR* name) {
    CrashIf(!owner || owner->linkHandler != this);
    Controller* ctrl = owner->ctrl;
    if (!ctrl) {
        return;
    }

    // Match order:
    // 1. Exact match on internal destination name
    // 2. Fuzzy match on full ToC item title
    // 3. Fuzzy match on a part of a ToC item title
    // 4. Exact match on page label
    PageDestination* dest = ctrl->GetNamedDest(name);
    bool hasDest = dest != NULL;
    if (dest) {
        ScrollTo(dest);
        delete dest;
    } else if (ctrl->HacToc()) {
        auto* docTree = ctrl->GetToc();
        TocItem* root = docTree->root;
        AutoFreeWstr fuzName(NormalizeFuzzy(name));
        dest = FindTocItem(root, fuzName);
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

    if (win->AsEbook()) {
        labelBgCol = GetAppColor(AppColor::DocumentBg, true);
        labelTxtCol = GetAppColor(AppColor::DocumentText, true);
        treeTxtCol = labelTxtCol;
        treeBgCol = labelBgCol;
        float factor = 14.f;
        int sign = GetLightness(labelBgCol) + factor > 255 ? 1 : -1;
        splitterCol = AdjustLightness2(labelBgCol, sign * factor);
        flatTreeWnd = true;
    }

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
