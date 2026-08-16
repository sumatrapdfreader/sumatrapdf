/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "ReaderModel.h"
#include "gui/UIModels.h"
#include "gui/Gfx.h"
#include "gui/PlatformCanvas.h"
#include "gui/DocumentView.h"

struct CachedDocumentPage {
    Pixmap* pixmap = nullptr;
    float zoom = 0;
    int rotation = 0;
};

struct DocumentViewData {
    ReaderModel* reader = nullptr;
    DocumentLayout layout;
    Vec<CachedDocumentPage> cache;
    Size viewSize;
    Point viewOffset;
    float zoomVirtual = kZoomFitWidth;
    DisplayMode displayMode = DisplayMode::Continuous;
    int startPage = 1;
    int rotation = 0;
    bool isDragging = false;
    Point dragStart;
    Point dragOffset;
};

static DocumentViewData* ViewData(DocumentView* view) {
    return (DocumentViewData*)view->data;
}

static void FreeCache(DocumentViewData* data) {
    for (CachedDocumentPage& page : data->cache) {
        FreePixmap(page.pixmap);
    }
    data->cache.Reset();
}

static DocumentLayoutParams MakeLayoutParams(DocumentViewData* data) {
    DocumentLayoutParams p;
    p.displayMode = data->displayMode;
    p.startPage = data->startPage;
    p.viewPortSize = data->viewSize;
    p.viewPortOffset = data->viewOffset;
    p.zoomVirtual = data->zoomVirtual;
    p.dpiFactor = 72.0f / data->reader->FileDPI();
    p.rotation = data->rotation;
    p.windowMargin = {12, 12, 12, 12};
    p.pageSpacing = Size(0, 14);
    return p;
}

static void Relayout(DocumentView* view, Size viewSize) {
    auto* data = ViewData(view);
    if (!data->reader) {
        data->layout.Reset(0);
        data->viewSize = viewSize;
        data->viewOffset = {};
        return;
    }
    data->viewSize = viewSize;
    data->viewOffset.x = std::max(data->viewOffset.x, 0);
    data->viewOffset.y = std::max(data->viewOffset.y, 0);
    data->reader->Layout(MakeLayoutParams(data), &data->layout);
    data->viewOffset = {data->layout.viewPort.x, data->layout.viewPort.y};
}

static void Invalidate(DocumentView* view) {
    if (view->canvas) {
        view->canvas->Invalidate();
    }
}

static void SetViewOffset(DocumentView* view, Point offset) {
    auto* data = ViewData(view);
    data->viewOffset = offset;
    Relayout(view, data->viewSize);
    Invalidate(view);
}

static int PageAtPoint(DocumentViewData* data, Point pt) {
    for (int pageNo = 1; pageNo <= len(data->layout.pages); pageNo++) {
        const DocumentLayoutPage* page = data->layout.GetPage(pageNo);
        if (page->isShown && page->pageOnScreen.Contains(pt)) {
            return pageNo;
        }
    }
    return data->layout.CurrentPageNo();
}

static void OnPaint(DocumentView* view, PlatformCanvasPaintEvent* ev) {
    auto* data = ViewData(view);
    Size size = ev->clientRect.Size();
    if (size != data->viewSize) {
        Relayout(view, size);
    }

    ev->gfx->FillRect(ev->clientRect, MkRgb(78, 81, 86));
    if (!data->reader) {
        return;
    }

    for (int pageNo = 1; pageNo <= len(data->layout.pages); pageNo++) {
        DocumentLayoutPage* page = data->layout.GetPage(pageNo);
        CachedDocumentPage& cached = data->cache[pageNo - 1];
        if (!page->isShown || page->visibleRatio <= 0) {
            FreePixmap(cached.pixmap);
            cached = {};
            continue;
        }

        Rect target = page->pageOnScreen;
        Rect shadow = target;
        shadow.Offset(3, 3);
        ev->gfx->FillRect(shadow, MkGray(40));
        ev->gfx->FillRect(target, kColWhite);

        if (!cached.pixmap || cached.zoom != page->zoomReal || cached.rotation != data->rotation) {
            FreePixmap(cached.pixmap);
            cached.pixmap = data->reader->RenderPage(pageNo, page->zoomReal, data->rotation);
            cached.zoom = page->zoomReal;
            cached.rotation = data->rotation;
        }
        if (cached.pixmap) {
            ev->gfx->DrawPixmap(cached.pixmap, target);
        }
        ev->gfx->DrawRect(target, MkGray(155));
    }
}

static void OnCanvasPaint(DocumentView* view, PlatformCanvasPaintEvent* ev) {
    OnPaint(view, ev);
}

