/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "base/Pixmap.h"
#include "base/Win.h"
#include "base/UITask.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "Theme.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "PagePosition.h"
#include "PageThumbnails.h"

constexpr int kThumbnailDx = 120;
constexpr int kThumbnailDy = 170;
constexpr int kThumbnailGap = 16;
constexpr int kThumbnailPadding = 16;
// the sidebar can be as narrow as 150 px: one 120 px column must still fit
constexpr int kThumbnailSidebarPadding = 8;
constexpr int kThumbnailMaxCols = 6;
constexpr int kThumbnailRenderScreens = 1;
constexpr int kThumbnailKeepScreens = 2;
constexpr int kDropMarkerDx = 3;
constexpr Color kCurrentPageColor = MkRgb(0, 120, 215);

static Pixmap* const kThumbnailRenderFailed = (Pixmap*)(intptr_t)-1;

struct PageThumbnailsCache {
    Vec<Pixmap*> thumbnails;
    // shown until re-rendered, after the document changed (an annotation)
    Vec<u8> stale;
    PageThumbnailsCtrl* ctrl = nullptr;
    EngineBase* renderEngine = nullptr;
    AtomicInt cancelRendering = 0;
    int rotation = 0;
    int thumbDx = 0;
    int thumbDy = 0;
    // render with the document's own engine, so unsaved page edits show;
    // else with a copy loaded from the file, which doesn't compete with the view
    bool liveEngine = false;
    bool workerRunning = false;
    bool deleteWhenWorkerFinishes = false;
};

struct ThumbnailRowsModel : ListBoxModel {
    int rows = 0;

    int ItemsCount() override { return rows; }
    Str Item(int) override { return {}; }
};

struct ThumbnailRenderTask {
    PageThumbnailsCache* cache = nullptr;
    int pageNo = 0;
    Location loc;
    Pixmap* bitmap = nullptr;
};

struct ThumbnailRenderWorker {
    PageThumbnailsCache* cache = nullptr;
    EngineBase* sourceEngine = nullptr;
    Vec<int> pages;
    Vec<Location> locs;
    int rotation = 0;
    int thumbDx = 0;
    int thumbDy = 0;
};

static void FreeThumbnail(Pixmap* thumbnail) {
    if (thumbnail != kThumbnailRenderFailed) {
        FreePixmap(thumbnail);
    }
}

static Pixmap* ThumbnailToDraw(PageThumbnailsCache* cache, int idx) {
    Pixmap* thumbnail = cache->thumbnails[idx];
    return thumbnail == kThumbnailRenderFailed ? nullptr : thumbnail;
}

static void FreeThumbnailRenderEngine(PageThumbnailsCache* cache) {
    ReportIf(cache->workerRunning);
    if (cache->renderEngine) {
        cache->renderEngine->Release();
        cache->renderEngine = nullptr;
    }
}

static void DeleteThumbnailCache(PageThumbnailsCache* cache) {
    for (Pixmap* thumbnail : cache->thumbnails) {
        FreeThumbnail(thumbnail);
    }
    VecReset(cache->thumbnails);
    FreeThumbnailRenderEngine(cache);
    delete cache;
}

// the control is done with the cache; a running worker deletes it when it ends
static void DetachThumbnailCache(PageThumbnailsCache* cache) {
    if (!cache) {
        return;
    }
    cache->ctrl = nullptr;
    AtomicIntSet(&cache->cancelRendering, 1);
    if (cache->workerRunning) {
        cache->deleteWhenWorkerFinishes = true;
        return;
    }
    DeleteThumbnailCache(cache);
}

static Pixmap* RenderPageThumbnail(EngineBase* engine, int pageNo, Location loc, int rotation, int thumbDx,
                                   int thumbDy) {
    // reflow docs share one mediabox; don't use a flat pageNo that a clone's
    // chapter layout may have already shifted
    int boxPage = loc.IsValid() && engine->isReflowable ? 1 : pageNo;
    RectF pageRect = engine->PageMediabox(boxPage);
    if (pageRect.IsEmpty()) {
        return nullptr;
    }

    pageRect = engine->Transform(pageRect, boxPage, 1.0f, rotation);
    if (pageRect.dx <= 0 || pageRect.dy <= 0) {
        return nullptr;
    }
    float zoom = (float)thumbDx / pageRect.dx;
    pageRect.dy = std::min(pageRect.dy, (float)thumbDy / zoom);
    pageRect = engine->Transform(pageRect, boxPage, 1.0f, rotation, true);
    RenderPageArgs args(pageNo, zoom, rotation, &pageRect, RenderTarget::View);
    args.loc = loc;
    return engine->RenderPage(args);
}

