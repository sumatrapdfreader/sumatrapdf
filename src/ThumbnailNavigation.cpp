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

struct ThumbnailNavigationCache {
    Vec<Pixmap*> thumbnails;
    HWND hwnd = nullptr;
    AtomicInt cancelRendering = 0;
    int pageCount = 0;
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
    int maxScrollY = 0;
    OverlayScrollbar* scrollbar = nullptr;
};

static const WCHAR* kThumbnailNavigationClassName = L"SumatraPDF_ThumbnailNavigation";

static void DeleteThumbnailNavigationCache(ThumbnailNavigationCache* cache) {
    for (Pixmap* bitmap : cache->thumbnails) {
        FreePixmap(bitmap);
    }
    cache->thumbnails.Reset();
    delete cache;
}

struct ThumbnailRenderTask {
    ThumbnailNavigationCache* cache = nullptr;
    int pageNo = 0;
    Pixmap* bitmap = nullptr;
};

struct ThumbnailRenderWorker {
    ThumbnailNavigationCache* cache = nullptr;
    EngineBase* sourceEngine = nullptr;
    int pageCount = 0;
    int firstVisiblePage = 0;
    int lastVisiblePage = 0;
    int rotation = 0;
    int thumbDx = 0;
    int thumbDy = 0;
};

static Pixmap* RenderPageThumbnail(EngineBase* engine, int pageNo, int rotation, int thumbDx, int thumbDy) {
    RectF pageRect = engine->PageMediabox(pageNo);
    if (pageRect.IsEmpty()) {
        return nullptr;
    }

    pageRect = engine->Transform(pageRect, pageNo, 1.0f, 0);
    if (pageRect.dx <= 0 || pageRect.dy <= 0) {
        return nullptr;
    }
    float zoom = (float)thumbDx / pageRect.dx;
    pageRect.dy = std::min(pageRect.dy, (float)thumbDy / zoom);
    pageRect = engine->Transform(pageRect, pageNo, 1.0f, 0, true);
    RenderPageArgs args(pageNo, zoom, rotation, &pageRect, RenderTarget::View);
    return engine->RenderPage(args);
}

static void FinishThumbnailRender(ThumbnailRenderTask* task) {
    ThumbnailNavigationCache* cache = task->cache;
    if (!cache->deleteWhenWorkerFinishes && task->bitmap) {
        FreePixmap(cache->thumbnails[task->pageNo - 1]);
        cache->thumbnails[task->pageNo - 1] = task->bitmap;
        task->bitmap = nullptr;
        if (cache->hwnd) {
            InvalidateRect(cache->hwnd, nullptr, FALSE);
        }
    }
    FreePixmap(task->bitmap);
    delete task;
}

