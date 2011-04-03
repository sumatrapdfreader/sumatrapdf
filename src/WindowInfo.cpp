/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "DisplayModel.h"
#include "WindowInfo.h"
#include "PdfSync.h"
#include "Resource.h"
#include "FileUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "FileWatch.h"
#ifdef BUILD_COMIC_ENGINE
#include "ComicBook.h"
#endif

WindowInfo::WindowInfo(HWND hwnd) :
    dm(NULL), hwndFrame(hwnd),
    linkOnLastButtonDown(NULL), url(NULL), selectionOnPage(NULL),
    tocLoaded(false), tocShow(false), tocState(NULL), tocRoot(NULL),
    fullScreen(false), presentation(PM_DISABLED),
    hwndCanvas(NULL), hwndToolbar(NULL), hwndReBar(NULL),
    hwndFindText(NULL), hwndFindBox(NULL), hwndFindBg(NULL),
    hwndPageText(NULL), hwndPageBox(NULL), hwndPageBg(NULL), hwndPageTotal(NULL),
    hwndTocBox(NULL), hwndTocTree(NULL), hwndSpliter(NULL),
    hwndInfotip(NULL), infotipVisible(false), hwndPdfProperties(NULL),
    menu(NULL), hdc(NULL),
    findThread(NULL), findCanceled(false), findPercent(0), findStatusVisible(false),
    findStatusThread(NULL), stopFindStatusThreadEvent(NULL), findStatusHighlight(false),
    showSelection(false), mouseAction(MA_IDLE),
    hdcToDraw(NULL), hdcDoubleBuffer(NULL), bmpDoubleBuffer(NULL),
    prevZoomVirtual(INVALID_ZOOM), prevDisplayMode(DM_AUTOMATIC),
    loadedFilePath(NULL), currPageNo(0),
    xScrollSpeed(0), yScrollSpeed(0), wheelAccumDelta(0),
    delayedRepaintTimer(0), resizingTocBox(false), watcher(NULL),
    pdfsync(NULL), threadStressRunning(false),
    comicPages(NULL)
{
    ZeroMemory(&selectionRect, sizeof(selectionRect));
    prevCanvasSize.dx = prevCanvasSize.dy = -1;

    HDC hdcFrame = GetDC(hwndFrame);
    dpi = GetDeviceCaps(hdcFrame, LOGPIXELSY);
    // round untypical resolutions up to the nearest quarter
    uiDPIFactor = ceil(dpi * 4.0f / USER_DEFAULT_SCREEN_DPI) / 4.0f;
    ReleaseDC(hwndFrame, hdcFrame);

    fwdsearchmark.show = false;
}

WindowInfo::~WindowInfo() {
    this->AbortFinding();
    delete this->dm;
    delete this->watcher;
    delete this->pdfsync;

    CloseHandle(this->stopFindStatusThreadEvent);
    CloseHandle(this->findStatusThread);

    this->DoubleBuffer_Delete();

    free(this->loadedFilePath);

    delete this->tocRoot;
    free(this->tocState);

#ifdef BUILD_COMIC_ENGINE
    if (comicPages) {
        DeleteVecMembers(*comicPages);
        delete comicPages;
    }
#endif
}

void WindowInfo::UpdateCanvasSize()
{
    this->canvasRc = ClientRect(hwndCanvas);
}