static void FinishThumbnailRender(ThumbnailRenderTask* task) {
    PageThumbnailsCache* cache = task->cache;
    int idx = task->pageNo - 1;
    bool isValid = !cache->deleteWhenWorkerFinishes && idx >= 0 && idx < len(cache->thumbnails);
    if (isValid) {
        cache->stale[idx] = 0;
        if (task->bitmap) {
            FreeThumbnail(cache->thumbnails[idx]);
            cache->thumbnails[idx] = task->bitmap;
            task->bitmap = nullptr;
            if (cache->ctrl && cache->ctrl->active) {
                cache->ctrl->Invalidate();
            }
        } else if (!cache->thumbnails[idx]) {
            cache->thumbnails[idx] = kThumbnailRenderFailed;
        }
    }
    FreePixmap(task->bitmap);
    delete task;
}

static void FinishThumbnailWorker(PageThumbnailsCache* cache) {
    cache->workerRunning = false;
    if (cache->deleteWhenWorkerFinishes) {
        DeleteThumbnailCache(cache);
        return;
    }
    if (!cache->ctrl || !cache->ctrl->active) {
        FreeThumbnailRenderEngine(cache);
        return;
    }
    cache->ctrl->StartRendering();
}

static void RenderAndPostThumbnail(ThumbnailRenderWorker* worker, EngineBase* engine, int pageNo, Location loc) {
    auto* task = new ThumbnailRenderTask;
    task->cache = worker->cache;
    task->pageNo = pageNo;
    task->loc = loc;
    task->bitmap = RenderPageThumbnail(engine, pageNo, loc, worker->rotation, worker->thumbDx, worker->thumbDy);
    uitask::Post(MkFunc0<ThumbnailRenderTask>(FinishThumbnailRender, task));
}

static void RenderThumbnailsInBackground(ThumbnailRenderWorker* worker) {
    PageThumbnailsCache* cache = worker->cache;
    if (worker->sourceEngine) {
        if (cache->liveEngine) {
            // the reference taken for the worker becomes the cache's
            cache->renderEngine = worker->sourceEngine;
        } else {
            cache->renderEngine = worker->sourceEngine->Clone();
            worker->sourceEngine->Release();
        }
        worker->sourceEngine = nullptr;
    }
    EngineBase* engine = cache->renderEngine;
    if (engine) {
        int n = len(worker->pages);
        for (int i = 0; i < n; i++) {
            if (AtomicIntGet(&cache->cancelRendering) != 0) {
                break;
            }
            Location loc = i < len(worker->locs) ? worker->locs[i] : kInvalidLocation;
            RenderAndPostThumbnail(worker, engine, worker->pages[i], loc);
        }
    }
    uitask::Post(MkFunc0<PageThumbnailsCache>(FinishThumbnailWorker, cache));
    delete worker;
}

// Moving pages and dropping PDFs in edit the PDF: only in Edit PDF mode
static bool CanDropPages(PageThumbnailsCtrl* c) {
    return c->win && c->win->pdfAnnotationsToolbarEnabled && CanEditPagesInTab(c->tab);
}

// the tab's document, unless the window has moved on to another tab (which
// may have closed this one)
static DisplayModel* CurrentDoc(PageThumbnailsCtrl* c) {
    if (!c->tab || !c->win || c->win->CurrentTab() != c->tab) {
        return nullptr;
    }
    return c->tab->AsFixed();
}

PageThumbnailsCtrl::PageThumbnailsCtrl(MainWindow* win, PlatformFont* font, int dpi, ThumbnailsHost host) {
    this->host = host;
    this->win = win;
    this->font = font;
    this->dpi = dpi;
    // the palette's edit box keeps the keyboard focus; the sidebar takes it
    SetFlag(vwfFocusable, host == ThumbnailsHost::Sidebar);
    SetColor(kColListText, ThemeWindowTextColor());
    SetColor(kColListBg, ThemeWindowControlBackgroundColor());

    rowsModel = new ThumbnailRowsModel();

    onDrawItem = MkMethod1<PageThumbnailsCtrl, DrawItemEvent*, &PageThumbnailsCtrl::DrawRow>(this);
    VirtCtrl::onMouseDown = MkMethod1<PageThumbnailsCtrl, VirtMouseEvent*, &PageThumbnailsCtrl::OnThumbMouseDown>(this);
    VirtCtrl::onMouseMove = MkMethod1<PageThumbnailsCtrl, VirtMouseEvent*, &PageThumbnailsCtrl::OnThumbMouseMove>(this);
    VirtCtrl::onMouseUp = MkMethod1<PageThumbnailsCtrl, VirtMouseEvent*, &PageThumbnailsCtrl::OnThumbMouseUp>(this);
    VirtCtrl::onMouseWheel =
        MkMethod1<PageThumbnailsCtrl, VirtMouseEvent*, &PageThumbnailsCtrl::OnThumbMouseWheel>(this);
    VirtCtrl::onDoubleClick =
        MkMethod1<PageThumbnailsCtrl, VirtMouseEvent*, &PageThumbnailsCtrl::OnThumbDoubleClick>(this);
    VirtCtrl::onKeyDown = MkMethod1<PageThumbnailsCtrl, VirtKeyEvent*, &PageThumbnailsCtrl::OnThumbKeyDown>(this);
    VirtCtrl::onCaptureLost = MkMethod0<PageThumbnailsCtrl, &PageThumbnailsCtrl::OnThumbCaptureLost>(this);

    SetTab(win->CurrentTab());
}