static void ZoomBy(DocumentView* view, bool zoomIn, Point anchor) {
    auto* data = ViewData(view);
    if (!data->reader) {
        return;
    }
    float zoom = data->zoomVirtual;
    if (zoom <= 0) {
        float dpiFactor = 72.0f / data->reader->FileDPI();
        zoom = dpiFactor > 0 ? data->layout.zoomReal * 100.0f / dpiFactor : kZoomActualSize;
    }
    zoom *= zoomIn ? 1.2f : 1.0f / 1.2f;
    zoom = limitValue(zoom, kZoomMin, kZoomMax);
    view->SetZoom(zoom, &anchor);
}

static void OnCanvasScroll(DocumentView* view, PlatformCanvasScrollEvent* ev) {
    auto* data = ViewData(view);
    if (!data->reader) {
        return;
    }
    if (ev->isCtrl) {
        ZoomBy(view, ev->deltaY < 0, ev->pos);
    } else {
        Point offset = data->viewOffset;
        offset.x += (int)(ev->deltaX * 55.0);
        offset.y += (int)(ev->deltaY * 55.0);
        SetViewOffset(view, offset);
    }
    ev->didHandle = true;
}

static void OnCanvasPointer(DocumentView* view, PlatformCanvasPointerEvent* ev) {
    auto* data = ViewData(view);
    if (ev->type == PlatformCanvasPointerEventType::Down && ev->button == 1) {
        data->isDragging = true;
        data->dragStart = ev->pos;
        data->dragOffset = data->viewOffset;
        view->canvas->Focus();
        view->canvas->SetCursor(CursorId::Move);
        ev->didHandle = true;
    } else if (ev->type == PlatformCanvasPointerEventType::Move && data->isDragging) {
        Point delta(ev->pos.x - data->dragStart.x, ev->pos.y - data->dragStart.y);
        SetViewOffset(view, Point(data->dragOffset.x - delta.x, data->dragOffset.y - delta.y));
        ev->didHandle = true;
    } else if (ev->type == PlatformCanvasPointerEventType::Up && data->isDragging) {
        data->isDragging = false;
        view->canvas->SetCursor(CursorId::Arrow);
        ev->didHandle = true;
    }
}

static void ScrollBy(DocumentView* view, int dx, int dy) {
    auto* data = ViewData(view);
    SetViewOffset(view, Point(data->viewOffset.x + dx, data->viewOffset.y + dy));
}

static void OnCanvasKey(DocumentView* view, PlatformCanvasKeyEvent* ev) {
    auto* data = ViewData(view);
    if (!data->reader) {
        return;
    }
    int pageStep = std::max(data->viewSize.dy - 60, 60);
    switch (ev->key) {
        case PlatformKey::Home:
            view->GoToPage(1);
            ev->didHandle = true;
            return;
        case PlatformKey::End:
            view->GoToPage(data->reader->PageCount());
            ev->didHandle = true;
            return;
        case PlatformKey::PageUp:
            if (IsContinuous(data->displayMode)) {
                ScrollBy(view, 0, -pageStep);
            } else {
                view->GoToPage(data->startPage - 1);
            }
            ev->didHandle = true;
            return;
        case PlatformKey::PageDown:
            if (IsContinuous(data->displayMode)) {
                ScrollBy(view, 0, pageStep);
            } else {
                view->GoToPage(data->startPage + 1);
            }
            ev->didHandle = true;
            return;
        case PlatformKey::Left:
            ScrollBy(view, -45, 0);
            ev->didHandle = true;
            return;
        case PlatformKey::Up:
            ScrollBy(view, 0, -45);
            ev->didHandle = true;
            return;
        case PlatformKey::Right:
            ScrollBy(view, 45, 0);
            ev->didHandle = true;
            return;
        case PlatformKey::Down:
            ScrollBy(view, 0, 45);
            ev->didHandle = true;
            return;
        case PlatformKey::Plus:
            ZoomBy(view, true, Point(data->viewSize.dx / 2, data->viewSize.dy / 2));
            ev->didHandle = true;
            return;
        case PlatformKey::Minus:
            ZoomBy(view, false, Point(data->viewSize.dx / 2, data->viewSize.dy / 2));
            ev->didHandle = true;
            return;
        case PlatformKey::Zero:
            if (ev->isCtrl) {
                view->SetZoom(kZoomActualSize);
                ev->didHandle = true;
            }
            return;
        default:
            break;
    }

    int c = ev->codepoint;
    if (c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
    }
    if (c == 'c') {
        view->SetContinuous(!view->IsContinuous());
    } else if (c == 'f') {
        view->SetZoom(kZoomFitPage);
    } else if (c == 'w') {
        view->SetZoom(kZoomFitWidth);
    } else if (c == '1') {
        view->SetZoom(kZoomActualSize);
    } else if (c == 'r') {
        view->RotateBy(ev->isShift ? -90 : 90);
    } else {
        return;
    }
    ev->didHandle = true;
}

DocumentView* DocumentView::Create() {
    auto* view = new DocumentView();
    view->data = new DocumentViewData();
    view->canvas = PlatformCanvas::Create(view);
    if (!view->canvas) {
        delete ViewData(view);
        delete view;
        return nullptr;
    }
    view->canvas->onPaint = MkFunc1(OnCanvasPaint, view);
    view->canvas->onPointer = MkFunc1(OnCanvasPointer, view);
    view->canvas->onScroll = MkFunc1(OnCanvasScroll, view);
    view->canvas->onKey = MkFunc1(OnCanvasKey, view);
    return view;
}

