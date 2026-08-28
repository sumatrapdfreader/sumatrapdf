/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "gui/PlatformFont.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "Theme.h"
#include "Translations.h"
#include "WindowTab.h"
#include "ThumbnailNavigation.h"

constexpr int kThumbnailDx = 120;
constexpr int kThumbnailDy = 170;
constexpr int kThumbnailNavigationHeaderDy = 36;
constexpr int kThumbnailNavigationGap = 16;
constexpr int kThumbnailNavigationPadding = 24;
constexpr int kThumbnailNavigationMaxCols = 6;

// Rendering and keeping a thumbnail for every page would cost minutes of CPU
// and hundreds of MB on a large document, for pages the user never scrolls to.
// Only pages within this many screenfuls of the visible rows are rendered, and
// thumbnails further out than the keep window are dropped again.
constexpr int kThumbnailRenderScreens = 1;
constexpr int kThumbnailKeepScreens = 2;

// stored in ThumbnailNavigationCache::thumbnails for a page the engine failed
// to render, so that following render passes don't keep retrying it
static Pixmap* const kThumbnailFailedToRender = (Pixmap*)(intptr_t)-1;

struct ThumbnailNavigationCache {
    // null for a page not rendered yet, kThumbnailFailedToRender for one that
    // can't be rendered
    Vec<Pixmap*> thumbnails;
    HWND hwnd = nullptr;
    // the engine clone the render passes draw from, kept for as long as the
    // grid is on screen. Cloning per pass re-reads the whole document (for a
    // .cbr that's the archive directory) and starts over on the per-page state
    // the previous pass built up. Only ever touched by the render thread while
    // workerRunning, and only by the ui thread while it isn't
    EngineBase* renderEngine = nullptr;
    AtomicInt cancelRendering = 0;
    int pageCount = 0;
    // what the cached thumbnails were rendered for. A mismatch (document
    // reloaded, rotated, DPI changed) means they don't describe this document
    // any more and the cache has to be thrown away
    int rotation = 0;
    int thumbDx = 0;
    int thumbDy = 0;
    bool workerRunning = false;
    bool deleteWhenWorkerFinishes = false;
};

struct ThumbnailNavigationState {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    HWND hwnd = nullptr;
    ThumbnailNavigationCache* cache = nullptr;
    HFONT font = nullptr;
    int pageCount = 0;
    int selectedPage = 1;
    int firstRow = 0;
    int cols = 1;
    int visibleRows = 1;
    int thumbDx = 0;
    int thumbDy = 0;
    int headerDy = 0;
    int gap = 0;
    int padding = 0;
    int maxScrollY = 0;
    OverlayScrollbar* scrollbar = nullptr;
};

static const WCHAR* kThumbnailNavigationClassName = L"SumatraPDF_ThumbnailNavigation";

static void StartThumbnailRendering(ThumbnailNavigationState* state);

static void FreeThumbnail(Pixmap* thumbnail) {
    if (thumbnail != kThumbnailFailedToRender) {
        FreePixmap(thumbnail);
    }
}

// the thumbnail to draw for a page, null if there isn't one to draw
static Pixmap* ThumbnailToDraw(ThumbnailNavigationCache* cache, int idx) {
    Pixmap* thumbnail = cache->thumbnails[idx];
    return thumbnail == kThumbnailFailedToRender ? nullptr : thumbnail;
}

// the clone holds the document open (and its own decoded-page cache), so it's
// only worth keeping while the grid is on screen. Must not be called while a
// render pass is running: that's the thread using it
static void FreeThumbnailRenderEngine(ThumbnailNavigationCache* cache) {
    ReportIf(cache->workerRunning);
    if (cache->renderEngine) {
        cache->renderEngine->Release();
        cache->renderEngine = nullptr;
    }
}

static void DeleteThumbnailNavigationCache(ThumbnailNavigationCache* cache) {
    for (Pixmap* thumbnail : cache->thumbnails) {
        FreeThumbnail(thumbnail);
    }
    VecReset(cache->thumbnails);
    FreeThumbnailRenderEngine(cache);
    delete cache;
}