void WindowInfo::DoubleBuffer_Show(HDC hdc)
{
    if (this->hdc != this->hdcToDraw) {
        assert(this->hdcToDraw == this->hdcDoubleBuffer);
        BitBlt(hdc, 0, 0, this->canvasRc.dx, this->canvasRc.dy, this->hdcDoubleBuffer, 0, 0, SRCCOPY);
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

void WindowInfo::ChangePresentationMode(PresentationMode mode)
{
    presentation = mode;
    RedrawAll();
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
            int page = ((PdfTocItem *)item.lParam)->pageNo;
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
    if (!this->tocLoaded || !this->tocShow)
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

void WindowInfo::UpdateToCExpansionState(HTREEITEM hItem)
{
    while (hItem) {
        TVITEM item;
        item.hItem = hItem;
        item.mask = TVIF_PARAM | TVIF_STATE;
        item.stateMask = TVIS_EXPANDED;
        TreeView_GetItem(this->hwndTocTree, &item);

        // add the ids of toggled items to tocState
        PdfTocItem *tocItem = item.lParam ? (PdfTocItem *)item.lParam : NULL;
        bool wasToggled = tocItem && !(item.state & TVIS_EXPANDED) == tocItem->open;
        if (wasToggled) {
            int *newState = (int *)realloc(this->tocState, (++this->tocState[0] + 1) * sizeof(int));
            if (newState) {
                this->tocState = newState;
                this->tocState[this->tocState[0]] = tocItem->id;
            }
        }

        if (tocItem && tocItem->child)
            this->UpdateToCExpansionState(TreeView_GetChild(this->hwndTocTree, hItem));
        hItem = TreeView_GetNextSibling(this->hwndTocTree, hItem);
    }
}

void WindowInfo::DisplayStateFromToC(DisplayState *ds)
{
    ds->showToc = this->tocShow;

    if (this->tocLoaded) {
        free(this->tocState);
        this->tocState = SAZA(int, 1);
        HTREEITEM hRoot = TreeView_GetRoot(this->hwndTocTree);
        if (this->tocState && hRoot)
            this->UpdateToCExpansionState(hRoot);
    }

    free(ds->tocState);
    ds->tocState = NULL;
    if (this->tocState)
        ds->tocState = (int *)memdup(this->tocState, (this->tocState[0] + 1) * sizeof(int));
}

void WindowInfo::ResizeIfNeeded(bool resizeWindow)
{
    ClientRect rc(hwndCanvas);

    if (hdcToDraw && canvasRc.dx == rc.dx && canvasRc.dy == rc.dy)
        return;

    DoubleBuffer_New();
    if (!resizeWindow)
        return;

    if (PdfLoaded())
        dm->changeTotalDrawAreaSize(this->canvasRc.Size());
    else if (ComicBookLoaded()) {
        // TODO: code for comic book
    } else
        assert(false);
}

void WindowInfo::ToggleZoom()
{
    assert(this->dm);
    if (!this->PdfLoaded()) return;

    this->prevCanvasSize.dx = this->prevCanvasSize.dy = -1;
    if (ZOOM_FIT_PAGE == this->dm->zoomVirtual())
        this->dm->zoomTo(ZOOM_FIT_WIDTH);
    else if (ZOOM_FIT_WIDTH == this->dm->zoomVirtual())
        this->dm->zoomTo(ZOOM_FIT_CONTENT);
    else if (ZOOM_FIT_CONTENT == this->dm->zoomVirtual())
        this->dm->zoomTo(ZOOM_FIT_PAGE);
}

void WindowInfo::ZoomToSelection(float factor, bool relative)
{
    assert(this->dm);
    if (!this->PdfLoaded()) return;

    PointI pt;
    bool zoomToPt = this->showSelection && this->selectionOnPage;

    if (zoomToPt) {
        RectI selRect = this->selectionOnPage->selectionCanvas;
        for (SelectionOnPage *sel = this->selectionOnPage->next; sel; sel = sel->next)
            selRect = selRect.Union(sel->selectionCanvas);

        ClientRect rc(this->hwndCanvas);
        pt.x = 2 * selRect.x + selRect.dx - rc.dx / 2;
        pt.y = 2 * selRect.y + selRect.dy - rc.dy / 2;

        pt.x = CLAMP(pt.x, selRect.x, selRect.x + selRect.dx);
        pt.y = CLAMP(pt.y, selRect.y, selRect.y + selRect.dy);

        ScrollState ss(0, pt.x, pt.y);
        if (!this->dm->cvtScreenToUser(&ss.page, &ss) ||
            !this->dm->pageVisible(ss.page))
            zoomToPt = false;
    }

    this->prevCanvasSize.dx = this->prevCanvasSize.dy = -1;
    if (relative)
        this->dm->zoomBy(factor, zoomToPt ? &pt : NULL);
    else
        this->dm->zoomTo(factor, zoomToPt ? &pt : NULL);

    this->UpdateToolbarState();
}

void WindowInfo::UpdateToolbarState()
{
    if (!this->PdfLoaded())
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
    if (!this->PdfLoaded()) return;
    assert(!this->linkOnLastButtonDown);
    if (this->linkOnLastButtonDown) return;
    if (0 != dx)
        this->dm->scrollXBy(dx);
    if (0 != dy)
        this->dm->scrollYBy(dy, FALSE);
}

#define MULTILINE_INFOTIP_WIDTH_PX 300

void WindowInfo::CreateInfotip(const TCHAR *text, RectI *rc)
{
    if (Str::IsEmpty(text)) {
        this->DeleteInfotip();
        return;
    }

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = this->hwndCanvas;
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = (TCHAR *)text;
    if (rc)
        ti.rect = rc->ToRECT();

    if (Str::FindChar(text, '\n'))
        SendMessage(this->hwndInfotip, TTM_SETMAXTIPWIDTH, 0, MULTILINE_INFOTIP_WIDTH_PX);
    else
        SendMessage(this->hwndInfotip, TTM_SETMAXTIPWIDTH, 0, -1);

    SendMessage(this->hwndInfotip, this->infotipVisible ? TTM_NEWTOOLRECT : TTM_ADDTOOL, 0, (LPARAM)&ti);
    this->infotipVisible = true;
}

void WindowInfo::DeleteInfotip()
{
    if (!this->infotipVisible)
        return;

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = this->hwndCanvas;

    SendMessage(this->hwndInfotip, TTM_DELTOOL, 0, (LPARAM)&ti);
    this->infotipVisible = false;
}
