/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "WindowInfo.h"
#include "PdfSync.h"
#include "Resource.h"
#include "FileWatch.h"
#include "Notifications.h"

WindowInfo::WindowInfo(HWND hwnd) :
    dm(NULL), menu(NULL), hwndFrame(hwnd),
    linkOnLastButtonDown(NULL), url(NULL), selectionOnPage(NULL),
    tocLoaded(false), tocVisible(false), tocRoot(NULL),
    fullScreen(false), presentation(PM_DISABLED),
    hwndCanvas(NULL), hwndToolbar(NULL), hwndReBar(NULL),
    hwndFindText(NULL), hwndFindBox(NULL), hwndFindBg(NULL),
    hwndPageText(NULL), hwndPageBox(NULL), hwndPageBg(NULL), hwndPageTotal(NULL),
    hwndTocBox(NULL), hwndTocTree(NULL), hwndSidebarSpliter(NULL),
    hwndInfotip(NULL), infotipVisible(false), hwndProperties(NULL),
    findThread(NULL), findCanceled(false), printThread(NULL), printCanceled(false),
    showSelection(false), mouseAction(MA_IDLE),
    prevZoomVirtual(INVALID_ZOOM), prevDisplayMode(DM_AUTOMATIC),
    loadedFilePath(NULL), currPageNo(0),
    xScrollSpeed(0), yScrollSpeed(0), wheelAccumDelta(0),
    delayedRepaintTimer(0), sidebarBeingResized(false), watcher(NULL),
    pdfsync(NULL), stressTest(NULL), suppressPwdUI(false),
    hwndFavBox(NULL), hwndFavTree(NULL), favVisible(false)
{
    ZeroMemory(&selectionRect, sizeof(selectionRect));

    dpi = win::GetHwndDpi(hwndFrame, &uiDPIFactor);

    buffer = new DoubleBuffer(hwndCanvas, canvasRc);
    linkHandler = new LinkHandler(*this);
    messages = new MessageWndList();
    fwdSearchMark.show = false;
}

WindowInfo::~WindowInfo() 
{
    AbortFinding();
    AbortPrinting();
    delete stressTest;

    delete dm;
    delete watcher;
    delete pdfsync;
    delete linkHandler;

    delete buffer;
    delete selectionOnPage;
    delete linkOnLastButtonDown;
    delete messages;

    free(loadedFilePath);

    delete tocRoot;
}

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
        dm->ChangeViewPortSize(GetViewPortSize());
    }

    // keep the notifications visible (only needed for right-to-left layouts)
    if (IsUIRightToLeft())
        messages->Relayout();
}

SizeI WindowInfo::GetViewPortSize()
{
    SizeI size = canvasRc.Size();

    DWORD style = GetWindowLong(hwndCanvas, GWL_STYLE);
    if ((style & WS_VSCROLL))
        size.dx += GetSystemMetrics(SM_CXVSCROLL);
    if ((style & WS_HSCROLL))
        size.dy += GetSystemMetrics(SM_CYHSCROLL);

    return size;
}

void WindowInfo::ShowNotification(const TCHAR *message, bool autoDismiss, bool highlight, NotificationGroup groupId)
{
    MessageWnd *wnd = new MessageWnd(this->hwndCanvas, message, autoDismiss ? 3000 : 0, highlight, this->messages);
    this->messages->Add(wnd, groupId);
}

void WindowInfo::AbortFinding(bool hideMessage)
{
    if (this->findThread) {
        this->findCanceled = true;
        WaitForSingleObject(this->findThread, INFINITE);
    }
    this->findCanceled = false;

    if (hideMessage)
        this->messages->CleanUp(NG_FIND_PROGRESS);
}

void WindowInfo::AbortPrinting()
{
    if (this->printThread) {
        this->printCanceled = true;
        WaitForSingleObject(this->printThread, INFINITE);
    }
    this->printCanceled = false;
}

void WindowInfo::RedrawAll(bool update)
{
    InvalidateRect(this->hwndCanvas, NULL, false);
    if (update)
        UpdateWindow(this->hwndCanvas);
}

