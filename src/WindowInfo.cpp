#include "WindowInfo.h"
#include "Resource.h"
#include "win_util.h"
#include "file_util.h"
#include "tstr_util.h"

WindowInfo::WindowInfo(HWND hwnd) :
    dm(NULL), state(WS_ABOUT), hwndFrame(hwnd),
    linkOnLastButtonDown(NULL), url(NULL), selectionOnPage(NULL),
    tocLoaded(false), fullScreen(false), presentation(PM_DISABLED),
    hwndCanvas(NULL), hwndToolbar(NULL), hwndReBar(NULL),
    hwndFindText(NULL), hwndFindBox(NULL), hwndFindBg(NULL),
    hwndPageText(NULL), hwndPageBox(NULL), hwndPageBg(NULL), hwndPageTotal(NULL),
    hwndTocBox(NULL), hwndTocTree(NULL), hwndSpliter(NULL),
    hwndInfotip(NULL), infotipVisible(false), hwndPdfProperties(NULL),
    hMenu(NULL), hdc(NULL),
    findThread(NULL), findCanceled(false), findPercent(0), findStatusVisible(false),
    findStatusThread(NULL), stopFindStatusThreadEvent(NULL),
    showSelection(false), showForwardSearchMark(false), fwdsearchmarkHideStep(0),
    mouseAction(MA_IDLE), needrefresh(false),
    hdcToDraw(NULL), hdcDoubleBuffer(NULL), bmpDoubleBuffer(NULL),
    title(NULL), loadedFilePath(NULL), currPageNo(0),
    xScrollSpeed(0), yScrollSpeed(0), wheelAccumDelta(0),
    delayedRepaintTimer(0), resizingTocBox(false),
    pdfsync(NULL), pluginParent(NULL)
{
    ZeroMemory(&selectionRect, sizeof(selectionRect));
    fwdsearchmarkRects.clear();

    HDC hdcFrame = GetDC(hwndFrame);
    dpi = GetDeviceCaps(hdcFrame, LOGPIXELSY);
    // round untypical resolutions up to the nearest quarter
    uiDPIFactor = ceil(dpi * 4.0 / USER_DEFAULT_SCREEN_DPI) / 4.0;
    ReleaseDC(hwndFrame, hdcFrame);
}

WindowInfo::~WindowInfo() {
    this->AbortFinding();
    delete this->dm;
    this->dm = NULL;
    delete this->pdfsync;
    this->pdfsync = NULL;

    if (this->stopFindStatusThreadEvent) {
        CloseHandle(this->stopFindStatusThreadEvent);
        this->stopFindStatusThreadEvent = NULL;
    }
    if (this->findStatusThread) {
        CloseHandle(this->findStatusThread);
        this->findStatusThread = NULL;
    }
    this->DoubleBuffer_Delete();

    free(this->title);
    free(this->loadedFilePath);
}

void WindowInfo::DoubleBuffer_Show(HDC hdc)
{
    if (this->hdc != this->hdcToDraw) {
        assert(this->hdcToDraw == this->hdcDoubleBuffer);
        BitBlt(hdc, 0, 0, this->winDx(), this->winDy(), this->hdcDoubleBuffer, 0, 0, SRCCOPY);
    }
}

void WindowInfo::DoubleBuffer_Delete() {
    if (this->bmpDoubleBuffer) {
        DeleteObject(this->bmpDoubleBuffer);
        this->bmpDoubleBuffer = NULL;
    }

    if (this->hdcDoubleBuffer) {
        DeleteDC(this->hdcDoubleBuffer);
        this->hdcDoubleBuffer = NULL;
    }
    this->hdcToDraw = NULL;
}