PageThumbnailsCtrl::~PageThumbnailsCtrl() {
    DetachThumbnailCache(cache);
    cache = nullptr;
}

// (Re)start from the tab's document: its pages, the current page and the dpi.
// Rendered thumbnails are dropped; also called after the pages changed
void PageThumbnailsCtrl::SetTab(WindowTab* newTab) {
    tab = newTab;
    thumbDx = DpiScaleByDpi(dpi, kThumbnailDx);
    thumbDy = DpiScaleByDpi(dpi, kThumbnailDy);
    gap = DpiScaleByDpi(dpi, kThumbnailGap);
    rowGap = gap;
    itemDy = thumbDy + gap;
    int pad = host == ThumbnailsHost::Sidebar ? kThumbnailSidebarPadding : kThumbnailPadding;
    padding = DpiScaledInsets(pad, pad);

    dm = tab ? tab->AsFixed() : nullptr;
    pageCount = dm ? dm->PageCount() : 0;
    selectedPage = dm ? clampi(dm->CurrentPageNo(), 1, std::max(pageCount, 1)) : 1;
    VecReset(marked);
    VecAppendBlanks(marked, pageCount);
    anchorPage = 0;
    pressedPage = 0;
    dragging = false;
    dropBefore = 0;

    rowsModel->rows = (pageCount + cols - 1) / cols;
    SetModel(rowsModel);
    ResetCache();
    EnsureVisible((selectedPage - 1) / cols);
    Invalidate();
    if (active) {
        StartRendering();
    }
}

void PageThumbnailsCtrl::ResetCache() {
    DetachThumbnailCache(cache);
    cache = new PageThumbnailsCache();
    cache->ctrl = this;
    // the palette shows pages the way the view does; the sidebar the way they
    // are, which a view rotation doesn't change
    bool isPalette = host == ThumbnailsHost::Palette;
    cache->rotation = (dm && isPalette) ? dm->GetRotation() : 0;
    cache->liveEngine = !isPalette;
    cache->thumbDx = thumbDx;
    cache->thumbDy = thumbDy;
    VecAppendBlanks(cache->thumbnails, pageCount);
    VecAppendBlanks(cache->stale, pageCount);
}

// the document was re-rendered: so are the thumbnails, the old ones shown meanwhile
void PageThumbnailsCtrl::Refresh() {
    for (int i = 0; i < len(cache->thumbnails); i++) {
        cache->stale[i] = cache->thumbnails[i] ? 1 : 0;
    }
    StartRendering();
}

void PageThumbnailsCtrl::SetBounds(Rect r) {
    int reservedScrollbarDx = DpiScaleByDpi(dpi, 10);
    int availableDx = r.dx - padding.left - padding.right - reservedScrollbarDx;
    int newCols = (availableDx + gap) / (thumbDx + gap);
    newCols = clampi(newCols, 1, kThumbnailMaxCols);
    if (newCols != cols) {
        cols = newCols;
        rowsModel->rows = (pageCount + cols - 1) / cols;
        SetModel(rowsModel);
    }

    // the palette spreads the rows over its height; the sidebar scrolls
    rowGap = gap;
    int visibleRows = std::max(1, (r.dy - gap) / (thumbDy + gap));
    visibleRows = std::min(visibleRows, rowsModel->rows);
    if (host == ThumbnailsHost::Palette && visibleRows > 0) {
        int freeDy = r.dy - (visibleRows * thumbDy);
        rowGap = std::max(gap, freeDy / (visibleRows + 1));
    }
    itemDy = thumbDy + rowGap;
    padding.top = rowGap;
    padding.bottom = 0;

    VirtListBox::SetBounds(r);
    EnsureVisible((selectedPage - 1) / cols);
    if (active) {
        StartRendering();
    }
}

// the thumbnail of pageNo in window coords; empty when its row isn't visible
Rect PageThumbnailsCtrl::PageRect(int pageNo) {
    if (pageNo < 1 || pageNo > pageCount) {
        return {};
    }
    Rect rowRect = ItemRect((pageNo - 1) / cols);
    if (rowRect.IsEmpty()) {
        return {};
    }
    int gridDx = (cols * thumbDx) + ((cols - 1) * gap);
    int left = rowRect.x + std::max(0, (rowRect.dx - gridDx) / 2);
    int col = (pageNo - 1) % cols;
    return {left + (col * (thumbDx + gap)), rowRect.y, thumbDx, thumbDy};
}

