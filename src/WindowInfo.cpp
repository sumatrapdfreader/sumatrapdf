/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#include <dwmapi.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include "FileUtil.h"
#include "FrameRateWnd.h"
#include "WinUtil.h"
// rendering engines
#include "BaseEngine.h"
#include "EngineManager.h"
#include "Doc.h"
// layout controllers
#include "SettingsStructs.h"
#include "Controller.h"
#include "ChmModel.h"
#include "DisplayModel.h"
#include "EbookController.h"
#include "GlobalPrefs.h"
#include "TextSelection.h"
#include "TextSearch.h"
// ui
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "Caption.h"
#include "Notifications.h"
#include "Selection.h"
#include "StressTesting.h"
#include "Translations.h"
#include "uia/Provider.h"

WindowInfo::WindowInfo(HWND hwnd) :
    ctrl(nullptr), currentTab(nullptr), menu(nullptr), hwndFrame(hwnd), isMenuHidden(false),
    linkOnLastButtonDown(nullptr), url(nullptr),
    tocVisible(false), tocLoaded(false), tocKeepSelection(false),
    isFullScreen(false), presentation(PM_DISABLED),
    windowStateBeforePresentation(0), nonFullScreenWindowStyle(0),
    hwndCanvas(nullptr), hwndToolbar(nullptr), hwndReBar(nullptr),
    hwndFindText(nullptr), hwndFindBox(nullptr), hwndFindBg(nullptr),
    hwndPageText(nullptr), hwndPageBox(nullptr), hwndPageBg(nullptr), hwndPageTotal(nullptr),
    hwndTocBox(nullptr), hwndTocTree(nullptr), tocLabelWithClose(nullptr),
    sidebarSplitter(nullptr), favSplitter(nullptr),
    hwndInfotip(nullptr), infotipVisible(false),
    findThread(nullptr), findCanceled(false), printThread(nullptr), printCanceled(false),
    showSelection(false), mouseAction(MA_IDLE), dragStartPending(false),
    currPageNo(0),
    xScrollSpeed(0), yScrollSpeed(0), wheelAccumDelta(0),
    delayedRepaintTimer(0), stressTest(nullptr),
    hwndFavBox(nullptr), hwndFavTree(nullptr), favLabelWithClose(nullptr),
    uia_provider(nullptr), cbHandler(nullptr), frameRateWnd(nullptr),
    hwndTabBar(nullptr), tabsVisible(false), tabsInTitlebar(false), tabSelectionHistory(nullptr),
    hwndCaption(nullptr), caption(nullptr), extendedFrameHeight(0)
{
    touchState.panStarted = false;
    buffer = new DoubleBuffer(hwndCanvas, canvasRc);
    linkHandler = new LinkHandler(*this);
    notifications = new Notifications();
    fwdSearchMark.show = false;
}