HTREEITEM WindowInfo::TreeItemForPageNo(HTREEITEM hItem, int pageNo)
{
    HTREEITEM hCurrItem = NULL;

    while (hItem) {
        TVITEM item;
        item.hItem = hItem;
        item.mask = TVIF_PARAM | TVIF_STATE;
        item.stateMask = TVIS_EXPANDED;
        TreeView_GetItem(this->hwndTocTree, &item);

        // return if this item is on the specified page (or on a latter page)
        if (item.lParam) {
            int page = ((DocToCItem *)item.lParam)->pageNo;
            if (1 <= page && page <= pageNo)
                hCurrItem = hItem;
            if (page >= pageNo)
                break;
        }

        // find any child item closer to the specified page
        HTREEITEM hSubItem = NULL;
        if ((item.state & TVIS_EXPANDED))
            hSubItem = this->TreeItemForPageNo(TreeView_GetChild(this->hwndTocTree, hItem), pageNo);
        if (hSubItem)
            hCurrItem = hSubItem;

        hItem = TreeView_GetNextSibling(this->hwndTocTree, hItem);
    }

    return hCurrItem;
}

void WindowInfo::UpdateTocSelection(int currPageNo)
{
    if (!this->tocLoaded || !this->tocVisible)
        return;
    if (GetFocus() == this->hwndTocTree)
        return;

    HTREEITEM hRoot = TreeView_GetRoot(this->hwndTocTree);
    if (!hRoot)
        return;
    // select the item closest to but not after the current page
    // (or the root item, if there's no such item)
    HTREEITEM hCurrItem = this->TreeItemForPageNo(hRoot, currPageNo);
    TreeView_SelectItem(this->hwndTocTree, hCurrItem ? hCurrItem : hRoot);
}

void WindowInfo::UpdateToCExpansionState(HTREEITEM hItem)
{
    while (hItem) {
        TVITEM item;
        item.hItem = hItem;
        item.mask = TVIF_PARAM | TVIF_STATE;
        item.stateMask = TVIS_EXPANDED;
        TreeView_GetItem(hwndTocTree, &item);

        // add the ids of toggled items to tocState
        DocToCItem *tocItem = item.lParam ? (DocToCItem *)item.lParam : NULL;
        bool wasToggled = tocItem && !(item.state & TVIS_EXPANDED) == tocItem->open;
        if (wasToggled)
            tocState.Append(tocItem->id);

        if (tocItem && tocItem->child)
            UpdateToCExpansionState(TreeView_GetChild(hwndTocTree, hItem));
        hItem = TreeView_GetNextSibling(hwndTocTree, hItem);
    }
}

void WindowInfo::DisplayStateFromToC(DisplayState *ds)
{
    ds->tocVisible = tocVisible;

    if (tocLoaded) {
        tocState.Reset();
        HTREEITEM hRoot = TreeView_GetRoot(hwndTocTree);
        if (hRoot)
            UpdateToCExpansionState(hRoot);
    }

    delete ds->tocState;
    ds->tocState = NULL;
    if (tocState.Count() > 0)
        ds->tocState = new Vec<int>(tocState);
}

void WindowInfo::ToggleZoom()
{
    assert(this->dm);
    if (!this->IsDocLoaded()) return;

    if (ZOOM_FIT_PAGE == this->dm->zoomVirtual())
        this->dm->zoomTo(ZOOM_FIT_WIDTH);
    else if (ZOOM_FIT_WIDTH == this->dm->zoomVirtual())
        this->dm->zoomTo(ZOOM_FIT_CONTENT);
    else if (ZOOM_FIT_CONTENT == this->dm->zoomVirtual())
        this->dm->zoomTo(ZOOM_FIT_PAGE);
}