void PageThumbnailsCtrl::DrawRow(DrawItemEvent* ev) {
    int gridDx = (cols * thumbDx) + ((cols - 1) * gap);
    int left = ev->itemRect.x + std::max(0, (ev->itemRect.dx - gridDx) / 2);
    int firstPage = (ev->itemIndex * cols) + 1;
    int lastPage = std::min(pageCount, firstPage + cols - 1);
    DisplayModel* dm = CurrentDoc(this);
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    bool chapters = ShowChapterUi(dm);
    Color colMarked = AccentColor(GetColor(kColListBg), HasFlag(vwfFocused) ? 45 : 25);
    for (int pageNo = firstPage; pageNo <= lastPage; pageNo++) {
        int col = pageNo - firstPage;
        int x = left + (col * (thumbDx + gap));
        Rect pageRect{x, ev->itemRect.y, thumbDx, thumbDy};
        // the current page's blue frame marks it: no selection border too
        bool isCurrent = pageNo == selectedPage;
        if (host == ThumbnailsHost::Sidebar && marked[pageNo - 1] && !isCurrent) {
            Rect r = pageRect;
            int d = gap / 3;
            r.Inflate(d, d);
            ev->gfx->FillRect(r, colMarked);
        }
        ev->gfx->FillRect(pageRect, kColWhite);

        Pixmap* thumbnail = ThumbnailToDraw(cache, pageNo - 1);
        if (thumbnail) {
            int drawDx = std::min(thumbnail->width, thumbDx);
            int drawDy = std::min(thumbnail->height, thumbDy);
            Rect target{x + ((thumbDx - drawDx) / 2), pageRect.y + ((thumbDy - drawDy) / 2), drawDx, drawDy};
            ev->gfx->DrawPixmap(thumbnail, target);
        }

        if (isCurrent) {
            ev->gfx->DrawRect(pageRect, kCurrentPageColor, 3);
        }

        TempStr label = fmt("%d", pageNo);
        if (chapters) {
            Location loc = engine->LocationFromPageNo(pageNo);
            if (loc.IsValid()) {
                label = fmt("%d / %d", loc.chapter, loc.page);
            }
        }
        Size ts = ev->gfx->MeasureText(label, font);
        int padX = DpiScaleByDpi(dpi, 6);
        int padY = DpiScaleByDpi(dpi, 2);
        int inset = DpiScaleByDpi(dpi, 4);
        int boxDx = std::min(ts.dx + (padX * 2), pageRect.dx - (inset * 2));
        int boxDy = ts.dy + (padY * 2);
        int boxX = pageRect.x + ((pageRect.dx - boxDx) / 2);
        int boxY = pageRect.y + pageRect.dy - boxDy - inset;
        if (boxY < pageRect.y + inset) {
            boxY = pageRect.y + inset;
        }
        Rect box{boxX, boxY, boxDx, boxDy};
        // a pill: half the height rounds the short sides into semicircles
        ev->gfx->FillRoundedRect(box, boxDy / 2, ThemeWindowBackgroundColor());
        ev->gfx->DrawText(label, box, gfxTextCenter | gfxTextVCenter, font, ThemeWindowTextColor());
    }
}

// the rows, then where dragged pages or files would go: a bar in the gap in
// front of dropBefore (or after the last page)
void PageThumbnailsCtrl::Paint(VirtPaintCtx& ctx) {
    VirtListBox::Paint(ctx);
    if (dropBefore <= 0 || pageCount <= 0) {
        return;
    }
    bool atEnd = dropBefore > pageCount;
    Rect r = PageRect(atEnd ? pageCount : dropBefore);
    if (r.IsEmpty()) {
        return;
    }
    int barDx = DpiScaleByDpi(dpi, kDropMarkerDx);
    Rect bar;
    if (cols == 1) {
        int y = atEnd ? r.y + thumbDy + (rowGap / 2) : r.y - (rowGap / 2);
        bar = {r.x, y - (barDx / 2), thumbDx, barDx};
    } else {
        int x = atEnd ? r.x + thumbDx + (gap / 2) : r.x - (gap / 2);
        bar = {x - (barDx / 2), r.y, barDx, thumbDy};
    }
    ctx.gfx->PushClip(ctx.clip.Intersect(ctx.bounds));
    ctx.gfx->FillRect(bar, kCurrentPageColor);
    ctx.gfx->PopClip();
}