struct ThumbnailRenderTask {
    ThumbnailNavigationCache* cache = nullptr;
    int pageNo = 0;
    Pixmap* bitmap = nullptr;
};

struct ThumbnailRenderWorker {
    ThumbnailNavigationCache* cache = nullptr;
    // engine to clone into cache->renderEngine, null once we have that clone
    EngineBase* sourceEngine = nullptr;
    // pages to render, most useful first. Picked on the ui thread: the worker
    // must not look at cache->thumbnails, which the ui thread owns
    Vec<int> pages;
    int rotation = 0;
    int thumbDx = 0;
    int thumbDy = 0;
};

static Pixmap* RenderPageThumbnail(EngineBase* engine, int pageNo, int rotation, int thumbDx, int thumbDy) {
    RectF pageRect = engine->PageMediabox(pageNo);
    if (pageRect.IsEmpty()) {
        return nullptr;
    }

    // fit and crop in the rotated space the page is rendered in, otherwise a
    // rotated page yields a pixmap with swapped dimensions that BlitPixmap
    // then squashes non-uniformly into the cell
    pageRect = engine->Transform(pageRect, pageNo, 1.0f, rotation);
    if (pageRect.dx <= 0 || pageRect.dy <= 0) {
        return nullptr;
    }
    float zoom = (float)thumbDx / pageRect.dx;
    pageRect.dy = std::min(pageRect.dy, (float)thumbDy / zoom);
    pageRect = engine->Transform(pageRect, pageNo, 1.0f, rotation, true);
    RenderPageArgs args(pageNo, zoom, rotation, &pageRect, RenderTarget::View);
    return engine->RenderPage(args);
}

static void FinishThumbnailRender(ThumbnailRenderTask* task) {
    ThumbnailNavigationCache* cache = task->cache;
    int idx = task->pageNo - 1;
    bool isValid = !cache->deleteWhenWorkerFinishes && idx >= 0 && idx < cache->thumbnails.len;
    if (isValid) {
        if (task->bitmap) {
            FreeThumbnail(cache->thumbnails[idx]);
            cache->thumbnails[idx] = task->bitmap;
            task->bitmap = nullptr;
            if (cache->hwnd) {
                InvalidateRect(cache->hwnd, nullptr, FALSE);
            }
        } else if (!cache->thumbnails[idx]) {
            cache->thumbnails[idx] = kThumbnailFailedToRender;
        }
    }
    FreePixmap(task->bitmap);
    delete task;
}

static void FinishThumbnailWorker(ThumbnailNavigationCache* cache) {
    cache->workerRunning = false;
    if (cache->deleteWhenWorkerFinishes) {
        DeleteThumbnailNavigationCache(cache);
        return;
    }
    if (!cache->hwnd) {
        // the grid was closed while this pass ran, so WM_NCDESTROY couldn't
        // drop the clone (we were still using it)
        FreeThumbnailRenderEngine(cache);
        return;
    }
    // the user may have scrolled to pages this pass didn't cover
    auto* state = (ThumbnailNavigationState*)GetWindowLongPtrW(cache->hwnd, GWLP_USERDATA);
    if (state) {
        StartThumbnailRendering(state);
    }
}

static void RenderAndPostThumbnail(ThumbnailRenderWorker* worker, EngineBase* engine, int pageNo) {
    auto* task = new ThumbnailRenderTask;
    task->cache = worker->cache;
    task->pageNo = pageNo;
    task->bitmap = RenderPageThumbnail(engine, pageNo, worker->rotation, worker->thumbDx, worker->thumbDy);
    uitask::Post(MkFunc0<ThumbnailRenderTask>(FinishThumbnailRender, task));
}

static bool ShouldCancelThumbnailRendering(ThumbnailRenderWorker* worker) {
    return AtomicIntGet(&worker->cache->cancelRendering) != 0;
}