void WindowInfo::ZoomToSelection(float factor, bool relative)
{
    if (!this->IsDocLoaded())
        return;

    PointI pt;
    bool zoomToPt = this->showSelection && this->selectionOnPage;

    // either scroll towards the center of the current selection
    if (zoomToPt) {
        RectI selRect;
        for (size_t i = 0; i < this->selectionOnPage->Count(); i++)
            selRect = selRect.Union(this->selectionOnPage->At(i).GetRect(this->dm));

        ClientRect rc(this->hwndCanvas);
        pt.x = 2 * selRect.x + selRect.dx - rc.dx / 2;
        pt.y = 2 * selRect.y + selRect.dy - rc.dy / 2;

        pt.x = limitValue(pt.x, selRect.x, selRect.x + selRect.dx);
        pt.y = limitValue(pt.y, selRect.y, selRect.y + selRect.dy);

        int pageNo = this->dm->GetPageNoByPoint(pt);
        if (!this->dm->validPageNo(pageNo) || !this->dm->pageVisible(pageNo))
            zoomToPt = false;
    }
    // or towards the top-left-most part of the first visible page
    else {
        int page = this->dm->firstVisiblePageNo();
        PageInfo *pageInfo = this->dm->getPageInfo(page);
        if (pageInfo) {
            RectI visible = pageInfo->pageOnScreen.Intersect(this->canvasRc);
            pt = visible.TL();

            int pageNo = this->dm->GetPageNoByPoint(pt);
            if (!visible.IsEmpty() && this->dm->validPageNo(pageNo) && this->dm->pageVisible(pageNo))
                zoomToPt = true;
        }
    }

    if (relative)
        this->dm->zoomBy(factor, zoomToPt ? &pt : NULL);
    else
        this->dm->zoomTo(factor, zoomToPt ? &pt : NULL);

    this->UpdateToolbarState();
}

void WindowInfo::UpdateToolbarState()
{
    if (!this->IsDocLoaded())
        return;

    WORD state = (WORD)SendMessage(this->hwndToolbar, TB_GETSTATE, IDT_VIEW_FIT_WIDTH, 0);
    if (this->dm->displayMode() == DM_CONTINUOUS && this->dm->zoomVirtual() == ZOOM_FIT_WIDTH)
        state |= TBSTATE_CHECKED;
    else
        state &= ~TBSTATE_CHECKED;
    SendMessage(this->hwndToolbar, TB_SETSTATE, IDT_VIEW_FIT_WIDTH, state);

    bool isChecked = (state & TBSTATE_CHECKED);

    state = (WORD)SendMessage(this->hwndToolbar, TB_GETSTATE, IDT_VIEW_FIT_PAGE, 0);
    if (this->dm->displayMode() == DM_SINGLE_PAGE && this->dm->zoomVirtual() == ZOOM_FIT_PAGE)
        state |= TBSTATE_CHECKED;
    else
        state &= ~TBSTATE_CHECKED;
    SendMessage(this->hwndToolbar, TB_SETSTATE, IDT_VIEW_FIT_PAGE, state);

    isChecked &= (state & TBSTATE_CHECKED);
    if (!isChecked)
        prevZoomVirtual = INVALID_ZOOM;
}

void WindowInfo::MoveDocBy(int dx, int dy)
{
    assert(this->dm);
    if (!this->IsDocLoaded()) return;
    assert(!this->linkOnLastButtonDown);
    if (this->linkOnLastButtonDown) return;
    if (0 != dx)
        this->dm->scrollXBy(dx);
    if (0 != dy)
        this->dm->scrollYBy(dy, FALSE);
}

#define MULTILINE_INFOTIP_WIDTH_PX 300