int PageThumbnailsCtrl::PageAtPoint(Point pt) {
    int row = ItemFromPoint(pt);
    if (row < 0) {
        return -1;
    }
    Rect rowRect = ItemRect(row);
    if (rowRect.IsEmpty()) {
        return -1;
    }
    Point origin = OriginInWindow();
    rowRect.Offset(-origin.x, -origin.y);
    int gridDx = (cols * thumbDx) + ((cols - 1) * gap);
    int left = rowRect.x + std::max(0, (rowRect.dx - gridDx) / 2);
    if (pt.x < left || pt.y < rowRect.y || pt.y >= rowRect.y + thumbDy) {
        return -1;
    }
    int col = (pt.x - left) / (thumbDx + gap);
    if (col < 0 || col >= cols) {
        return -1;
    }
    int cellX = left + (col * (thumbDx + gap));
    if (pt.x >= cellX + thumbDx) {
        return -1;
    }
    int pageNo = (row * cols) + col + 1;
    return pageNo <= pageCount ? pageNo : -1;
}

// Where pages dropped at ptLocal go: in front of the returned page, at the
// gap nearest to the point (pageCount + 1: after the last). 0 when outside.
int PageThumbnailsCtrl::DropPosition(Point pt) {
    if (pageCount <= 0 || pt.x < 0 || pt.y < 0 || pt.x >= bounds.dx || pt.y >= bounds.dy) {
        return 0;
    }
    int row = (pt.y - padding.top + scrollY) / itemDy;
    int rows = rowsModel->rows;
    if (pt.y - padding.top + scrollY < 0) {
        row = 0;
    }
    if (row >= rows) {
        return pageCount + 1;
    }
    int step = cols == 1 ? itemDy : thumbDx + gap;
    int pos = cols == 1 ? pt.y - padding.top + scrollY - (row * itemDy) : pt.x;
    int slot = 0;
    if (cols == 1) {
        // the upper half of a page is in front of it, the lower half after it
        slot = pos < thumbDy / 2 ? 0 : 1;
    } else {
        Rect r = PageRect((row * cols) + 1);
        if (r.IsEmpty()) {
            return 0;
        }
        int left = r.x - OriginInWindow().x;
        slot = clampi((pos - left + (gap / 2) + (step / 2)) / step, 0, cols);
    }
    return std::min((row * cols) + slot + 1, pageCount + 1);
}

void PageThumbnailsCtrl::SetDropPosition(int before) {
    if (before == dropBefore) {
        return;
    }
    dropBefore = before;
    Invalidate();
}

void PageThumbnailsCtrl::MarkedPages(Vec<int>& out) {
    for (int i = 0; i < len(marked); i++) {
        if (marked[i]) {
            VecAppend(out, i + 1);
        }
    }
}

void PageThumbnailsCtrl::SelectPage(int pageNo) {
    if (pageCount <= 0) {
        return;
    }
    pageNo = clampi(pageNo, 1, pageCount);
    if (pageNo == selectedPage) {
        return;
    }
    selectedPage = pageNo;
    EnsureVisible((selectedPage - 1) / cols);
    Invalidate();
    StartRendering();
}

// the document's current page changed: the sidebar follows it
void PageThumbnailsCtrl::SetCurrentPage(int pageNo) {
    if (pageCount <= 0 || pageNo == selectedPage) {
        return;
    }
    SelectPage(pageNo);
}

void PageThumbnailsCtrl::OpenSelectedPage() {
    DisplayModel* dm = CurrentDoc(this);
    if (!dm) {
        return;
    }
    dm->GoToPage(selectedPage, 0, true);
    onPageOpened.Call();
}

void PageThumbnailsCtrl::MarkOnly(int pageNo) {
    for (u8& m : marked) {
        m = 0;
    }
    marked[pageNo - 1] = 1;
    anchorPage = pageNo;
    Invalidate();
}

// sidebar: Ctrl adds or removes a page, Shift selects from the anchor to it
void PageThumbnailsCtrl::ClickPage(int pageNo, bool ctrl, bool shift) {
    if (!shift) {
        if (!ctrl) {
            MarkOnly(pageNo);
            return;
        }
        marked[pageNo - 1] = !marked[pageNo - 1];
        anchorPage = pageNo;
        Invalidate();
        return;
    }
    int from = anchorPage > 0 ? anchorPage : selectedPage;
    if (!ctrl) {
        for (u8& m : marked) {
            m = 0;
        }
    }
    for (int p = std::min(from, pageNo); p <= std::max(from, pageNo); p++) {
        marked[p - 1] = 1;
    }
    Invalidate();
}

void PageThumbnailsCtrl::EndPress() {
    pressedPage = 0;
    dragging = false;
    SetDropPosition(0);
    if (root && root->captured == this) {
        root->ReleaseCapture();
    }
}

// the selected pages go together in front of dropBefore and stay selected
void PageThumbnailsCtrl::DropMarkedPages() {
    Vec<int> pages;
    MarkedPages(pages);
    int before = dropBefore;
    if (len(pages) == 0 || before <= 0 || !MovePagesInTab(tab, pages, before)) {
        return;
    }
    // the move called SetTab(), which cleared the selection
    int nInFront = 0;
    for (int p : pages) {
        if (p < before) {
            nInFront++;
        }
    }
    int first = before - nInFront;
    for (int i = 0; i < len(pages) && first - 1 + i < len(marked); i++) {
        marked[first - 1 + i] = 1;
    }
    anchorPage = first;
    Invalidate();
}

