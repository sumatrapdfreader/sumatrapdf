/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "DisplayModel.h"
#include "WindowInfo.h"
#include "PdfSync.h"
#include "Resource.h"
#include "FileUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "FileWatch.h"

WindowInfo::WindowInfo(HWND hwnd) :
    dm(NULL), menu(NULL), hwndFrame(hwnd),
    linkOnLastButtonDown(NULL), url(NULL), selectionOnPage(NULL),
    tocLoaded(false), tocShow(false), tocState(NULL), tocRoot(NULL),
    fullScreen(false), presentation(PM_DISABLED),
    hwndCanvas(NULL), hwndToolbar(NULL), hwndReBar(NULL),
    hwndFindText(NULL), hwndFindBox(NULL), hwndFindBg(NULL),
    hwndPageText(NULL), hwndPageBox(NULL), hwndPageBg(NULL), hwndPageTotal(NULL),
    hwndTocBox(NULL), hwndTocTree(NULL), hwndSpliter(NULL),
    hwndInfotip(NULL), infotipVisible(false), hwndProperties(NULL),
    findThread(NULL), findCanceled(false), findPercent(0), findStatusVisible(false),
    findStatusThread(NULL), stopFindStatusThreadEvent(NULL), findStatusHighlight(false),
    showSelection(false), mouseAction(MA_IDLE),
    prevZoomVirtual(INVALID_ZOOM), prevDisplayMode(DM_AUTOMATIC),
    loadedFilePath(NULL), currPageNo(0),
    xScrollSpeed(0), yScrollSpeed(0), wheelAccumDelta(0),
    delayedRepaintTimer(0), resizingTocBox(false), watcher(NULL),
    pdfsync(NULL), threadStressRunning(false)
{
    ZeroMemory(&selectionRect, sizeof(selectionRect));

    HDC hdcFrame = GetDC(hwndFrame);
    dpi = GetDeviceCaps(hdcFrame, LOGPIXELSY);
    // round untypical resolutions up to the nearest quarter
    uiDPIFactor = ceil(dpi * 4.0f / USER_DEFAULT_SCREEN_DPI) / 4.0f;
    ReleaseDC(hwndFrame, hdcFrame);

    buffer = new DoubleBuffer(hwndCanvas, canvasRc);

    linkHandler = new PdfLinkHandler(this);
    fwdsearchmark.show = false;
}

WindowInfo::~WindowInfo() {
    this->AbortFinding();
    delete this->dm;
    delete this->watcher;
    delete this->pdfsync;
    delete this->linkHandler;

    CloseHandle(this->stopFindStatusThreadEvent);
    CloseHandle(this->findStatusThread);

    delete this->buffer;
    delete this->selectionOnPage;
    delete this->linkOnLastButtonDown;

    free(this->loadedFilePath);

    delete this->tocRoot;
    free(this->tocState);
}