WindowInfo::~WindowInfo()
{
    FinishStressTest(this);

    CrashIf(tabs.Count() > 0);
    CrashIf(ctrl || linkOnLastButtonDown);

    // release our copy of UIA provider
    // the UI automation still might have a copy somewhere
    if (uia_provider) {
        if (AsFixed())
            uia_provider->OnDocumentUnload();
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

    DeleteFrameRateWnd(frameRateWnd);
    free(sidebarSplitter);
    free(favSplitter);
    free(tocLabelWithClose);
    free(favLabelWithClose);
}

bool WindowInfo::IsAboutWindow() const
{
    return nullptr == currentTab;
}

bool WindowInfo::IsDocLoaded() const
{
    CrashIf(!this->ctrl != !(currentTab && currentTab->ctrl));
    return this->ctrl != nullptr;
}

DisplayModel *WindowInfo::AsFixed() const { return ctrl ? ctrl->AsFixed() : nullptr; }
ChmModel *WindowInfo::AsChm() const { return ctrl ? ctrl->AsChm() : nullptr; }
EbookController *WindowInfo::AsEbook() const { return ctrl ? ctrl->AsEbook() : nullptr; }

// Notify both display model and double-buffer (if they exist)
// about a potential change of available canvas size
void WindowInfo::UpdateCanvasSize()
{
    RectI rc = ClientRect(hwndCanvas);
    if (canvasRc == rc)
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

SizeI WindowInfo::GetViewPortSize()
{
    SizeI size = canvasRc.Size();

    DWORD style = GetWindowLong(hwndCanvas, GWL_STYLE);
    if ((style & WS_VSCROLL))
        size.dx += GetSystemMetrics(SM_CXVSCROLL);
    if ((style & WS_HSCROLL))
        size.dy += GetSystemMetrics(SM_CYHSCROLL);
    CrashIf((style & (WS_VSCROLL | WS_HSCROLL)) && !AsFixed());

    return size;
}

void WindowInfo::RedrawAll(bool update)
{
    InvalidateRect(this->hwndCanvas, nullptr, false);
    if (this->AsEbook())
        this->AsEbook()->RequestRepaint();
    if (update)
        UpdateWindow(this->hwndCanvas);
}

void WindowInfo::ChangePresentationMode(PresentationMode mode)
{
    presentation = mode;
    if (PM_BLACK_SCREEN == mode || PM_WHITE_SCREEN == mode) {
        DeleteInfotip();
    }
    RedrawAll();
}

void WindowInfo::Focus()
{
    if (IsIconic(hwndFrame))
        ShowWindow(hwndFrame, SW_RESTORE);
    SetForegroundWindow(hwndFrame);
    SetFocus(hwndFrame);
}

void WindowInfo::ToggleZoom()
{
    CrashIf(!this->ctrl);
    if (!this->IsDocLoaded()) return;

    if (ZOOM_FIT_PAGE == this->ctrl->GetZoomVirtual())
        this->ctrl->SetZoomVirtual(ZOOM_FIT_WIDTH);
    else if (ZOOM_FIT_WIDTH == this->ctrl->GetZoomVirtual())
        this->ctrl->SetZoomVirtual(ZOOM_FIT_CONTENT);
    else
        this->ctrl->SetZoomVirtual(ZOOM_FIT_PAGE);
}

void WindowInfo::MoveDocBy(int dx, int dy)
{
    CrashIf(!this->AsFixed());
    if (!this->AsFixed()) return;
    CrashIf(this->linkOnLastButtonDown);
    if (this->linkOnLastButtonDown) return;
    DisplayModel *dm = this->ctrl->AsFixed();
    if (0 != dx)
        dm->ScrollXBy(dx);
    if (0 != dy)
        dm->ScrollYBy(dy, false);
}

#define MULTILINE_INFOTIP_WIDTH_PX 500

void WindowInfo::CreateInfotip(const WCHAR *text, RectI& rc, bool multiline)
{
    if (str::IsEmpty(text)) {
        this->DeleteInfotip();
        return;
    }

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = this->hwndCanvas;
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = (WCHAR *)text;
    ti.rect = rc.ToRECT();

    if (multiline || str::FindChar(text, '\n'))
        SendMessage(this->hwndInfotip, TTM_SETMAXTIPWIDTH, 0, MULTILINE_INFOTIP_WIDTH_PX);
    else
        SendMessage(this->hwndInfotip, TTM_SETMAXTIPWIDTH, 0, -1);

    SendMessage(this->hwndInfotip, this->infotipVisible ? TTM_NEWTOOLRECT : TTM_ADDTOOL, 0, (LPARAM)&ti);
    this->infotipVisible = true;
}

void WindowInfo::DeleteInfotip()
{
    if (!infotipVisible)
        return;

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = hwndCanvas;

    SendMessage(hwndInfotip, TTM_DELTOOL, 0, (LPARAM)&ti);
    infotipVisible = false;
}

void WindowInfo::ShowNotification(const WCHAR *message, int options, NotificationGroup groupId)
{
    int timeoutMS = (options & NOS_PERSIST) ? 0 : 3000;
    bool highlight = (options & NOS_HIGHLIGHT);

    NotificationWnd *wnd = new NotificationWnd(hwndCanvas, message, timeoutMS, highlight, notifications);
    if (NG_CURSOR_POS_HELPER == groupId) {
        wnd->shrinkLimit = 0.7f;
    }
    notifications->Add(wnd, groupId);
}

bool WindowInfo::CreateUIAProvider()
{
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

class RemoteDestination : public PageDestination {
    PageDestType type;
    int pageNo;
    RectD rect;
    ScopedMem<WCHAR> value;
    ScopedMem<WCHAR> name;

public:
    RemoteDestination(PageDestination *dest) :
        type(dest->GetDestType()), pageNo(dest->GetDestPageNo()),
        rect(dest->GetDestRect()), value(dest->GetDestValue()),
        name(dest->GetDestName()) { }
    virtual ~RemoteDestination() { }

    virtual PageDestType GetDestType() const { return type; }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const { return rect; }
    virtual WCHAR *GetDestValue() const { return str::Dup(value); }
    virtual WCHAR *GetDestName() const { return str::Dup(name); }
};

void LinkHandler::GotoLink(PageDestination *link)
{
    CrashIf(!owner || owner->linkHandler != this);
    if (!link || !owner->IsDocLoaded())
        return;

    TabInfo *tab = owner->currentTab;
    ScopedMem<WCHAR> path(link->GetDestValue());
    PageDestType type = link->GetDestType();
    if (Dest_ScrollTo == type) {
        // TODO: respect link->ld.gotor.new_window for PDF documents ?
        ScrollTo(link);
    }
    else if (Dest_LaunchURL == type) {
        if (!path)
            /* ignore missing URLs */;
        else {
            WCHAR *colon = str::FindChar(path, ':');
            WCHAR *hash = str::FindChar(path, '#');
            if (!colon || (hash && colon > hash)) {
                // treat relative URIs as file paths (without fragment identifier)
                if (hash)
                    *hash = '\0';
                // LaunchFile will reject unsupported file types
                LaunchFile(path, nullptr);
            }
            else {
                // LaunchBrowser will reject unsupported URI schemes
                LaunchBrowser(path);
            }
        }
    }
    else if (Dest_LaunchEmbedded == type) {
        // open embedded PDF documents in a new window
        if (path && str::StartsWith(path.Get(), tab->filePath.Get())) {
            WindowInfo *newWin = FindWindowInfoByFile(path, true);
            if (!newWin) {
                LoadArgs args(path, owner);
                newWin = LoadDocument(args);
            }
            if (newWin)
                newWin->Focus();
        }
        // offer to save other attachments to a file
        else {
            LinkSaver linkSaverTmp(tab, owner->hwndFrame, path);
            link->SaveEmbedded(linkSaverTmp);
        }
    }
    else if (Dest_LaunchFile == type) {
        if (path) {
            // LaunchFile only opens files inside SumatraPDF
            // (except for allowed perceived file types)
            LaunchFile(path, link);
        }
    }
    // predefined named actions
    else if (Dest_NextPage == type)
        tab->ctrl->GoToNextPage();
    else if (Dest_PrevPage == type)
        tab->ctrl->GoToPrevPage();
    else if (Dest_FirstPage == type)
        tab->ctrl->GoToFirstPage();
    else if (Dest_LastPage == type)
        tab->ctrl->GoToLastPage();
    // Adobe Reader extensions to the spec, cf. http://www.tug.org/applications/hyperref/manual.html
    else if (Dest_FindDialog == type)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_FIND_FIRST, 0);
    else if (Dest_FullScreen == type)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_VIEW_PRESENTATION_MODE, 0);
    else if (Dest_GoBack == type)
        tab->ctrl->Navigate(-1);
    else if (Dest_GoForward == type)
        tab->ctrl->Navigate(1);
    else if (Dest_GoToPageDialog == type)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_GOTO_PAGE, 0);
    else if (Dest_PrintDialog == type)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_PRINT, 0);
    else if (Dest_SaveAsDialog == type)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_SAVEAS, 0);
    else if (Dest_ZoomToDialog == type)
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_ZOOM_CUSTOM, 0);
    else
        CrashIf(Dest_None != type);
}

