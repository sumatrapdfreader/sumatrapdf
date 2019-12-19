/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
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
#include "wingui/FrameRateWnd.h"
#include "wingui/DropDownCtrl.h"
#include "wingui/TooltipCtrl.h"

#include "EngineBase.h"
#include "EngineManager.h"
#include "Doc.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "GlobalPrefs.h"
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
#include "resource.h"
#include "Caption.h"
#include "Selection.h"
#include "StressTesting.h"
#include "Translations.h"
#include "uia/Provider.h"

NotificationGroupId NG_CURSOR_POS_HELPER = "cursorPosHelper";
NotificationGroupId NG_RESPONSE_TO_ACTION = "responseToAction";

WindowInfo::WindowInfo(HWND hwnd) {
    hwndFrame = hwnd;
    touchState.panStarted = false;
    linkHandler = new LinkHandler(this);
    notifications = new Notifications();
    fwdSearchMark.show = false;
}

WindowInfo::~WindowInfo() {
    FinishStressTest(this);

    CrashIf(tabs.size() > 0);
    CrashIf(ctrl || linkOnLastButtonDown);

    // release our copy of UIA provider
    // the UI automation still might have a copy somewhere
    if (uia_provider) {
        if (AsFixed()) {
            uia_provider->OnDocumentUnload();
        }
        uia_provider->Release();
    }

    delete linkHandler;
    delete buffer;
    delete notifications;
    delete tabSelectionHistory;
    delete caption;
    DeleteVecMembers(tabs);
    // cbHandler is passed into Controller and must be deleted afterwards
    // (all controllers should have been deleted prior to WindowInfo, though)
    delete cbHandler;

    delete altBookmarks;
    delete frameRateWnd;
    delete infotip;
    delete tocTreeCtrl;
    free(sidebarSplitter);
    free(favSplitter);
    free(tocLabelWithClose);
    free(favLabelWithClose);
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
    RectI rc = ClientRect(hwndCanvas);
    if (buffer && canvasRc == rc)
        return;
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
    if (IsUIRightToLeft())
        notifications->Relayout();
}

SizeI WindowInfo::GetViewPortSize() {
    SizeI size = canvasRc.Size();

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

void WindowInfo::ChangePresentationMode(PresentationMode mode) {
    presentation = mode;
    if (PM_BLACK_SCREEN == mode || PM_WHITE_SCREEN == mode) {
        HideInfoTip();
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
    CrashIf(!this->ctrl);
    if (!this->IsDocLoaded())
        return;

    if (ZOOM_FIT_PAGE == this->ctrl->GetZoomVirtual())
        this->ctrl->SetZoomVirtual(ZOOM_FIT_WIDTH, nullptr);
    else if (ZOOM_FIT_WIDTH == this->ctrl->GetZoomVirtual())
        this->ctrl->SetZoomVirtual(ZOOM_FIT_CONTENT, nullptr);
    else
        this->ctrl->SetZoomVirtual(ZOOM_FIT_PAGE, nullptr);
}

void WindowInfo::MoveDocBy(int dx, int dy) {
    CrashIf(!this->AsFixed());
    if (!this->AsFixed())
        return;
    CrashIf(this->linkOnLastButtonDown);
    if (this->linkOnLastButtonDown)
        return;
    DisplayModel* dm = this->ctrl->AsFixed();
    if (0 != dx)
        dm->ScrollXBy(dx);
    if (0 != dy)
        dm->ScrollYBy(dy, false);
}

void WindowInfo::ShowInfoTip(const WCHAR* text, RectI& rc, bool multiline) {
    if (str::IsEmpty(text)) {
        this->HideInfoTip();
        return;
    }
    infotip->Show(text, rc, multiline);
}

void WindowInfo::HideInfoTip() {
    infotip->Hide();
}

void WindowInfo::ShowNotification(const WCHAR* msg, int options, NotificationGroupId groupId) {
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
}

bool WindowInfo::CreateUIAProvider() {
    if (!uia_provider) {
        uia_provider = new SumatraUIAutomationProvider(this->hwndCanvas);
        if (!uia_provider)
            return false;
        // load data to provider
        if (AsFixed())
            uia_provider->OnDocumentLoad(AsFixed());
    }

    return true;
}

// TODO: we use RemoteDestination only in LinkHandler::LaunchFile to make a copy
// of PageDestination, so we'll be able to replace it with just PageDestination
class RemoteDestination : public PageDestination {
  public:
    RemoteDestination(PageDestination* dest) {
        destKind = dest->destKind;
        destPageNo = dest->GetDestPageNo();
        destRect = dest->GetDestRect();
        destValue = dest->GetDestValue();
        destName = dest->GetDestName();
    }
};

void LinkHandler::GotoLink(PageDestination* link) {
    CrashIf(!owner || owner->linkHandler != this);
    if (!link || !owner->IsDocLoaded())
        return;

    TabInfo* tab = owner->currentTab;
    AutoFreeWstr path(link->GetDestValue());
    Kind kind = link->GetDestKind();
    if (kindDestinationScrollTo == kind) {
        // TODO: respect link->ld.gotor.new_window for PDF documents ?
        ScrollTo(link);
    } else if (kindDestinationLaunchURL == kind) {
        if (!path)
            /* ignore missing URLs */;
        else {
            WCHAR* colon = str::FindChar(path, ':');
            WCHAR* hash = str::FindChar(path, '#');
            if (!colon || (hash && colon > hash)) {
                // treat relative URIs as file paths (without fragment identifier)
                if (hash)
                    *hash = '\0';
                str::TransChars(path.Get(), L"/", L"\\");
                url::DecodeInPlace(path.Get());
                // LaunchFile will reject unsupported file types
                LaunchFile(path, nullptr);
            } else {
                // LaunchBrowser will reject unsupported URI schemes
                // TODO: support file URIs?
                LaunchBrowser(path);
            }
        }
    } else if (kindDestinationLaunchEmbedded == kind) {
        // open embedded PDF documents in a new window
        if (path && str::StartsWith(path.Get(), tab->filePath.Get())) {
            WindowInfo* newWin = FindWindowInfoByFile(path, true);
            if (!newWin) {
                LoadArgs args(path, owner);
                newWin = LoadDocument(args);
            }
            if (newWin)
                newWin->Focus();
        }
        // offer to save other attachments to a file
        else {
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/1336
        }
    } else if (kindDestinationLaunchFile == kind) {
        if (path) {
            // LaunchFile only opens files inside SumatraPDF
            // (except for allowed perceived file types)
            LaunchFile(path, link);
        }
    }
    // predefined named actions
    else if (kindDestinationNextPage == kind)
        tab->ctrl->GoToNextPage();
    else if (kindDestinationPrevPage == kind)
        tab->ctrl->GoToPrevPage();
    else if (kindDestinationFirstPage == kind)
        tab->ctrl->GoToFirstPage();
    else if (kindDestinationLastPage == kind)
        tab->ctrl->GoToLastPage();
    // Adobe Reader extensions to the spec, cf. http://www.tug.org/applications/hyperref/manual.html
    else if (kindDestinationFindDialog == kind)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_FIND_FIRST, 0);
    else if (kindDestinationFullScreen == kind)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_VIEW_PRESENTATION_MODE, 0);
    else if (kindDestinationGoBack == kind)
        tab->ctrl->Navigate(-1);
    else if (kindDestinationGoForward == kind)
        tab->ctrl->Navigate(1);
    else if (kindDestinationGoToPageDialog == kind)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_GOTO_PAGE, 0);
    else if (kindDestinationPrintDialog == kind)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_PRINT, 0);
    else if (kindDestinationSaveAsDialog == kind)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_SAVEAS, 0);
    else if (kindDestinationZoomToDialog == kind)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_ZOOM_CUSTOM, 0);
    else
        CrashIf(nullptr != kind);
}