void WindowInfo::AbortFinding()
{
    if (this->findThread) {
        this->findCanceled = true;
        WaitForSingleObject(this->findThread, INFINITE);
    }
    this->findCanceled = false;
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
        if (item.lParam && PDF_LGOTO == ((pdf_link *)item.lParam)->kind) {
            int page = this->dm->pdfEngine->findPageNo(((pdf_link *)item.lParam)->dest);
            if (page <= pageNo)
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
    if (!this->dm || !this->dm->_showToc || !this->tocLoaded)
        return;
    if (GetFocus() == this->hwndTocTree)
        return;

    HTREEITEM hRoot = TreeView_GetRoot(this->hwndTocTree);
    if (!hRoot)
        return;
    HTREEITEM hCurrItem = this->TreeItemForPageNo(hRoot, currPageNo);
    if (hCurrItem)
        TreeView_SelectItem(this->hwndTocTree, hCurrItem);
}

void WindowInfo::ResizeToWindow()
{
    assert(this->dm);
    if (!this->dm) return;

    this->dm->changeTotalDrawAreaSize(this->winSize());
}

void WindowInfo::ToggleZoom()
{
    assert(this->dm);
    if (!this->dm) return;

    this->prevCanvasBR.x = this->prevCanvasBR.y = -1;
    if (ZOOM_FIT_PAGE == this->dm->zoomVirtual())
        this->dm->zoomTo(ZOOM_FIT_WIDTH);
    else if (ZOOM_FIT_WIDTH == this->dm->zoomVirtual())
        this->dm->zoomTo(ZOOM_FIT_CONTENT);
    else if (ZOOM_FIT_CONTENT == this->dm->zoomVirtual())
        this->dm->zoomTo(ZOOM_FIT_PAGE);
}

void WindowInfo::ZoomToSelection(double factor, bool relative)
{
    assert(this->dm);
    if (!this->dm) return;

    POINT pt;
    bool zoomToPt = this->showSelection && this->selectionOnPage;

    if (zoomToPt) {
        RectI selRect = this->selectionOnPage->selectionCanvas;
        for (SelectionOnPage *sel = this->selectionOnPage->next; sel; sel = sel->next)
            selRect = RectI_Union(selRect, sel->selectionCanvas);

        RECT rc;
        GetClientRect(this->hwndCanvas, &rc);
        pt.x = 2 * selRect.x + selRect.dx - RectDx(&rc) / 2;
        pt.y = 2 * selRect.y + selRect.dy - RectDy(&rc) / 2;

        pt.x = CLAMP(pt.x, selRect.x, selRect.x + selRect.dx);
        pt.y = CLAMP(pt.y, selRect.y, selRect.y + selRect.dy);

        ScrollState ss = { 0, pt.x, pt.y };
        if (!this->dm->cvtScreenToUser(&ss.page, &ss.x, &ss.y) ||
            !this->dm->pageVisible(ss.page))
            zoomToPt = false;
    }

    this->prevCanvasBR.x = this->prevCanvasBR.y = -1;
    if (relative)
        this->dm->zoomBy(factor, zoomToPt ? &pt : NULL);
    else
        this->dm->zoomTo(factor, zoomToPt ? &pt : NULL);

    this->UpdateToolbarState();
}

void WindowInfo::UpdateToolbarState()
{
    if (!this->dm)
        return;

    DWORD state = SendMessage(this->hwndToolbar, TB_GETSTATE, IDT_VIEW_FIT_WIDTH, 0);
    if (this->dm->displayMode() == DM_CONTINUOUS && this->dm->zoomVirtual() == ZOOM_FIT_WIDTH)
        state |= TBSTATE_CHECKED;
    else
        state &= ~TBSTATE_CHECKED;
    SendMessage(this->hwndToolbar, TB_SETSTATE, IDT_VIEW_FIT_WIDTH, state);

    state = SendMessage(this->hwndToolbar, TB_GETSTATE, IDT_VIEW_FIT_PAGE, 0);
    if (this->dm->displayMode() == DM_SINGLE_PAGE && this->dm->zoomVirtual() == ZOOM_FIT_PAGE)
        state |= TBSTATE_CHECKED;
    else
        state &= ~TBSTATE_CHECKED;
    SendMessage(this->hwndToolbar, TB_SETSTATE, IDT_VIEW_FIT_PAGE, state);
}

/* :::::: WindowInfoList :::::: */

void WindowInfoList::remove(WindowInfo *win) {
    assert(win);
    if (!win) return;

    for (size_t i = 0; i < this->size(); i++) {
        if ((*this)[i] == win) {
            this->erase(i);
            break;
        }
    }
}

WindowInfo* WindowInfoList::find(HWND hwnd)
{
    for (size_t i = 0; i < this->size(); i++) {
        WindowInfo *win = (*this)[i];
        if (hwnd == win->hwndFrame)
            return win;
        if (hwnd == win->hwndCanvas)
            return win;
        if (hwnd == win->hwndReBar)
            return win;
        if (hwnd == win->hwndFindBox)
            return win;
        if (hwnd == win->hwndFindStatus)
            return win;
        if (hwnd == win->hwndPageBox)
            return win;
        if (hwnd == win->hwndTocBox)
            return win;
        if (hwnd == win->hwndTocTree)
            return win;
        if (hwnd == win->hwndSpliter)
            return win;
        if (hwnd == win->hwndPdfProperties)
            return win;
    }

    return NULL;
}

// Find the first windows showing a given PDF file 
WindowInfo* WindowInfoList::find(TCHAR * file) {
    TCHAR * normFile = FilePath_Normalize(file, FALSE);
    if (!normFile)
        return NULL;

    WindowInfo *found = NULL;
    for (size_t i = 0; i < this->size(); i++) {
        WindowInfo *win = (*this)[i];
        if (win->loadedFilePath && tstr_ieq(win->loadedFilePath, normFile)) {
            found = win;
            break;
        }
    }

    free(normFile);
    return found;
}