void LinkHandler::ScrollTo(PageDestination *dest)
{
    CrashIf(!owner || owner->linkHandler != this);
    if (!dest || !owner->IsDocLoaded())
        return;

    int pageNo = dest->GetDestPageNo();
    if (pageNo > 0)
        owner->ctrl->ScrollToLink(dest);
}

void LinkHandler::LaunchFile(const WCHAR *path, PageDestination *link)
{
    // for safety, only handle relative paths and only open them in SumatraPDF
    // (unless they're of an allowed perceived type) and never launch any external
    // file in plugin mode (where documents are supposed to be self-contained)
    WCHAR drive;
    if (str::StartsWith(path, L"\\") || str::Parse(path, L"%c:\\", &drive) || gPluginMode) {
        return;
    }

    // TODO: link is deleted when opening the document in a new tab
    RemoteDestination *remoteLink = nullptr;
    if (link) {
        remoteLink = new RemoteDestination(link);
        link = nullptr;
    }

    ScopedMem<WCHAR> fullPath(path::GetDir(owner->ctrl->FilePath()));
    fullPath.Set(path::Join(fullPath, path));
    fullPath.Set(path::Normalize(fullPath));
    // TODO: respect link->ld.gotor.new_window for PDF documents ?
    WindowInfo *newWin = FindWindowInfoByFile(fullPath, true);
    // TODO: don't show window until it's certain that there was no error
    if (!newWin) {
        LoadArgs args(fullPath, owner);
        newWin = LoadDocument(args);
        if (!newWin) {
            delete remoteLink;
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
            ScopedMem<WCHAR> msg(str::Format(_TR("Error loading %s"), fullPath));
            owner->ShowNotification(msg, NOS_HIGHLIGHT);
        }
        delete remoteLink;
        return;
    }

    newWin->Focus();
    if (!remoteLink)
        return;

    ScopedMem<WCHAR> destName(remoteLink->GetDestName());
    if (destName) {
        PageDestination *dest = newWin->ctrl->GetNamedDest(destName);
        if (dest) {
            newWin->linkHandler->ScrollTo(dest);
            delete dest;
        }
    }
    else {
        newWin->linkHandler->ScrollTo(remoteLink);
    }
    delete remoteLink;
}