static void FinishThumbnailWorker(ThumbnailNavigationCache* cache) {
    cache->workerRunning = false;
    if (cache->deleteWhenWorkerFinishes) {
        DeleteThumbnailNavigationCache(cache);
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
    EngineBase* engine = worker->sourceEngine->Clone();
    worker->sourceEngine->Release();
    if (engine && !ShouldCancelThumbnailRendering(worker)) {
        for (int pageNo = worker->firstVisiblePage;
             pageNo <= worker->lastVisiblePage && !ShouldCancelThumbnailRendering(worker); pageNo++) {
            RenderAndPostThumbnail(worker, engine, pageNo);
        }
        for (int pageNo = 1; pageNo < worker->firstVisiblePage && !ShouldCancelThumbnailRendering(worker); pageNo++) {
            RenderAndPostThumbnail(worker, engine, pageNo);
        }
        for (int pageNo = worker->lastVisiblePage + 1;
             pageNo <= worker->pageCount && !ShouldCancelThumbnailRendering(worker); pageNo++) {
            RenderAndPostThumbnail(worker, engine, pageNo);
        }
    }
    if (engine) {
        engine->Release();
    }
    uitask::Post(MkFunc0<ThumbnailNavigationCache>(FinishThumbnailWorker, worker->cache));
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

static void SelectThumbnailPage(ThumbnailNavigationState* state, int pageNo) {
    if (pageNo < 1 || pageNo > state->pageCount) {
        return;
    }
    state->selectedPage = pageNo;
    int row = (pageNo - 1) / state->cols;
    if (row < state->firstRow) {
        state->firstRow = row;
    } else if (row >= state->firstRow + state->visibleRows) {
        state->firstRow = row - state->visibleRows + 1;
    }
    if (state->scrollbar) {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        si.nPos = state->firstRow * (state->thumbDy + kThumbnailNavigationGap);
        OverlayScrollbarSetInfo(state->scrollbar, &si, true);
    }
    InvalidateRect(state->hwnd, nullptr, FALSE);
}

static void ActivateThumbnailPage(ThumbnailNavigationState* state) {
    if (!state || !state->win || !state->tab || !state->tab->AsFixed()) {
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
    int cellDy = state->thumbDy + kThumbnailNavigationGap;
    int left = kThumbnailNavigationPadding;
    int top = kThumbnailNavigationHeaderDy + kThumbnailNavigationPadding;
    if (x < left || y < top) {
        return -1;
    }
    int col = (x - left) / (state->thumbDx + kThumbnailNavigationGap);
    int row = (y - top) / cellDy + state->firstRow;
    if (col < 0 || col >= state->cols || row < state->firstRow || row >= state->firstRow + state->visibleRows) {
        return -1;
    }
    int pageNo = row * state->cols + col + 1;
    int cellX = left + col * (state->thumbDx + kThumbnailNavigationGap);
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
    RECT title = {kThumbnailNavigationPadding, 0, client.right - kThumbnailNavigationPadding,
                  kThumbnailNavigationHeaderDy};
    DrawTextW(buffer, CWStrTemp(_TRA("Select page (Esc to cancel)")), -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int cellDy = state->thumbDy + kThumbnailNavigationGap;
    int left = kThumbnailNavigationPadding;
    int top = kThumbnailNavigationHeaderDy + kThumbnailNavigationPadding;
    int firstPage = state->firstRow * state->cols + 1;
    int lastPage = std::min(state->pageCount, firstPage + state->visibleRows * state->cols - 1);
    for (int pageNo = firstPage; pageNo <= lastPage; pageNo++) {
        int index = pageNo - 1;
        int col = index % state->cols;
        int row = index / state->cols - state->firstRow;
        int x = left + col * (state->thumbDx + kThumbnailNavigationGap);
        int y = top + row * cellDy;
        RECT cell = {x, y, x + state->thumbDx, y + state->thumbDy};

        HBRUSH pageBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(buffer, &cell, pageBrush);
        DeleteObject(pageBrush);
        if (state->cache->thumbnails[index]) {
            Pixmap* thumbnail = state->cache->thumbnails[index];
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
            int row = state->firstRow - (delta > 0 ? 3 : -3);
            int totalRows = (state->pageCount + state->cols - 1) / state->cols;
            row = std::max(0, std::min(row, std::max(0, totalRows - state->visibleRows)));
            if (row != state->firstRow) {
                state->firstRow = row;
                if (state->scrollbar) {
                    SCROLLINFO si{};
                    si.cbSize = sizeof(si);
                    si.fMask = SIF_POS;
                    si.nPos = row * (state->thumbDy + kThumbnailNavigationGap);
                    OverlayScrollbarSetInfo(state->scrollbar, &si, true);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_VSCROLL: {
            if (state->scrollbar) {
                SCROLLINFO si{};
                si.cbSize = sizeof(si);
                si.fMask = SIF_ALL;
                OverlayScrollbarGetInfo(state->scrollbar, &si);
                int scrollY = si.nPos;
                if (LOWORD(wp) == SB_THUMBTRACK) {
                    scrollY = si.nTrackPos;
                }
                int row = scrollY / (state->thumbDy + kThumbnailNavigationGap);
                int totalRows = (state->pageCount + state->cols - 1) / state->cols;
                state->firstRow = std::max(0, std::min(row, std::max(0, totalRows - state->visibleRows)));
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
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
            OverlayScrollbarDestroy(state->scrollbar);
            state->scrollbar = nullptr;
            delete state;
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
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
    if (!cache) {
        cache = new ThumbnailNavigationCache;
        cache->pageCount = dm->PageCount();
        cache->thumbnails.AppendBlanks(cache->pageCount);
        tab->thumbnailNavigationCache = cache;
    }

    auto* state = new ThumbnailNavigationState;
    state->win = win;
    state->tab = tab;
    state->cache = cache;
    state->font = GetDefaultGuiFont()->GetHFont();
    state->pageCount = dm->PageCount();
    state->selectedPage = std::max(1, std::min(dm->CurrentPageNo(), state->pageCount));
    state->thumbDx = DpiScale(kThumbnailDx);
    state->thumbDy = DpiScale(kThumbnailDy);

    Rect frameRect = HwndWindowRect(win->hwndFrame);
    int availableDx = frameRect.dx - 2 * kThumbnailNavigationPadding;
    state->cols = std::max(1, std::min(kThumbnailNavigationMaxCols, (availableDx + kThumbnailNavigationGap) /
                                                                        (state->thumbDx + kThumbnailNavigationGap)));
    int maxRows = std::max(1, (frameRect.dy * 3 / 4 - kThumbnailNavigationHeaderDy - 2 * kThumbnailNavigationPadding +
                               kThumbnailNavigationGap) /
                                  (state->thumbDy + kThumbnailNavigationGap));
    int totalRows = (state->pageCount + state->cols - 1) / state->cols;
    state->visibleRows = std::min(maxRows, std::max(1, totalRows));
    int cellDy = state->thumbDy + kThumbnailNavigationGap;
    state->maxScrollY = std::max(0, (totalRows - state->visibleRows) * cellDy);
    int selectedRow = (state->selectedPage - 1) / state->cols;
    state->firstRow = std::max(0, std::min(selectedRow - state->visibleRows / 2, totalRows - state->visibleRows));

    int width =
        2 * kThumbnailNavigationPadding + state->cols * state->thumbDx + (state->cols - 1) * kThumbnailNavigationGap;
    int height = kThumbnailNavigationHeaderDy + 2 * kThumbnailNavigationPadding + state->visibleRows * state->thumbDy +
                 (state->visibleRows - 1) * kThumbnailNavigationGap;
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
            si.nMax = state->maxScrollY + state->visibleRows * cellDy;
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

    if (cache->workerRunning) {
        return;
    }
    bool needsRendering = false;
    for (Pixmap* thumbnail : cache->thumbnails) {
        if (!thumbnail) {
            needsRendering = true;
            break;
        }
    }
    if (!needsRendering) {
        return;
    }

    auto* worker = new ThumbnailRenderWorker;
    worker->cache = cache;
    worker->sourceEngine = dm->GetEngine();
    worker->pageCount = state->pageCount;
    worker->firstVisiblePage = state->firstRow * state->cols + 1;
    worker->lastVisiblePage =
        std::min(state->pageCount, worker->firstVisiblePage + state->visibleRows * state->cols - 1);
    worker->rotation = dm->GetRotation();
    worker->thumbDx = state->thumbDx;
    worker->thumbDy = state->thumbDy;
    worker->sourceEngine->AddRef();
    cache->workerRunning = true;
    RunAsync(MkFunc0<ThumbnailRenderWorker>(RenderThumbnailsInBackground, worker), StrL("ThumbnailNavigationRender"));
}