DocumentView::~DocumentView() {
    auto* viewData = ViewData(this);
    if (viewData) {
        FreeCache(viewData);
        delete viewData->reader;
        delete viewData;
    }
    delete canvas;
    canvas = nullptr;
    data = nullptr;
}

bool DocumentView::Open(Str path) {
    auto* viewData = ViewData(this);
    ReaderModel* reader = ReaderModel::Create(path);
    if (!reader) {
        return false;
    }
    FreeCache(viewData);
    delete viewData->reader;
    viewData->reader = reader;
    VecResize(viewData->cache, reader->PageCount());
    viewData->viewOffset = {};
    viewData->startPage = 1;
    Relayout(this, canvas->ClientRect().Size());
    Invalidate(this);
    return true;
}

void* DocumentView::NativeWidget() const {
    return canvas ? canvas->NativeWidget() : nullptr;
}

void DocumentView::Focus() {
    if (canvas) {
        canvas->Focus();
    }
}

int DocumentView::PageCount() const {
    auto* viewData = ViewData((DocumentView*)this);
    return viewData->reader ? viewData->reader->PageCount() : 0;
}

int DocumentView::CurrentPageNo() const {
    auto* viewData = ViewData((DocumentView*)this);
    return viewData->reader ? viewData->layout.CurrentPageNo() : 0;
}

void DocumentView::GoToPage(int pageNo) {
    auto* viewData = ViewData(this);
    if (!viewData->reader) {
        return;
    }
    pageNo = limitValue(pageNo, 1, viewData->reader->PageCount());
    viewData->startPage = pageNo;
    if (::IsContinuous(viewData->displayMode)) {
        DocumentLayoutPage* page = viewData->layout.GetPage(pageNo);
        if (page) {
            viewData->viewOffset.y = page->pos.y;
        }
    } else {
        viewData->viewOffset = {};
    }
    Relayout(this, viewData->viewSize);
    Invalidate(this);
}

void DocumentView::SetContinuous(bool continuous) {
    auto* viewData = ViewData(this);
    if (!viewData->reader || continuous == ::IsContinuous(viewData->displayMode)) {
        return;
    }
    viewData->startPage = viewData->layout.CurrentPageNo();
    viewData->displayMode = continuous ? DisplayMode::Continuous : DisplayMode::SinglePage;
    viewData->viewOffset = {};
    Relayout(this, viewData->viewSize);
    Invalidate(this);
}

bool DocumentView::IsContinuous() const {
    return ::IsContinuous(ViewData((DocumentView*)this)->displayMode);
}

void DocumentView::SetZoom(float zoomVirtual, Point* anchor) {
    auto* viewData = ViewData(this);
    if (!viewData->reader || !IsValidZoom(zoomVirtual)) {
        return;
    }

    Point fix = anchor ? *anchor : Point(viewData->viewSize.dx / 2, viewData->viewSize.dy / 2);
    int pageNo = PageAtPoint(viewData, fix);
    DocumentLayoutPage* oldPage = viewData->layout.GetPage(pageNo);
    float relX = oldPage && oldPage->pageOnScreen.dx > 0
                     ? (float)(fix.x - oldPage->pageOnScreen.x) / oldPage->pageOnScreen.dx
                     : 0.5f;
    float relY = oldPage && oldPage->pageOnScreen.dy > 0
                     ? (float)(fix.y - oldPage->pageOnScreen.y) / oldPage->pageOnScreen.dy
                     : 0.5f;

    viewData->zoomVirtual = zoomVirtual;
    FreeCache(viewData);
    VecResize(viewData->cache, viewData->reader->PageCount());
    Relayout(this, viewData->viewSize);
    DocumentLayoutPage* newPage = viewData->layout.GetPage(pageNo);
    if (anchor && newPage) {
        viewData->viewOffset.x = newPage->pos.x + (int)(relX * newPage->pos.dx) - fix.x;
        viewData->viewOffset.y = newPage->pos.y + (int)(relY * newPage->pos.dy) - fix.y;
        Relayout(this, viewData->viewSize);
    }
    Invalidate(this);
}

float DocumentView::Zoom() const {
    return ViewData((DocumentView*)this)->zoomVirtual;
}

void DocumentView::RotateBy(int degrees) {
    auto* viewData = ViewData(this);
    if (!viewData->reader) {
        return;
    }
    viewData->rotation = NormalizeRotation(viewData->rotation + degrees);
    FreeCache(viewData);
    VecResize(viewData->cache, viewData->reader->PageCount());
    Relayout(this, viewData->viewSize);
    Invalidate(this);
}

int DocumentView::Rotation() const {
    return ViewData((DocumentView*)this)->rotation;
}