static void RenderThumbnailsInBackground(ThumbnailRenderWorker* worker) {
    ThumbnailNavigationCache* cache = worker->cache;
    if (worker->sourceEngine) {
        // first pass since the grid opened; later ones reuse this clone
        cache->renderEngine = worker->sourceEngine->Clone();
        worker->sourceEngine->Release();
        worker->sourceEngine = nullptr;
    }
    EngineBase* engine = cache->renderEngine;
    if (engine) {
        for (int pageNo : worker->pages) {
            if (ShouldCancelThumbnailRendering(worker)) {
                break;
            }
            RenderAndPostThumbnail(worker, engine, pageNo);
        }
    }
    uitask::Post(MkFunc0<ThumbnailNavigationCache>(FinishThumbnailWorker, cache));
    delete worker;
}

void FreeThumbnailNavigationCache(WindowTab* tab) {
    if (!tab || !tab->thumbnailNavigationCache) {
        return;
    }
    ThumbnailNavigationCache* cache = tab->thumbnailNavigationCache;
    tab->thumbnailNavigationCache = nullptr;
    AtomicIntSet(&cache->cancelRendering, 1);
    if (cache->hwnd) {
        DestroyWindow(cache->hwnd);
    }
    if (cache->workerRunning) {
        cache->deleteWhenWorkerFinishes = true;
        return;
    }
    DeleteThumbnailNavigationCache(cache);
}

static int ThumbnailTotalRows(ThumbnailNavigationState* state) {
    return (state->pageCount + state->cols - 1) / state->cols;
}

// scrolls to row, clamped. updateScrollbar is false when the caller already
// wrote the position back to the scrollbar (i.e. when handling WM_VSCROLL)
static void SetThumbnailFirstRow(ThumbnailNavigationState* state, int row, bool updateScrollbar) {
    int maxRow = std::max(0, ThumbnailTotalRows(state) - state->visibleRows);
    row = std::max(0, std::min(row, maxRow));
    if (row == state->firstRow) {
        return;
    }
    state->firstRow = row;
    if (state->scrollbar && updateScrollbar) {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        si.nPos = row * (state->thumbDy + state->gap);
        OverlayScrollbarSetInfo(state->scrollbar, &si, true);
    }
    InvalidateRect(state->hwnd, nullptr, FALSE);
    StartThumbnailRendering(state);
}

static void SelectThumbnailPage(ThumbnailNavigationState* state, int pageNo) {
    if (pageNo < 1 || pageNo > state->pageCount) {
        return;
    }
    if (pageNo == state->selectedPage) {
        // happens on every WM_MOUSEMOVE within a thumbnail, don't repaint
        return;
    }
    state->selectedPage = pageNo;
    int row = (pageNo - 1) / state->cols;
    int firstRow = state->firstRow;
    if (row < firstRow) {
        firstRow = row;
    } else if (row >= firstRow + state->visibleRows) {
        firstRow = row - state->visibleRows + 1;
    }
    SetThumbnailFirstRow(state, firstRow, true);
    InvalidateRect(state->hwnd, nullptr, FALSE);
}

static void ActivateThumbnailPage(ThumbnailNavigationState* state) {
    if (!state || !state->win || !state->tab || !state->tab->AsFixed()) {
        return;
    }
    if (state->win->CurrentTab() != state->tab) {
        // the tab was switched while we were up: navigating now would move a
        // document the user isn't looking at
        DestroyWindow(state->hwnd);
        return;
    }
    DisplayModel* dm = state->tab->AsFixed();
    int pageNo = state->selectedPage;
    HWND hwndFrame = state->win->hwndFrame;
    DestroyWindow(state->hwnd);
    dm->GoToPage(pageNo, 0, true);
    SetForegroundWindow(hwndFrame);
    SetFocus(hwndFrame);
}