void PageThumbnailsCtrl::OnThumbMouseDown(VirtMouseEvent* ev) {
    int pageNo = PageAtPoint(ev->pt);
    if (pageNo > 0 && host == ThumbnailsHost::Palette) {
        SelectPage(pageNo);
        ev->didHandle = true;
        return;
    }
    if (pageNo > 0 && ev->button == 0) {
        // a plain click on a selected page may start dragging all of them, so
        // only on release does it select just that page
        if (ev->isCtrl || ev->isShift) {
            ClickPage(pageNo, ev->isCtrl, ev->isShift);
        } else {
            if (!marked[pageNo - 1]) {
                MarkOnly(pageNo);
            }
            SelectPage(pageNo);
            OpenSelectedPage();
        }
        pressedPage = pageNo;
        pressPt = ev->ptWindow;
        if (root) {
            root->SetCapture(this);
        }
        ev->didHandle = true;
        return;
    }
    if (pageNo > 0) {
        ev->didHandle = true;
        return;
    }
    int oldScrollY = scrollY;
    VirtListBox::OnMouseDown(ev);
    if (scrollY != oldScrollY) {
        StartRendering();
    }
}

void PageThumbnailsCtrl::OnThumbMouseMove(VirtMouseEvent* ev) {
    int oldScrollY = scrollY;
    VirtListBox::OnMouseMove(ev);
    if (scrollY != oldScrollY) {
        StartRendering();
    }
    if (ev->didHandle) {
        return;
    }
    if (host == ThumbnailsHost::Palette) {
        int pageNo = PageAtPoint(ev->pt);
        if (pageNo > 0) {
            SelectPage(pageNo);
            ev->didHandle = true;
        }
        return;
    }
    if (pressedPage <= 0) {
        return;
    }
    ev->didHandle = true;
    // captured: ev->pt is in window coords
    Point origin = OriginInWindow();
    Point pt{ev->ptWindow.x - origin.x, ev->ptWindow.y - origin.y};
    if (!dragging) {
        int dx = std::abs(ev->ptWindow.x - pressPt.x);
        int dy = std::abs(ev->ptWindow.y - pressPt.y);
        bool moved = dx > GetSystemMetrics(SM_CXDRAG) || dy > GetSystemMetrics(SM_CYDRAG);
        if (!moved || !CanDropPages(this)) {
            return;
        }
        dragging = true;
        if (!marked[pressedPage - 1]) {
            MarkOnly(pressedPage);
        }
    }
    // near the top or bottom edge the list scrolls towards it
    int edge = thumbDy / 4;
    int scrollDy = 0;
    if (pt.y < edge) {
        scrollDy = -edge / 2;
    } else if (pt.y > bounds.dy - edge) {
        scrollDy = edge / 2;
    }
    if (scrollDy != 0 && ScrollBy(scrollDy)) {
        StartRendering();
    }
    SetDropPosition(DropPosition(pt));
}

void PageThumbnailsCtrl::OnThumbMouseUp(VirtMouseEvent* ev) {
    VirtListBox::OnMouseUp(ev);
    if (host == ThumbnailsHost::Palette || pressedPage <= 0) {
        return;
    }
    int pageNo = pressedPage;
    bool wasDragging = dragging;
    bool modifiers = ev->isCtrl || ev->isShift;
    if (wasDragging) {
        DropMarkedPages();
    }
    EndPress();
    if (!wasDragging && !modifiers && pageNo <= pageCount) {
        MarkOnly(pageNo);
    }
}

void PageThumbnailsCtrl::OnThumbCaptureLost() {
    VirtListBox::OnCaptureLost();
    pressedPage = 0;
    dragging = false;
    SetDropPosition(0);
}

// Scrolls in proportion to the delta, 3 thumbnails a notch, so a touchpad's
// small deltas scroll too. The wheel is ours even at the list's ends: unhandled,
// it would scroll the document
void PageThumbnailsCtrl::OnThumbMouseWheel(VirtMouseEvent* ev) {
    ev->didHandle = true;
    int dy = -(ev->wheelDelta * 3 * GetItemHeight()) / WHEEL_DELTA;
    if (ScrollBy(dy)) {
        StartRendering();
    }
}

void PageThumbnailsCtrl::OnThumbDoubleClick(VirtMouseEvent* ev) {
    int pageNo = PageAtPoint(ev->pt);
    if (pageNo <= 0) {
        return;
    }
    ev->didHandle = true;
    if (host == ThumbnailsHost::Sidebar) {
        return;
    }
    SelectPage(pageNo);
    OpenSelectedPage();
}