// Notify both display model and double-buffer (if they exist)
// about a potential change of available canvas size
void WindowInfo::UpdateCanvasSize()
{
    ClientRect rc(hwndCanvas);
    if (canvasRc == rc)
        return;
    canvasRc = ClientRect(hwndCanvas);

    // create a new output buffer and notify the model
    // about the change of the canvas size
    delete buffer;
    buffer = new DoubleBuffer(hwndCanvas, canvasRc);

    if (IsDocLoaded()) {
        // the display model needs to know the full size (including scroll bars)
        dm->ChangeViewPortSize(WindowRect(hwndCanvas).Size());
    }
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
    assert(this->dm);
    if (!this->IsDocLoaded()) return;

    PointI pt;
    bool zoomToPt = this->showSelection && this->selectionOnPage;

    // either scroll towards the center of the current selection
    if (zoomToPt) {
        RectI selRect;
        for (SelectionOnPage *sel = this->selectionOnPage; sel; sel = sel->next)
            selRect = selRect.Union(sel->GetCanvasRect(this->dm));

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
    // or towards the top-left-most part of the first visible page
    else {
        int page = this->dm->firstVisiblePageNo();
        PageInfo *pageInfo = this->dm->getPageInfo(page);
        if (pageInfo) {
            RectI visible = pageInfo->pageOnScreen.Intersect(this->canvasRc);
            pt = visible.TL();

            ScrollState ss(0, pt.x, pt.y);
            if (!visible.IsEmpty() &&
                this->dm->cvtScreenToUser(&ss.page, &ss) &&
                this->dm->pageVisible(ss.page))
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

// TODO: find a better place to put this

DoubleBuffer::DoubleBuffer(HWND hwnd, RectI rect) :
    hTarget(hwnd), rect(rect), hdcBuffer(NULL), doubleBuffer(NULL)
{
    hdcCanvas = ::GetDC(hwnd);

    if (rect.IsEmpty())
        return;

    doubleBuffer = CreateCompatibleBitmap(hdcCanvas, rect.dx, rect.dy);
    if (!doubleBuffer)
        return;

    hdcBuffer = CreateCompatibleDC(hdcCanvas);
    if (!hdcBuffer)
        return;

    DeleteObject(SelectObject(hdcBuffer, doubleBuffer));
}

DoubleBuffer::~DoubleBuffer()
{
    if (doubleBuffer)
        DeleteObject(doubleBuffer);
    if (hdcBuffer)
        DeleteDC(hdcBuffer);
    ReleaseDC(hTarget, hdcCanvas);
}

void DoubleBuffer::Flush(HDC hdc)
{
    assert(hdc != hdcBuffer);
    if (hdcBuffer)
        BitBlt(hdc, rect.x, rect.y, rect.dx, rect.dy, hdcBuffer, 0, 0, SRCCOPY);
}

RectI SelectionOnPage::GetCanvasRect(DisplayModel *dm)
{
    // if the page is not visible, we return an empty rectangle
    PageInfo *pageInfo = dm->getPageInfo(pageNo);
    if (pageInfo->visibleRatio <= 0.0)
        return RectI();

    RectD canvasRect = rect;
    dm->cvtUserToScreen(pageNo, &canvasRect);
    return canvasRect.Convert<int>();
}

SelectionOnPage *SelectionOnPage::FromRectangle(DisplayModel *dm, RectI rect)
{
    SelectionOnPage *sel = NULL;

    for (int pageNo = dm->pageCount(); pageNo >= 1; --pageNo) {
        PageInfo *pageInfo = dm->getPageInfo(pageNo);
        assert(0.0 == pageInfo->visibleRatio || pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        RectI intersect = rect.Intersect(pageInfo->pageOnScreen);
        if (intersect.IsEmpty())
            continue;

        /* selection intersects with a page <pageNo> on the screen */
        int selPageNo;
        RectD isectD = intersect.Convert<double>();
        if (!dm->cvtScreenToUser(&selPageNo, &isectD))
            continue;

        assert(pageNo == selPageNo);

        SelectionOnPage *selOnPage = new SelectionOnPage(selPageNo, &isectD);
        selOnPage->next = sel;
        sel = selOnPage;
    }

    return sel;
}

SelectionOnPage *SelectionOnPage::FromTextSelect(TextSel *textSel)
{
    SelectionOnPage *sel = NULL;

    for (int i = textSel->len - 1; i >= 0; i--) {
        SelectionOnPage *selOnPage = new SelectionOnPage(textSel->pages[i], &textSel->rects[i].Convert<double>());
        selOnPage->next = sel;
        sel = selOnPage;
    }

    return sel;
}

PdfEngine *PdfLinkHandler::engine()
{
    if (!owner || !owner->dm)
        return NULL;
    return owner->dm->pdfEngine;
}

void PdfLinkHandler::GotoPdfLink(PdfLink *link)
{
    assert(owner && owner->linkHandler == this);
    if (!engine())
        return;
    if (!link)
        return;

    DisplayModel *dm = owner->dm;
    ScopedMem<TCHAR> path(link->GetValue());
    if (PDF_LINK_URI == link->kind() && path) {
        if (Str::StartsWithI(path, _T("http:")) || Str::StartsWithI(path, _T("https:")))
            LaunchBrowser(path);
        /* else: unsupported uri type */
    }
    else if (PDF_LINK_GOTO == link->kind()) {
        GotoPdfDest(link->dest());
    }
    else if (PDF_LINK_LAUNCH == link->kind() && fz_dict_gets(link->dest(), "EF")) {
        fz_obj *embeddedList = fz_dict_gets(link->dest(), "EF");
        fz_obj *embedded = fz_dict_gets(embeddedList, "UF");
        if (!embedded)
            embedded = fz_dict_gets(embeddedList, "F");
        if (path && Str::EndsWithI(path, _T(".pdf"))) {
            // open embedded PDF documents in a new window
            ScopedMem<TCHAR> combinedPath(Str::Format(_T("%s:%d:%d"), dm->fileName(), fz_to_num(embedded), fz_to_gen(embedded)));
            LoadDocument(combinedPath);
        } else {
            // offer to save other attachments to a file
            fz_buffer *data = dm->pdfEngine->getStreamData(fz_to_num(embedded), fz_to_gen(embedded));
            if (data) {
                SaveEmbeddedFile(data->data, data->len, path);
                fz_drop_buffer(data);
            }
        }
    }
    else if (PDF_LINK_LAUNCH == link->kind() && path) {
        /* for safety, only handle relative PDF paths and only open them in SumatraPDF */
        if (!Str::StartsWith(path.Get(), _T("\\")) &&
            Str::EndsWithI(path.Get(), _T(".pdf"))) {
            ScopedMem<TCHAR> basePath(Path::GetDir(dm->fileName()));
            ScopedMem<TCHAR> combinedPath(Path::Join(basePath, path));
            LoadDocument(combinedPath);
        }
    }
    else if (PDF_LINK_NAMED == link->kind()) {
        char *name = fz_to_name(link->dest());
        if (Str::Eq(name, "NextPage"))
            dm->goToNextPage(0);
        else if (Str::Eq(name, "PrevPage"))
            dm->goToPrevPage(0);
        else if (Str::Eq(name, "FirstPage"))
            dm->goToFirstPage();
        else if (Str::Eq(name, "LastPage"))
            dm->goToLastPage();
        // Adobe Reader extensions to the spec, cf. http://www.tug.org/applications/hyperref/manual.html
        else if (Str::Eq(name, "FullScreen"))
            PostMessage(owner->hwndFrame, WM_COMMAND, IDM_VIEW_PRESENTATION_MODE, 0);
        else if (Str::Eq(name, "GoBack"))
            dm->navigate(-1);
        else if (Str::Eq(name, "GoForward"))
            dm->navigate(1);
        else if (Str::Eq(name, "Print"))
            PostMessage(owner->hwndFrame, WM_COMMAND, IDM_PRINT, 0);
        else if (Str::Eq(name, "SaveAs"))
            PostMessage(owner->hwndFrame, WM_COMMAND, IDM_SAVEAS, 0);
        else if (Str::Eq(name, "ZoomTo"))
            PostMessage(owner->hwndFrame, WM_COMMAND, IDM_ZOOM_CUSTOM, 0);
    }
    else if (PDF_LINK_ACTION == link->kind()) {
        char *type = fz_to_name(fz_dict_gets(link->dest(), "S"));
        if (type && Str::Eq(type, "GoToR") && fz_dict_gets(link->dest(), "D") && path) {
            /* for safety, only handle relative PDF paths and only open them in SumatraPDF */
            if (!Str::StartsWith(path.Get(), _T("\\")) &&
                Str::EndsWithI(path.Get(), _T(".pdf"))) {
                ScopedMem<TCHAR> basePath(Path::GetDir(dm->fileName()));
                ScopedMem<TCHAR> combinedPath(Path::Join(basePath, path));
                // TODO: respect fz_to_bool(fz_dict_gets(link->dest(), "NewWindow"))
                WindowInfo *newWin = LoadDocument(combinedPath);
                if (newWin && newWin->IsDocLoaded())
                    newWin->linkHandler->GotoPdfDest(fz_dict_gets(link->dest(), "D"));
            }
        }
        /* else unsupported action */
    }
}

void PdfLinkHandler::GotoPdfDest(fz_obj *dest)
{
    assert(owner && owner->linkHandler == this);
    if (!engine())
        return;

    int pageNo = engine()->findPageNo(dest);
    if (pageNo <= 0)
        return;

    DisplayModel *dm = owner->dm;
    PointD scroll(-1, 0);
    fz_obj *obj = fz_array_get(dest, 1);
    if (Str::Eq(fz_to_name(obj), "XYZ")) {
        scroll.x = fz_to_real(fz_array_get(dest, 2));
        scroll.y = fz_to_real(fz_array_get(dest, 3));
        dm->cvtUserToScreen(pageNo, &scroll);

        // goToPage needs scrolling info relative to the page's top border
        // and the page line's left margin
        PageInfo * pageInfo = dm->getPageInfo(pageNo);
        // TODO: These values are not up-to-date, if the page has not been shown yet
        if (pageInfo->shown) {
            scroll.x -= pageInfo->pageOnScreen.x;
            scroll.y -= pageInfo->pageOnScreen.y;
        }

        // NULL values for the coordinates mean: keep the current position
        if (fz_is_null(fz_array_get(dest, 2)))
            scroll.x = -1;
        if (fz_is_null(fz_array_get(dest, 3))) {
            pageInfo = dm->getPageInfo(dm->currentPageNo());
            scroll.y = -(pageInfo->pageOnScreen.y - dm->getPadding()->top);
            scroll.y = MAX(scroll.y, 0); // Adobe Reader never shows the previous page
        }
    }
    else if (Str::Eq(fz_to_name(obj), "FitR")) {
        scroll.x = fz_to_real(fz_array_get(dest, 2)); // left
        scroll.y = fz_to_real(fz_array_get(dest, 5)); // top
        dm->cvtUserToScreen(pageNo, &scroll);
        // TODO: adjust zoom so that the bottom right corner is also visible?

        // goToPage needs scrolling info relative to the page's top border
        // and the page line's left margin
        PageInfo * pageInfo = dm->getPageInfo(pageNo);
        // TODO: These values are not up-to-date, if the page has not been shown yet
        if (pageInfo->shown) {
            scroll.x -= pageInfo->pageOnScreen.x;
            scroll.y -= pageInfo->pageOnScreen.y;
        }
    }
    /* // ignore author-set zoom settings (at least as long as there's no way to overrule them)
    else if (Str::Eq(fz_to_name(obj), "Fit")) {
        dm->zoomTo(ZOOM_FIT_PAGE);
        owner->UpdateToolbarState();
    }
    // */
    dm->goToPage(pageNo, (int)scroll.y, true, (int)scroll.x);
}

void PdfLinkHandler::GotoNamedDest(const TCHAR *name)
{
    assert(owner && owner->linkHandler == this);
    if (!engine())
        return;

    ScopedMem<char> name_utf8(Str::Conv::ToUtf8(name));
    GotoPdfDest(engine()->getNamedDest(name_utf8));
}