static int PageAtPoint(ThumbnailNavigationState* state, int x, int y) {
    int cellDy = state->thumbDy + state->gap;
    int left = state->padding;
    int top = state->headerDy + state->padding;
    if (x < left || y < top) {
        return -1;
    }
    int col = (x - left) / (state->thumbDx + state->gap);
    int row = (y - top) / cellDy + state->firstRow;
    if (col < 0 || col >= state->cols || row < state->firstRow || row >= state->firstRow + state->visibleRows) {
        return -1;
    }
    int pageNo = row * state->cols + col + 1;
    int cellX = left + col * (state->thumbDx + state->gap);
    int cellY = top + (row - state->firstRow) * cellDy;
    if (x >= cellX + state->thumbDx || y >= cellY + state->thumbDy) {
        return -1;
    }
    return pageNo <= state->pageCount ? pageNo : -1;
}

static void PaintThumbnailNavigation(ThumbnailNavigationState* state, HDC hdc) {
    RECT client;
    GetClientRect(state->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    HDC buffer = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBitmap = SelectObject(buffer, bitmap);
    HGDIOBJ oldFont = SelectObject(buffer, state->font);

    Color background = ThemeControlBackgroundColor();
    HBRUSH backgroundBrush = CreateSolidBrush(background);
    FillRect(buffer, &client, backgroundBrush);
    DeleteObject(backgroundBrush);

    SetBkMode(buffer, TRANSPARENT);
    SetTextColor(buffer, (COLORREF)ThemeWindowTextColor());
    RECT title = {state->padding, 0, client.right - state->padding, state->headerDy};
    DrawTextW(buffer, CWStrTemp(_TRA("Select page (Esc to cancel)")), -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int cellDy = state->thumbDy + state->gap;
    int left = state->padding;
    int top = state->headerDy + state->padding;
    int firstPage = state->firstRow * state->cols + 1;
    int lastPage = std::min(state->pageCount, firstPage + state->visibleRows * state->cols - 1);
    for (int pageNo = firstPage; pageNo <= lastPage; pageNo++) {
        int index = pageNo - 1;
        int col = index % state->cols;
        int row = index / state->cols - state->firstRow;
        int x = left + col * (state->thumbDx + state->gap);
        int y = top + row * cellDy;
        RECT cell = {x, y, x + state->thumbDx, y + state->thumbDy};

        HBRUSH pageBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(buffer, &cell, pageBrush);
        DeleteObject(pageBrush);
        Pixmap* thumbnail = ThumbnailToDraw(state->cache, index);
        if (thumbnail) {
            int drawDx = std::min(thumbnail->width, state->thumbDx);
            int drawDy = std::min(thumbnail->height, state->thumbDy);
            Rect target{x + (state->thumbDx - drawDx) / 2, y + (state->thumbDy - drawDy) / 2, drawDx, drawDy};
            BlitPixmap(thumbnail, buffer, target);
        }

        HPEN pen = CreatePen(PS_SOLID, pageNo == state->selectedPage ? 3 : 1,
                             pageNo == state->selectedPage ? RGB(0, 120, 215) : RGB(150, 150, 150));
        HGDIOBJ oldPen = SelectObject(buffer, pen);
        HGDIOBJ oldBrush = SelectObject(buffer, GetStockObject(NULL_BRUSH));
        Rectangle(buffer, cell.left, cell.top, cell.right, cell.bottom);
        SelectObject(buffer, oldBrush);
        SelectObject(buffer, oldPen);
        DeleteObject(pen);

        RECT label = {x, y + state->thumbDy + 2, x + state->thumbDx, y + cellDy};
        TempStr pageText = fmt("%d", pageNo);
        DrawTextW(buffer, CWStrTemp(ToWStrTemp(pageText)), -1, &label, DT_CENTER | DT_TOP | DT_SINGLELINE);
    }

    BitBlt(hdc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, oldFont);
    SelectObject(buffer, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
}

static void MoveThumbnailSelection(ThumbnailNavigationState* state, int dx, int dy) {
    int pageNo = state->selectedPage;
    if (dx != 0) {
        pageNo += dx;
    } else {
        pageNo += dy * state->cols;
    }
    pageNo = std::max(1, std::min(pageNo, state->pageCount));
    SelectThumbnailPage(state, pageNo);
}

static void OnThumbnailVScroll(ThumbnailNavigationState* state, WPARAM wp) {
    if (!state->scrollbar) {
        return;
    }
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    OverlayScrollbarGetInfo(state->scrollbar, &si);
    int cellDy = state->thumbDy + state->gap;
    int scrollY = si.nPos;
    switch (LOWORD(wp)) {
        case SB_TOP:
            scrollY = si.nMin;
            break;
        case SB_BOTTOM:
            scrollY = si.nMax;
            break;
        case SB_LINEUP:
            scrollY -= cellDy;
            break;
        case SB_LINEDOWN:
            scrollY += cellDy;
            break;
        case SB_PAGEUP:
            scrollY -= (int)si.nPage;
            break;
        case SB_PAGEDOWN:
            scrollY += (int)si.nPage;
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            scrollY = si.nTrackPos;
            break;
        default:
            return;
    }
    int maxRow = std::max(0, ThumbnailTotalRows(state) - state->visibleRows);
    int row = std::max(0, std::min(scrollY / cellDy, maxRow));
    // OverlayScrollbar doesn't advance nPos itself for the line / page codes,
    // the owner computes the new position and writes it back (as Canvas does).
    // We only scroll by whole rows, so snap the thumb to the row we picked
    si.fMask = SIF_POS;
    si.nPos = row * cellDy;
    OverlayScrollbarSetInfo(state->scrollbar, &si, true);
    SetThumbnailFirstRow(state, row, false);
}

static LRESULT CALLBACK ThumbnailNavigationWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* state = (ThumbnailNavigationState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lp;
        state = (ThumbnailNavigationState*)cs->lpCreateParams;
        state->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);
    }
    if (!state) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintThumbnailNavigation(state, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KILLFOCUS:
            // a popup that outlives its focus would act on a tab that is no
            // longer current (the user clicked the tab bar or switched
            // windows). Post rather than destroy: we may already be inside our
            // own teardown, which is what took the focus away
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        case WM_KEYDOWN:
            switch (wp) {
                case VK_ESCAPE:
                    DestroyWindow(hwnd);
                    return 0;
                case VK_RETURN:
                    ActivateThumbnailPage(state);
                    return 0;
                case VK_LEFT:
                    MoveThumbnailSelection(state, -1, 0);
                    return 0;
                case VK_RIGHT:
                    MoveThumbnailSelection(state, 1, 0);
                    return 0;
                case VK_UP:
                    MoveThumbnailSelection(state, 0, -1);
                    return 0;
                case VK_DOWN:
                    MoveThumbnailSelection(state, 0, 1);
                    return 0;
                case VK_HOME:
                    SelectThumbnailPage(state, 1);
                    return 0;
                case VK_END:
                    SelectThumbnailPage(state, state->pageCount);
                    return 0;
            }
            break;
        case WM_MOUSEMOVE: {
            int pageNo = PageAtPoint(state, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (pageNo > 0) {
                SelectThumbnailPage(state, pageNo);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            SetThumbnailFirstRow(state, state->firstRow - (delta > 0 ? 3 : -3), true);
            return 0;
        }
        case WM_VSCROLL:
            OnThumbnailVScroll(state, wp);
            return 0;
        case WM_LBUTTONDOWN: {
            int pageNo = PageAtPoint(state, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (pageNo > 0) {
                SelectThumbnailPage(state, pageNo);
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            int pageNo = PageAtPoint(state, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (pageNo > 0) {
                SelectThumbnailPage(state, pageNo);
                ActivateThumbnailPage(state);
            }
            return 0;
        }
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            if (state->cache->hwnd == hwnd) {
                state->cache->hwnd = nullptr;
            }
            // nothing is looking at the thumbnails any more, don't leave a
            // worker rendering the rest of the document in the background
            AtomicIntSet(&state->cache->cancelRendering, 1);
            if (!state->cache->workerRunning) {
                // a running pass is still using the clone; FinishThumbnailWorker
                // drops it when that pass ends
                FreeThumbnailRenderEngine(state->cache);
            }
            OverlayScrollbarDestroy(state->scrollbar);
            state->scrollbar = nullptr;
            delete state;
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// renders the pages around the visible rows that aren't cached yet, and drops
// cached pages far enough away that they'd otherwise pile up without bound
static void StartThumbnailRendering(ThumbnailNavigationState* state) {
    ThumbnailNavigationCache* cache = state->cache;
    if (cache->workerRunning) {
        // FinishThumbnailWorker calls us again for wherever the user has
        // scrolled to by the time this pass ends
        return;
    }
    DisplayModel* dm = state->tab ? state->tab->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return;
    }

    int perScreen = std::max(1, state->visibleRows * state->cols);
    int firstVisible = state->firstRow * state->cols + 1;
    int lastVisible = std::min(state->pageCount, firstVisible + perScreen - 1);
    int firstPage = std::max(1, firstVisible - kThumbnailRenderScreens * perScreen);
    int lastPage = std::min(state->pageCount, lastVisible + kThumbnailRenderScreens * perScreen);

    int keepFirst = std::max(1, firstPage - kThumbnailKeepScreens * perScreen);
    int keepLast = std::min(state->pageCount, lastPage + kThumbnailKeepScreens * perScreen);
    for (int idx = 0; idx < cache->thumbnails.len; idx++) {
        int pageNo = idx + 1;
        if (cache->thumbnails[idx] && (pageNo < keepFirst || pageNo > keepLast)) {
            FreeThumbnail(cache->thumbnails[idx]);
            cache->thumbnails[idx] = nullptr;
        }
    }

    auto* worker = new ThumbnailRenderWorker;
    // visible pages first, then the rows just off-screen in either direction
    for (int pageNo = firstVisible; pageNo <= lastVisible; pageNo++) {
        if (!cache->thumbnails[pageNo - 1]) {
            worker->pages.Append(pageNo);
        }
    }
    for (int pageNo = firstPage; pageNo < firstVisible; pageNo++) {
        if (!cache->thumbnails[pageNo - 1]) {
            worker->pages.Append(pageNo);
        }
    }
    for (int pageNo = lastVisible + 1; pageNo <= lastPage; pageNo++) {
        if (!cache->thumbnails[pageNo - 1]) {
            worker->pages.Append(pageNo);
        }
    }
    if (worker->pages.len == 0) {
        delete worker;
        return;
    }

    worker->cache = cache;
    worker->rotation = cache->rotation;
    worker->thumbDx = cache->thumbDx;
    worker->thumbDy = cache->thumbDy;
    if (!cache->renderEngine) {
        // the worker makes the clone (Clone() re-reads the document, which is
        // slow for an archive) and hands it back through cache->renderEngine
        worker->sourceEngine = engine;
        engine->AddRef();
    }
    // a previous pass may have been cancelled by closing the overlay
    AtomicIntSet(&cache->cancelRendering, 0);
    cache->workerRunning = true;
    RunAsync(MkFunc0<ThumbnailRenderWorker>(RenderThumbnailsInBackground, worker), StrL("ThumbnailNavigationRender"));
}

void ShowThumbnailNavigation(MainWindow* win) {
    if (!win || !win->AsFixed()) {
        return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = ThumbnailNavigationWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kThumbnailNavigationClassName;
        if (!RegisterClassExW(&wc)) {
            return;
        }
        registered = true;
    }

    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->AsFixed()) {
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    ThumbnailNavigationCache* cache = tab->thumbnailNavigationCache;
    if (cache && cache->hwnd) {
        SetForegroundWindow(cache->hwnd);
        SetFocus(cache->hwnd);
        return;
    }

    int pageCount = dm->PageCount();
    int rotation = dm->GetRotation();
    int thumbDx = DpiScale(kThumbnailDx);
    int thumbDy = DpiScale(kThumbnailDy);
    if (cache && (cache->pageCount != pageCount || cache->rotation != rotation || cache->thumbDx != thumbDx ||
                  cache->thumbDy != thumbDy)) {
        // the document was reloaded, rotated or moved to a monitor with a
        // different DPI: the cached thumbnails no longer describe it, and one
        // sized for the old page count would be indexed past its end
        FreeThumbnailNavigationCache(tab);
        cache = nullptr;
    }
    if (!cache) {
        cache = new ThumbnailNavigationCache;
        cache->pageCount = pageCount;
        cache->rotation = rotation;
        cache->thumbDx = thumbDx;
        cache->thumbDy = thumbDy;
        cache->thumbnails.AppendBlanks(pageCount);
        tab->thumbnailNavigationCache = cache;
    }

    auto* state = new ThumbnailNavigationState;
    state->win = win;
    state->tab = tab;
    state->cache = cache;
    state->font = GetDefaultGuiFont()->GetHFont();
    state->pageCount = pageCount;
    state->selectedPage = std::max(1, std::min(dm->CurrentPageNo(), state->pageCount));
    state->thumbDx = thumbDx;
    state->thumbDy = thumbDy;
    state->headerDy = DpiScale(kThumbnailNavigationHeaderDy);
    state->gap = DpiScale(kThumbnailNavigationGap);
    state->padding = DpiScale(kThumbnailNavigationPadding);

    Rect frameRect = HwndWindowRect(win->hwndFrame);
    int availableDx = frameRect.dx - 2 * state->padding;
    state->cols =
        std::max(1, std::min(kThumbnailNavigationMaxCols, (availableDx + state->gap) / (state->thumbDx + state->gap)));
    int maxRows = std::max(
        1, (frameRect.dy * 3 / 4 - state->headerDy - 2 * state->padding + state->gap) / (state->thumbDy + state->gap));
    int totalRows = (state->pageCount + state->cols - 1) / state->cols;
    state->visibleRows = std::min(maxRows, std::max(1, totalRows));
    int cellDy = state->thumbDy + state->gap;
    state->maxScrollY = std::max(0, (totalRows - state->visibleRows) * cellDy);
    int selectedRow = (state->selectedPage - 1) / state->cols;
    state->firstRow = std::max(0, std::min(selectedRow - state->visibleRows / 2, totalRows - state->visibleRows));

    int width = 2 * state->padding + state->cols * state->thumbDx + (state->cols - 1) * state->gap;
    int height = state->headerDy + 2 * state->padding + state->visibleRows * state->thumbDy +
                 (state->visibleRows - 1) * state->gap;
    int x = frameRect.x + (frameRect.dx - width) / 2;
    int y = frameRect.y + (frameRect.dy - height) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kThumbnailNavigationClassName, nullptr, WS_POPUP | WS_BORDER, x, y,
                                width, height, win->hwndFrame, nullptr, GetModuleHandleW(nullptr), state);
    if (!hwnd) {
        delete state;
        return;
    }
    cache->hwnd = hwnd;
    HwndSetFont(hwnd, state->font);
    if (state->maxScrollY > 0) {
        state->scrollbar = OverlayScrollbarCreate(hwnd, OverlayScrollbar::Type::Vert, OverlayScrollbar::Mode::Smart);
        if (state->scrollbar) {
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
            si.nMin = 0;
            // the largest position a scrollbar hands back is nMax - nPage + 1,
            // so this makes the last row exactly reachable
            si.nMax = state->maxScrollY + state->visibleRows * cellDy - 1;
            si.nPage = state->visibleRows * cellDy;
            si.nPos = state->firstRow * cellDy;
            OverlayScrollbarSetInfo(state->scrollbar, &si, false);
            OverlayScrollbarShow(state->scrollbar, true);
            OverlayScrollbarUpdatePos(state->scrollbar);
        }
    }
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    StartThumbnailRendering(state);
}