// the page a navigation key moves to, 0 for other keys
static int PageForKey(PageThumbnailsCtrl* c, int vkey) {
    int pageNo = c->selectedPage;
    int pageStep = std::max(1, c->UsableDy() / c->itemDy) * c->cols;
    switch (vkey) {
        case VK_LEFT:
            pageNo--;
            break;
        case VK_RIGHT:
            pageNo++;
            break;
        case VK_UP:
            pageNo -= c->cols;
            break;
        case VK_DOWN:
            pageNo += c->cols;
            break;
        case VK_PRIOR:
            pageNo -= pageStep;
            break;
        case VK_NEXT:
            pageNo += pageStep;
            break;
        case VK_HOME:
            pageNo = 1;
            break;
        case VK_END:
            pageNo = c->pageCount;
            break;
        default:
            return 0;
    }
    return clampi(pageNo, 1, c->pageCount);
}

void PageThumbnailsCtrl::HandleKey(int vkey) {
    if (pageCount <= 0) {
        return;
    }
    if (vkey == VK_RETURN) {
        OpenSelectedPage();
        return;
    }
    int pageNo = PageForKey(this, vkey);
    if (pageNo > 0) {
        SelectPage(pageNo);
    }
}

// sidebar: Up / Down go to a page, with Shift they select up to it; Esc
// cancels a drag. The sidebar sends every other key to the canvas
void PageThumbnailsCtrl::OnThumbKeyDown(VirtKeyEvent* ev) {
    if (host == ThumbnailsHost::Palette || pageCount <= 0) {
        return;
    }
    if (ev->vkey == VK_ESCAPE && dragging) {
        EndPress();
        ev->didHandle = true;
        return;
    }
    if (ev->vkey != VK_UP && ev->vkey != VK_DOWN) {
        return;
    }
    int pageNo = PageForKey(this, ev->vkey);
    if (ev->isShift) {
        ClickPage(pageNo, false, true);
    } else {
        MarkOnly(pageNo);
    }
    SelectPage(pageNo);
    OpenSelectedPage();
    ev->didHandle = true;
}

void PageThumbnailsCtrl::Activate() {
    if (active) {
        return;
    }
    active = true;
    AtomicIntSet(&cache->cancelRendering, 0);
    EnsureVisible((selectedPage - 1) / cols);
    StartRendering();
    Invalidate();
}

void PageThumbnailsCtrl::Deactivate() {
    if (!active) {
        return;
    }
    active = false;
    AtomicIntSet(&cache->cancelRendering, 1);
    if (!cache->workerRunning) {
        FreeThumbnailRenderEngine(cache);
    }
}

int PageThumbnailsCtrl::RenderedCount() const {
    int n = 0;
    for (Pixmap* thumbnail : cache->thumbnails) {
        if (thumbnail && thumbnail != kThumbnailRenderFailed) {
            n++;
        }
    }
    return n;
}

void PageThumbnailsCtrl::StartRendering() {
    if (!active || cache->workerRunning || pageCount <= 0) {
        return;
    }
    DisplayModel* dm = CurrentDoc(this);
    if (!dm || dm->PageCount() != pageCount) {
        return;
    }
    if (host == ThumbnailsHost::Palette && dm->GetRotation() != cache->rotation) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return;
    }

    int visibleRows = std::max(1, UsableDy() / itemDy);
    int firstVisible = ((scrollY / itemDy) * cols) + 1;
    int perScreen = std::max(1, visibleRows * cols);
    int lastVisible = std::min(pageCount, firstVisible + perScreen - 1);
    int firstPage = std::max(1, firstVisible - (kThumbnailRenderScreens * perScreen));
    int lastPage = std::min(pageCount, lastVisible + (kThumbnailRenderScreens * perScreen));

    int keepFirst = std::max(1, firstPage - (kThumbnailKeepScreens * perScreen));
    int keepLast = std::min(pageCount, lastPage + (kThumbnailKeepScreens * perScreen));
    for (int idx = 0; idx < len(cache->thumbnails); idx++) {
        int pageNo = idx + 1;
        if (cache->thumbnails[idx] && (pageNo < keepFirst || pageNo > keepLast)) {
            FreeThumbnail(cache->thumbnails[idx]);
            cache->thumbnails[idx] = nullptr;
            cache->stale[idx] = 0;
        }
    }

    auto* worker = new ThumbnailRenderWorker;
    auto queuePage = [&](int pageNo) {
        if (cache->thumbnails[pageNo - 1] && !cache->stale[pageNo - 1]) {
            return;
        }
        VecAppend(worker->pages, pageNo);
        VecAppend(worker->locs, engine->LocationFromPageNo(pageNo));
    };
    for (int pageNo = firstVisible; pageNo <= lastVisible; pageNo++) {
        queuePage(pageNo);
    }
    for (int pageNo = firstPage; pageNo < firstVisible; pageNo++) {
        queuePage(pageNo);
    }
    for (int pageNo = lastVisible + 1; pageNo <= lastPage; pageNo++) {
        queuePage(pageNo);
    }
    if (len(worker->pages) == 0) {
        delete worker;
        return;
    }

    worker->cache = cache;
    worker->rotation = cache->rotation;
    worker->thumbDx = cache->thumbDx;
    worker->thumbDy = cache->thumbDy;
    if (!cache->renderEngine) {
        worker->sourceEngine = engine;
        engine->AddRef();
    }
    AtomicIntSet(&cache->cancelRendering, 0);
    cache->workerRunning = true;
    RunAsync(MkFunc0<ThumbnailRenderWorker>(RenderThumbnailsInBackground, worker), StrL("PageThumbnailsRender"));
}