void WindowInfo::CreateInfotip(const TCHAR *text, RectI& rc)
{
    if (str::IsEmpty(text)) {
        this->DeleteInfotip();
        return;
    }

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = this->hwndCanvas;
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = (TCHAR *)text;
    ti.rect = rc.ToRECT();

    if (str::FindChar(text, '\n'))
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

RectI SelectionOnPage::GetRect(DisplayModel *dm)
{
    // if the page is not visible, we return an empty rectangle
    PageInfo *pageInfo = dm->getPageInfo(pageNo);
    if (!pageInfo || pageInfo->visibleRatio <= 0.0)
        return RectI();

    return dm->CvtToScreen(pageNo, rect);
}

Vec<SelectionOnPage> *SelectionOnPage::FromRectangle(DisplayModel *dm, RectI rect)
{
    Vec<SelectionOnPage> *sel = new Vec<SelectionOnPage>();

    for (int pageNo = dm->pageCount(); pageNo >= 1; --pageNo) {
        PageInfo *pageInfo = dm->getPageInfo(pageNo);
        assert(0.0 == pageInfo->visibleRatio || pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        RectI intersect = rect.Intersect(pageInfo->pageOnScreen);
        if (intersect.IsEmpty())
            continue;

        /* selection intersects with a page <pageNo> on the screen */
        RectD isectD = dm->CvtFromScreen(intersect, pageNo);
        sel->Append(SelectionOnPage(pageNo, &isectD));
    }
    sel->Reverse();

    return sel;
}

Vec<SelectionOnPage> *SelectionOnPage::FromTextSelect(TextSel *textSel)
{
    Vec<SelectionOnPage> *sel = new Vec<SelectionOnPage>(textSel->len);

    for (int i = textSel->len - 1; i >= 0; i--)
        sel->Append(SelectionOnPage(textSel->pages[i], &textSel->rects[i].Convert<double>()));
    sel->Reverse();

    return sel;
}

BaseEngine *LinkHandler::engine() const
{
    if (!owner || !owner->dm)
        return NULL;
    return owner->dm->engine;
}

void LinkHandler::GotoLink(PageDestination *link)
{
    assert(owner && owner->linkHandler == this);
    if (!link)
        return;
    if (!engine())
        return;

    DisplayModel *dm = owner->dm;
    ScopedMem<TCHAR> path(link->GetDestValue());
    const char *type = link->GetType();
    if (str::Eq(type, "ScrollTo")) {
        ScrollTo(link);
    }
    else if (str::Eq(type, "LaunchURL") && path) {
        // LaunchBrowser will reject unsupported URI schemes
        LaunchBrowser(path);
    }
    else if (str::Eq(type, "LaunchEmbedded")) {
        // open embedded PDF documents in a new window
        if (path && str::StartsWith(path.Get(), dm->fileName()))
            LoadDocument(path);
        // offer to save other attachments to a file
        else
            link->SaveEmbedded(LinkSaver(owner->hwndFrame, path));
    }
    else if ((str::Eq(type, "LaunchFile") || str::Eq(type, "ScrollToEx")) && path) {
        /* for safety, only handle relative PDF paths and only open them in SumatraPDF */
        if (!str::StartsWith(path.Get(), _T("\\")) &&
            str::EndsWithI(path.Get(), _T(".pdf"))) {
            ScopedMem<TCHAR> basePath(path::GetDir(dm->fileName()));
            ScopedMem<TCHAR> combinedPath(path::Join(basePath, path));
            // TODO: respect fz_to_bool(fz_dict_gets(link->dest, "NewWindow")) for ScrollToEx
            WindowInfo *newWin = LoadDocument(combinedPath);

            if (str::Eq(type, "ScrollToEx") && newWin && newWin->IsDocLoaded())
                newWin->linkHandler->ScrollTo(link);
        }
    }
    // predefined named actions
    else if (str::Eq(type, "NextPage"))
        dm->goToNextPage(0);
    else if (str::Eq(type, "PrevPage"))
        dm->goToPrevPage(0);
    else if (str::Eq(type, "FirstPage"))
        dm->goToFirstPage();
    else if (str::Eq(type, "LastPage"))
        dm->goToLastPage();
    // Adobe Reader extensions to the spec, cf. http://www.tug.org/applications/hyperref/manual.html
    else if (str::Eq(type, "FullScreen"))
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_VIEW_PRESENTATION_MODE, 0);
    else if (str::Eq(type, "GoBack"))
        dm->navigate(-1);
    else if (str::Eq(type, "GoForward"))
        dm->navigate(1);
    else if (str::Eq(type, "Print"))
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_PRINT, 0);
    else if (str::Eq(type, "SaveAs"))
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_SAVEAS, 0);
    else if (str::Eq(type, "ZoomTo"))
        PostMessage(owner->hwndFrame, WM_COMMAND, IDM_ZOOM_CUSTOM, 0);
}