// normalizes case and whitespace in the string
// caller needs to free() the result
static WCHAR *NormalizeFuzzy(const WCHAR *str)
{
    WCHAR *dup = str::Dup(str);
    CharLower(dup);
    str::NormalizeWS(dup);
    // cf. AddTocItemToView
    return dup;
}

static bool MatchFuzzy(const WCHAR *s1, const WCHAR *s2, bool partially=false)
{
    if (!partially)
        return str::Eq(s1, s2);

    // only match at the start of a word (at the beginning and after a space)
    for (const WCHAR *last = s1; (last = str::Find(last, s2)) != nullptr; last++) {
        if (last == s1 || *(last - 1) == ' ')
            return true;
    }
    return false;
}

// finds the first ToC entry that (partially) matches a given normalized name
// (ignoring case and whitespace differences)
PageDestination *LinkHandler::FindTocItem(DocTocItem *item, const WCHAR *name, bool partially)
{
    for (; item; item = item->next) {
        ScopedMem<WCHAR> fuzTitle(NormalizeFuzzy(item->title));
        if (MatchFuzzy(fuzTitle, name, partially))
            return item->GetLink();
        PageDestination *dest = FindTocItem(item->child, name, partially);
        if (dest)
            return dest;
    }
    return nullptr;
}

void LinkHandler::GotoNamedDest(const WCHAR *name)
{
    CrashIf(!owner || owner->linkHandler != this);
    Controller *ctrl = owner->ctrl;
    if (!ctrl)
        return;

    // Match order:
    // 1. Exact match on internal destination name
    // 2. Fuzzy match on full ToC item title
    // 3. Fuzzy match on a part of a ToC item title
    // 4. Exact match on page label
    PageDestination *dest = ctrl->GetNamedDest(name);
    bool hasDest = dest != NULL;
    if (dest) {
        ScrollTo(dest);
        delete dest;
    }
    else if (ctrl->HasTocTree()) {
        DocTocItem *root = ctrl->GetTocTree();
        ScopedMem<WCHAR> fuzName(NormalizeFuzzy(name));
        dest = FindTocItem(root, fuzName);
        if (!dest)
            dest = FindTocItem(root, fuzName, true);
        if (dest) {
            ScrollTo(dest);
            hasDest = true;
        }
        delete root;
    }
    if (!hasDest && ctrl->HasPageLabels()) {
        int pageNo = ctrl->GetPageByLabel(name);
        if (ctrl->ValidPageNo(pageNo))
            ctrl->GoToPage(pageNo, true);
    }
}