void LinkHandler::ScrollTo(PageDestination* dest) {
    CrashIf(!owner || owner->linkHandler != this);
    if (!dest || !owner->IsDocLoaded())
        return;

    int pageNo = dest->GetDestPageNo();
    if (pageNo > 0)
        owner->ctrl->ScrollToLink(dest);
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
    RemoteDestination* remoteLink = nullptr;
    if (link) {
        remoteLink = new RemoteDestination(link);
        link = nullptr;
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
            AutoFreeWstr msg(str::Format(_TR("Error loading %s"), fullPath.get()));
            owner->ShowNotification(msg, NOS_HIGHLIGHT);
        }
        return;
    }

    newWin->Focus();
    if (!remoteLink)
        return;

    AutoFreeWstr destName(remoteLink->GetDestName());
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
    if (!partially)
        return str::Eq(s1, s2);

    // only match at the start of a word (at the beginning and after a space)
    for (const WCHAR* last = s1; (last = str::Find(last, s2)) != nullptr; last++) {
        if (last == s1 || *(last - 1) == ' ')
            return true;
    }
    return false;
}

// finds the first ToC entry that (partially) matches a given normalized name
// (ignoring case and whitespace differences)
PageDestination* LinkHandler::FindTocItem(DocTocItem* item, const WCHAR* name, bool partially) {
    for (; item; item = item->next) {
        AutoFreeWstr fuzTitle(NormalizeFuzzy(item->title));
        if (MatchFuzzy(fuzTitle, name, partially))
            return item->GetPageDestination();
        PageDestination* dest = FindTocItem(item->child, name, partially);
        if (dest)
            return dest;
    }
    return nullptr;
}

void LinkHandler::GotoNamedDest(const WCHAR* name) {
    CrashIf(!owner || owner->linkHandler != this);
    Controller* ctrl = owner->ctrl;
    if (!ctrl)
        return;

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
    } else if (ctrl->HasTocTree()) {
        auto* docTree = ctrl->GetTocTree();
        DocTocItem* root = docTree->root;
        AutoFreeWstr fuzName(NormalizeFuzzy(name));
        dest = FindTocItem(root, fuzName);
        if (!dest)
            dest = FindTocItem(root, fuzName, true);
        if (dest) {
            ScrollTo(dest);
            hasDest = true;
        }
    }
    if (!hasDest && ctrl->HasPageLabels()) {
        int pageNo = ctrl->GetPageByLabel(name);
        if (ctrl->ValidPageNo(pageNo))
            ctrl->GoToPage(pageNo, true);
    }
}