//--- dropping PDF files on the sidebar's thumbnails

static bool IsPdfPath(Str path) {
    return str::EndsWithI(path, StrL(".pdf"));
}

static void PdfPathsFromDataObject(IDataObject* dataObj, StrVec& out) {
    FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    if (FAILED(dataObj->GetData(&fmt, &medium)) || !medium.hGlobal) {
        return;
    }
    HDROP hDrop = (HDROP)medium.hGlobal;
    int nFiles = DragQueryFileW(hDrop, DRAGQUERY_NUMFILES, nullptr, 0);
    WCHAR pathW[MAX_PATH]{};
    for (int i = 0; i < nFiles; i++) {
        DragQueryFileW(hDrop, i, pathW, dimof(pathW));
        TempStr path = ToUtf8Temp(pathW);
        if (IsPdfPath(path)) {
            out.Append(path);
        }
    }
    ReleaseStgMedium(&medium);
}

struct InsertDroppedPdfs {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    StrVec paths;
    int before = 0;
};

// after Drop() returned, so the drag source isn't kept waiting
static void InsertDroppedPdfsNow(InsertDroppedPdfs* d) {
    if (IsMainWindowValidAndNotClosing(d->win) && d->win->CurrentTab() == d->tab) {
        int before = d->before;
        for (Str path : d->paths) {
            before += InsertPdfInTab(d->tab, path, before);
        }
    }
    delete d;
}

class ThumbnailsDropTarget : public IDropTarget {
    AtomicInt refCount = 1;
    HWND hwnd = nullptr;
    PageThumbnailsCtrl* ctrl = nullptr;
    bool hasPdf = false;

    int PositionAt(POINTL ptScreen) {
        // either sidebar panel may host the thumbnails: only that one takes the drop
        bool hosts = ctrl->GetHwnd() == hwnd;
        if (!hasPdf || !hosts || !ctrl->IsVisible() || !CanDropPages(ctrl)) {
            return 0;
        }
        POINT p{ptScreen.x, ptScreen.y};
        ScreenToClient(hwnd, &p);
        Point pt{p.x, p.y};
        UnmirrorRtl(hwnd, pt);
        Point origin = ctrl->OriginInWindow();
        return ctrl->DropPosition({pt.x - origin.x, pt.y - origin.y});
    }

  public:
    ThumbnailsDropTarget(PageThumbnailsCtrl* c, HWND h) : hwnd(h), ctrl(c) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AtomicIntInc(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }

    STDMETHODIMP DragEnter(IDataObject* dataObj, DWORD keys, POINTL pt, DWORD* pdwEffect) override {
        StrVec paths;
        PdfPathsFromDataObject(dataObj, paths);
        hasPdf = len(paths) > 0;
        return DragOver(keys, pt, pdwEffect);
    }

    STDMETHODIMP DragOver(DWORD /*keys*/, POINTL pt, DWORD* pdwEffect) override {
        int before = PositionAt(pt);
        ctrl->SetDropPosition(before);
        *pdwEffect = before > 0 ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }

    STDMETHODIMP DragLeave() override {
        ctrl->SetDropPosition(0);
        return S_OK;
    }

    STDMETHODIMP Drop(IDataObject* dataObj, DWORD /*keys*/, POINTL pt, DWORD* pdwEffect) override {
        int before = PositionAt(pt);
        ctrl->SetDropPosition(0);
        *pdwEffect = DROPEFFECT_NONE;
        if (before <= 0) {
            return S_OK;
        }
        auto* d = new InsertDroppedPdfs();
        PdfPathsFromDataObject(dataObj, d->paths);
        d->win = ctrl->win;
        d->tab = ctrl->tab;
        d->before = before;
        uitask::Post(MkFunc0<InsertDroppedPdfs>(InsertDroppedPdfsNow, d));
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
};

void RegisterThumbnailsDropTarget(PageThumbnailsCtrl* ctrl, HWND hwnd) {
    auto* dt = new ThumbnailsDropTarget(ctrl, hwnd);
    RegisterDragDrop(hwnd, dt);
    dt->Release(); // RegisterDragDrop AddRef'd it
}

void RevokeThumbnailsDropTarget(HWND hwnd) {
    RevokeDragDrop(hwnd);
}