void LinkHandler::ScrollTo(PageDestination *dest)
{
    assert(owner && owner->linkHandler == this);
    int pageNo = dest->GetDestPageNo();
    if (pageNo <= 0)
        return;

    DisplayModel *dm = owner->dm;
    PointI scroll(-1, 0);
    RectD rect = dest->GetDestRect();

    if (rect.IsEmpty()) {
        // PDF: /XYZ top left
        // scroll to rect.TL()
        PointD scrollD = dm->engine->Transform(rect.TL(), pageNo, dm->zoomReal(), dm->rotation());
        scroll = scrollD.Convert<int>();

        // default values for the coordinates mean: keep the current position
        if (DEST_USE_DEFAULT == rect.x)
            scroll.x = -1;
        if (DEST_USE_DEFAULT == rect.y) {
            PageInfo *pageInfo = dm->getPageInfo(dm->currentPageNo());
            scroll.y = -(pageInfo->pageOnScreen.y - dm->getPadding()->top);
            scroll.y = max(scroll.y, 0); // Adobe Reader never shows the previous page
        }
    }
    else if (rect.dx != DEST_USE_DEFAULT && rect.dy != DEST_USE_DEFAULT) {
        // PDF: /FitR left bottom right top
        RectD rectD = dm->engine->Transform(rect, pageNo, dm->zoomReal(), dm->rotation());
        scroll = rectD.TL().Convert<int>();

        // Rect<float> rectF = dm->engine->Transform(rect, pageNo, 1.0, dm->rotation()).Convert<float>();
        // zoom = 100.0f * min(owner->canvasRc.dx / rectF.dx, owner->canvasRc.dy / rectF.dy);
    }
    else if (rect.y != DEST_USE_DEFAULT) {
        // PDF: /FitH top  or  /FitBH top
        PointD scrollD = dm->engine->Transform(rect.TL(), pageNo, dm->zoomReal(), dm->rotation());
        scroll.y = max(scrollD.Convert<int>().y, 0); // Adobe Reader never shows the previous page

        // zoom = FitBH ? ZOOM_FIT_CONTENT : ZOOM_FIT_WIDTH
    }
    // else if (Fit || FitV) zoom = ZOOM_FIT_PAGE
    // else if (FitB || FitBV) zoom = ZOOM_FIT_CONTENT
    /* // ignore author-set zoom settings (at least as long as there's no way to overrule them)
    if (zoom != INVALID_ZOOM) {
        // TODO: adjust the zoom level before calculating the scrolling coordinates
        dm->zoomTo(zoom);
        owner->UpdateToolbarState();
    }
    // */
    dm->goToPage(pageNo, scroll.y, true, scroll.x);
}

// normalizes case and whitespace in the string
// caller needs to free() the result
static TCHAR *NormalizeFuzzy(const TCHAR *str)
{
    TCHAR *dup = str::Dup(str);
    CharLower(dup);
    // cf. AddTocItemToView in SumatraPDF.cpp
    str::NormalizeWS(dup);
    return dup;
}

static bool MatchFuzzy(const TCHAR *s1, const TCHAR *s2, bool partially=false)
{
    if (!partially)
        return str::Eq(s1, s2);

    // only match at the start of a word (at the beginning and after a space)
    for (const TCHAR *last = s1; (last = str::Find(last, s2)); last++)
        if (last == s1 || *(last - 1) == ' ')
            return true;
    return false;
}

// finds the first ToC entry that (partially) matches a given normalized name
// (ignoring case and whitespace differences)
PageDestination *LinkHandler::FindToCItem(DocToCItem *item, const TCHAR *name, bool partially)
{
    for (; item; item = item->next) {
        ScopedMem<TCHAR> fuzTitle(NormalizeFuzzy(item->title));
        if (MatchFuzzy(fuzTitle, name, partially))
            return item->GetLink();
        PageDestination *dest = FindToCItem(item->child, name, partially);
        if (dest)
            return dest;
    }
    return NULL;
}

void LinkHandler::GotoNamedDest(const TCHAR *name)
{
    assert(owner && owner->linkHandler == this);
    if (!engine())
        return;

    // Match order:
    // 1. Exact match on internal destination name
    // 2. Fuzzy match on full ToC item title
    // 3. Fuzzy match on a part of a ToC item title
    PageDestination *dest = engine()->GetNamedDest(name);
    if (dest) {
        ScrollTo(dest);
        delete dest;
    }
    else if (engine()->HasToCTree()) {
        DocToCItem *root = engine()->GetToCTree();
        ScopedMem<TCHAR> fuzName(NormalizeFuzzy(name));
        dest = FindToCItem(root, fuzName);
        if (!dest)
            dest = FindToCItem(root, fuzName, true);
        if (dest)
            ScrollTo(dest);
        delete root;
    }
}
